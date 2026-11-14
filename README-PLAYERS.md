# Hunting Companion

An in-game hunting and crafting companion for **Red Dead Redemption 2 (Story Mode)**.
Focus an animal and it tells you what you still need from it, which weapon and
ammunition give the cleanest kill, and what you can craft with what you are carrying.

> The mod is AI-coded. The ideas, direction and in-game testing are human; the
> code was written by AI assistants.

## Features

- **Animal indicators:** see whether the animal you are looking at can supply a
  material that an unowned Pearson or Trapper recipe still needs.
- **Weapon and ammo suggestions:** icons and names of the weapon and ammunition
  for a clean kill, exactly as the game's own Animal Compendium advises. Legendary
  animals, whose pelts cannot be damaged, show "Any weapon". Toggle the icons in
  Settings or with a hotkey.
- **Pearson and Trapper:** recipes, required materials, what you own, donation
  stock, and a "craftable now" filter.
- **Search:** type an animal, a part or an item (for example "alligator") and the
  list narrows with every letter to the Pearson and Trapper recipes it is used in.
  Works in the menu's language and in English, without caring about capitals or
  accents; Japanese, Korean and Chinese can be typed with the Windows IME.
- **Crafting Uses:** the uncrafted recipes that can use the pelts and parts you are
  carrying, on you, in your satchel or on your saddle horse. Greyed out when nothing
  you carry matches.
- **Challenges:** live rank totals for all nine Story Mode challenge categories and
  what the next rank asks for.
- **Reinforced equipment:** unlock requirements for all 36 bandoliers, gun belts and holsters.
- **Garment sets:** progress for all 16 Trapper garment sets.
- **13 languages, switchable in-game:** English, French, German, Italian, Spanish
  (Spain and Mexico), Brazilian Portuguese, Polish, Russian, Japanese, Korean,
  Simplified Chinese and Traditional Chinese. Item and challenge names are the
  game's own.
- **Menu fonts:** the game's own fonts to choose from in Settings, a different set
  for each language (whatever the game itself has for it), remembered per language.
- **Colours:** choose the colour of the menu text, the menu headings and the names
  under the weapon and ammo icons.
- **HUD layout:** move and resize the HUD icons, all together or the weapon/ammo and
  Pearson/Trapper groups on their own, with a live preview (Settings > HUD layout).

The mod only reads game data. It never changes your progress, inventory or saves.
**Do not use it in Red Dead Online.**

## Install

1. Install [ScriptHookRDR2](https://www.nexusmods.com/reddeadredemption2/mods/1472) and an [ASI loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases).
2. Copy `HuntingCompanion.asi` (and optionally `HuntingCompanion.ini`) into the game
   folder, next to `RDR2.exe`.
3. Start Story Mode and press **F8**.

Everything else is built into the `.asi`. To uninstall, delete the files.

## Controls

| Action | Default |
|---|---|
| Open / close the menu | F8 |
| Toggle the "craftable now" filter | `[` |
| Toggle weapon/ammo icons | `]` |
| Navigate | Up / Down, Enter, Backspace |

All keys, the language and the menu font can be changed on the in-game Settings
page (Backspace cancels a key change). Settings are saved to `HuntingCompanion.ini`.

## Japanese, Korean and Chinese

If the game itself is set to one of these languages, the mod uses the game's own
fonts. If you pick one of them in the mod while the game runs in another language
(including another Asian language), the mod loads the game's own font for it from
the game files, so no extra files or mods are needed. The menu switches as soon as
the font is ready (a fraction of a second); if it cannot be loaded, the menu shows
English.

The same works the other way round: on a Japanese, Korean or Chinese game, a
European language (Polish, Spanish, Russian, ...) uses the game's own Latin or
Cyrillic font, with all its accents.

If text shows as squares or stays in English, set `FontLog=1` in
`HuntingCompanion.ini`, reproduce the problem, and report it with the generated
`HuntingCompanion.fonts.log`.

## Configuration

`HuntingCompanion.ini` is optional; the defaults work. Options are described in the file.

## Credits

- Weapon advice is the game's own Animal Compendium text, read from the game for
  the animal in focus.
- Texture, model and animal-type names were checked against the community catalogue
  <https://github.com/femga/rdr3_discoveries>.
- Built with [MinHook](https://github.com/TsudaKageyu/minhook). The way the game's
  own font libraries are loaded follows the open-source RedM project; see
  `THIRD-PARTY-NOTICES.md`.

## Licence

The mod's code is MIT-licensed (`LICENSE`). Third-party components keep their own
licences (`THIRD-PARTY-NOTICES.md`, `LICENSES/`). Rockstar's text and trademarks are
not covered by the MIT licence. Hunting Companion is an unofficial fan modification,
not affiliated with or endorsed by Rockstar Games or Take-Two Interactive.

Source code: <https://github.com/Kennyluzz/Hunting-Companion-for-Red-Dead-Redemption-2>
