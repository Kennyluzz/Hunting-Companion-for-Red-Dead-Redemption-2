# Hunting Companion 1.15.0

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
python tests/test_hunting_recommendations.py
python tests/test_crafting_uses.py
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

## Version 1.12.0

- **Hunting recommendations:** weapon choices and named ammunition cover every huntable species in the standard 162-entry Story Mode Animal Compendium. Domestic / working animals without a sensible hunting recommendation are explicitly classified rather than assigned guessed weapons. Icons are 50% larger, grouped above vendor indicators and placed over a layered dark backdrop for contrast.
- **Craftable Now:** the filter state is prominent in the category/recipe header. Its toggle key is configurable in Settings and saved in HuntingCompanion.ini. Categories with no matching recipes are hidden; empty or unknown catalog categories are not shown.
- **One-file installation remains unchanged.** Weapon/ammunition textures are streamed from the installed game's `inventory_items` texture dictionary; no extracted Rockstar artwork or extra runtime files are shipped.

### Recommendation source

The recommendation data is based on the two-page Bloxodes Story Mode animal guide (updated November 2, 2026). Model variants that share a Compendium animal group reuse that group's weapon/ammunition guidance:

- https://bloxodes.com/wiki/red-dead-redemption-2/animals
- https://bloxodes.com/wiki/red-dead-redemption-2/animals/page/2

The guide's individual animal entries and group-level recommendation table are used where available. The remaining species are cross-checked against the supplementary clean-kill and animal-size references recorded in the JSON provenance. Weapon alternatives are preserved, and a named ammunition type is attached only to the weapon choice that uses it. Panther, regional Pronghorn, crane, egret, heron, cormorant, pelican and macaw variants that share a runtime model are grouped under that model; model notes document where the game API cannot distinguish their Compendium variants.

The texture dictionary and sprite names were cross-checked against the community RDR3 discoveries inventory-texture catalogue:
https://github.com/femga/rdr3_discoveries/tree/master/useful_info_from_rpfs/textures/inventory_items

### Animal coverage

The recommendation dataset classifies all 162 standard Animal Compendium entries. Fifteen domestic or working-animal entries (the cat, dogs, donkey and mule) are deliberately marked as having no hunting recommendation. Crayfish is included with its Compendium Bow / Small Game Arrow note, although it is normally collected from holes. Legendary animals are separate Compendium entries and are outside this 162-entry standard-animal set.

### Craftable Now rules

The filter checks the live material quantities in Pearson's or the Trapper's donation storage against each recipe's individual material costs. Already-owned items are excluded. Garment sets are treated as their remaining individual components, with repeated material requirements aggregated across the entire set so shared stock is not counted twice. Reinforced equipment is excluded because it is challenge-unlocked and purchased rather than crafted from donated pelts.

Craftable Now is an animal-material readiness indicator, not a full purchase-readiness check. It considers only recognized animal parts (pelts, hides, feathers, antlers and carcasses) in the vendor's stored donations. Cash and all other non-animal recipe costs are ignored by this filter, so they neither enable nor block the status. Craftable Now itself does not inspect Arthur's carried inventory or horse cargo. The separate Crafting Uses submenu reads those locations without changing them; neither feature donates, consumes, buys, or modifies items.


## Version 1.13.0

- **Crafting Uses submenu:** disabled and greyed out until the read-only haul scan finds at least one recognized animal part.
- **Live haul sources:** Arthur's satchel inventory, the entity he is physically carrying, and carriable pelts/carcasses attached to the player's active saddle horse. The haul is refreshed every 500 ms while the companion menu is open.
- **Only remaining uses:** Pearson and Trapper recipes are listed only when an unowned item requires a material present in the current haul. Garment sets expand into their individual pieces, so a hat already crafted is not suggested again while the remaining coat or gloves can still appear.
- **Haul versus vendor stock:** item details show the amount in the current haul and the amount already donated to that vendor. Readiness is labelled "Ready after donation" only when animal and other non-cash costs are accounted for; cash is not queried, so cash-priced items are labelled "Animal parts ready" instead.
- **Read-only:** no inventory transfer, horse-cargo transfer, donation, crafting, purchase, consumption, or game-pause native is called by this feature.

Intact carcasses only match catalogued carcass recipes until they are skinned; carried pelt entities are resolved back to their source animal. The public entity API does not expose a reliable legendary-pelt item identity for every carried/horse-attached pelt. Legendary pelt recipes are therefore included when the inventory native reports that exact item, but are not guessed from an ordinary animal's model or skinning-quality value.

## 1.15.0 notes

Pearson is retained alongside the Trapper. This release fixes the menu layout, adds a weapon/ammo icon toggle (Settings or the `]` key; the craftable filter now defaults to `[`), simplifies the recipe amounts to "held / needed", and rebuilds the embedded CJK fonts and translations.
Ordinary Pearson recipes, satchels and camp decorations remain visible.
