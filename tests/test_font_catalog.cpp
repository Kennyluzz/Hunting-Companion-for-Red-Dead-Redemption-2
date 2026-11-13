// Host test for src/font_catalog.hpp with a small hand-written font map in the
// format of the game's fontmap.xml (the real file is Rockstar's and not included).
#include "font_catalog.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

using namespace hunting_companion::fonts;

namespace {
const char* kMap = R"(
<!-- font mappings -->
<font_lib>
  <efigs name="$catalog1" mapping="Latin Catalogue" bold="0" flashFile="efigs" characterRange="1|2|3|4"/>
  <efigs name="$body"     mapping="Latin Body"      bold="0" flashFile="efigs"/>
  <efigs name="$title1"   mapping="Shared Title"    bold="0" flashFile="efigs"/>
  <efigs name="$ledger"   mapping="Latin Ledger"    bold="0" flashFile="efigs"/>
  <efigs name="$catalog4" mapping="Latin Bold"      bold="1" flashFile="efigs"/>
  <efigs name="$wantedPostersGeneric" mapping="Latin Wanted" bold="1" flashFile="efigs"/>
  <efigs name="$Font2"    mapping="Arial DEBUG"     bold="0" flashFile="efigs"/>
  <russian name="$catalog1" mapping="Latin Catalogue" bold="0" flashFile="russian"/>
  <russian name="$body"     mapping="Latin Body"      bold="0" flashFile="russian"/>
  <russian name="$title1"   mapping="Shared Title"    bold="0" flashFile="russian"/>
  <russian name="$ledger"   mapping="Latin Body"      bold="0" flashFile="russian"/>
  <russian name="$wantedPostersGeneric" mapping="Latin Wanted" bold="1" flashFile="russian"/>
  <japanese name="$catalog1" mapping="JP Gothic"    bold="0" flashFile="japanese"/>
  <japanese name="$body"     mapping="JP Gothic"    bold="0" flashFile="japanese"/>
  <japanese name="$title1"   mapping="JP Title  W7" bold="1" flashFile="japanese"/>
  <japanese name="$ledger"   mapping="Shared Title" bold="0" flashFile="japanese"/>
  <korean name="$catalog1" mapping="KR Myungjo" bold="0" flashFile="korean"/>
  <korean name="$Font5"    mapping="KR Cursive" bold="0" flashFile="korean"/>
</font_lib>
)";

std::string families(const MenuFontList& list) {
    std::string out;
    for (int i = 0; i < list.count; ++i) out += std::string(i ? "|" : "") + list.fonts[i].family;
    return out;
}
}  // namespace

int main() {
    static FontMap map;
    assert(map.parse(kMap, std::strlen(kMap)) == 18);
    assert(std::strcmp(map.find("japanese", "$title1")->family, "JP Title  W7") == 0);  // spacing kept
    assert(map.find("japanese", "$title1")->bold);

    // English on an English game: every distinct non-utility family, by alias.
    MenuFontList en = menu_fonts_for(map, 0, 0);
    // Catalogue Bold ($catalog4) is never offered; the rest are named by role.
    assert(!en.byFamily && families(en) == "Latin Catalogue|Latin Body|Shared Title|Latin Ledger|Latin Wanted");
    assert(std::strcmp(en.fonts[3].alias, "$ledger") == 0 && std::strcmp(en.fonts[3].label, "LEDGER") == 0);
    assert(std::strcmp(en.fonts[0].label, "CATALOGUE") == 0);
    // Russian: Rockstar maps Ledger to Body there, so Ledger is not offered;
    // RDR Turri (wanted poster) has no Cyrillic.
    assert(families(menu_fonts_for(map, 7, 7)) == "Latin Catalogue|Latin Body|Shared Title");
    assert(families(menu_fonts_for(map, 7, 0)) == "Latin Catalogue|Latin Body|Shared Title");
    // Polish on a Russian game: drawn through the Russian library, so no Ledger
    // either, but the wanted-poster font is kept for languages other than Russian.
    assert(families(menu_fonts_for(map, 6, 7)) == "Latin Catalogue|Latin Body|Shared Title|Latin Wanted");
    // Japanese on an English game: by family, CJK families only.
    MenuFontList ja = menu_fonts_for(map, 10, 0);
    assert(ja.byFamily && families(ja) == "JP Gothic|JP Title  W7" && ja.fonts[1].bold);
    assert(std::strcmp(ja.fonts[0].label, "STANDARD") == 0 && std::strcmp(ja.fonts[1].label, "TITLE") == 0);
    // Polish on a Japanese game: by family, without the family the Japanese library also has.
    MenuFontList plOnJa = menu_fonts_for(map, 6, 10);
    assert(plOnJa.byFamily && families(plOnJa) == "Latin Catalogue|Latin Body|Latin Ledger|Latin Wanted");
    // Korean on a Japanese game, and an unknown language.
    assert(families(menu_fonts_for(map, 8, 10)) == "KR Myungjo|KR Cursive");
    assert(menu_fonts_for(map, 99, 0).count == 0);
    // An empty or broken file gives no fonts (the mod then uses its fixed list).
    static FontMap empty;
    assert(empty.parse("<font_lib><efigs name=\"$x\"", 27) == 0 && menu_fonts_for(empty, 0, 0).count == 0);
    std::puts("font catalogue tests passed.");
    return 0;
}
