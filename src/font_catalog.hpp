#pragma once
// Which game fonts each companion language can use, from the game's own
// fontmap.xml (common:/data/ui/fontmap.xml, read at run time; never bundled).
// Platform-neutral so it can be unit-tested on the build host
// (tests/test_font_catalog.cpp).
//
// fontmap.xml maps every font alias ($catalog1, $ledger, ...) to a real font
// family in each of the game's font libraries:
//   <efigs name="$ledger" mapping="RDR Ledger Hand" bold="0" flashFile="efigs" .../>
// Rockstar substitutes a family where a library cannot show it (Russian maps
// $ledger to the Body font, for example), so the distinct families a library
// uses are exactly the fonts that work for that library's languages.

#include <cstddef>
#include <cstring>

namespace hunting_companion::fonts {

// The font library ("flashFile") RDR2 uses for each GET_CURRENT_LANGUAGE value.
inline const char* library_key(int language) noexcept {
    switch (language) {
        case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 11: return "efigs";
        case 7: return "russian";
        case 8: return "korean";
        case 9: return "chinese";
        case 10: return "japanese";
        case 12: return "simplified_chinese";
        default: return nullptr;
    }
}

inline bool is_cjk_library(const char* key) noexcept {
    return key != nullptr && (std::strcmp(key, "korean") == 0 || std::strcmp(key, "japanese") == 0 ||
                              std::strcmp(key, "chinese") == 0 || std::strcmp(key, "simplified_chinese") == 0);
}

struct FontMapEntry {
    char library[24];
    char alias[40];
    char family[64];
    bool bold;
};

class FontMap {
public:
    static constexpr std::size_t kCapacity = 512;

    // Parses the XML. Returns the number of mappings read (0 = unusable).
    std::size_t parse(const char* text, std::size_t length) noexcept {
        count_ = 0;
        const char* end = text + length;
        for (const char* c = text; c < end && count_ < kCapacity; ++c) {
            if (*c != '<') continue;
            const char* tag = c + 1;
            const char* tagEnd = tag;
            while (tagEnd < end && (is_alnum(*tagEnd) || *tagEnd == '_')) ++tagEnd;
            if (tagEnd == tag) continue;  // comment, closing tag, ...
            const char* close = tagEnd;
            while (close < end && *close != '>') ++close;
            if (close >= end) break;
            FontMapEntry entry{};
            bool haveName = false, haveMapping = false, haveFlash = false;
            for (const char* a = tagEnd; a < close;) {
                while (a < close && !is_alnum(*a)) ++a;
                const char* key = a;
                while (a < close && (is_alnum(*a) || *a == '_')) ++a;
                const std::size_t keyLength = static_cast<std::size_t>(a - key);
                while (a < close && *a == ' ') ++a;
                if (a >= close || *a != '=') continue;
                ++a;
                while (a < close && *a == ' ') ++a;
                if (a >= close || *a != '"') continue;
                const char* value = ++a;
                while (a < close && *a != '"') ++a;
                const std::size_t valueLength = static_cast<std::size_t>(a - value);
                if (a < close) ++a;
                if (key_is(key, keyLength, "name")) haveName = copy(entry.alias, value, valueLength);
                else if (key_is(key, keyLength, "mapping")) haveMapping = copy(entry.family, value, valueLength);
                else if (key_is(key, keyLength, "flashFile")) haveFlash = copy(entry.library, value, valueLength);
                else if (key_is(key, keyLength, "bold")) entry.bold = valueLength == 1 && value[0] == '1';
            }
            if (!haveFlash) copy(entry.library, tag, static_cast<std::size_t>(tagEnd - tag));
            if (haveName && haveMapping && entry.alias[0] == '$' && entry.family[0] != '\0')
                entries_[count_++] = entry;
            c = close;
        }
        return count_;
    }

    std::size_t size() const noexcept { return count_; }

    const FontMapEntry* find(const char* library, const char* alias) const noexcept {
        if (library == nullptr || alias == nullptr) return nullptr;
        for (std::size_t i = 0; i < count_; ++i) {
            if (std::strcmp(entries_[i].library, library) == 0 && std::strcmp(entries_[i].alias, alias) == 0)
                return &entries_[i];
        }
        return nullptr;
    }

    bool library_uses_family(const char* library, const char* family) const noexcept {
        if (library == nullptr || family == nullptr) return false;
        for (std::size_t i = 0; i < count_; ++i) {
            if (std::strcmp(entries_[i].library, library) == 0 && std::strcmp(entries_[i].family, family) == 0)
                return true;
        }
        return false;
    }

private:
    static bool is_alnum(char c) noexcept {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
    }
    static bool key_is(const char* key, std::size_t length, const char* wanted) noexcept {
        return std::strlen(wanted) == length && std::strncmp(key, wanted, length) == 0;
    }
    template <std::size_t N>
    static bool copy(char (&out)[N], const char* value, std::size_t length) noexcept {
        if (length == 0 || length >= N) return false;
        std::memcpy(out, value, length);
        out[length] = '\0';
        return true;
    }

    FontMapEntry entries_[kCapacity]{};
    std::size_t count_ = 0;
};

// Aliases offered in the Menu font setting, in display order, with the name shown
// in Settings (Latin/Cyrillic libraries, and Asian ones), a rough text width
// factor relative to the catalogue face (used to fit text on the panel), a size
// factor for faces that draw larger than the others at the same size, and
// whether the font is left out for Russian.
//
// Left out on purpose: $handwritten (1871 Dreamer Script lacks Polish letters
// such as ę), $catalog4 (RDR Catalogue Bold draws far too small to read) and
// $Font5_limited (in Chinese a separate font with a limited set of characters).
// RDR Turri ($wantedPostersGeneric) has no Cyrillic.
struct MenuAlias {
    const char* alias;
    const char* label;
    const char* cjkLabel;
    float widthFactor;
    bool notForRussian;
    float sizeScale = 1.0f;
};
inline constexpr MenuAlias kMenuAliases[] = {
    {"$catalog1", "CATALOGUE", "STANDARD", 1.00f, false},
    {"$body", "BODY", "BODY", 0.94f, false},
    {"$body1", "BODY 1", "BODY 1", 0.94f, false},
    {"$title1", "TITLE", "TITLE", 0.74f, false},
    {"$ledger", "LEDGER", "LEDGER", 0.92f, false},
    // Chalk draws noticeably larger and wider than the other faces.
    {"$chalk", "CHALK", "CHALK", 1.12f, false, 0.86f},
    {"$wantedPostersGeneric", "WANTED POSTER", "WANTED POSTER", 1.05f, true},
    {"$Font5", "REDEMPTION", "DISPLAY", 0.85f, false},
    {"$catalog5", "CATALOGUE SANS", "CATALOGUE SANS", 0.95f, false},
    {"$title", "TITLE", "TITLE", 0.80f, false},
};
inline constexpr std::size_t kMaxMenuFonts = sizeof(kMenuAliases) / sizeof(kMenuAliases[0]);

struct MenuFont {
    const char* alias;    // what to draw with when the game's own library is used
    const char* family;   // the real font (drawn by this name when `byFamily`)
    bool bold;
    float widthFactor;
    const char* label;    // shown in Settings
    float sizeScale = 1.0f;  // applied to the text size when this font is used
};

struct MenuFontList {
    MenuFont fonts[kMaxMenuFonts]{};
    int count = 0;
    bool byFamily = false;   // text names the family (a library the game loaded for us)
};

// Plain utility faces that are not offered as menu fonts.
inline bool utility_family(const char* family) noexcept {
    return std::strncmp(family, "Arial", 5) == 0 || std::strncmp(family, "HelveticaNeue", 13) == 0 ||
           std::strncmp(family, "Rockstar", 8) == 0 || std::strstr(family, "Numbers") != nullptr;
}

// The fonts a companion language can use while the game runs in `gameLanguage`.
//  * Same library as the game: every distinct family of that library, drawn by alias.
//  * Latin <-> Cyrillic: drawn by alias through the game's library, so only the
//    aliases both libraries map to the same family (the Russian set without
//    Ledger, for example).
//  * Any other pairing: the library is loaded by native_fonts and text names the
//    family directly. Families the game's own library also has are left out, as
//    Scaleform would find the game's copy first, which lacks the letters. A CJK
//    library's Latin-only families (shared with the Latin library) are left out.
inline MenuFontList menu_fonts_for(const FontMap& map, int language, int gameLanguage) noexcept {
    MenuFontList list;
    const char* library = library_key(language);
    const char* gameLibrary = library_key(gameLanguage);
    if (library == nullptr || gameLibrary == nullptr || map.size() == 0) return list;
    const bool sameLibrary = std::strcmp(library, gameLibrary) == 0;
    const bool latinCyrillic = !is_cjk_library(library) && !is_cjk_library(gameLibrary);
    list.byFamily = !sameLibrary && !latinCyrillic;
    for (const MenuAlias& candidate : kMenuAliases) {
        const FontMapEntry* entry = map.find(library, candidate.alias);
        if (entry == nullptr || utility_family(entry->family)) continue;
        if (candidate.notForRussian && language == 7) continue;
        if (is_cjk_library(library) && map.library_uses_family("efigs", entry->family)) continue;
        if (!sameLibrary) {
            if (latinCyrillic) {
                const FontMapEntry* drawn = map.find(gameLibrary, candidate.alias);
                if (drawn == nullptr || std::strcmp(drawn->family, entry->family) != 0) continue;
            } else if (map.library_uses_family(gameLibrary, entry->family)) {
                continue;
            }
        }
        bool duplicate = false;
        for (int i = 0; i < list.count; ++i) duplicate |= std::strcmp(list.fonts[i].family, entry->family) == 0;
        if (duplicate) continue;
        list.fonts[list.count++] = MenuFont{entry->alias, entry->family, entry->bold, candidate.widthFactor,
                                            is_cjk_library(library) ? candidate.cjkLabel : candidate.label,
                                            candidate.sizeScale};
    }
    return list;
}

}  // namespace hunting_companion::fonts
