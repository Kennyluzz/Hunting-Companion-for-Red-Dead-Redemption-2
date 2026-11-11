#!/usr/bin/env python3
"""Generate exact whole-sentence translations for the 14 non-Rockstar challenge objectives.

The other 68 unique challenge objectives are looked up from the embedded Rockstar string
hash table. Polish and Korean custom translations are retained from their reviewed .lang
source files; the other ten non-English locales are provided by the development TSV.
"""
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[1]
LANG = ROOT / "HuntingCompanion-Languages"
DATA = ROOT / "data" / "challenge_translations.tsv"
MANIFEST = ROOT / "data" / "challenge_translation_manifest.json"
MARKER = "# Custom challenge objective translations (objectives without Rockstar label IDs)."
COLUMNS = {
    "fr-FR": 1, "de-DE": 2, "it-IT": 3, "es-ES": 4, "pt-BR": 5,
    "ru-RU": 6, "zh-TW": 7, "ja-JP": 8, "es-MX": 9, "zh-CN": 10,
}


def read_pairs(path):
    pairs = {}
    for line_no, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith(("#", ";")) or "=" not in line:
            continue
        key, value = line.split("=", 1)
        if key in pairs:
            raise SystemExit(f"{path.name}:{line_no}: duplicate key {key!r}")
        pairs[key] = value
    return pairs


def read_tsv(custom):
    translations = {}
    for line_no, raw in enumerate(DATA.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split("=")
        if len(parts) != 11:
            raise SystemExit(f"{DATA.name}:{line_no}: expected English + 10 locale columns, got {len(parts)}")
        key, *values = parts
        if key not in custom:
            raise SystemExit(f"{DATA.name}:{line_no}: Rockstar-backed objective should not be manually translated: {key}")
        if key in translations:
            raise SystemExit(f"{DATA.name}:{line_no}: duplicate objective: {key}")
        if any(not value for value in values):
            raise SystemExit(f"{DATA.name}:{line_no}: empty translation for {key}")
        translations[key] = dict(zip(COLUMNS, values))
    if set(translations) != custom:
        raise SystemExit(f"Custom challenge TSV mismatch; missing={sorted(custom-set(translations))[:5]}, extra={sorted(set(translations)-custom)[:5]}")
    return translations


def base_and_existing_custom(path, custom):
    text = path.read_text(encoding="utf-8")
    existing = read_pairs(path)
    preserved = {key: existing[key] for key in custom if key in existing}
    if MARKER in text:
        text = text.split(MARKER, 1)[0]
    # Drop any earlier development marker sections, regardless of spelling.
    lines = []
    for line in text.splitlines():
        if line.startswith("# Exact challenge-objective") or line.startswith("# Complete challenge objective"):
            break
        if line.startswith("# Exact whole-sentence challenge"):
            break
        lines.append(line)
    return "\n".join(lines).rstrip(), preserved


def main():
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    custom = set(manifest["customObjectives"])
    if len(custom) != 14:
        raise SystemExit(f"Expected 14 custom objectives, found {len(custom)}")
    translations = read_tsv(custom)

    # Read the existing reviewed Polish/Korean custom translations before stripping old blocks.
    preserved = {}
    for locale in ("pl-PL", "ko-KR"):
        path = LANG / f"{locale}.lang"
        _, existing = base_and_existing_custom(path, custom)
        if set(existing) != custom:
            raise SystemExit(f"{locale}: missing reviewed custom translations: {sorted(custom-set(existing))[:5]}")
        preserved[locale] = existing

    for locale in COLUMNS:
        path = LANG / f"{locale}.lang"
        base, _ = base_and_existing_custom(path, custom)
        rows = [f"{objective}={translations[objective][locale]}" for objective in sorted(custom)]
        path.write_text(base + "\n\n" + MARKER + "\n" + "\n".join(rows) + "\n", encoding="utf-8")
        print(f"{locale}: wrote {len(rows)} custom objectives; 68 Rockstar objectives use official hashes")

    for locale in ("pl-PL", "ko-KR"):
        path = LANG / f"{locale}.lang"
        base, _ = base_and_existing_custom(path, custom)
        rows = [f"{objective}={preserved[locale][objective]}" for objective in sorted(custom)]
        path.write_text(base + "\n\n" + MARKER + "\n" + "\n".join(rows) + "\n", encoding="utf-8")
        print(f"{locale}: preserved {len(rows)} reviewed custom objectives")

    # English source strings are already English; no duplicate custom translations needed.
    print("en-US: English challenge objective source strings used directly")


if __name__ == "__main__":
    main()
