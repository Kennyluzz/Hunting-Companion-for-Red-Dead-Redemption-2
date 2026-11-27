# Changelog

## 1.40.2

- **Pelts after loading a save.** A pelt already on you or on your horse when you
  load a save is now recognised; the horse's cargo is read directly, so stacked
  pelts and whole animals there count correctly too.
- **New setting: Icons on low-star animals.** Shown (as before) tells you a species
  is still needed so you can look for a three-star one; Hidden only shows icons on
  one- and two-star animals for what they can actually give. Saved as
  `LowStarIcons` in the ini.
- **Each HUD icon can be moved and sized on its own.** Settings > HUD layout now
  also has the weapon icon, the ammo icon, the Pearson icon and the Trapper icon
  separately, on top of the groups and "All icons".
- **The crafting side opens after chapter 1.** Until "Eastward Bound" is finished,
  Pearson, Trapper, Search and Crafting Uses are greyed out as "Available later"
  and no vendor icons show on animals; weapon advice, Challenges and Settings work
  from the start.
- **Pearson's tools.** Until the Leather Working Tools are bought at the camp
  ledger, every Pearson recipe still to make has a "*" after its status, and its
  page explains it in the game's own words ("Pearson cannot craft this yet.
  Purchase leather working tools at the ledger."); one whose materials are all there
  says "Needs: Leather Working Tools" instead of "Ready after donation". All in
  Rockstar's wording, in every language.
- Recipes that cannot be completed at your point in the story are left out until
  they can be.
- **The ini updates itself.** An older `HuntingCompanion.ini` is rewritten in the
  current layout the first time the mod runs, keeping every setting you have made:
  new settings appear with their explanations, outdated ones are removed, and
  `FontLog` becomes the new `Log` switch (`[General]`, writing
  `HuntingCompanion.log`).

## 1.40.1

- Some changes to how the mod works later in the story.

## 1.40.0

### Search

- **A new Search page** on the main menu finds every Pearson and Trapper recipe by
  an animal, a part or an item. Press **Space** to open the search box, type for
  example "alligator", and the list narrows with every letter to the recipes it
  appears in, uncrafted ones first.
- Names match in the menu's language and in English, ignoring capitals and accents
  ("skora" finds "skóra"); several words must all match ("wolf perfect"). Hiragana
  also finds katakana, and Japanese, Korean and Chinese can be typed with the
  Windows IME.
- The search box is a box of its own beside the menu, in the menu's style. It
  starts small, grows with the text and then scrolls; a search can be up to 100
  characters.
- Opening the box darkens the screen around it. The search is cleared when you
  leave the Search page or close the menu; it stays while you look at a recipe.
- **Enter** or the arrows close the box, **Enter** on a result opens the recipe and
  **Backspace** goes back. Inside the box Backspace only deletes, and only when
  pressed there, so a key still held from leaving a recipe never removes letters.
- While you type, neither the game nor other mods react to the keys: the game
  ignores its controls while the search box is open, and other mods' hotkeys (for
  example a mod bound to K) do not fire on typed letters. With the box closed you
  can move as usual; only jumping is held back on the Search page, as Space opens
  the box.

### HUD

- **HUD layout** (Settings > HUD layout): move and resize the HUD icons, all of them
  together, or the weapon and ammo icons and the Pearson and Trapper icons on their
  own. Sample icons show while the page is open. Pick a group, then the arrows move
  it (Shift for bigger steps), Page Up/Down or +/- change its size (50-250%), Enter
  saves and Backspace puts it back. "Reset layout" restores the defaults.
- **Animal indicators no longer count legendary pelts for ordinary animals.** An
  ordinary alligator showed the Trapper icon because the Trapper still needed the
  Legendary Alligator Skin. An ordinary animal now only counts its own materials,
  and a legendary animal only its legendary pelt.

### Menu

- **Colours:** Settings has a Text colour, a Heading colour and a HUD label colour
  (the names under the weapon and ammo icons), each with 20 choices: Default,
  White, Cream, Gold, Red, Orange, Green, Blue, Purple, Grey, Yellow, Pink, Teal,
  Light blue, Brown, Crimson, Lime, Lavender, Sand and Silver. Coloured status text
  (what you have or still need) keeps its own colours.
- **Garment-set pieces name their set.** A Trapper piece that belongs to a garment
  set (for example the Boar Riding Gloves) shows its set in the recipe list and on
  its recipe page ("Part of The Bear Hunter"), and in Search and Crafting Uses. Set
  names are now shown in the menu's language there as well.
- **Settings wraps around:** Down on the last row goes to the first and Up on the
  first goes to the last. Settings shows seven rows at a time and scrolls.

### Crafting Uses

- **A whole animal counts for what skinning it will give.** An animal you carry or
  have on your horse now counts for its pelt (and feathers or antlers) as well as
  its carcass, so a muskrat on your shoulder shows the recipes its pelt is for.
  Once you have skinned or plucked it (small animals included), the carcass you
  carry off to sell counts for nothing. A perfect pelt or carcass, whole or already skinned, is only counted
  from a three-star animal killed cleanly. A legendary animal, or its pelt, counts
  for its own legendary pelt (the white bison and Tatanka told apart), never for
  the ordinary materials of its species.
- **Animals in the satchel count too.** A small animal put away whole (for example
  a Perfect Squirrel Carcass) counts for the pelt and feathers breaking it down
  gives, like an animal you carry; a perfect pelt only from a perfect carcass.
- **Pelts on the horse count again.** Pelts stacked on the saddle horse are read by
  inventory item, and a pelt you carried keeps its identity after the game removes
  the animal it came from, so it no longer drops out of Crafting Uses once it is on
  the horse.

### Settings (`HuntingCompanion.ini`)

- New: `TextColour`, `HeadingColour` and `HudLabelColour` under `[General]`, and a
  `[HUD]` section (`AllX`, `AllY`, `AllSize`, `WeaponX`, `WeaponY`, `WeaponSize`,
  `VendorX`, `VendorY`, `VendorSize`). All of them are set from the in-game Settings
  page; an existing ini keeps working.
- `FontLog=1` also logs what the haul scan sees (carried, on the horse, stacked),
  for bug reports.

### For developers

- New `tools/build_satchel_carcasses.py` builds the satchel carcass items
  (`src/embedded_satchel_carcasses.hpp`) from the game's English string table.
- New `src/text_input.*` (typed text through the game window and the Windows IME)
  and `src/search_text.hpp` (case- and accent-insensitive matching), with host
  tests `tests/test_search.py` and `tests/test_hud_layout.py`.
- While the search box is open, `GetAsyncKeyState` and `GetKeyState` are hooked so
  that other modules see keyboard keys as up; the game window is subclassed for
  typed characters. Both pass everything through while the box is closed.

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
