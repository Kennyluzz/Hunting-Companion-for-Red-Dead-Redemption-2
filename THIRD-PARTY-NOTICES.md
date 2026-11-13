# Third-party notices and content attribution

Hunting Companion's original source code is licensed under MIT; see `LICENSE`.
The following components retain their own licences. These notices apply to
both the source distribution and the compiled ASI.

## MinHook v1.3.4
Copyright (C) 2009-2017 Tsuda Kageyu.
BSD 2-Clause licence. Full text: `LICENSES/MINHOOK-BSD-2-CLAUSE.txt`.

## RedM / FiveM (credit, no code included)
The way `src/native_fonts.cpp` loads one of the game's own font libraries at run
time follows the approach the RedM project (citizenfx/fivem, `sfFontStuff.cpp` and
the RDR3 streaming code) uses for its `REGISTER_FONT_FILE` native, including the
byte patterns that locate the game functions. The implementation here is written
independently; no RedM/FiveM source code is included.

## Animal Compendium text and game data names
The clean-kill weapon advice in `data/compendium_advice.json` quotes the game's own
Animal Compendium text (Rockstar's text), taken from the game's English string table.
Texture, model and animal-type names come from the community catalogue
femga/rdr3_discoveries (https://github.com/femga/rdr3_discoveries). Rockstar's text is covered by the
"Rockstar-authored localization content" notice below.

## Rockstar fonts — not included
Japanese, Korean and Chinese text on a game running in another language, and
Latin/Cyrillic text on a Japanese, Korean or Chinese game, is drawn with the fonts
that ship inside the player's own copy of RDR2 (`font_lib_*.gfx` in the game's
archives), loaded at run time from the game's files. No Rockstar font data is
included in this project or in the ASI.

## Rockstar-authored localization content — separate rights
The embedded official item/material and challenge labels are Rockstar-authored
game text taken from RDR2's own language resources and are not original
Hunting Companion translations. They are **not** relicensed under MIT or under
any third-party open-source licence by this project, and this notice does not
grant any right to redistribute Rockstar content. Anyone redistributing builds
is responsible for complying with Rockstar's modding terms.
