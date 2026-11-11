# Hunting Companion 1.10.0

A read-only RDR2 Story Mode companion for Pearson, the Trapper, challenges,
reinforced equipment, garment sets and focused-animal crafting indicators.

## Release highlights

- **Independent CJK rendering:** Japanese, Korean, Simplified Chinese and
  Traditional Chinese text can render even when RDR2 is running in another
  language. Regional Noto Sans CJK subsets and the Latin/Cyrillic fallback font
  are embedded in the ASI.
- **One-file installation:** all companion code, localization dictionaries,
  official item/challenge label tables and font subsets are compiled into
  `HuntingCompanion.asi`. The mod creates no `commonx` folder, `fontmap.xml`,
  language directory or external font files.
- **All 82 challenge objectives localized:** 68 objectives use Rockstar's
  official locale string hashes; 14 companion-authored composite objectives
  use exact full-sentence translations in all 12 non-English locales.
- **Official item/material names:** 290 recipe/material hashes are embedded
  for all 13 supported locales.
- **Immediate language switching:** item/material and challenge strings resolve
  against the newly selected locale on the next draw.
- **Read-only challenge tracking:** progress is read from the game's challenge
  system and is never modified.

## Supported languages

English, French, German, Italian, Spanish (Spain), Portuguese (Brazil), Polish,
Russian, Korean, Chinese (Traditional), Japanese, Spanish (Mexico), and
Chinese (Simplified).

## Installation

Copy `HuntingCompanion.asi` into the RDR2 game directory alongside the normal
ScriptHookRDR2/ASI-loader requirements. No other Hunting Companion files are
required by end users.

## Build (Windows x64 / MinGW-w64)

**Always configure a fresh build directory.** Do not reuse a copied
`build-win64` folder or its CMake/Ninja cache; those files can retain absolute
paths from another machine and can produce an old binary.

```bash
cmake -S . -B build-release -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --clean-first --verbose
python tools/verify_release_binary.py build-release/HuntingCompanion.asi
python tools/package_release.py build-release/HuntingCompanion.asi
```

The verifier checks that the compiled x64 ASI actually contains all 68 official
challenge objective translations in all 13 locales, plus the 14 custom challenge
translations in every non-English locale. The package script reruns that verifier
and creates a clean end-user ZIP with the ASI and all required licence notices.

Development-only generators and source inputs are in `tools/` and `data/`.
The official game-language ZIP archives are not needed to build or run this
release: the generated string tables are already included in `src/`.

## Source checks

```bash
python tools/embed_languages.py
python tools/validate_localizations.py
python tests/test_catalog_contract.py
python tests/test_overlay_integration.py
g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -I src \
  src/official_strings.cpp tests/test_official_strings.cpp \
  -o /tmp/test_official_strings
/tmp/test_official_strings
g++ -std=c++17 -pthread -I src tests/test_overlay_queue.cpp \
  -o /tmp/test_overlay_queue
/tmp/test_overlay_queue
```

## Runtime notes

- The CJK overlay uses Vulkan. Native Windows DirectX 12 currently falls back
  to English when the selected companion language requires the overlay.
- HDR output is not specially handled.
- Overlay text is composited above the game frame, so it cannot be occluded by
  later native game UI draws.
- ScriptHookRDR2 and a compatible ASI loader remain external game-plugin-host
  requirements; they are not Hunting Companion dependencies.

## Licensing

Hunting Companion's original source code is MIT-licensed. Third-party code and
fonts retain their own licences; see `THIRD-PARTY-NOTICES.md` and `LICENSES/`.
Rockstar-authored localization strings are not relicensed by this project MIT
licence. See the release/legal note in `THIRD-PARTY-NOTICES.md`.
