# 1.10.0 release checklist

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
