#!/usr/bin/env python3
"""Validate exact UI phrases and the compact embedded Rockstar item tables."""
from collections import Counter
from pathlib import Path
import json
import re

ROOT = Path(__file__).resolve().parents[1]
LANG_DIR = ROOT / "HuntingCompanion-Languages"
SOURCE = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
LOCALIZATION_CPP = (ROOT / "src" / "localization.cpp").read_text(encoding="utf-8")
OFFICIAL_CPP = (ROOT / "src" / "official_strings.cpp").read_text(encoding="utf-8")
OFFICIAL_HEADER = (ROOT / "src" / "embedded_official_strings.hpp").read_text(encoding="utf-8")
MANIFEST = json.loads((ROOT / "data" / "official_string_manifest.json").read_text(encoding="utf-8"))
LOCALES = sorted(p.stem for p in LANG_DIR.glob("*.lang"))
EXPECTED = {
    "en-US", "fr-FR", "de-DE", "it-IT", "es-ES", "pt-BR", "pl-PL",
    "ru-RU", "ko-KR", "zh-TW", "ja-JP", "es-MX", "zh-CN",
}

if set(LOCALES) != EXPECTED:
    raise AssertionError(f"Locale set mismatch: {sorted(set(LOCALES) ^ EXPECTED)}")
if "@word:" in LOCALIZATION_CPP or "@word:" in SOURCE:
    raise AssertionError("Word-level phrase composition must not be used")
if "kGetStringFromHashKey" not in SOURCE:
    raise AssertionError("Active-game-language native path is missing")
if "companionUsesGameLanguage" not in SOURCE:
    raise AssertionError("Native strings must be gated by companion/game language match")
if "official_item_string(labelHash)" not in SOURCE:
    raise AssertionError("Cross-language labels must use official embedded table")
if "official_item_string(itemHash)" not in SOURCE:
    raise AssertionError("Curated recipe/material IDs must resolve directly from embedded tables")
if '{"$catalog1", "CATALOGUE"' not in SOURCE:
    raise AssertionError("Latin/RDR2 catalog font fallback is missing")
# CJK for a language the game is not running in uses the game's own font library
# for that language, loaded at run time; never a generated fontmap.xml or guessed
# hard-coded font family names (names come from the library or the ini).
NATIVE_FONTS = (ROOT / "src" / "native_fonts.cpp").read_text(encoding="utf-8")
if "native_text_ready(" not in SOURCE or "needs_native_library(" not in SOURCE:
    raise AssertionError("CJK text must use the game's native font libraries")
# (The game's own fontmap.xml is read at run time; nothing may write one into
# the game folder.)
for forbidden in ("commonx", "YDMyungjo", "MotoyaExGothic", "MSung", "CreateDirectory"):
    if forbidden in SOURCE or forbidden in NATIVE_FONTS:
        raise AssertionError(f"Font-map provisioning or guessed font names are referenced: {forbidden}")
for forbidden in ("CreateFile", "WriteFile(", "CreateDirectory"):
    if forbidden in SOURCE:
        raise AssertionError(f"main.cpp must not write files into the game folder ({forbidden})")
if "falls_back_to_english(wanted)) {" not in SOURCE or "native_fonts::loading(wanted)" not in SOURCE:
    raise AssertionError("A CJK language must keep the current language while loading and fall back to English if it fails")
for library in ("font_lib_japanese", "font_lib_korean", "font_lib_chinese", "font_lib_simplified_chinese"):
    if f'"{library}"' not in NATIVE_FONTS:
        raise AssertionError(f"Native font library missing: {library}")
if "draw_wrapped_text_raw(label" not in SOURCE or "draw_text_raw(componentName" not in SOURCE:
    raise AssertionError("Official catalogue names must bypass companion phrase translation")
if "official_item_string(entry.labelHash)" not in SOURCE:
    raise AssertionError("Official item labels must resolve dynamically from the currently selected locale")
if "draw_wrapped_centered_text(" not in SOURCE or "kSetTextCentre" not in SOURCE:
    raise AssertionError("Shared-stock note must use the centered wrapped renderer")
if "Unlabelled Pearson item" in SOURCE:
    raise AssertionError("Opaque Pearson placeholder recipes must not be player-facing")
if "Unknown recipe %08X" in SOURCE:
    raise AssertionError("Internal item hashes must not be exposed as recipe names")

for forbidden in ("load_official_table", "CreateFileW", "Translation.xml", "HuntingCompanion-Languages\\\\official", "import_official_language_xml"):
    if forbidden in LOCALIZATION_CPP or forbidden in OFFICIAL_CPP:
        raise AssertionError(f"Runtime still references external language data: {forbidden}")
if "embedded_official::kLocales" not in OFFICIAL_CPP:
    raise AssertionError("Runtime official string lookup is not using embedded tables")

CHALLENGE_MANIFEST = json.loads((ROOT / "data" / "challenge_translation_manifest.json").read_text(encoding="utf-8"))
if MANIFEST.get("uniqueHashes") != 358 or MANIFEST.get("entriesPerLocale") != 358:
    raise AssertionError("Expected 358 official Rockstar hashes (290 catalogue + 68 challenge labels) per locale")
if (len(MANIFEST.get("locales", [])) != 13 or len(MANIFEST.get("items", [])) != 290 or
        len(MANIFEST.get("challengeObjectives", [])) != 68):
    raise AssertionError("Official hash manifest is incomplete")
if MANIFEST.get("recipeMaterialHashes") != 290 or MANIFEST.get("challengeObjectiveHashes") != 68:
    raise AssertionError("Official hash manifest category counts are incorrect")
if "kEntryCountPerLocale = 358u" not in OFFICIAL_HEADER:
    raise AssertionError("Generated official table count does not match manifest")
if "kChallengeEntryCount = 68u" not in OFFICIAL_HEADER:
    raise AssertionError("Generated challenge label count does not match manifest")

# Every labelled source recipe/material hash must be present, and its English
# string must be an exact match. The two intentionally opaque labels are skipped.
recipe_start = SOURCE.index("constexpr RecipeIdentity kRecipeIdentities")
recipe_end = SOURCE.index("struct MaterialName", recipe_start)
material_start = SOURCE.index("constexpr MaterialName kMaterialNames")
material_end = SOURCE.index("};", material_start)
expected_items = {}
for raw_hash, name in re.findall(r'RecipeIdentity\{0x([0-9A-Fa-f]+)u,\s*0x[0-9A-Fa-f]+u,\s*"([^"]+)"\}', SOURCE[recipe_start:recipe_end]):
    if not name.startswith("Unlabelled Pearson item"):
        expected_items[int(raw_hash, 16)] = name
for raw_hash, name in re.findall(r'MaterialName\{0x([0-9A-Fa-f]+)u,\s*"([^"]+)"\}', SOURCE[material_start:material_end]):
    expected_items[int(raw_hash, 16)] = name
manifest_items = {int(item["id"], 16): item["english"] for item in MANIFEST["items"]}
if manifest_items != expected_items:
    missing = sorted(set(expected_items) - set(manifest_items))
    stale = sorted(set(manifest_items) - set(expected_items))
    raise AssertionError(f"Official manifest/source mismatch; missing={missing[:5]}, stale={stale[:5]}")

for locale in MANIFEST["locales"]:
    symbol = "k_" + locale.replace("-", "_")
    match = re.search(r"inline constexpr Entry " + re.escape(symbol) + r"\[\] = \{(.*?)\n\};", OFFICIAL_HEADER, re.S)
    if match is None:
        raise AssertionError(f"Embedded official table missing for {locale}")
    ids = [int(value, 16) for value in re.findall(r"\{0x([0-9A-Fa-f]{8})u,", match.group(1))]
    if len(ids) != 358 or ids != sorted(set(ids)):
        raise AssertionError(f"{locale}: expected 358 unique sorted official IDs, got {len(ids)}")


def parse(path: Path):
    rows = [line for line in path.read_text(encoding="utf-8").splitlines()
            if "=" in line and not line.lstrip().startswith(("#", ";"))]
    keys = [line.split("=", 1)[0] for line in rows]
    duplicates = sorted(k for k, count in Counter(keys).items() if count > 1)
    if duplicates:
        raise AssertionError(f"{path.stem}: duplicate keys: {duplicates[:8]}")
    if any(k.startswith("@word:") for k in keys):
        raise AssertionError(f"{path.stem}: legacy word-level glossary entry remains")
    for line in rows:
        key, value = line.split("=", 1)
        if not key or not value:
            raise AssertionError(f"{path.stem}: blank key/value in {line!r}")
        if len(key.encode("utf-8")) >= 192 or len(value.encode("utf-8")) >= 320:
            raise AssertionError(f"{path.stem}: key/value exceeds capacity")
    if len(rows) > 1024:
        raise AssertionError(f"{path.stem}: too many entries ({len(rows)})")
    return set(keys), len(rows)

static_literals = set()
for function in ("draw_text", "draw_wrapped_text", "draw_text_raw", "draw_wrapped_text_raw",
                 "draw_menu_header", "draw_menu_row"):
    for call in re.findall(r"\b" + function + r"\((.*?)\);", SOURCE, re.S):
        static_literals.update(re.findall(r'"([^"\\]*(?:\\.[^"\\]*)*)"', call))
static_literals.update(re.findall(r'ui_text\(\s*"([^"]*)"', SOURCE))
static_literals.discard("HUNTING COMPANION")
static_literals.discard("")

for locale in LOCALES:
    keys, count = parse(LANG_DIR / f"{locale}.lang")
    missing = sorted(static_literals - keys)
    if locale != "en-US" and missing:
        raise AssertionError(f"{locale}: missing exact UI phrases: {missing[:20]}")
    print(f"{locale}: {count} exact UI phrase entries; word-level fallback absent")

challenge_start = SOURCE.index("constexpr auto kChallengeDefinitions")
challenge_end = SOURCE.index("constexpr bool all_challenge_objectives_defined", challenge_start)
objectives = set(re.findall(r'^\s*"([^"\n]+\.)",?\s*$', SOURCE[challenge_start:challenge_end], re.M))
rockstar_objectives = set(CHALLENGE_MANIFEST["rockstarObjectiveIds"])
custom_objectives = set(CHALLENGE_MANIFEST["customObjectives"])
if len(objectives) != 82:
    raise AssertionError(f"Expected 82 unique challenge sentences, found {len(objectives)}")
if len(rockstar_objectives) != 68 or len(custom_objectives) != 14 or rockstar_objectives & custom_objectives:
    raise AssertionError("Challenge manifest must classify 68 Rockstar and 14 custom objectives")
if rockstar_objectives | custom_objectives != objectives:
    raise AssertionError("Challenge manifest/source objective set mismatch")

# The source must route both challenge-detail displays through the same resolver.
if "localized_challenge_objective(" not in SOURCE or "official_item_string(found->labelHash)" not in SOURCE:
    raise AssertionError("Challenge objective resolver must prefer the official Rockstar hash table")
if SOURCE.count("localized_challenge_objective(") < 3:
    raise AssertionError("Both challenge detail and reinforced-reward detail must use the objective resolver")
source_hash_map = dict((objective, int(raw, 16))
                       for objective, raw in re.findall(r'\{\s*"((?:[^"\\]|\\.)+)"\s*,\s*(0x[0-9A-Fa-f]{8})u\s*\}',
                                                        (ROOT / "src" / "embedded_challenge_objective_hashes.hpp").read_text(encoding="utf-8")))
expected_hash_map = {objective: int(raw, 16) for objective, raw in CHALLENGE_MANIFEST["rockstarObjectiveIds"].items()}
if source_hash_map != expected_hash_map:
    raise AssertionError("Generated challenge objective hash map differs from challenge manifest")
manifest_hash_map = {row["objective"]: int(row["id"], 16) for row in MANIFEST["challengeObjectives"]}
if manifest_hash_map != expected_hash_map:
    raise AssertionError("Embedded official-string manifest differs from challenge objective IDs")

for locale in LOCALES:
    keys, _ = parse(LANG_DIR / f"{locale}.lang")
    missing_custom = sorted(custom_objectives - keys) if locale != "en-US" else []
    if missing_custom:
        raise AssertionError(f"{locale}: missing custom challenge translations: {missing_custom[:8]}")
    accidental_manual = sorted(rockstar_objectives & keys)
    if accidental_manual:
        raise AssertionError(f"{locale}: Rockstar-backed objectives must use official hashes, not manual phrase translations: {accidental_manual[:5]}")

print("Localization and embedded official-string audits passed: 290 catalogue + 68 official challenge hashes, 14 custom challenge translations, 13 locales.")
