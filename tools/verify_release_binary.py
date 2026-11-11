#!/usr/bin/env python3
"""Verify that a release ASI contains the current challenge translations.

Run after a clean Windows x64 build:
    python tools/verify_release_binary.py build-release/HuntingCompanion.asi

This catches a stale ASI built before the generated official challenge table
or current locale dictionaries were regenerated.
"""
import argparse
import ast
import json
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = json.loads((ROOT / "data/challenge_translation_manifest.json").read_text(encoding="utf-8"))
OFFICIAL = (ROOT / "src/embedded_official_strings.hpp").read_text(encoding="utf-8")


def parse_official_tables():
    result = {}
    for locale in MANIFEST["locales"]:
        symbol = "k_" + locale.replace("-", "_")
        match = re.search(
            r"inline constexpr Entry " + re.escape(symbol) +
            r"\[\] = \{(.*?)\n\};", OFFICIAL, re.S)
        if not match:
            raise SystemExit(f"Missing official locale table: {locale}")
        entries = {}
        for raw_id, literal in re.findall(
            r'\{0x([0-9A-Fa-f]{8})u,\s*("(?:[^"\\]|\\.)*")\}', match.group(1)):
            entries[int(raw_id, 16)] = ast.literal_eval(literal)
        result[locale] = entries
    return result


def read_lang(locale):
    path = ROOT / "HuntingCompanion-Languages" / f"{locale}.lang"
    entries = {}
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith(("#", ";")) or "=" not in line:
            continue
        key, value = line.split("=", 1)
        entries[key.strip()] = value.strip()
    return entries


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("asi", type=Path)
    args = parser.parse_args()
    data = args.asi.read_bytes()
    if len(data) < 0x100 or data[:2] != b"MZ":
        raise SystemExit("Not a Windows PE image (missing MZ header)")
    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe_offset:pe_offset + 4] != b"PE\0\0":
        raise SystemExit("Invalid PE signature")
    machine = struct.unpack_from("<H", data, pe_offset + 4)[0]
    if machine != 0x8664:
        raise SystemExit(f"Expected x64 PE (0x8664), got 0x{machine:04X}")

    objective_ids = {int(value, 16) for value in MANIFEST["rockstarObjectiveIds"].values()}
    official = parse_official_tables()
    checked = 0
    missing = []
    for locale, entries in official.items():
        for label_id in objective_ids:
            value = entries.get(label_id)
            if not value:
                missing.append(f"{locale}: hash 0x{label_id:08X} missing from source table")
            elif value.encode("utf-8") not in data:
                missing.append(f"{locale}: {value!r} absent from ASI")
            checked += 1

    custom = set(MANIFEST["customObjectives"])
    for locale in MANIFEST["locales"]:
        if locale == "en-US":
            continue
        language = read_lang(locale)
        for objective in custom:
            value = language.get(objective)
            if not value:
                missing.append(f"{locale}: custom objective {objective!r} has no translation")
            elif value.encode("utf-8") not in data:
                missing.append(f"{locale}: custom translation {value!r} absent from ASI")

    if missing:
        print(f"FAILED: {len(missing)} translation payload checks failed.")
        for item in missing[:40]:
            print(" -", item)
        if len(missing) > 40:
            print(f" ... and {len(missing) - 40} more")
        raise SystemExit(1)

    print(f"PASS: x64 ASI contains all {checked} official challenge translations "
          "and all custom challenge translations for 13 locales.")


if __name__ == "__main__":
    main()
