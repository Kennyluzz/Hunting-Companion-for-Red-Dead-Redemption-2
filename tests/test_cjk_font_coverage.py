#!/usr/bin/env python3
"""Every character of every CJK UI string must exist in the embedded overlay fonts.

If this fails, run:  python3 tools/build_cjk_fonts.py --ttc <NotoSansCJK-Regular.ttc>
(then tools/embed_languages.py if the .lang files changed) and rebuild.
"""
import io, re, sys
from pathlib import Path
from fontTools.ttLib import TTFont

ROOT = Path(__file__).resolve().parent.parent
header = (ROOT / "src" / "embedded_cjk_fonts.hpp").read_text(encoding="utf-8")
SYMBOLS = {"ja-JP": "kJaJP", "ko-KR": "kKoKR", "zh-CN": "kZhCN", "zh-TW": "kZhTW"}
bad = []
for locale, symbol in SYMBOLS.items():
    m = re.search(r"unsigned char " + symbol + r"\[\] = \{(.*?)\};", header, re.S)
    cmap = TTFont(io.BytesIO(bytes(int(x) for x in re.findall(r"\d+", m.group(1))))).getBestCmap()
    text = ""
    for line in (ROOT / "HuntingCompanion-Languages" / f"{locale}.lang").read_text(encoding="utf-8").splitlines():
        if line and line[0] not in "#;" and "=" in line:
            text += line.split("=", 1)[1]
    missing = sorted({c for c in text if c not in "\r\n\t" and ord(c) not in cmap})
    if missing:
        bad.append((locale, "".join(missing)))
if bad:
    sys.exit(f"Embedded CJK fonts are stale (characters would draw as '?'): {bad}")
print("CJK font coverage check passed.")
