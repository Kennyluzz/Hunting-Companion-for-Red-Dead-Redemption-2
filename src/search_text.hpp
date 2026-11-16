#pragma once
// Text matching for the menu's search box: case- and accent-insensitive, for
// every menu language. "aligator", "ALLIGATOR" and "Alligátor" all find
// "Alligator"; hiragana finds katakana; full-width letters find ASCII.

#include <cstddef>
#include <cstdint>

namespace hunting_companion::search_text {

// Base letters for U+00C0..U+00FF (Latin-1) and U+0100..U+017F (Latin
// Extended-A, which covers Polish), lower case and without accents.
inline constexpr char kLatin1Base[] =
    "aaaaaaaceeeeiiiidnooooo ouuuuyts"   // U+00C0..U+00DF
    "aaaaaaaceeeeiiiidnooooo ouuuuyty";  // U+00E0..U+00FF
inline constexpr char kLatinExtendedABase[] =
    "aaaaaa" "cccccccc" "dddd" "eeeeeeeeee" "gggggggg" "hhhh" "iiiiiiiiii" "ii" "jj" "kkk"
    "llllllllll" "nnnnnnnnn" "oooooo" "oo" "rrrrrr" "ssssssss" "tttttt" "uuuuuuuuuuuu" "ww"
    "yyy" "zzzzzz" "s";
static_assert(sizeof(kLatin1Base) == 64 + 1, "Latin-1 table covers U+00C0..U+00FF");
static_assert(sizeof(kLatinExtendedABase) == 128 + 1, "Latin Extended-A table covers U+0100..U+017F");

inline char32_t fold_codepoint(char32_t c) noexcept {
    if (c >= U'A' && c <= U'Z') return c + 0x20;
    if (c >= 0xC0 && c <= 0xFF) return static_cast<unsigned char>(kLatin1Base[c - 0xC0]);
    if (c >= 0x100 && c <= 0x17F) return static_cast<unsigned char>(kLatinExtendedABase[c - 0x100]);
    if (c >= 0x410 && c <= 0x42F) c += 0x20;                 // Cyrillic capitals
    else if (c >= 0x400 && c <= 0x40F) c += 0x50;            // Ѐ..Џ
    if (c == 0x451) return 0x435;                            // ё -> е
    if (c == 0x439) return 0x438;                            // й -> и
    if (c >= 0xFF01 && c <= 0xFF5E) return fold_codepoint(c - 0xFEE0);  // full-width ASCII
    if (c == 0x3000) return U' ';                            // ideographic space
    if (c >= 0x3041 && c <= 0x3096) return c + 0x60;         // hiragana -> katakana
    if (c == U'\t' || c == U'\n' || c == U'\r') return U' ';
    return c;
}

// Decodes UTF-8 and folds it into `out`; runs of spaces become one space.
// Returns the folded length (at most `capacity`).
inline std::size_t fold_utf8(const char* text, char32_t* out, std::size_t capacity) noexcept {
    std::size_t length = 0;
    if (text == nullptr || out == nullptr) return 0;
    const auto* p = reinterpret_cast<const unsigned char*>(text);
    while (*p != 0 && length < capacity) {
        char32_t c = *p;
        int extra = 0;
        if (c >= 0xF0) { c &= 0x07; extra = 3; }
        else if (c >= 0xE0) { c &= 0x0F; extra = 2; }
        else if (c >= 0xC0) { c &= 0x1F; extra = 1; }
        else if (c >= 0x80) { ++p; continue; }  // stray continuation byte
        ++p;
        bool valid = true;
        for (int i = 0; i < extra; ++i) {
            if ((*p & 0xC0) != 0x80) { valid = false; break; }
            c = (c << 6) | (*p & 0x3F);
            ++p;
        }
        if (!valid) continue;
        const char32_t folded = fold_codepoint(c);
        if (folded == U' ' && (length == 0 || out[length - 1] == U' ')) continue;
        out[length++] = folded;
    }
    if (length > 0 && out[length - 1] == U' ') --length;
    return length;
}

inline bool contains(const char32_t* haystack, std::size_t haystackLength,
                     const char32_t* needle, std::size_t needleLength) noexcept {
    if (needleLength == 0) return true;
    if (needleLength > haystackLength) return false;
    for (std::size_t start = 0; start + needleLength <= haystackLength; ++start) {
        std::size_t i = 0;
        while (i < needleLength && haystack[start + i] == needle[i]) ++i;
        if (i == needleLength) return true;
    }
    return false;
}

// True when every space-separated word of the folded query appears somewhere in
// the folded haystack ("wolf perfect" finds "Perfect Wolf Pelt").
inline bool matches_all_words(const char32_t* haystack, std::size_t haystackLength,
                              const char32_t* query, std::size_t queryLength) noexcept {
    std::size_t start = 0;
    bool any = false;
    while (start < queryLength) {
        while (start < queryLength && query[start] == U' ') ++start;
        std::size_t end = start;
        while (end < queryLength && query[end] != U' ') ++end;
        if (end > start) {
            any = true;
            if (!contains(haystack, haystackLength, query + start, end - start)) return false;
        }
        start = end;
    }
    return any;
}

}  // namespace hunting_companion::search_text
