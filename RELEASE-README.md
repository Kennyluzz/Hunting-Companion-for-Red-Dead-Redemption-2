# Hunting Companion 1.15.0 — install package

This archive contains the compiled Hunting Companion plugin and its licence
documentation. The only Hunting Companion runtime file to install is
`HuntingCompanion.asi`.

## Install

1. Copy `HuntingCompanion.asi` into the Red Dead Redemption 2 game directory,
   beside `RDR2.exe`.
2. Launch the game using your existing compatible ASI loader and ScriptHookRDR2
   setup.
3. Open Hunting Companion and choose its language in the mod settings.

## Languages

English, French, German, Italian, Spanish (Spain), Portuguese (Brazil), Polish,
Russian, Korean, Traditional Chinese, Japanese, Spanish (Mexico), and Simplified
Chinese.

## Runtime notes

- The CJK overlay uses Vulkan. Native Windows DirectX 12 currently falls back
  to English when the selected companion language requires the overlay.
- HDR output is not specially handled.
- Overlay text is composited above the game frame and cannot be occluded by
  later native UI draws.
- ScriptHookRDR2 and a compatible ASI loader are general game-plugin-host
  requirements; they are not additional Hunting Companion components.

## Licences

The project's original code is MIT-licensed. Bundled third-party components and
fonts retain their own licences; see `LICENSES/` and `THIRD-PARTY-NOTICES.md`.
Rockstar-authored localization strings are not licensed by the project's MIT
licence. The project notice identifies this separate rights issue; public
redistribution should be confirmed against applicable Rockstar terms/permissions.

## 1.12.0 additions

- Animal-specific hunting weapon and named-ammunition icons for every huntable species in the standard 162-entry Animal Compendium; domestic / working animals without weapon guidance are explicitly classified.
- Larger weapon/ammunition sprites with a layered dark contrast backdrop, positioned above vendor indicators.
- The Craftable Now state is prominent in the menu header; its toggle key is configurable in Settings.
- Empty unknown catalog categories are omitted while Craftable Now is active.

Install only `HuntingCompanion.asi`, as before. No extra runtime data files or extracted game textures are installed.

## 1.13.0 additions

- Crafting Uses is greyed out until the player has a recognized animal part in the satchel, in hand, or on the active saddle horse.
- The submenu lists only uncrafted Pearson and Trapper recipes that use a material in the current haul; garment sets are expanded into unowned individual pieces.
- Item details compare haul quantities with material already donated to the selected vendor and distinguish animal-part readiness from readiness after donation.
- The scan is read-only and refreshes while the menu is open. It does not transfer, donate, consume, craft, purchase or pause the game.
