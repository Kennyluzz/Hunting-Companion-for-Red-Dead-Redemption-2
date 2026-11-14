# Changelog

## Unreleased

- **Search.** A new Search page on the main menu: type an animal, a part or an item
  (for example "alligator") and the list narrows with every letter to the Pearson
  and Trapper recipes it appears in, crafted ones last. Names match in the menu's
  language and in English, ignoring capitals and accents (hiragana also finds
  katakana); Japanese, Korean and Chinese can be typed with the Windows IME. Enter
  opens the recipe, Backspace deletes (and goes back once the box is empty), and
  the game ignores its controls while you type.
- **HUD layout.** Settings > HUD layout moves and resizes the HUD icons: all of them
  together, or the weapon and ammo icons and the Pearson and Trapper icons on their
  own. Sample icons show while the page is open; pick a group, then arrows move it
  (Shift for bigger steps), Page Up/Down or +/- size it, Enter saves and Backspace
  puts it back. "Reset layout" restores the defaults. Saved under `[HUD]` in the ini.
- **Colours.** Settings has a Text colour, a Heading colour and a HUD label colour,
  each with Default, White, Cream, Gold, Red, Orange, Green, Blue, Purple and Grey.
  Coloured status text (what you have or still need) keeps its own colours.
- **Settings wraps around.** Down on the last row goes back to the first, and Up on
  the first goes to the last. Settings shows seven rows at a time and scrolls.
- **Pelts on the horse count again in Crafting Uses.** Pelts stacked on the saddle
  horse are now read by inventory item, and a pelt you carried keeps its identity
  after the game removes the animal it came from, so it no longer drops out of
  Crafting Uses once it is on the horse.
- **Animal indicators no longer count legendary pelts for ordinary animals.** The
  Trapper icon showed on an ordinary alligator because the Trapper still needed the
  Legendary Alligator Skin; an ordinary animal now only counts its own materials,
  and a legendary animal only its legendary pelt.
- `FontLog=1` also logs what the haul scan sees (carried, on the horse, stacked),
  for bug reports.

## 1.30.1

- **Weapon and ammunition advice is read from the game itself.** For the animal in
  focus, the mod reads the clean-kill line of that animal's own Animal Compendium
  entry ("Use a ... for a clean kill.") and shows exactly that weapon and
  ammunition, so the icons always match the game's Show Info panel, for every breed
  and variant (for example the Pekin Duck now shows a Bow with Small Game Arrows).
  It works for every animal in the Compendium, whatever its model.
- An animal whose Compendium line the mod does not know shows no weapon icons rather
  than a guess; with `FontLog=1` it is written to `HuntingCompanion.fonts.log`.
- For developers: `tools/build_compendium_advice.py` builds the advice table
  (`data/compendium_advice.json`) from the game's English string table;
  `data/weapon_icons.json` holds the HUD icon for each weapon and ammunition.

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
