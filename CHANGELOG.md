# Changelog

## 1.30.0

Changes since 1.20.0 / 1.20.1.

### Languages and fonts

- **Japanese, Korean and Chinese without an overlay.** On a game running in another
  language, these are now drawn by the game itself with Rockstar's own fonts, which
  the mod loads from the game files when you pick the language. The Vulkan and
  DirectX 12 overlays, and the fonts bundled with them, are gone: Asian languages
  now work the same on every renderer, HDR included.
- **Any language on any game language.** Another Asian language on an Asian-language
  game (for example Japanese on a Korean game) loads its own fonts, and European
  languages on a Japanese, Korean or Chinese game use the game's Latin or Cyrillic
  fonts, so Polish, Spanish and other accented letters show correctly.
- **Smooth language switching.** When you pick an Asian language, the menu keeps its
  current language until the new fonts are ready (a fraction of a second) instead of
  showing English in between.
- **A font list for every language.** The Menu font setting offers the fonts the game
  itself uses for the selected language, and remembers the choice per language:
  - Latin languages: Catalogue, Body, Body 1, Title, Ledger, Chalk, Wanted Poster,
    Redemption and Catalogue Sans.
  - Russian: Catalogue, Body, Body 1, Title, Chalk and Redemption.
  - Japanese and Korean: Standard, Title and Display.
  - Simplified and Traditional Chinese: Standard.

  Settings shows the font's name and its place in the list (for example
  "Chalk 6/9"). Fonts that cannot show a language's letters are not offered for it.
- **Wrapping in Japanese, Korean and Chinese** no longer splits numbers or Latin words
  across two lines.

### Hunting

- **Weapon and ammunition advice comes only from the game's own Animal Compendium**
  ("Use a ... for a clean kill."). Where the Compendium just says "a rifle", the HUD
  shows **Rifle**. Updated animals:
  - Bow with Small Game Arrows: chicken, rooster, pigeon, raven, gulls, cormorant,
    California condor.
  - Varmint Rifle: macaw.
  - Repeater: Canada goose, fox.
  - Rifle: bison, boar, deer, cougar, panther, pronghorn, bighorn ram.
  - Rifle or bow: black bear.
  - Bow with Improved Arrows or long-scope rifle: grizzly bear, alligator.
  - Long-scope rifle: buck, elk, moose, ox, bull.
- **Every snake is recognised.** Timber, Western and Diamondback rattlesnakes,
  Fer-de-Lance, the boas, copperheads, the cottonmouth and the water snakes now show
  their weapon advice (previously only the Black-tailed Rattlesnake did).
- **Legendary animals** show the game's bounty-hunter icon with **"Any weapon"**, as
  their pelts cannot be damaged. They are told apart by the game's own animal type,
  so this works for all 16 Story Mode legendaries.
- The weapon and ammunition icons sit closer together.

### Menu

- An empty list (for example a category with the "craftable now" filter on) says
  "Nothing can be crafted right now" instead of showing an empty panel.
- **Key rebinding:** the prompt is shown on its own, **Backspace** cancels, the keys
  the menu uses cannot be bound by accident, and keys show their real names.

### Performance

- The catalogue scan does a fixed amount of work per frame instead of one game query
  per frame, so it finishes far sooner. `ScanSliceMicroseconds` in the ini tunes it.
- `HuntingCompanion.asi` is about 0.7 MB instead of 3.1 MB, uses less memory, and
  draws text with less work per label.

### Settings (`HuntingCompanion.ini`)

- New: `LegendaryIcon`, `ScanSliceMicroseconds`, `MenuFont.<language>` (saved by the
  Settings page), and a `[Fonts]` section with `FontLog` and optional font overrides.
- Removed: the `[Overlay]` section (`CjkTextScalePercent`, `CjkTextYOffsetPermille`,
  `CjkHdrPaperWhiteNits`, `OverlayBackend`, `OverlayLog`). An existing `MenuFont`
  value is kept as the default for every language.

### For developers

- Dear ImGui, the Vulkan headers and the embedded Noto/Liberation font subsets are
  removed; MinHook is the only third-party code.
- Builds with mingw-w64 v11 or newer.
- New host tests for the localisation tables, the per-language font lists and the
  font-loading wiring.
