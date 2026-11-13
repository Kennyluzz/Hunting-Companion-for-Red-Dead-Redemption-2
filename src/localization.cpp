#include "localization.hpp"
#include "official_strings.hpp"
#include "embedded_languages.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <iterator>
#include <memory>
#include <vector>

namespace hunting_companion {
namespace {
constexpr int kLanguageCount = 13;

// One parsed UI dictionary. Built once per locale from the embedded text and
// never modified or freed afterwards, so the pointers ui_text() hands out stay
// valid for the whole session, even across language switches and on threads
// other than the one that switched.
struct TranslationEntry { const char* key; const char* value; };
struct TranslationTable {
    std::vector<char> storage;               // the locale text, split in place
    std::vector<TranslationEntry> entries;   // sorted by key, unique keys
};

TranslationTable* g_tables[kLanguageCount]{};  // only touched under g_tablesLock
SRWLOCK g_tablesLock = SRWLOCK_INIT;
std::atomic<const TranslationTable*> g_activeTable{nullptr};
std::atomic<const char*> g_locale{"en-US"};
std::atomic<int> g_activeLanguage{0};
OfficialStringTable g_officialStrings{};
SRWLOCK g_officialStringsLock = SRWLOCK_INIT;
std::size_t g_officialStringCount{};

constexpr const char* kLocales[] = {
    "en-US", "fr-FR", "de-DE", "it-IT", "es-ES", "pt-BR", "pl-PL",
    "ru-RU", "ko-KR", "zh-TW", "ja-JP", "es-MX", "zh-CN"
};
static_assert(std::size(kLocales) == kLanguageCount, "locale table size");
constexpr const char* kLanguageNames[] = {
    "English", "French", "German", "Italian", "Spanish (Spain)",
    "Portuguese (Brazil)", "Polish", "Russian", "Korean",
    "Chinese (Traditional)", "Japanese", "Spanish (Mexico)",
    "Chinese (Simplified)"
};

const char* embedded_locale_data(const char* locale) noexcept {
    for (const auto& blob : embedded::kLocales) {
        if (std::strcmp(blob.locale, locale) == 0) return blob.contents;
    }
    return nullptr;
}

bool is_trim_space(char c) noexcept { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

// Trims [first, last) in place and NUL-terminates it; returns the new start.
char* trim_in_place(char* first, char* last) noexcept {
    while (first < last && is_trim_space(*first)) ++first;
    while (last > first && is_trim_space(last[-1])) --last;
    *last = '\0';
    return first;
}

// "key = value" lines; '#' and ';' start comments; a repeated key keeps the
// last value. The text is copied once and split in place, so nothing is
// truncated and there is no per-line allocation.
void parse_table(const char* text, TranslationTable& table) {
    const std::size_t length = std::strlen(text);
    table.storage.assign(text, text + length + 1);
    char* cursor = table.storage.data();
    char* const end = cursor + length;
    while (cursor < end) {
        char* lineEnd = static_cast<char*>(std::memchr(cursor, '\n', static_cast<std::size_t>(end - cursor)));
        if (lineEnd == nullptr) lineEnd = end;
        char* line = cursor;
        cursor = lineEnd < end ? lineEnd + 1 : end;
        while (line < lineEnd && is_trim_space(*line)) ++line;
        if (line == lineEnd || *line == '#' || *line == ';') continue;
        char* separator = static_cast<char*>(std::memchr(line, '=', static_cast<std::size_t>(lineEnd - line)));
        if (separator == nullptr) continue;
        char* key = trim_in_place(line, separator);
        char* value = trim_in_place(separator + 1, lineEnd);
        if (*key == '\0' || *value == '\0') continue;
        table.entries.push_back(TranslationEntry{key, value});
    }
    // Stable sort keeps file order among equal keys; keep the last of each run.
    std::stable_sort(table.entries.begin(), table.entries.end(),
                     [](const TranslationEntry& a, const TranslationEntry& b) noexcept {
                         return std::strcmp(a.key, b.key) < 0;
                     });
    std::size_t out = 0;
    for (std::size_t i = 0; i < table.entries.size(); ++i) {
        const bool lastOfRun = i + 1 == table.entries.size() ||
                               std::strcmp(table.entries[i].key, table.entries[i + 1].key) != 0;
        if (lastOfRun) table.entries[out++] = table.entries[i];
    }
    table.entries.resize(out);
    table.entries.shrink_to_fit();
}

const TranslationTable* table_for(int language) noexcept {
    AcquireSRWLockExclusive(&g_tablesLock);
    TranslationTable* table = g_tables[language];
    if (table == nullptr) {
        try {
            auto parsed = std::make_unique<TranslationTable>();
            if (const char* data = embedded_locale_data(kLocales[language])) parse_table(data, *parsed);
            table = parsed.release();
            g_tables[language] = table;
        } catch (...) {
            table = nullptr;  // out of memory: show English, retry on the next switch
        }
    }
    ReleaseSRWLockExclusive(&g_tablesLock);
    return table;
}

const TranslationEntry* find_translation(const TranslationTable* table, const char* key) noexcept {
    if (table == nullptr || key == nullptr) return nullptr;
    // Runs for every visible string on every frame: binary search, no copies.
    const auto& entries = table->entries;
    const auto found = std::lower_bound(entries.begin(), entries.end(), key,
        [](const TranslationEntry& entry, const char* wanted) noexcept {
            return std::strcmp(entry.key, wanted) < 0;
        });
    return found != entries.end() && std::strcmp(found->key, key) == 0 ? &*found : nullptr;
}
} // namespace

void load_ui_localization(int gameLanguage, int uiLanguageOverride) noexcept {
    int language = (uiLanguageOverride >= 0 && uiLanguageOverride < kLanguageCount)
        ? uiLanguageOverride : gameLanguage;
    if (language < 0 || language >= kLanguageCount) language = 0;
    const char* locale = kLocales[language];
    g_activeTable.store(table_for(language), std::memory_order_release);
    g_locale.store(locale, std::memory_order_release);
    g_activeLanguage.store(language, std::memory_order_release);
    // Select the compact, immutable Rockstar catalogue slice embedded in
    // this ASI. There are no runtime XML, sidecar folder, or language-pack reads.
    AcquireSRWLockExclusive(&g_officialStringsLock);
    g_officialStrings.select_locale(locale);
    g_officialStringCount = g_officialStrings.size();
    ReleaseSRWLockExclusive(&g_officialStringsLock);
}

int active_ui_language() noexcept { return g_activeLanguage.load(std::memory_order_acquire); }
const char* ui_locale() noexcept { return g_locale.load(std::memory_order_acquire); }
const char* locale_name(int language) noexcept {
    return language >= 0 && language < kLanguageCount ? kLanguageNames[language] : "English";
}

const char* ui_text(const char* englishText) noexcept {
    if (englishText == nullptr || englishText[0] == '\0') return englishText;
    // Exact source-phrase lookup only. Never translate a phrase by replacing
    // individual words; doing so breaks grammar and official product names.
    const TranslationTable* table = g_activeTable.load(std::memory_order_acquire);
    if (const TranslationEntry* exact = find_translation(table, englishText)) return exact->value;
    return englishText;
}

const char* official_item_string(std::uint32_t labelHash) noexcept {
    if (labelHash == 0) return nullptr;
    AcquireSRWLockShared(&g_officialStringsLock);
    const char* value = g_officialStrings.find(labelHash);
    ReleaseSRWLockShared(&g_officialStringsLock);
    return value;
}

std::size_t loaded_official_string_count() noexcept {
    AcquireSRWLockShared(&g_officialStringsLock);
    const std::size_t result = g_officialStringCount;
    ReleaseSRWLockShared(&g_officialStringsLock);
    return result;
}

bool uses_cjk_font(int language) noexcept {
    return language == 8 || language == 9 || language == 10 || language == 12;
}
bool uses_body_font(int language) noexcept {
    return language == 7 || uses_cjk_font(language);
}
std::size_t loaded_ui_translation_count() noexcept {
    const TranslationTable* table = g_activeTable.load(std::memory_order_acquire);
    return table != nullptr ? table->entries.size() : 0;
}

} // namespace hunting_companion
