# Hunting Companion

An in-game hunting and crafting companion for **Red Dead Redemption 2 (Story Mode)**,
built as a single ScriptHookRDR2 plugin (`HuntingCompanion.asi`).

**Just want to play?** Download the mod from
[Nexus Mods](https://www.nexusmods.com/reddeadredemption2/mods/11452). Install steps,
controls and options are in [`README-PLAYERS.md`](README-PLAYERS.md), which is also
the readme shipped in the release zip.

> The mod is AI-coded. The ideas, direction and in-game testing are human; the
> code was written by AI assistants.

This page is for people who want to build or change the mod.

## Building

Target is Windows x64. You need MinGW-w64, CMake and Ninja; it cross-compiles
from Linux. All third-party sources are vendored in `third_party/`.

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --clean-first
```

The result is `build/HuntingCompanion.asi`. Use a fresh build directory each time
so an old CMake cache cannot produce a stale binary.

To make a release zip (the `.asi`, the player readme, the default ini and the licences):

```bash
python tools/package_release.py build/HuntingCompanion.asi
```

## Layout

| Path | What it is |
|---|---|
| `src/main.cpp` | The mod: game natives, recipe and challenge tracking, HUD icons, the menu |
| `src/localization.*` | Loads the embedded language tables |
| `src/official_strings.*` | The game's own item and challenge names, per language |
| `src/cjk_overlay.cpp` | Draws Japanese/Korean/Chinese text on Vulkan |
| `src/cjk_overlay_dx12.cpp` | The same on DirectX 12 (experimental) |
| `src/overlay_queue.hpp` | Text queue shared by the script thread and the overlay, plus HDR colour conversion |
| `src/embedded_*.hpp` | Generated files; do not edit by hand (see below) |
| `HuntingCompanion-Languages/` | One `.lang` file per language |
| `data/` | Source data for the generators |
| `tools/` | Generators, the localisation validator and the release packager |
| `tests/` | Checks that run without the game |
| `third_party/` | Dear ImGui, MinHook and the Vulkan headers |

## Generated files

Rerun the generator after changing its input:

| Input | Command | Output |
|---|---|---|
| `HuntingCompanion-Languages/*.lang` | `python tools/embed_languages.py` | `src/embedded_languages.hpp` |
| `data/hunting_recommendations.json` | `python tools/embed_hunting_recommendations.py` | `src/embedded_hunting_recommendations.hpp` |
| `.lang` files and official strings | `python tools/build_cjk_fonts.py --ttc NotoSansCJK-Bold.ttc` | `src/embedded_cjk_fonts.hpp` |

After editing a `.lang` file, run both the language and the font generator.
Otherwise new characters can show up as `?` in Japanese, Korean or Chinese.
The font generator needs Python's `fontTools` and a copy of `NotoSansCJK-Bold.ttc`.

## Tests

None of these need the game:

```bash
python tools/validate_localizations.py
python tests/test_catalog_contract.py
python tests/test_hunting_recommendations.py
python tests/test_crafting_uses.py
python tests/test_cjk_font_coverage.py
python tests/test_overlay_integration.py
g++ -std=c++17 -pthread -I src tests/test_overlay_queue.cpp -o test_overlay_queue && ./test_overlay_queue
g++ -std=c++17 -I src src/official_strings.cpp tests/test_official_strings.cpp -o test_official_strings && ./test_official_strings
```

They check data and wiring only. Anything visible has to be tested in the game.

## How it works

- **Read-only.** The mod only reads game data through script natives. It never
  changes progress, inventory or saves.
- **Text.** Menu text is drawn with the game's own fonts. The game only loads
  Japanese, Korean or Chinese glyphs when it runs in that language, so for those
  languages on any other game language the text is drawn by an overlay instead:
  Dear ImGui renders it into the game's swapchain just before each present, using
  embedded subsets of Noto Sans CJK.
- **Overlay hooks.** They are installed from a worker thread at load, on Vulkan and
  DirectX 12. The DirectX 12 renderer creates nothing until there is text to draw
  and switches itself off for the session after any failure.
- **Diagnostics.** `OverlayLog=1` in `HuntingCompanion.ini` writes
  `HuntingCompanion.overlay.log` to the game folder.

## Credits

- Texture names were checked against the community catalogue
  <https://github.com/femga/rdr3_discoveries>.
- Built with [Dear ImGui](https://github.com/ocornut/imgui),
  [MinHook](https://github.com/TsudaKageyu/minhook) and Noto Sans CJK; see
  `THIRD-PARTY-NOTICES.md`.

## Licence

The mod's code is MIT-licensed (`LICENSE`). Third-party components keep their own
licences (`THIRD-PARTY-NOTICES.md`, `LICENSES/`). Rockstar's text and trademarks are
not covered by the MIT licence. Hunting Companion is an unofficial fan modification,
not affiliated with or endorsed by Rockstar Games or Take-Two Interactive.
