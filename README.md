# Hunting Companion

An in-game companion for **Red Dead Redemption 2 (Story Mode)**, built as a
ScriptHookRDR2 plugin (`HuntingCompanion.asi`).

> **This mod is entirely AI-coded.** The ideas, direction and in-game testing
> are human; the code was written by AI assistants.

## Features

- **Animal indicators:** when you focus an animal, see whether it can supply a
  material still needed by an unowned Pearson or Trapper recipe.
- **Weapon and ammo suggestions:** icons and names of the weapon and ammunition
  that give the cleanest kill for the focused animal, using the weapon wheel's
  own ammunition icons. Toggle them from Settings or with a hotkey.
- **Pearson and Trapper:** browse recipes, required materials, ownership and
  donation-stock counts, with a "craftable now" filter.
- **Crafting Uses:** lists the uncrafted recipes that can use the pelts and
  animal parts you are carrying (satchel, in hand, or on your horse).
- **Challenges:** live rank totals for all nine Story Mode challenge categories
  and a short description of each next rank.
- **Reinforced equipment:** unlock requirements for all 36 bandoliers, gun
  belts, holsters and off-hand holsters.
- **Garment sets:** crafted-piece totals for all 16 Trapper garment sets.
- **13 languages, switchable in-game:** English, French, German, Italian,
  Spanish (Spain and Mexico), Brazilian Portuguese, Polish, Russian, Japanese,
  Korean, Simplified Chinese and Traditional Chinese.

The mod only reads game data. It never changes progress, inventory, cargo or
save data. **Do not use it with Red Dead Online.**

## Install

Requires ScriptHookRDR2 and an ASI loader. Copy `HuntingCompanion.asi` next to
`RDR2.exe` and press **F8** in Story Mode. Everything the mod needs (language
tables, item names, fonts) is built into that single file; it writes only
`HuntingCompanion.ini`.

Default keys (changeable in the in-game Settings page, or in the ini file):

| Action | Default |
|---|---|
| Open / close the menu | F8 |
| Toggle the craftable filter | `[` |
| Toggle weapon/ammo icons | `]` |

See `HuntingCompanion.ini.example` for all options.

### Asian languages

Japanese, Korean and Chinese text is drawn by a small built-in overlay when the
game itself is set to another language. The overlay needs the **Vulkan**
renderer (Linux/Proton included). On native Windows DirectX 12 the mod shows
English instead; if the game itself is set to that language, any renderer works.

## Building

Windows x64 with MinGW-w64, CMake and Ninja. Third-party sources are vendored
in `third_party/` (see `third_party/README.md`).

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --clean-first
python tools/verify_release_binary.py build/HuntingCompanion.asi
python tools/package_release.py build/HuntingCompanion.asi
```

Use a fresh build directory each time so an old CMake cache cannot produce a
stale binary.

### Generated files

Some source files are generated; rerun the generator after changing its input:

| Input | Command | Output |
|---|---|---|
| `HuntingCompanion-Languages/*.lang` | `python tools/embed_languages.py` | `src/embedded_languages.hpp` |
| `data/hunting_recommendations.json` | `python tools/embed_hunting_recommendations.py` | `src/embedded_hunting_recommendations.hpp` |
| `.lang` files + official strings | `python tools/build_cjk_fonts.py --ttc NotoSansCJK-Regular.ttc` | `src/embedded_cjk_fonts.hpp` |

After editing a `.lang` file, run both the language and font generators, or
characters can show up as `?` in-game. `tests/test_cjk_font_coverage.py` checks
this.

### Tests

```bash
python tools/validate_localizations.py
python tests/test_catalog_contract.py
python tests/test_hunting_recommendations.py
python tests/test_crafting_uses.py
python tests/test_cjk_font_coverage.py
python tests/test_overlay_integration.py
g++ -std=c++17 -pthread -I src tests/test_overlay_queue.cpp -o /tmp/test_overlay_queue && /tmp/test_overlay_queue
g++ -std=c++17 -I src src/official_strings.cpp tests/test_official_strings.cpp -o /tmp/test_official_strings && /tmp/test_official_strings
```

## How the Asian-language overlay works

Text for a language the game cannot draw is queued by the script thread and
drawn with Dear ImGui into the Vulkan swapchain just before each present, using
embedded subsets of Noto Sans CJK (and a Latin/Cyrillic font for accented
letters when the game itself is in a CJK language). No files are created. Set
`[Overlay] OverlayLog=1` in the ini for diagnostics, and `CjkTextScalePercent` /
`CjkTextYOffsetPermille` to adjust size and position.

Limitations: Vulkan only; HDR is not specially handled; overlay text is drawn
over the finished frame, so later native UI cannot cover it.

## Data sources

- Weapon and ammo suggestions are based on the Bloxodes Story Mode animal guide
  (<https://bloxodes.com/wiki/red-dead-redemption-2/animals>), with sources
  recorded in `data/hunting_recommendations.json`.
- Item, material and challenge names come from the game's own language packs
  and are embedded at build time (`tools/build_official_strings.py`).
- Texture names were checked against the community RDR3 discoveries catalogue
  (<https://github.com/femga/rdr3_discoveries>). Game textures are streamed from
  the installed game; no Rockstar artwork is distributed.

## Licence

Original code is MIT-licensed (see `LICENSE`). Third-party components keep their
own licences (`THIRD-PARTY-NOTICES.md`, `LICENSES/`). Rockstar-authored text and
trademarks are not covered by the MIT licence. Hunting Companion is an
unofficial fan modification, not affiliated with or endorsed by Rockstar Games
or Take-Two Interactive.
