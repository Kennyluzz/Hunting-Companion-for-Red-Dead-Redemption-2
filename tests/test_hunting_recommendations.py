#!/usr/bin/env python3
"""Contract checks for hunting recommendations and Craftable Now."""
from __future__ import annotations

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DATA = json.loads((ROOT / "data/hunting_recommendations.json").read_text(encoding="utf-8"))
SOURCE = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/embedded_hunting_recommendations.hpp").read_text(encoding="utf-8")
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")

rows = DATA["recommendations"]
species = [row["species"] for row in rows]
if len(species) != len(set(species)):
    raise AssertionError("Duplicate runtime species")

material_block = SOURCE.split("constexpr std::array<MaterialSource, 83> kMaterialSources", 1)[1].split("}};", 1)[0]
tracked_species = set(re.findall(r'"([^"]+)"', material_block))
if not tracked_species.issubset(set(species)):
    raise AssertionError(
        f"Pearson/Trapper material species missing recommendations: {sorted(tracked_species-set(species))}"
    )

# Every standard Animal Compendium number must be represented: either by a
# weapon recommendation or by an explicit no-recommendation classification.
numbered = [n for row in rows for n in row["compendiumNumbers"]]
no_recommendation = DATA.get("noRecommendationEntries", [])
numbered.extend(n for row in no_recommendation for n in row["compendiumNumbers"])
if len(numbered) != len(set(numbered)):
    raise AssertionError("Duplicate Compendium number in recommendation / no-recommendation data")
if set(numbered) != set(range(1, 163)):
    raise AssertionError(
        f"Standard Animal Compendium coverage mismatch; missing={sorted(set(range(1,163))-set(numbered))}, "
        f"extra={sorted(set(numbered)-set(range(1,163)))}"
    )
if len(no_recommendation) != 15:
    raise AssertionError("Expected 15 domestic / working animal entries without a hunting recommendation")

model_block = SOURCE.split("constexpr auto kAnimalModels = std::array{", 1)[1].split("};", 1)[0]
model_species = set(re.findall(r'AnimalModel\{joaat\("[^"]+"\), "([^"]+)"\}', model_block))
if set(species) != model_species:
    raise AssertionError(
        f"Animal-model recommendation coverage mismatch; missing={sorted(model_species-set(species))}, "
        f"unreachable={sorted(set(species)-model_species)}"
    )

if DATA["source"]["updated"] != "2026-11-02":
    raise AssertionError("Bloxodes source revision date changed")
if DATA["source"]["page1"] != "https://bloxodes.com/wiki/red-dead-redemption-2/animals":
    raise AssertionError("Bloxodes page 1 reference mismatch")
if DATA["source"]["page2"] != "https://bloxodes.com/wiki/red-dead-redemption-2/animals/page/2":
    raise AssertionError("Bloxodes page 2 reference mismatch")

for row in rows:
    if not row["compendiumNumbers"] or any(not 1 <= n <= 162 for n in row["compendiumNumbers"]):
        raise AssertionError(f"{row['species']}: invalid Compendium references")
    if not row["sourceRule"] or not row["choices"]:
        raise AssertionError(f"{row['species']}: missing source rule or recommendation")
    for item in row["choices"]:
        if item["weapon"] not in DATA["weaponTextures"]:
            raise AssertionError(f"{row['species']}: unknown weapon {item['weapon']}")
        for ammo in item.get("ammo", []):
            if ammo not in DATA["ammoTextures"]:
                raise AssertionError(f"{row['species']}: unknown ammo {ammo}")

for texture in list(DATA["weaponTextures"].values()) + list(DATA["ammoTextures"].values()):
    if texture not in HEADER:
        raise AssertionError(f"Generated C++ header missing texture {texture}")

required_source = (
    '#include "embedded_hunting_recommendations.hpp"',
    'hunting_companion::hunting::find(species)',
    'inventory_texture_dictionary_ready()',
    'recipe_craftable_now(vendor, recipe)',
    'recipe_craftable_now(vendor, vendor.recipes[i])',
    'key_pressed(g_filterHotkey)',
    'garment_set_craftable_now',
    'std::array<RecipeCost, kMaxMaterials>',
    'is_animal_part_material(std::uint32_t materialHash)',
    'if (includeInVendorDemand && is_animal_part_material(materialHash))',
    'g_filterHotkey',
    'GetPrivateProfileIntA("General", "FilterKey", kDefaultFilterKey, g_iniPath)',
    'save_filter_hotkey()',
    'g_menu.captureFilterHotkey',
    'Craftable filter key: %s',
    'draw_hunting_recommendation_icons(*recommendation, layout)',
    'draw_icon_with_shadow',
    'count > 0',
    '"FILTER: CRAFTABLE"'
)
for token in required_source:
    if token not in SOURCE:
        raise AssertionError(f"Missing integration token: {token}")

if "SET_GAME_PAUSED" in SOURCE or "kSetGamePaused" in SOURCE:
    raise AssertionError("The explicitly rejected pause feature was added")
if "key_pressed(VK_SPACE)" in SOURCE:
    raise AssertionError("Craftable filter still uses a hard-coded Space key")
if 'MenuPage::Settings: return 6' not in SOURCE:
    raise AssertionError("Settings page count does not include the filter-key option")
if 'if (count > 0)' not in SOURCE[SOURCE.index("// Keep new/unknown catalog groups visible"):SOURCE.index("void build_menu_recipe_list")]:
    raise AssertionError("Empty unknown categories can still be displayed")
if 'if ((!needsPearson && !needsTrapper) || !blip_texture_dictionary_ready()) return;' not in SOURCE:
    raise AssertionError("Vendor icons and animal recommendations are not independently gated")
if "playerMoney" in SOURCE:
    raise AssertionError("Unverified wallet-count logic must not be used")
craftable_block = SOURCE.split("bool recipe_materials_ready", 1)[1].split(
    "bool recipe_uses_current_haul", 1
)[0]
if craftable_block.count("if (!is_animal_part_material(cost.hash)) continue;") != 2:
    raise AssertionError("Both recipe and garment-set checks must ignore non-animal costs")
if "cost.hash == kMoneyHash" in craftable_block:
    raise AssertionError("Cash must not block Craftable Now")
if "src/embedded_hunting_recommendations.hpp" not in CMAKE:
    raise AssertionError("CMake does not include the generated recommendation header")
if "VERSION 1.15.0" not in CMAKE:
    raise AssertionError("CMake project version is not 1.15.0")

locales = ("en-US","fr-FR","de-DE","it-IT","es-ES","pt-BR","pl-PL","ru-RU","ko-KR","zh-TW","ja-JP","es-MX","zh-CN")
for locale in locales:
    content = (ROOT / f"HuntingCompanion-Languages/{locale}.lang").read_text(encoding="utf-8")
    for key in ("FILTER: ALL","FILTER: CRAFTABLE","SPACE: Toggle Filter"):
        if not re.search(r"^" + re.escape(key) + r"=", content, re.M):
            raise AssertionError(f"{locale}: missing localization key {key}")

# Every weapon and ammunition name shown on the HUD must be translated in every locale.
shown_names = set(DATA["weaponTextures"]) | set(DATA["ammoTextures"])
for locale in locales:
    entries = {}
    for line in (ROOT / f"HuntingCompanion-Languages/{locale}.lang").read_text(encoding="utf-8").splitlines():
        if line and not line.startswith("#") and "=" in line:
            key, value = line.split("=", 1)
            entries[key] = value
    for name in sorted(shown_names):
        if not entries.get(name):
            raise AssertionError(f"{locale}: weapon/ammo name not translated: {name}")
    if locale == "en-US":
        for name in sorted(shown_names):
            if entries[name] != name:
                raise AssertionError(f"en-US must show the official English name: {name}")

for token in ("struct IconRow { RecommendationIcon icons[2]; int count; };",
              "wrap_to_width(text, size, correction, maxWidth, 2, &fitted)",
              "const TextRightLimitScope keepTextInsidePanel("):
    if token not in SOURCE:
        raise AssertionError(f"Missing icon layout / text fitting integration: {token}")

print(f"PASS: {len(rows)} huntable runtime species; all 162 standard Animal Compendium entries classified; recommendation, icon, filter and 13-locale contracts.")
