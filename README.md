# Hunting Companion

An in-game hunting and crafting companion for **Red Dead Redemption 2 (Story Mode)**,
built as a single ScriptHookRDR2 plugin (`HuntingCompanion.asi`).

**Just want to play?** Download the mod from
[Nexus Mods](https://www.nexusmods.com/reddeadredemption2/mods/11452). Install steps,
controls and options are in [`README-PLAYERS.md`](README-PLAYERS.md), which is also
the readme shipped in the release zip.

> The mod is AI-coded. The ideas, direction and in-game testing are human; the
> code was written by AI assistants.

This page is for people who want to build or change the mod. Release notes are in
[`CHANGELOG.md`](CHANGELOG.md).

## Building

Target is Windows x64. You need MinGW-w64 (v11 or newer), CMake and Ninja; it
cross-compiles from Linux. All third-party sources are vendored in `third_party/`.
On Ubuntu: `sudo apt-get install mingw-w64 cmake ninja-build`.

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --clean-first
```

The result is `build/HuntingCompanion.asi`. Use a fresh build directory each time
so an old CMake cache cannot produce a stale binary.

To make a release zip (the `.asi`, the player readme, the changelog, the default ini
and the licences):

```bash
python tools/package_release.py build/HuntingCompanion.asi
```

## Layout

| Path | What it is |
|---|---|
| `src/main.cpp` | The mod: game natives, recipe and challenge tracking, HUD icons, the menu |
| `src/localization.*` | Loads the embedded language tables |
| `src/official_strings.*` | The game's own item and challenge names, per language |
| `src/native_fonts.cpp` | Loads the game's own font libraries (Japanese, Korean, Chinese, Latin, Cyrillic) on demand |
| `src/font_catalog.hpp` | Reads the game's `fontmap.xml` and builds each language's font list |
| `src/embedded_*.hpp` | Generated files; do not edit by hand (see below) |
| `HuntingCompanion-Languages/` | One `.lang` file per language |
| `data/` | Source data for the generators |
| `tools/` | Generators, the localisation validator and the release packager |
| `tests/` | Checks that run without the game |
| `third_party/` | MinHook |

## Generated files

Rerun the generator after changing its input:

| Input | Command | Output |
|---|---|---|
| `HuntingCompanion-Languages/*.lang` | `python tools/embed_languages.py` | `src/embedded_languages.hpp` |
| The game's English string table (`.xml`, or a `.zip` holding it) | `python tools/build_compendium_advice.py <file>` | `data/compendium_advice.json` |
| `data/compendium_advice.json`, `data/weapon_icons.json` | `python tools/embed_hunting_recommendations.py` | `src/embedded_hunting_recommendations.hpp` |

## Tests

None of these need the game:

```bash
python tools/validate_localizations.py
python tests/test_catalog_contract.py
python tests/test_hunting_recommendations.py
python tests/test_crafting_uses.py
python tests/test_native_fonts_integration.py
python tests/test_localization.py
g++ -std=c++17 -I src src/official_strings.cpp tests/test_official_strings.cpp -o test_official_strings && ./test_official_strings
```

They check data and wiring only. Anything visible has to be tested in the game.

## How it works

- **Read-only.** The mod only reads game data through script natives. It never
  changes progress, inventory or saves.
- **Weapon advice.** For the animal in focus the game returns its Animal Compendium
  short-description label (`COMPENDIUM_GET_SHORT_DESCRIPTION_FROM_PED`); the HUD
  shows the weapon and ammunition from that label's clean-kill line
  (`data/compendium_advice.json`, built from the game's English string table).
  Legendary animals are recognised by their animal type and show "Any weapon".
- **Text.** All text is drawn by the game with its own fonts. The game only loads
  the font library of its own language (`font_lib_efigs`, `font_lib_japanese`,
  `font_lib_korean`, `font_lib_chinese`, `font_lib_simplified_chinese`, ... in
  `scaleform_frontend.rpf`). When the companion language needs another one,
  `native_fonts.cpp` asks the game's streaming system to load it and adds it to
  Scaleform's font library with the game's own function, the way RedM's
  `REGISTER_FONT_FILE` does. The font names and each language's menu fonts come
  from the game's own `fontmap.xml`, read through the game's file system. While
  it loads the menu keeps its current language; if it cannot be loaded, a CJK
  language falls back to English and a Latin/Cyrillic one to the game's basic font.
- **Hooks.** One game function is hooked from a worker thread at load (found by
  byte pattern; skipped if the pattern is not unique) so the mod learns where the
  game keeps its font library. Nothing else in the game is changed.
- **Diagnostics.** `FontLog=1` under `[Fonts]` in `HuntingCompanion.ini` writes
  `HuntingCompanion.fonts.log` to the game folder.

## Credits

- Weapon advice is the game's own Animal Compendium text, read from the game for
  the animal in focus.
- Texture, model and animal-type names were checked against the community catalogue
  <https://github.com/femga/rdr3_discoveries>.
- Built with [MinHook](https://github.com/TsudaKageyu/minhook).
- Loading the game's own font libraries follows the approach of the open-source
  [RedM/FiveM](https://github.com/citizenfx/fivem) project; see `THIRD-PARTY-NOTICES.md`.

## Licence

The mod's code is MIT-licensed (`LICENSE`). Third-party components keep their own
licences (`THIRD-PARTY-NOTICES.md`, `LICENSES/`). Rockstar's text and trademarks are
not covered by the MIT licence. Hunting Companion is an unofficial fan modification,
not affiliated with or endorsed by Rockstar Games or Take-Two Interactive.
