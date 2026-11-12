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
  for a clean kill. Toggle them in Settings or with a hotkey.
- **Pearson and Trapper:** recipes, required materials, what you own, donation
  stock, and a "craftable now" filter.
- **Crafting Uses:** the uncrafted recipes that can use the pelts and parts you are
  carrying. Greyed out when nothing you carry matches.
- **Challenges:** live rank totals for all nine Story Mode challenge categories and
  what the next rank asks for.
- **Reinforced equipment:** unlock requirements for all 36 bandoliers, gun belts and holsters.
- **Garment sets:** progress for all 16 Trapper garment sets.
- **13 languages, switchable in-game:** English, French, German, Italian, Spanish
  (Spain and Mexico), Brazilian Portuguese, Polish, Russian, Japanese, Korean,
  Simplified Chinese and Traditional Chinese. Item and challenge names are the
  game's own.
- **Menu fonts:** five game fonts to choose from in Settings.

The mod only reads game data. It never changes your progress, inventory or saves.
**Do not use it in Red Dead Online.**

## Install

1. Install [ScriptHookRDR2](http://www.dev-c.com/rdr2/scripthookrdr2/) and an ASI loader.
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
page. Settings are saved to `HuntingCompanion.ini`.

## Japanese, Korean and Chinese

If the game itself is set to one of these languages, the mod uses the game's own
fonts. If you pick one of them in the mod while the game runs in another language,
the mod draws the text itself, which works on both renderers:

- **Vulkan** (including Linux/Proton): tested and reliable.
- **DirectX 12:** **experimental.** If it cannot start, the mod shows English instead.
  If you hit a problem, set `OverlayBackend=1` in `HuntingCompanion.ini` to
  switch it off, or set `OverlayLog=1`, reproduce the problem, and report it with the
  generated `HuntingCompanion.overlay.log`.

On HDR displays the text brightness can be tuned with `CjkHdrPaperWhiteNits`.

## Configuration

`HuntingCompanion.ini` is optional; the defaults work. Options are described in the file.

## Credits

- Weapon and ammo suggestions are based on the Bloxodes Story Mode animal guide
  (<https://bloxodes.com/wiki/red-dead-redemption-2/animals>).
- Texture names were checked against the community catalogue
  <https://github.com/femga/rdr3_discoveries>.
- Built with [Dear ImGui](https://github.com/ocornut/imgui),
  [MinHook](https://github.com/TsudaKageyu/minhook) and Noto Sans CJK; see
  `THIRD-PARTY-NOTICES.md`.

## Licence

The mod's code is MIT-licensed (`LICENSE`). Third-party components keep their own
licences (`THIRD-PARTY-NOTICES.md`, `LICENSES/`). Rockstar's text and trademarks are
not covered by the MIT licence. Hunting Companion is an unofficial fan modification,
not affiliated with or endorsed by Rockstar Games or Take-Two Interactive.

Building from source: see `BUILDING.md`.
