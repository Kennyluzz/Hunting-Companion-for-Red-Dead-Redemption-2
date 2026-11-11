#include "official_strings.hpp"
#include "embedded_challenge_objective_hashes.hpp"

#include <cassert>
#include <cstring>
#include <iostream>

int main() {
    using hunting_companion::OfficialStringTable;
    OfficialStringTable table;

    // All values below are read from Rockstar's supplied Translation.xml packs,
    // matched by the same stable label ID used by ItemDatabase.
    assert(table.select_locale("en-US"));
    assert(table.size() == 358);
    assert(std::strcmp(table.find(0xC24749A2u), "Perfect Deer Pelt") == 0);
    assert(std::strcmp(table.find(0x158AFE6Eu), "Perfect Armadillo Skin") == 0);
    assert(std::strcmp(table.find(0x2A348502u), "Snake Skin Banjo Head") == 0);
    assert(std::strcmp(table.find(0xCB06E38Du), "Legendary Buck Pelt") == 0);

    // Official challenge labels use the same Rockstar hash lookup as catalogue labels.
    assert(std::strcmp(table.find(0x000DC536u), "Hold up 5 townsfolk") == 0);
    assert(std::strcmp(table.find(0x4EA1E738u), "Pick 6 Yarrow") == 0);

    assert(table.select_locale("fr-FR"));
    assert(std::strcmp(table.find(0x000DC536u), "Braquez 5 citadins.") == 0);
    assert(std::strcmp(table.find(0x4EA1E738u), "Ramassez 6 millefeuilles.") == 0);

    assert(table.select_locale("pl-PL"));
    assert(std::strcmp(table.find(0x000DC536u), "Okradnij 5 miejscowych.") == 0);
    assert(std::strcmp(table.find(0x4EA1E738u), "Zbierz 6 sztuk krwawnika.") == 0);
    assert(std::strcmp(table.find(0xC24749A2u), "Skóra jelenia (idealny stan)") == 0);
    assert(std::strcmp(table.find(0xCB06E38Du), "Skóra legend. samca jelenia") == 0);

    assert(table.select_locale("ko-KR"));
    assert(std::strcmp(table.find(0x000DC536u), "마을 사람 5명을 강탈하십시오.") == 0);
    assert(std::strcmp(table.find(0x4EA1E738u), "톱풀 6개를 채집하십시오.") == 0);
    assert(std::strcmp(table.find(0xC24749A2u), "완벽한 사슴 가죽") == 0);
    assert(std::strcmp(table.find(0xCB06E38Du), "전설의 수사슴 가죽") == 0);

    assert(table.select_locale("ja-JP"));
    assert(std::strcmp(table.find(0x000DC536u), "町民を5人脅せ。") == 0);
    assert(std::strcmp(table.find(0x4EA1E738u), "ノコギリソウを6つ採取しろ。") == 0);
    assert(std::strcmp(table.find(0xC24749A2u), "鹿の毛皮(最良)") == 0);

    assert(table.select_locale("zh-CN"));
    assert(std::strcmp(table.find(0x000DC536u), "抢劫 5 名城镇居民。") == 0);
    assert(std::strcmp(table.find(0x4EA1E738u), "采摘 6 株蓍草。") == 0);
    assert(std::strcmp(table.find(0xC24749A2u), "完美的鹿皮") == 0);

    assert(table.select_locale("zh-TW"));
    assert(std::strcmp(table.find(0x000DC536u), "挾持 5 位城鎮居民。") == 0);
    assert(std::strcmp(table.find(0x4EA1E738u), "採摘 6 株西洋蓍草。") == 0);
    assert(std::strcmp(table.find(0xC24749A2u), "鹿毛皮（完美）") == 0);

    // Every Rockstar-backed challenge objective must resolve in every locale.
    constexpr const char* allLocales[] = {
        "en-US", "fr-FR", "de-DE", "it-IT", "es-ES", "pt-BR", "pl-PL",
        "ru-RU", "ko-KR", "zh-TW", "ja-JP", "es-MX", "zh-CN"
    };
    static_assert(hunting_companion::embedded_challenge::kObjectiveHashCount == 68);
    for (const char* locale : allLocales) {
        assert(table.select_locale(locale));
        for (const auto& objective : hunting_companion::embedded_challenge::kObjectiveHashes) {
            const char* translated = table.find(objective.labelHash);
            assert(translated != nullptr && translated[0] != '\0');
        }
    }

    // Switching the selected language replaces the lookup slice immediately.
    assert(table.select_locale("pt-BR"));
    const char* portugueseBuck = table.find(0xCB06E38Du);
    assert(portugueseBuck != nullptr && std::strcmp(portugueseBuck, "Legendary Buck Pelt") != 0);
    assert(table.select_locale("en-US"));
    assert(std::strcmp(table.find(0xCB06E38Du), "Legendary Buck Pelt") == 0);

    assert(!table.select_locale("not-a-locale"));
    assert(table.size() == 0);
    assert(table.find(0xC24749A2u) == nullptr);

    std::cout << "Embedded official string tests passed (358 hashes x 13 locales, including challenge objectives).\n";
    return 0;
}
