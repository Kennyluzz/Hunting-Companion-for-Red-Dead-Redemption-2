#!/usr/bin/env python3
"""Embed the exact Rockstar item/material and challenge labels used by Hunting Companion.

The 290 recipe/material IDs are verified against their exact English ItemDatabase labels.
The 68 challenge IDs come from the checked-in challenge manifest; challenge objective copy
in main.cpp is a concise companion description and may differ slightly from Rockstar's
localized label text. IDs are checked for existence in all 13 official locale tables.
"""
from pathlib import Path
import zipfile
import re
import xml.etree.ElementTree as ET
import json
import argparse

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src" / "main.cpp"
OUT = ROOT / "src" / "embedded_official_strings.hpp"
CHALLENGE_IDS_OUT = ROOT / "src" / "embedded_challenge_objective_hashes.hpp"
MANIFEST = ROOT / "data" / "official_string_manifest.json"
CHALLENGE_MANIFEST = ROOT / "data" / "challenge_translation_manifest.json"
LOCALE_FILES = {
    "en-US": "English", "fr-FR": "French", "de-DE": "German", "it-IT": "Italian",
    "es-ES": "Spanish (Spain)", "pt-BR": "Portuguese (Brazil)", "pl-PL": "Polish",
    "ru-RU": "Russian", "ko-KR": "Korean", "zh-TW": "Chinese (Traditional)",
    "ja-JP": "Japanese", "es-MX": "Spanish (Latin America)", "zh-CN": "Chinese (Simplified)",
}


def archive_for(folder: Path, language: str) -> Path:
    candidates = sorted(folder.glob(f"RDR2 Official Language - {language} *.zip"))
    if len(candidates) != 1:
        raise RuntimeError(f"{language}: expected exactly one language ZIP in {folder}, found {len(candidates)}")
    return candidates[0]


def rows_from_zip(path: Path) -> dict[int, str]:
    with zipfile.ZipFile(path) as zf:
        xml_names = [n for n in zf.namelist() if Path(n).name.lower().endswith(".xml")]
        if len(xml_names) != 1:
            raise RuntimeError(f"{path.name}: expected one XML, found {len(xml_names)}")
        out: dict[int, str] = {}
        with zf.open(xml_names[0]) as stream:
            for _, el in ET.iterparse(stream, events=("end",)):
                if el.tag.lower() == "string":
                    raw_id = el.attrib.get("id", "")
                    if raw_id.lower().startswith("0x"):
                        try:
                            key = int(raw_id, 16)
                        except ValueError:
                            key = 0
                        value = "".join(el.itertext()).strip()
                        if key and value:
                            out[key] = value
                    el.clear()
        return out


def challenge_source_objectives(source: str) -> set[str]:
    start = source.index("constexpr auto kChallengeDefinitions")
    end = source.index("constexpr bool all_challenge_objectives_defined", start)
    block = source[start:end]
    return set(re.findall(r'^\s*"([^"\n]+\.)",?\s*$', block, re.M))


def parse_item_manifest(source: str) -> list[tuple[int, str, str]]:
    recipe_block = source[source.index("constexpr RecipeIdentity kRecipeIdentities"):source.index("struct MaterialName")]
    material_start = source.index("constexpr MaterialName kMaterialNames")
    material_block = source[material_start:source.index("};", material_start)]
    pairs: dict[int, tuple[str, str]] = {}
    for h, name in re.findall(r'RecipeIdentity\{0x([0-9A-Fa-f]+)u,\s*0x[0-9A-Fa-f]+u,\s*"([^"]+)"\}', recipe_block):
        if name.startswith("Unlabelled Pearson item"):
            continue
        pairs[int(h, 16)] = ("recipe", name)
    for h, name in re.findall(r'MaterialName\{0x([0-9A-Fa-f]+)u,\s*"([^"]+)"\}', material_block):
        pairs[int(h, 16)] = ("material", name)
    if len(pairs) < 280:
        raise RuntimeError(f"Only {len(pairs)} candidate catalogue hashes found; refusing to generate incomplete data")
    return [(h, kind, name) for h, (kind, name) in sorted(pairs.items())]


def parse_all_ids(source: str) -> tuple[list[tuple[int, str, str]], dict[str, str]]:
    items = parse_item_manifest(source)
    challenge_data = json.loads(CHALLENGE_MANIFEST.read_text(encoding="utf-8"))
    source_objectives = challenge_source_objectives(source)
    ids = challenge_data.get("rockstarObjectiveIds", {})
    custom = set(challenge_data.get("customObjectives", []))
    mapped = set(ids)
    if len(source_objectives) != 82:
        raise RuntimeError(f"Expected 82 unique challenge objectives in source, found {len(source_objectives)}")
    if mapped & custom:
        raise RuntimeError("Challenge manifest has objectives classified as both Rockstar and custom")
    if mapped | custom != source_objectives:
        raise RuntimeError(
            f"Challenge manifest/source mismatch; missing={sorted(source_objectives-(mapped|custom))[:5]}, "
            f"extra={sorted((mapped|custom)-source_objectives)[:5]}"
        )
    challenge_pairs = []
    for objective, raw_hash in ids.items():
        try:
            label_hash = int(raw_hash, 16)
        except ValueError as exc:
            raise RuntimeError(f"Invalid challenge label ID for {objective!r}: {raw_hash!r}") from exc
        challenge_pairs.append((label_hash, "challenge", objective))
    occupied = {h for h, _, _ in items}
    overlap = occupied & {h for h, _, _ in challenge_pairs}
    if overlap:
        raise RuntimeError(f"Challenge IDs unexpectedly overlap catalogue labels: {[hex(x) for x in sorted(overlap)[:5]]}")
    all_pairs = sorted(items + challenge_pairs, key=lambda row: row[0])
    if len(all_pairs) != len({row[0] for row in all_pairs}):
        raise RuntimeError("Duplicate official label hashes found")
    return all_pairs, ids


def c_string(value: str) -> str:
    out = '"'
    for ch in value:
        n = ord(ch)
        if ch == "\\": out += "\\\\"
        elif ch == '"': out += '\\"'
        elif ch == "\n": out += "\\n"
        elif ch == "\r": out += "\\r"
        elif ch == "\t": out += "\\t"
        elif n < 0x20: out += f"\\x{n:02X}"
        else: out += ch
    return out + '"'


def write_challenge_hash_header(ids: dict[str, str]) -> None:
    lines = [
        "#pragma once",
        "// Generated from data/challenge_translation_manifest.json.",
        "#include <cstddef>",
        "#include <cstdint>",
        "namespace hunting_companion::embedded_challenge {",
        "struct ObjectiveHash { const char* objective; std::uint32_t labelHash; };",
        "inline constexpr ObjectiveHash kObjectiveHashes[] = {",
    ]
    for objective, raw_hash in sorted(ids.items()):
        lines.append(f"    {{{c_string(objective)}, 0x{int(raw_hash, 16):08X}u}},")
    lines += [
        "};",
        "inline constexpr std::size_t kObjectiveHashCount = sizeof(kObjectiveHashes) / sizeof(kObjectiveHashes[0]);",
        "} // namespace hunting_companion::embedded_challenge",
        "",
    ]
    CHALLENGE_IDS_OUT.write_text("\n".join(lines), encoding="utf-8")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input-dir", type=Path, default=Path("/mnt/data"))
    args = ap.parse_args()
    source = SRC.read_text(encoding="utf-8")
    manifest_pairs, challenge_ids = parse_all_ids(source)

    english = rows_from_zip(archive_for(args.input_dir, "English"))
    valid = []
    skipped = []
    for h, kind, source_name in manifest_pairs:
        official_english = english.get(h)
        if not official_english:
            raise RuntimeError(f"English table is missing required {kind} label ID 0x{h:08X} ({source_name})")
        if kind != "challenge" and official_english != source_name:
            skipped.append({"id": f"0x{h:08X}", "kind": kind,
                            "expectedEnglish": source_name, "englishTableValue": official_english})
            continue
        valid.append((h, kind, source_name, official_english))

    # Recipe/material hashes must all match exactly; the challenge strings deliberately
    # use concise source descriptions that can differ from Rockstar's official label copy.
    expected_catalogue = sum(kind != "challenge" for _, kind, _ in manifest_pairs)
    actual_catalogue = sum(kind != "challenge" for _, kind, _, _ in valid)
    if actual_catalogue < 280 or actual_catalogue != expected_catalogue:
        raise RuntimeError(f"Only {actual_catalogue}/{expected_catalogue} recipe/material IDs verified in English")
    expected_challenges = len(challenge_ids)
    actual_challenges = sum(kind == "challenge" for _, kind, _, _ in valid)
    if actual_challenges != expected_challenges:
        raise RuntimeError(f"Only {actual_challenges}/{expected_challenges} challenge label IDs verified in English")

    all_rows = {}
    ids = [h for h, _, _, _ in valid]
    for locale, language in LOCALE_FILES.items():
        table = rows_from_zip(archive_for(args.input_dir, language))
        missing = [h for h in ids if h not in table]
        if missing:
            raise RuntimeError(f"{language}: {len(missing)} missing required IDs, e.g. {[hex(x) for x in missing[:5]]}")
        all_rows[locale] = {h: table[h] for h in ids}

    lines = [
        "#pragma once",
        "// Generated from Rockstar Translation.xml packs.",
        "// Contains only Hunting Companion recipe/material and challenge label hashes.",
        "#include <cstddef>", "#include <cstdint>",
        '#include "official_strings.hpp"',
        "namespace hunting_companion::embedded_official {",
        "using Entry = ::hunting_companion::OfficialStringEntry;",
        "struct Locale { const char* locale; const Entry* entries; std::size_t count; };",
    ]
    symbols = {}
    for locale, table in all_rows.items():
        symbol = "k_" + locale.replace("-", "_")
        symbols[locale] = symbol
        lines.append(f"inline constexpr Entry {symbol}[] = {{")
        for h in sorted(table):
            lines.append(f"    {{0x{h:08X}u, {c_string(table[h])}}},")
        lines.append("};")
    lines.append("inline constexpr Locale kLocales[] = {")
    for locale, table in all_rows.items():
        symbol = symbols[locale]
        lines.append(f"    {{{c_string(locale)}, {symbol}, sizeof({symbol}) / sizeof({symbol}[0])}},")
    lines += [
        "};",
        f"inline constexpr std::size_t kEntryCountPerLocale = {len(ids)}u;",
        f"inline constexpr std::size_t kRecipeMaterialEntryCount = {actual_catalogue}u;",
        f"inline constexpr std::size_t kChallengeEntryCount = {actual_challenges}u;",
        "} // namespace hunting_companion::embedded_official",
        "",
    ]
    OUT.write_text("\n".join(lines), encoding="utf-8")
    write_challenge_hash_header(challenge_ids)

    valid_catalogue = [
        {"id": f"0x{h:08X}", "kind": kind, "english": source_name}
        for h, kind, source_name, _ in valid if kind != "challenge"
    ]
    valid_challenges = [
        {"id": f"0x{h:08X}", "objective": source_name, "officialEnglishLabel": official_english}
        for h, kind, source_name, official_english in valid if kind == "challenge"
    ]
    MANIFEST.write_text(json.dumps({
        "description": "Exact Rockstar label hashes referenced by Hunting Companion.",
        "sourceLanguage": "English",
        "locales": list(LOCALE_FILES.keys()),
        "uniqueHashes": len(ids),
        "entriesPerLocale": len(ids),
        "recipeMaterialHashes": len(valid_catalogue),
        "challengeObjectiveHashes": len(valid_challenges),
        "verifiedAgainstEnglish": len(valid),
        "excludedUnlabelledEntries": skipped,
        "items": valid_catalogue,
        "challengeObjectives": valid_challenges,
        "bytesByLocale": {loc: sum(len(v.encode("utf-8")) for v in table.values())
                           for loc, table in all_rows.items()},
    }, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    print(f"Generated {OUT}: {len(ids)} official hashes x {len(LOCALE_FILES)} locales.")
    print(f"  Recipe/material: {actual_catalogue}; challenge objectives: {actual_challenges}.")
    print(f"Generated {CHALLENGE_IDS_OUT} with {len(challenge_ids)} objective-to-label mappings.")
    print(f"Generated manifest {MANIFEST}; excluded {len(skipped)} unlabelled entries.")
    print(f"Official table header size: {OUT.stat().st_size:,} bytes")
    print(f"Challenge hash header size: {CHALLENGE_IDS_OUT.stat().st_size:,} bytes")
    for locale, table in all_rows.items():
        raw = sum(len(v.encode("utf-8")) for v in table.values())
        print(f"  {locale}: {len(table)} rows, {raw:,} UTF-8 text bytes")


if __name__ == "__main__":
    main()
