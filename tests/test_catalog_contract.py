#!/usr/bin/env python3
"""Static guardrails for Pearson retention and opaque-placeholder filtering."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
README = (ROOT / "README.md").read_text(encoding="utf-8")

# Pearson must remain a first-class vendor, not an empty compatibility slot.
required = [
    "constexpr std::uint32_t kShopPearson",
    "constexpr std::uint32_t kCostCraftingPearson",
    "constexpr std::uint32_t kPearsonStorageSlot",
    "constexpr CategoryDefinition kPearsonCategories",
    "scan_vendor_catalog(pearson, kShopPearson, kCostCraftingPearson",
    "vendor_material_count(inventoryId, &rootGuid",
    '"Pearson", "Trapper", "Search", "Crafting Uses", "Challenges", "Settings", "Close"',
    'draw_vendor_icon("blip_shop_butcher"',
]
for token in required:
    if token not in SOURCE:
        raise AssertionError(f"Pearson support was removed: missing {token!r}")

# Only the two known opaque Pearson records are excluded, and only for Pearson.
for token in (
    "kPearsonUnlabelledItem1 = 0xC998B07Cu",
    "kPearsonUnlabelledItem2 = 0xF9C120CDu",
    "is_pearson_unlabelled_placeholder",
    "shopHash == kShopPearson && is_pearson_unlabelled_placeholder(itemHash)",
):
    if token not in SOURCE:
        raise AssertionError(f"Opaque Pearson placeholder filter missing: {token!r}")

if "UNLABELLED PEARSON ITEM" in SOURCE.upper() or "Unknown recipe %08X" in SOURCE:
    raise AssertionError("An internal placeholder/hash recipe label can reach the UI")

# All other unknown catalog rows must have a real name in the embedded official
# table before they can become recipes; no generic fabricated recipe label.
if "if (identity == nullptr && !hasEmbeddedOfficialName)" not in SOURCE:
    raise AssertionError("Unresolved, uncurated recipe rows are not filtered")
if "Unknown recipe" in SOURCE:
    raise AssertionError("Generic unknown recipe fallback remains")

print("Pearson retention and unknown-recipe exclusion checks passed.")

# Pearson's crafting ends at a point in the story (details kept out of texts).
PEARSON_STORY = (
    "invoke_native<int>(kMissionDataWasCompleted, kPearsonEndMission) != 0",
    "g_pearsonUnavailable = missionDone || laterCharacter;",
    "if (index == kRootPearson) return g_pearsonUnavailable;",
    "const bool needsPearson = !g_pearsonUnavailable && species != nullptr &&",
    "i < g_tracker.pearson.recipeCount && !g_pearsonUnavailable;",
    "for (int vendorIndex = g_pearsonUnavailable ? 1 : 0; vendorIndex < 2; ++vendorIndex) {",
    "g_pearsonUnavailable = g_pearsonSetting == 0;",
    'WritePrivateProfileStringA("General", "PearsonAvailable", value, g_iniPath);',
)
for token in PEARSON_STORY:
    if token not in SOURCE:
        raise AssertionError(f"Pearson story handling missing: {token}")
print("Pearson switched off by the story check.")
