#!/usr/bin/env python3
"""ini updater: host-compiled test, and the embedded template matches the shipped ini."""
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ini = (ROOT / "HuntingCompanion.ini").read_text(encoding="utf-8").replace("\r\n", "\n")
header = (ROOT / "src" / "embedded_ini_template.hpp").read_text(encoding="utf-8")
if ini not in header:
    raise SystemExit("src/embedded_ini_template.hpp is out of date: run python tools/embed_ini.py")
version = re.search(r"^IniVersion=(\d+)$", ini, re.M)
if version is None or f"kIniVersion = {version.group(1)};" not in header:
    raise SystemExit("IniVersion mismatch between HuntingCompanion.ini and the embedded template")

with tempfile.TemporaryDirectory() as temp:
    exe = Path(temp) / "test_ini_upgrade"
    subprocess.run(["g++", "-std=c++17", "-O1", "-Wall", "-Wextra", "-I", str(ROOT / "tests" / "ini_shim"),
                    "-I", str(ROOT / "src"), str(ROOT / "src" / "ini_upgrade.cpp"),
                    str(ROOT / "tests" / "test_ini_upgrade.cpp"), "-o", str(exe)], check=True)
    result = subprocess.run([str(exe), temp], capture_output=True, text=True)
    print(result.stdout, end="")
    if result.returncode != 0:
        raise SystemExit("ini updater test failed")
