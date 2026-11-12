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
    "if (perfectOnly && quality != 2) continue;",
    "g_currentHaul.hasAnyAnimalPart = true;",
    "if (isPelt ? (carcassMaterial || !perfectOnly) : !carcassMaterial) continue;",
    "if (legendaryOnly) continue;",
    'model == joaat("a_c_chipmunk_01")',
    "now - g_currentHaul.lastRefreshTick < 500u",
    "void refresh_current_haul_if_due()",
    "if (g_menu.page != MenuPage::Closed) refresh_current_haul_if_due();",
    "bool root_item_disabled(int index) noexcept",
    "case MenuPage::CraftingUses:",
    "case MenuPage::CraftingUseDetail:",
    "void build_crafting_uses_list()",
    "if (recipe.owned || !recipe.hasAnyCosts) return false;",
    "recipe_uses_current_haul(recipe)",
    "current_haul_count(cost.hash) + donated < cost.amount",
    "READY AFTER DONATION",
    "ANIMAL PARTS READY",
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
if "VERSION 1.13.0" not in CMAKE:
    raise AssertionError("CMake version is not 1.13.0")

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
