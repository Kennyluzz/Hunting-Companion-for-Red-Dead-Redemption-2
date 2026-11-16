// Host test for the search box's text matching (src/search_text.hpp).
#include "search_text.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace hunting_companion::search_text;

static int g_failures = 0;

static bool match(const char* haystack, const char* query) {
    char32_t h[512]{};
    char32_t q[64]{};
    const std::size_t hl = fold_utf8(haystack, h, 512);
    const std::size_t ql = fold_utf8(query, q, 64);
    return matches_all_words(h, hl, q, ql);
}

static void expect(bool value, const char* haystack, const char* query) {
    if (match(haystack, query) != value) {
        std::printf("FAIL: \"%s\" %s \"%s\"\n", query, value ? "should find" : "should not find", haystack);
        ++g_failures;
    }
}

int main() {
    expect(true, "Perfect Alligator Skin", "alligator");
    expect(true, "Perfect Alligator Skin", "ALLIGATOR");
    expect(true, "Perfect Alligator Skin", "  skin   perfect ");
    expect(true, "Perfect Alligator Skin", "Alligátor");
    expect(false, "Perfect Alligator Skin", "wolf");
    expect(false, "Perfect Alligator Skin", "");
    expect(false, "Perfect Alligator Skin", "   ");
    expect(true, "Idealna skóra aligatora", "skora aligator");     // Polish
    expect(true, "Idealna skóra aligatora", "SKÓRA");
    expect(true, "Żółw", "zolw");                                   // Latin Extended-A
    expect(true, "Peau d’alligator parfaite", "PEAU");
    expect(true, "Безупречная шкура аллигатора", "АЛЛИГАТОР");      // Cyrillic
    expect(true, "Ёж", "еж");
    expect(true, "ワニの皮", "わに");                                 // hiragana finds katakana
    expect(true, "完璧なワニの皮", "ワニ");
    expect(true, "完美的短吻鳄皮", "短吻鳄");
    expect(true, "완벽한 악어 가죽", "악어");
    expect(true, "Perfect Wolf Pelt", "ｗｏｌｆ");                    // full-width
    if (g_failures != 0) return EXIT_FAILURE;
    std::printf("PASS: search matching is case-, accent- and kana-insensitive in every menu script.\n");
    return 0;
}
