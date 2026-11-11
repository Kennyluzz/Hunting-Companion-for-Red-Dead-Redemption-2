# Hunting Companion 1.10.0 localization notes

## Official Rockstar item/material strings

- Exactly 290 unique recipe/material IDs are embedded for each of 13 locales.
- IDs are matched by numeric label hash, not by translating words or fuzzy matching.
- Runtime lookup uses immutable sorted tables and binary search.
- Every visible item/material is resolved against the currently selected locale
  on draw; language changes do not trigger an asynchronous catalogue rescan.

## Challenge objectives

- The companion displays 90 challenge ranks across the nine Story Mode categories.
- There are 82 unique objective sentences in the curated display list.
- 68 objectives have official Rockstar label hashes and use the embedded official
  locale tables. These are included in all 13 locale tables.
- 14 companion-authored composite objectives do not have a single Rockstar label
  hash. They use exact full-sentence translations in all 12 non-English `.lang`
  dictionaries, including reviewed Polish and Korean entries.
- The `localized_challenge_objective()` resolver is shared by both the challenge
  detail page and reinforced-reward detail page.
- `tests/test_official_strings.cpp` verifies that all 68 official challenge hashes
  resolve in all 13 locales. `tools/validate_localizations.py` verifies the
  14 custom objective translations in all 13 dictionaries.

## Font routing

- CJK companion text that differs from the game language is drawn by the
  Vulkan/ImGui overlay using embedded regional Noto Sans CJK subsets.
- A bundled Latin/Cyrillic subset is used when the game language is CJK but the
  companion language is not.
- Font subsets are generated from the `.lang` dictionaries and the official
  embedded item/challenge strings. Regenerate fonts after changing those inputs.

## Build freshness

The release archive intentionally excludes build directories and compiled ASIs.
Always configure a fresh build directory. Run
`python tools/verify_release_binary.py <path-to-HuntingCompanion.asi>` after
building; it checks that the binary includes the current official and custom
challenge translations rather than an older embedded-string header.
