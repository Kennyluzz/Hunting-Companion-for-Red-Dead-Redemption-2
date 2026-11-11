# Hunting Companion 1.10.0 — install package

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
