#pragma once
// CJK text overlay.
//
// RDR2 only loads Chinese/Japanese/Korean glyphs when the game itself runs in
// that language, so the native text natives cannot show them otherwise. For a
// CJK companion language that differs from the game language, the mod instead
// queues its text here and the overlay draws it with ImGui, using subset Noto
// Sans CJK fonts embedded in the .asi, into the game's Vulkan swapchain just
// before each present.
//
// Nothing is written to disk (no imgui.ini, no font files) unless the optional
// OverlayLog ini switch is turned on.

#include <cstdint>

namespace hunting_companion::overlay {

struct Settings {
    float textScale = 1.0f;       // ini: CjkTextScalePercent / 100
    float yOffset = 0.0f;         // ini: CjkTextYOffsetPermille / 1000, fraction of screen height
    float hdrPaperWhiteNits = 230.0f;  // ini: CjkHdrPaperWhiteNits, brightness of white text in HDR
    bool fileLog = false;         // ini: OverlayLog=1 writes HuntingCompanion.overlay.log
    const char* logDirectory = nullptr;  // directory for the optional log file
};

// Call once from DllMain. Starts a worker thread that hooks the Vulkan loader.
// Cheap and harmless when the game is not using Vulkan (it then never becomes ready).
void install(void* moduleHandle, const Settings& settings) noexcept;

// True once a Vulkan swapchain has been seen and the overlay can draw.
bool ready() noexcept;

// Which companion language the overlay should draw (0-12, same numbering as the mod).
void set_language(int language) noexcept;

// Script thread, once per game tick: begin_frame() ... submit()* ... end_frame().
void begin_frame() noexcept;
void submit(const char* utf8, float x, float y, float referencePixelHeight,
            int r, int g, int b, int a, bool centered) noexcept;
void end_frame() noexcept;

} // namespace hunting_companion::overlay
