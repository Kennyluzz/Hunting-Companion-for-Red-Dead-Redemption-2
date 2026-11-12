# 1.15.0 release checklist

## Source-side checks completed for this package

- [x] Official catalogue/material tables: 290 hashes x 13 locales.
- [x] Official challenge objective tables: 68 hashes x 13 locales.
- [x] Companion-authored challenge objectives: 14 exact translations x 12 non-English locales.
- [x] Embedded CJK and Latin/Cyrillic font subsets generated after the current
  official string and `.lang` data.
- [x] Unit, catalogue, localization and overlay source checks pass.

## Required before publishing a binary release

- [ ] Perform a fresh Windows x64 MinGW-w64 configure/build from this source tree.
- [ ] Run `tools/verify_release_binary.py` against the newly built ASI.
- [ ] Test all 13 companion locales in-game, including challenge objectives.
- [ ] Confirm the Vulkan overlay with the target RDR2/driver/loader combination.
- [ ] Confirm public redistribution of embedded Rockstar-authored localization
  text is permitted, or change the distribution design accordingly.
- [ ] Include the newly built `HuntingCompanion.asi` in the release archive.

**Important:** no pre-existing build directory or ASI is included. The previous
build cache pointed at a different absolute source path and the bundled ASI was
compiled before the 358-entry official table and final font/header regeneration.
It is intentionally excluded to prevent shipping that stale binary.

## 1.12.0 additions

- [x] Recommendation table covers all 162 standard Animal Compendium entries: weapon guidance for 77 huntable runtime species, plus explicit no-recommendation classifications for 15 domestic / working-animal entries.
- [x] Alternative weapon choices and named ammunition are stored separately; ammo is associated with the applicable weapon.
- [x] Panther and regional Pronghorn runtime-model grouping is documented.
- [x] Additional model hashes are mapped for bats, amphibians, crustaceans, cranes, egrets, herons, parakeets, macaws, pelicans, boobies, spoonbills and snapping turtles.
- [x] Original game textures are streamed from `inventory_items`; no Rockstar artwork is redistributed.
- [x] Craftable filter key is configurable in the in-mod Settings page and persisted as `General/FilterKey` in HuntingCompanion.ini.
- [x] Filter status is shown in the header; empty unknown categories are omitted in Craftable mode.
- [x] Weapon/ammo sprites are shown for recognized animals regardless of Pearson/Trapper demand, 50% larger with a layered dark backdrop.
- [x] Craftability is checked against actual vendor stock; garment-set material requirements are aggregated.
- [x] Craftable Now checks only recognized animal-part materials; cash and all non-animal costs are ignored by the filter.
- [x] All 13 locale dictionaries and generated embedded language data include the new filter phrases.
- [ ] Run `python tools/embed_languages.py` and confirm no diff.
- [ ] Run `python tools/embed_hunting_recommendations.py` and confirm no diff.
- [ ] Confirm all 162 standard Compendium entries resolve in-game where a runtime animal model exists; confirm pet / working-animal entries display no invented guidance.
- [ ] Run `python tools/validate_localizations.py`.
- [ ] Run all existing Python/static tests plus `tests/test_hunting_recommendations.py`.
- [ ] Run existing C++ unit tests.
- [ ] Fresh Windows x64 MinGW-w64 configure/build; do not reuse a stale build directory or ASI.
- [ ] Verify fresh x64 ASI and create release ZIP.
- [ ] In-game check all icon textures at common resolutions/aspect ratios.
- [ ] In-game check Pearson, Trapper, garment sets, empty categories and filter selection persistence.
- [ ] Test all 13 UI locales and existing overlay/native rendering paths.
- [ ] Confirm redistribution requirements for embedded localization payloads before publication.

## 1.13.0 Crafting Uses checks

- [x] Read-only player inventory count for recognized animal parts.
- [x] Resolve Arthur's currently carried animal/pelt through carriable slots 0-4.
- [x] Enumerate attached carriable entities on the active saddle horse and resolve pelt entities back to their source carcass.
- [x] Refresh the haul snapshot while the menu is open; keep the root entry disabled when no recognized material is present.
- [x] Exclude owned Pearson/Trapper recipes and already-owned garment components.
- [x] Expand Trapper garment sets into individual remaining pieces.
- [x] Show current haul and vendor donation stock separately; cash is not falsely reported as checked.
- [ ] Run tests/test_crafting_uses.py and all existing source tests.
- [ ] Fresh Windows x64 MinGW-w64 configure/build.
- [ ] In-game test: pelt in Arthur's hands, pelt on saddle horse, multiple materials, already-crafted garment, donating a pelt while the menu is open, and empty-haul disable/enable transitions.
- [ ] Verify all 13 locales and ensure the haul scan never changes player inventory or horse cargo.

- [x] Intact carcasses only count as carcass materials; skinned pelt entities count as pelt materials.
- [x] Legendary pelt identity is not guessed from the ordinary ped-quality value; exact inventory item counts are used where available.
