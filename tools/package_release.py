#!/usr/bin/env python3
"""Create a clean end-user ZIP from a freshly built, verified Windows x64 ASI.

Example:
    python tools/package_release.py build-release/HuntingCompanion.asi

The script refuses stale/non-PE/wrong-architecture binaries or binaries that do
not contain the full embedded challenge translation payload.
"""
from __future__ import annotations

import argparse
import hashlib
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("asi", type=Path, help="freshly built HuntingCompanion.asi")
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="output ZIP (default: dist/HuntingCompanion-v<VERSION>.zip)",
    )
    args = parser.parse_args()

    asi = args.asi.expanduser().resolve()
    if not asi.is_file():
        parser.error(f"ASI not found: {asi}")

    required = [
        ROOT / "LICENSE",
        ROOT / "THIRD-PARTY-NOTICES.md",
        ROOT / "README.md",
        ROOT / "HuntingCompanion.ini",
        ROOT / "tools/verify_release_binary.py",
    ]
    missing = [str(path.relative_to(ROOT)) for path in required if not path.is_file()]
    licenses_dir = ROOT / "LICENSES"
    if not licenses_dir.is_dir() or not list(licenses_dir.glob("*.txt")):
        missing.append("LICENSES/*.txt")
    if missing:
        raise SystemExit("Missing release documentation: " + ", ".join(missing))

    version = (ROOT / "VERSION.txt").read_text(encoding="utf-8").strip()
    if not version:
        raise SystemExit("VERSION.txt is empty")

    verifier = ROOT / "tools/verify_release_binary.py"
    print("Verifying release binary...")
    subprocess.run([sys.executable, str(verifier), str(asi)], check=True)

    output = args.output
    if output is None:
        output = ROOT / "dist" / f"HuntingCompanion-v{version}.zip"
    elif not output.is_absolute():
        output = (Path.cwd() / output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)

    top = f"HuntingCompanion-{version}"
    digest = hashlib.sha256(asi.read_bytes()).hexdigest()
    license_files = sorted(licenses_dir.glob("*.txt"))

    with zipfile.ZipFile(
        output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9
    ) as archive:
        archive.write(asi, f"{top}/HuntingCompanion.asi")
        archive.write(ROOT / "README.md", f"{top}/README.md")
        archive.write(ROOT / "HuntingCompanion.ini", f"{top}/HuntingCompanion.ini")
        archive.write(ROOT / "LICENSE", f"{top}/LICENSE")
        archive.write(
            ROOT / "THIRD-PARTY-NOTICES.md",
            f"{top}/THIRD-PARTY-NOTICES.md",
        )
        for license_file in license_files:
            archive.write(license_file, f"{top}/LICENSES/{license_file.name}")
        archive.writestr(
            f"{top}/SHA256SUMS.txt",
            f"{digest}  HuntingCompanion.asi\n",
        )

    print(f"Created: {output}")
    print(f"ASI SHA-256: {digest}")
    print("End users install HuntingCompanion.asi only; licence files are documentation.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
