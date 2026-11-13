#!/usr/bin/env python3
"""Static checks that the CJK overlay is wired in the way the design requires."""
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
main = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
ov = (ROOT / "src" / "cjk_overlay.cpp").read_text(encoding="utf-8")
cfg = (ROOT / "src" / "hc_imconfig.h").read_text(encoding="utf-8")
cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")

def need(cond, msg):
    if not cond: raise AssertionError(msg)

# No new files in the game folder: the font-map provisioning is gone.
for bad in ("CreateDirectoryW", "commonx", "fontmap", "WriteFile("):
    need(bad not in main, f"main.cpp must not create game-folder files ({bad})")
# ImGui must never write imgui.ini / logs.
need("io.IniFilename = nullptr" in ov and "io.LogFilename = nullptr" in ov, "ImGui ini/log must be disabled")
need("IMGUI_DISABLE_FILE_FUNCTIONS" in cfg, "ImGui file functions must be compiled out")
need("IM_ASSERT(_EXPR) ((void)(_EXPR))" in cfg, "ImGui asserts must not abort the game")
# Overlay hooks installed at attach, before scripts are registered.
need(main.index("overlay::install(") < main.index("g_api.registerScript(module, ScriptMain)"),
     "overlay must be installed before the script is registered")
# Per-tick hand-off and fallbacks.
need(main.index("overlay::begin_frame()") < main.index("update_hud();\n            draw_menu();") <
     main.index("overlay::end_frame()"), "draw calls must sit between begin_frame and end_frame")
need("needs_overlay(hunting_companion::active_ui_language())" in main, "draw_text_impl must route to the overlay")
need("overlay::ready()" in main and "return 0;" in main[main.index("int effective_language()"):main.index("bool overlay_fallback_active")],
     "unavailable overlay must fall back to English")
# The game's own fonts are used when the companion language equals the game language.
need("language == g_detectedGameLanguage) return false" in main, "native CJK must be kept when the game runs in that language")
# Build wiring.
for token in ("VK_NO_PROTOTYPES", "IMGUI_USER_CONFIG", "src/cjk_overlay.cpp", "embedded_cjk_fonts.hpp", "third_party/README.md"):
    need(token in cmake, f"CMakeLists.txt missing {token}")
# Crude syntax sanity for the file that could not be compiled here.
for name, text in (("cjk_overlay.cpp", ov), ("main.cpp", main)):
    for a, b in ("()", "{}", "[]"):
        stripped = "".join(ch for ch in text if ch in "(){}[]")
        need(text.count(a) == text.count(b), f"{name}: unbalanced {a}{b}")
print("Overlay integration checks passed.")
