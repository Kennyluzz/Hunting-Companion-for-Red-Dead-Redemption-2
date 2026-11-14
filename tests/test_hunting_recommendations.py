#!/usr/bin/env python3
"""Contract checks for hunting recommendations and Craftable Now."""
from __future__ import annotations

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DATA = json.loads((ROOT / "data/weapon_icons.json").read_text(encoding="utf-8"))
ADVICE = json.loads((ROOT / "data/compendium_advice.json").read_text(encoding="utf-8"))["labels"]
SOURCE = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/embedded_hunting_recommendations.hpp").read_text(encoding="utf-8")
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")

# Weapon advice is only the game's own: the clean-kill line of each Animal
# Compendium short description, by label (tools/build_compendium_advice.py).
ids = [row["id"] for row in ADVICE]
if not ids or len(ids) != len(set(ids)):
    raise AssertionError("The Compendium advice table is empty or has duplicate labels")
for row in ADVICE:
    if not row["advice"].startswith("Use ") or not row["advice"].endswith("for a clean kill.") or not row["choices"]:
        raise AssertionError(f"{row['id']}: not a Compendium clean-kill line")
    for item in row["choices"]:
        if item["weapon"] not in DATA["weaponTextures"] or any(a not in DATA["ammoTextures"] for a in item["ammo"]):
            raise AssertionError(f"{row['id']}: weapon or ammunition without an icon")
    if f"0x{int(row['id'], 16):08X}u" not in HEADER:
        raise AssertionError(f"{row['id']}: missing from the generated header")
for token in ("find_by_label(compendiumLabel)", "kCompendiumShortDescriptionFromPed",
              "note_unknown_compendium_label("):
    if token not in SOURCE:
        raise AssertionError(f"The HUD must use the game's own Compendium label: {token}")
if "hunting::find(species)" in SOURCE:
    raise AssertionError("Weapon advice must not come from a per-species table")

for texture in list(DATA["weaponTextures"].values()) + list(DATA["ammoTextures"].values()):
    if texture not in HEADER:
        raise AssertionError(f"Generated C++ header missing texture {texture}")

required_source = (
    '#include "embedded_hunting_recommendations.hpp"',
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
if 'MenuPage::Settings: return kSettingsCount' not in SOURCE or 'constexpr int kSetFilterKey = 2;' not in SOURCE:
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
VERSION = (ROOT / "VERSION.txt").read_text(encoding="utf-8").strip()
if f"VERSION {VERSION} " not in CMAKE:
    raise AssertionError("CMake project version does not match VERSION.txt")

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

print(f"PASS: {len(ADVICE)} Compendium clean-kill labels; icon, filter and 13-locale contracts.")
