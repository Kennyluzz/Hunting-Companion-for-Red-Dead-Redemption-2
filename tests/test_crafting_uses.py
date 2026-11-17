#!/usr/bin/env python3
"""Static contract checks for the read-only Crafting Uses submenu."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")

native_hashes = {
    "kPlayerId": "0x217E9DC48139933D",
    "kGetSaddleHorseForPlayer": "0xB48050D326E9A2F3",
    "kGetCarriedAttachedInfoForSlot": "0x608BC6A6AACD5036",
    "kFindAllAttachedCarriableEntities": "0xB5ACE8B23A438EC0",
    "kCreateItemset": "0xA1AF16083320065A",
    "kDestroyItemset": "0x712BC69F10549B92",
    "kGetItemsetSize": "0x55F2E375AC6018A9",
    "kGetIndexedItemInItemset": "0x275A2E2C0FAB7612",
    "kGetEntityFromItem": "0xEE04C0AFD4EFAF0E",
    "kDoesEntityExist": "0xD42BD6EB2E0F1677",
    "kIsEntityAPed": "0xCF8176912DDA4EA5",
    "kGetIsCarriablePelt": "0x255B6DB4E3AD3C3E",
    "kGetCarcassFromPelt": "0x2A77EF9BEC8518F4",
    "kGetPedQuality": "0x7BCC6087D130312A",
}
for name, value in native_hashes.items():
    if not re.search(rf"{name}\s*=\s*{value}ull", SOURCE, re.I):
        raise AssertionError(f"Native hash mismatch or missing: {name}")

required = (
    "void update_current_haul()",
    "player_owned_count(inventoryId, material.hash)",
    "kGetCarriedAttachedInfoForSlot, &info, playerPed, slot, 0",
    "for (int slot = 0; slot <= 4; ++slot)",
    "kGetSaddleHorseForPlayer, playerId",
    "kFindAllAttachedCarriableEntities, horse, itemSet",
    "kGetCarcassFromPelt, entity",
    "kDestroyItemset, itemSet",
    "if (perfectOnly && ((qualityKnown && quality != 2) || !cleanKill)) continue;",
    "if (!carcassFound) return false;",
    "kGetCarriedPeltSkins, horse, skins.data()",
    "find_pelt_identity(entity)",
    "remember_carried_pelts();",
    "for (const auto& carcass : hunting_companion::satchel::kCarcassItems) {",
    "if (std::strstr(name, \"Perfect\") != nullptr && carcass.quality != 2) continue;",
    "if (isPelt ? (carcassMaterial || !perfectOnly) : (!carcassMaterial && skinned)) continue;",
    "kIsAnimalSkinned, carcass",
    "if (legendaryOnly) continue;",
    'model == joaat("a_c_chipmunk_01")',
    "now - g_currentHaul.lastRefreshTick < 500u",
    "void refresh_current_haul_if_due()",
    "if (g_menu.page != MenuPage::Closed) {\n            refresh_current_haul_if_due();",
    "bool root_item_disabled(int index) noexcept",
    "case MenuPage::CraftingUses:",
    "case MenuPage::CraftingUseDetail:",
    "void build_crafting_uses_list()",
    "if (recipe.owned || !recipe.hasAnyCosts) return false;",
    "recipe_uses_current_haul(recipe)",
    "current_haul_count(cost.hash) + donated < cost.amount",
    "READY AFTER DONATION",
    "OTHER COSTS NEEDED",
    "haul + donated, cost.amount",
    "g_menu.page = MenuPage::CraftingUseDetail",
)
for token in required:
    if token not in SOURCE:
        raise AssertionError(f"Missing Crafting Uses integration: {token}")

if "SET_GAME_PAUSED" in SOURCE or "kSetGamePaused" in SOURCE:
    raise AssertionError("Crafting Uses must not pause the game")
for forbidden in ("REMOVE_INVENTORY_ITEM", "CLEAR_PELT_FROM_HORSE", "SET_PELT_FOR_HORSE"):
    if forbidden in SOURCE:
        raise AssertionError(f"Crafting Uses must not mutate inventory/cargo: {forbidden}")
VERSION = (ROOT / "VERSION.txt").read_text(encoding="utf-8").strip()
if f"VERSION {VERSION} " not in CMAKE:
    raise AssertionError("CMake project version does not match VERSION.txt")

locales = ("en-US","fr-FR","de-DE","it-IT","es-ES","pt-BR","pl-PL","ru-RU","ko-KR","zh-TW","ja-JP","es-MX","zh-CN")
keys = ("Crafting Uses","NO HAUL","AVAILABLE","CRAFTING USES","READY AFTER DONATION",
        "ANIMAL PARTS READY","OTHER COSTS NEEDED","USES YOUR HAUL",
        "FILTER: ALL","FILTER: CRAFTABLE","SPACE: Toggle Filter")
for locale in locales:
    content = (ROOT / f"HuntingCompanion-Languages/{locale}.lang").read_text(encoding="utf-8")
    for key in keys:
        if not re.search(r"^" + re.escape(key) + r"=", content, re.M):
            raise AssertionError(f"{locale}: missing localization key {key}")

print("PASS: read-only player/horse haul scan, submenu gating, recipe filtering, readiness and 13-locale contracts.")

if "g_craftingUseCount == 0);" not in SOURCE:
    raise AssertionError("Crafting Uses row must be disabled when no recipe matches the haul")

# Satchel carcass items are the game's own IDs (checked against the English
# string table when generated); spot-check the naming and the perfect squirrel.
def joaat(text):
    value = 0
    for byte in text.lower().encode():
        value = (value + byte) & 0xFFFFFFFF
        value = (value + (value << 10)) & 0xFFFFFFFF
        value ^= value >> 6
    value = (value + (value << 3)) & 0xFFFFFFFF
    value ^= value >> 11
    return (value + (value << 15)) & 0xFFFFFFFF

SATCHEL = (ROOT / "src" / "embedded_satchel_carcasses.hpp").read_text(encoding="utf-8")
for item, species, quality in (("PROVISION_ANIMAL_CARCASS_SQUIRREL_PERFECT", "Squirrel", 2),
                               ("PROVISION_ANIMAL_CARCASS_MUSKRAT_HIGH_QUALITY", "Muskrat", 1),
                               ("PROVISION_ANIMAL_CARCASS_RABBIT_POOR", "Rabbit", 0)):
    row = f'{{0x{joaat(item):08X}u, "{species}", {quality}}}'
    if row not in SATCHEL:
        raise AssertionError(f"Satchel carcass table is missing {item}")
print("PASS: satchel carcass items count for their pelts and feathers.")
