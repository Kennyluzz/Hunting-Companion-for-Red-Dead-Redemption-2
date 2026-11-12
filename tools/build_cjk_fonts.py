#!/usr/bin/env python3
"""Build the embedded CJK subset fonts for the Hunting Companion overlay.

For each CJK companion locale this collects every character the mod can ever
display (the .lang phrases, the embedded official Rockstar item names and a
small safety set), subsets Noto Sans CJK down to just those glyphs, converts
the outlines to plain TrueType (so ImGui's rasteriser has no CFF edge cases)
and writes src/embedded_cjk_fonts.hpp.

Noto Sans CJK is licensed under the SIL Open Font License 1.1, which allows
embedding and subsetting. The licence/copyright notice must ship with the mod
(see THIRD-PARTY-NOTICES.md).

Usage:
  python3 tools/build_cjk_fonts.py --ttc /path/to/NotoSansCJK-Bold.ttc
"""
import argparse
import io
import re
import sys
from pathlib import Path

from fontTools import subset
from fontTools.pens.cu2quPen import Cu2QuPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTCollection, TTFont, newTable

ROOT = Path(__file__).resolve().parent.parent

# key -> (index inside NotoSansCJK-Bold.ttc, C++ symbol, locales whose text it must cover)
NON_CJK = ["en-US", "fr-FR", "de-DE", "it-IT", "es-ES", "pt-BR", "pl-PL", "ru-RU", "es-MX"]
LOCALES = {
    "ja-JP": (0, "kJaJP", ["ja-JP"]),
    "ko-KR": (1, "kKoKR", ["ko-KR"]),
    "zh-CN": (2, "kZhCN", ["zh-CN"]),
    "zh-TW": (3, "kZhTW", ["zh-TW"]),
    # Latin/Cyrillic font: used when the *game* runs in a CJK language, whose own fonts
    # lack accented Latin letters and Cyrillic.
    "latin": (None, "kLatin", NON_CJK),   # index None = Liberation Sans (--latin-ttf)
}

# Characters that are always included for a locale, on top of the strings.
SAFETY = {
    "common": [(0x20, 0x7E), (0xA0, 0xFF), (0x2010, 0x2027), (0x2030, 0x203A),
               (0x2190, 0x2193), (0x25A0, 0x25CF), (0x3000, 0x303F), (0xFF01, 0xFF5E)],
    "ja-JP": [(0x3040, 0x30FF)],
    "ko-KR": [(0x3130, 0x318F)],
    "zh-CN": [],
    "zh-TW": [(0x3100, 0x312F)],
    "latin": [(0x100, 0x17F), (0x400, 0x45F), (0x2018, 0x201F)],
}


def lang_chars(locale):
    chars = set()
    path = ROOT / "HuntingCompanion-Languages" / f"{locale}.lang"
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line[0] in "#;" or "=" not in line:
            continue
        chars.update(line.split("=", 1)[1])
    return chars


def official_chars(locale):
    sym = "k_" + locale.replace("-", "_")
    text = (ROOT / "src" / "embedded_official_strings.hpp").read_text(encoding="utf-8")
    m = re.search(r"Entry\s+" + sym + r"\[\]\s*=\s*\{(.*?)\n\};", text, re.S)
    if not m:
        sys.exit(f"cannot find {sym} in embedded_official_strings.hpp")
    chars = set()
    for s in re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1)):
        chars.update(s)
    return chars


def tsv_chars(locale):
    """Characters of the embedded challenge-objective translations for one locale."""
    path = ROOT / "data" / "challenge_translations.tsv"
    if not path.exists():
        return set()
    lines = path.read_text(encoding="utf-8").splitlines()
    header = [c.strip() for c in lines[0].lstrip("# ").split("|")]
    if locale not in header:
        return set()
    col = header.index(locale)
    chars = set()
    for line in lines[1:]:
        parts = line.split("=")
        if len(parts) == len(header):
            chars.update(parts[col])
        else:  # '=' inside text: be generous and take everything
            chars.update(line)
    return chars


def string_chars(sources):
    chars = set()
    for loc in sources:
        chars |= lang_chars(loc) | official_chars(loc) | tsv_chars(loc)
    return {c for c in chars if c not in "\r\n\t"}


def wanted_codepoints(key, sources):
    cps = {ord(c) for c in string_chars(sources)}
    for lo, hi in SAFETY["common"] + SAFETY[key]:
        cps.update(range(lo, hi + 1))
    return sorted(cps)


def to_truetype(font):
    """Convert a CFF (OTTO) font to TrueType outlines (the fontTools otf2ttf recipe)."""
    order = font.getGlyphOrder()
    gs = font.getGlyphSet()
    glyphs = {}
    for name in order:
        pen = TTGlyphPen(gs)
        gs[name].draw(Cu2QuPen(pen, 1.0, reverse_direction=True))
        glyphs[name] = pen.glyph()
    font["loca"] = newTable("loca")
    font["glyf"] = glyf = newTable("glyf")
    glyf.glyphOrder = order
    glyf.glyphs = glyphs
    del font["CFF "]
    if "VORG" in font:
        del font["VORG"]
    glyf.compile(font)
    hmtx = font["hmtx"]
    for name, g in glyf.glyphs.items():
        if hasattr(g, "xMin"):
            hmtx[name] = (hmtx[name][0], g.xMin)
    font["maxp"] = maxp = newTable("maxp")
    maxp.tableVersion = 0x00010000
    maxp.maxZones = 1
    maxp.maxTwilightPoints = maxp.maxStorage = maxp.maxFunctionDefs = 0
    maxp.maxInstructionDefs = maxp.maxStackElements = maxp.maxSizeOfInstructions = 0
    maxp.maxComponentElements = max(
        (len(getattr(g, "components", [])) for g in glyf.glyphs.values()), default=0)
    maxp.compile(font)
    post = font["post"]
    post.formatType = 2.0
    post.extraNames = []
    post.mapping = {}
    post.glyphOrder = order
    font.sfntVersion = "\x00\x01\x00\x00"


def rename_latin_derivative(font):
    """Avoid Liberation's Reserved Font Name in this modified/subsetted derivative."""
    replacements = {
        1: "Hunting Companion Sans",       # Font Family
        2: "Regular",                       # Subfamily
        3: "Hunting Companion Sans Regular",# Unique identifier
        4: "Hunting Companion Sans",        # Full name
        6: "HuntingCompanionSans",          # PostScript name
        16: "Hunting Companion Sans",       # Typographic family
        17: "Regular",                      # Typographic subfamily
        18: "Hunting Companion Sans",       # Compatible full name
        21: "Hunting Companion Sans",       # WWS family
        22: "Regular",                      # WWS subfamily
    }
    name_table = font["name"]
    for record in name_table.names:
        replacement = replacements.get(record.nameID)
        if replacement is None:
            continue
        try:
            record.string = replacement.encode(record.getEncoding())
        except Exception:
            record.string = replacement.encode("utf-16-be")
    # Keep copyright/licence/version fields intact; only the primary family identity changes.


def build(ttc_path, key, index, sources, latin_ttf=None):
    if index is None:
        font = TTFont(str(latin_ttf))   # already TrueType outlines
        rename_latin_derivative(font)
    else:
        font = TTCollection(str(ttc_path), lazy=False).fonts[index]
    cps = wanted_codepoints(key, sources)
    opts = subset.Options()
    opts.layout_features = []          # ImGui does no shaping; drop GSUB/GPOS
    opts.notdef_outline = True
    opts.name_IDs = [0, 1, 2, 3, 4, 5, 6, 13, 14]   # keep copyright + licence
    opts.hinting = False
    opts.drop_tables += ["DSIG", "vhea", "vmtx", "VORG", "BASE", "GDEF", "GPOS", "GSUB", "STAT"]
    sub = subset.Subsetter(opts)
    sub.populate(unicodes=cps)
    sub.subset(font)
    if index is not None:
        to_truetype(font)
    buf = io.BytesIO()
    font.save(buf)
    data = buf.getvalue()
    # Verify: every wanted codepoint (that the source font has) survived.
    check = TTFont(io.BytesIO(data))
    cmap = check.getBestCmap()
    missing_from_subset = [cp for cp in cps if cp not in cmap]
    return data, cps, missing_from_subset


def cpp_array(symbol, data):
    lines = [f"inline constexpr unsigned char {symbol}[] = {{"]
    for i in range(0, len(data), 24):
        lines.append("    " + ",".join(str(b) for b in data[i:i + 24]) + ",")
    lines.append("};")
    lines.append(f"inline constexpr unsigned int {symbol}Size = {len(data)};")
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ttc", required=True, help="path to NotoSansCJK-Bold.ttc")
    ap.add_argument("--latin-ttf", default="/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
                    help="Liberation Sans Regular (SIL OFL 1.1) for the Latin/Cyrillic slot")
    ap.add_argument("--out", default=str(ROOT / "src" / "embedded_cjk_fonts.hpp"))
    args = ap.parse_args()

    parts = [
        "#pragma once",
        "// Generated by tools/build_cjk_fonts.py. Do not edit.",
        "// Subsets of Noto Sans CJK Regular (SIL Open Font License 1.1), reduced to the",
        "// characters Hunting Companion can display. See THIRD-PARTY-NOTICES.md.",
        "namespace embedded_cjk_fonts {",
    ]
    total = 0
    for locale, (index, symbol, sources) in LOCALES.items():
        data, cps, missing = build(args.ttc, locale, index, sources, args.latin_ttf)
        total += len(data)
        # Codepoints in the safety set that the source font lacks are harmless;
        # codepoints from real strings must all be present.
        real = {ord(c) for c in string_chars(sources)}
        real_missing = sorted(set(missing) & real)
        print(f"{locale}: {len(cps)} wanted codepoints, subset {len(data)/1024:.0f} KB, "
              f"real-string codepoints missing from font: {len(real_missing)}")
        if real_missing:
            print("  MISSING:", " ".join(f"U+{c:04X}" for c in real_missing[:40]))
            sys.exit(1)
        parts.append(cpp_array(symbol, data))
    parts.append("} // namespace embedded_cjk_fonts")
    Path(args.out).write_text("\n".join(parts) + "\n", encoding="utf-8")
    print(f"wrote {args.out} ({total/1024:.0f} KB of font data)")


if __name__ == "__main__":
    main()
