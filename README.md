# Hunting Companion 1.0.0

DISCLAIMER: 100% AI-CODED

**For:** Red Dead Redemption 2 — Story Mode

Hunting Companion is a compact in-game menu for crafting information, challenge ranks, reinforced equipment unlocks, and Trapper garment-set progress.

## Features

- **Pearson and Trapper:** browse recipes, required materials, ownership, and relevant donation-storage counts.
- **Challenges:** read live completed-rank totals for all nine Story Mode challenge categories and see a short description of the next rank's objective.
- **Reinforced equipment:** check unlock requirements and status for all 36 reinforced bandoliers, gun belts, holsters, and off-hand holsters.
- **Garment sets:** track crafted-piece totals for all 16 Trapper garment sets and inspect their individual pieces.
- **Animal indicators:** see when a focused animal can provide a material still needed by an unowned Pearson or Trapper recipe.

Challenge objective descriptions are short reference summaries, not live objective counters. Use the in-game Challenges screen to check partial progress within a rank.

## Platform and requirements

- Red Dead Redemption 2.
- A compatible installation of ScriptHookRDR2.
- A compatible ASI loader.

**Story Mode only. Do not use with Red Dead Online.**

## Install

1. Copy `HuntingCompanion.asi` into the game's main folder, beside `RDR2.exe`.
2. Make sure ScriptHookRDR2 and your ASI loader are installed and working in the same game environment.
3. Launch Story Mode and press **F8** to open or close the menu.

The mod creates `HuntingCompanion.ini` beside the ASI on first launch. The default menu key is F8; it can also be changed from the in-game **Settings** page.

## Build from source

The project uses C++17, CMake 3.20+, Ninja, and an x86-64 MinGW-w64 toolchain. On Linux, the included toolchain file cross-compiles the Windows x64 ASI:

```sh
cmake -S . -B build-win64 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build-win64 --parallel
```

The output is `build-win64/HuntingCompanion.asi`.

This source archive does not include ScriptHookRDR2, an ASI loader, or a prebuilt ASI binary. Obtain third-party components separately and follow their own licences and installation instructions.

## Controls

| Key | Action |
| --- | --- |
| F8 | Open / close the menu |
| Up / Down | Navigate |
| Enter | Select / open |
| Backspace / Esc | Go back |
| Page Up / Page Down | Move faster through long lists |

## Read-only behavior

Challenge ranks and game catalog information are read from the running game. Hunting Companion does not advance, complete, reset, or otherwise modify challenge progress. It does not perform purchases or crafting, change inventory, or write to save data.

## Licence and disclaimer

Hunting Companion's project source and documentation are provided under the **MIT License**. See [LICENSE](LICENSE). You may use, modify, and redistribute the project under that licence; keep the copyright and licence notice with copies or substantial portions of the project.

The licence covers only project material that can be licensed by the project maintainer. It does not grant rights to Red Dead Redemption 2, Rockstar Games, Take-Two Interactive, their trademarks, game assets, ScriptHookRDR2, or other third-party software.

Hunting Companion is an unofficial fan-made modification. It is not affiliated with, endorsed by, or sponsored by Rockstar Games or Take-Two Interactive.
