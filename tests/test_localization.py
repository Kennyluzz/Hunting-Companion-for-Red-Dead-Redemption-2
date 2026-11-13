#!/usr/bin/env python3
"""Every phrase in every .lang file must come back from the embedded tables
exactly as written (trimmed, last duplicate wins), with no length limits."""
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LOCALES = ["en-US", "fr-FR", "de-DE", "it-IT", "es-ES", "pt-BR", "pl-PL",
           "ru-RU", "ko-KR", "zh-TW", "ja-JP", "es-MX", "zh-CN"]


def parse(path):
    entries = {}
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip(" \t\r\n")
        if not line or line[0] in "#;" or "=" not in line:
            continue
        key, value = line.split("=", 1)
        key, value = key.strip(" \t\r\n"), value.strip(" \t\r\n")
        if key and value:
            entries[key] = value
    return entries


def main():
    with tempfile.TemporaryDirectory() as tmp:
        exe = Path(tmp) / "test_localization"
        subprocess.run(["g++", "-std=c++17", "-O1", "-pthread", "-I", str(ROOT / "tests/host_shim"),
                        "-I", str(ROOT / "src"), str(ROOT / "src/localization.cpp"),
                        str(ROOT / "src/official_strings.cpp"), str(ROOT / "tests/test_localization.cpp"),
                        "-o", str(exe)], check=True)
        queries, expected = [], []
        for index, locale in enumerate(LOCALES):
            for key, value in parse(ROOT / "HuntingCompanion-Languages" / f"{locale}.lang").items():
                queries.append(f"{index}\t{key}")
                expected.append(value)
            queries.append(f"{index}\tThis phrase is not translated")
            expected.append("This phrase is not translated")
        result = subprocess.run([str(exe)], input="\n".join(queries) + "\n", capture_output=True,
                                text=True, encoding="utf-8", check=False)
        if result.returncode != 0:
            raise SystemExit(f"test_localization exited with {result.returncode}")
        actual = result.stdout.split("\n")[:-1]
        failures = [(q, e, a) for q, e, a in zip(queries, expected, actual) if e != a]
        if len(actual) != len(expected) or failures:
            for q, e, a in failures[:10]:
                print(f"FAIL {q!r}: expected {e!r}, got {a!r}")
            raise SystemExit(1)
    print(f"PASS: {len(queries)} UI phrases across {len(LOCALES)} locales resolve exactly; "
          "pointers survive language switches.")


if __name__ == "__main__":
    sys.exit(main())
