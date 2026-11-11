# CJK overlay — release notes

## Architecture

1. `draw_text_impl()` routes text to the overlay when the selected companion
   language requires glyphs unavailable to the game's current native font setup.
2. `ScriptMain` publishes the current frame's text queue.
3. A Vulkan `vkQueuePresentKHR` hook renders queued text with Dear ImGui immediately
   before presentation.
4. Regional Noto Sans CJK subsets and the Latin/Cyrillic fallback subset are
   embedded in the ASI. The font selection changes with the active locale.
5. The overlay creates no files in the game directory and has no external font
   or font-map dependency.

## In-game verification

The maintainer has tested the compiled overlay in RDR2. The supplied gameplay
screenshots show correctly rendered Chinese and French UI text over the live
game frame. This is a report of that tested setup, not a guarantee for every
GPU, driver, loader, API path or overlay combination.

## Known limitations

- Vulkan is supported. Native Windows DirectX 12 currently uses the English
  fallback when the requested companion language requires the overlay.
- HDR output is not specially handled; colours may differ in HDR configurations.
- Overlay text is composited above the completed game frame and cannot be
  occluded by later native game UI draws.
- The ASI installs Vulkan hooks early and pins itself in memory; it is not
  designed for hot reload.
- The font atlas is uploaded once after a drawable swapchain is initialized.

## Diagnostics

Set `[Overlay] OverlayLog=1` in `HuntingCompanion.ini` to write overlay diagnostics
beside the ASI. `CjkTextScalePercent` and `CjkTextYOffsetPermille` can adjust
overlay text size and vertical position. These settings are optional.
