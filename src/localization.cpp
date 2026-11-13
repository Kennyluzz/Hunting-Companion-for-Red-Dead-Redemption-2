#include "localization.hpp"
#include "official_strings.hpp"
#include "embedded_languages.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <string>

namespace hunting_companion {
namespace {
constexpr std::size_t kMaxTranslations = 1024;
constexpr std::size_t kKeyCapacity = 192;
constexpr std::size_t kValueCapacity = 320;

struct Translation { char key[kKeyCapacity]{}; char value[kValueCapacity]{}; };
std::array<Translation, kMaxTranslations> g_translations{};
std::size_t g_translationCount{};
const char* g_locale = "en-US";
int g_activeLanguage = 0;
OfficialStringTable g_officialStrings{};
SRWLOCK g_officialStringsLock = SRWLOCK_INIT;
std::size_t g_officialStringCount{};

constexpr const char* kLocales[] = {
    "en-US", "fr-FR", "de-DE", "it-IT", "es-ES", "pt-BR", "pl-PL",
    "ru-RU", "ko-KR", "zh-TW", "ja-JP", "es-MX", "zh-CN"
};
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

void trim_ascii(std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) { value.clear(); return; }
    const auto last = value.find_last_not_of(" \t\r\n");
    value = value.substr(first, last - first + 1);
}

const Translation* find_translation(const char* key) noexcept {
    if (key == nullptr) return nullptr;
    // The table is sorted by key once per language load; this runs for every
    // visible string on every frame.
    const Translation* first = g_translations.data();
    const Translation* last = first + g_translationCount;
    const Translation* found = std::lower_bound(first, last, key,
        [](const Translation& entry, const char* wanted) noexcept {
            return std::strcmp(entry.key, wanted) < 0;
        });
    return found != last && std::strcmp(found->key, key) == 0 ? found : nullptr;
}
} // namespace

void load_ui_localization(int gameLanguage, int uiLanguageOverride) noexcept {
    g_translations = {};
    g_translationCount = 0;
    g_activeLanguage = (uiLanguageOverride >= 0 && uiLanguageOverride < 13)
        ? uiLanguageOverride : gameLanguage;
    if (g_activeLanguage < 0 || g_activeLanguage >= 13) g_activeLanguage = 0;
    g_locale = kLocales[g_activeLanguage];
    const char* data = embedded_locale_data(g_locale);
    if (data != nullptr) {
        try {
            const char* cursor = data;
            while (*cursor != '\0' && g_translationCount < g_translations.size()) {
                const char* lineEnd = std::strchr(cursor, '\n');
                const std::size_t lineLength = lineEnd != nullptr
                    ? static_cast<std::size_t>(lineEnd - cursor) : std::strlen(cursor);
                std::string line(cursor, lineLength);
                cursor = lineEnd != nullptr ? lineEnd + 1 : cursor + lineLength;
                if (!line.empty() && line.back() == '\r') line.pop_back();
                trim_ascii(line);
                if (line.empty() || line[0] == '#' || line[0] == ';') continue;
                const std::size_t separator = line.find('=');
                if (separator == std::string::npos) continue;
                std::string key = line.substr(0, separator);
                std::string value = line.substr(separator + 1);
                trim_ascii(key); trim_ascii(value);
                if (key.empty() || value.empty() || key.size() >= kKeyCapacity ||
                    value.size() >= kValueCapacity) continue;
                Translation* destination = nullptr;
                for (std::size_t i = 0; i < g_translationCount; ++i) {
                    if (std::strcmp(g_translations[i].key, key.c_str()) == 0) {
                        destination = &g_translations[i]; break;
                    }
                }
                if (destination == nullptr) destination = &g_translations[g_translationCount++];
                std::snprintf(destination->key, sizeof(destination->key), "%s", key.c_str());
                std::snprintf(destination->value, sizeof(destination->value), "%s", value.c_str());
            }
            std::sort(g_translations.begin(), g_translations.begin() + g_translationCount,
                      [](const Translation& a, const Translation& b) noexcept {
                          return std::strcmp(a.key, b.key) < 0;
                      });
        } catch (...) {
            g_translations = {};
            g_translationCount = 0;
        }
    }
    // Select the compact, immutable Rockstar catalogue slice embedded in
    // this ASI. There are no runtime XML, sidecar folder, or language-pack reads.
    AcquireSRWLockExclusive(&g_officialStringsLock);
    g_officialStrings.select_locale(g_locale);
    g_officialStringCount = g_officialStrings.size();
    ReleaseSRWLockExclusive(&g_officialStringsLock);
}

int active_ui_language() noexcept { return g_activeLanguage; }
const char* ui_locale() noexcept { return g_locale; }
const char* locale_name(int language) noexcept {
    return language >= 0 && language < 13 ? kLanguageNames[language] : "English";
}

const char* ui_text(const char* englishText) noexcept {
    if (englishText == nullptr || englishText[0] == '\0') return englishText;
    // Exact source-phrase lookup only. Never translate a phrase by replacing
    // individual words; doing so breaks grammar and official product names.
    if (const Translation* exact = find_translation(englishText)) return exact->value;
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
std::size_t loaded_ui_translation_count() noexcept { return g_translationCount; }

} // namespace hunting_companion
