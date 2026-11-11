#pragma once
// ImGui build configuration for the Hunting Companion overlay (used through
// IMGUI_USER_CONFIG in CMakeLists.txt). The overlay only draws text, so
// everything else is compiled out, and nothing may touch the disk.

#define IMGUI_DISABLE_DEMO_WINDOWS
#define IMGUI_DISABLE_DEBUG_TOOLS
#define IMGUI_DISABLE_FILE_FUNCTIONS            // no imgui.ini / no fopen
#define IMGUI_DISABLE_WIN32_FUNCTIONS           // no clipboard / IME hooks into the game window

// Inside someone else's process an assertion must never abort the game.
#define IM_ASSERT(_EXPR) ((void)(_EXPR))
