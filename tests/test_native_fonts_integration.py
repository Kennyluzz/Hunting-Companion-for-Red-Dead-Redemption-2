#!/usr/bin/env python3
"""Static checks that native fonts are wired the way the design requires, plus
the host test for the per-language font lists."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
main = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
fonts = (ROOT / "src" / "native_fonts.cpp").read_text(encoding="utf-8")
cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")


def need(cond, msg):
    if not cond:
        raise AssertionError(msg)


# No files in the game folder apart from the ini; the font log and the fontmap
# copy are opt-in. (main.cpp reads the game's own fontmap.xml, it never writes one.)
for bad in ("CreateDirectoryW", "commonx", "WriteFile(", "CreateFile"):
    need(bad not in main, f"main.cpp must not create game-folder files ({bad})")
need("if (!g_settings.fileLog" in fonts, "the font log must only be written with FontLog=1")
# No overlay left behind.
for gone in ("overlay::", "imgui", "vulkan", "cjk_overlay"):
    need(gone not in main.lower() and gone not in cmake.lower(), f"overlay remnant: {gone}")
# The font hook is installed at attach, before the script is registered.
need(main.index("native_fonts::install(") < main.index("g_api.registerScript(module, ScriptMain)"),
     "native fonts must be installed before the script is registered")
# Loads advance every frame, before the language is applied and text is drawn.
need(main.index("native_fonts::update();") < main.index("apply_effective_language();\n        {")
     < main.index("update_hud();\n            draw_menu();"), "font loading must advance before drawing")
# English while a library is loading or unavailable.
need("falls_back_to_english(wanted)) {" in main and "native_fonts::loading(wanted)" in main and
     "return 0;\n    }\n    return wanted;" in main,
     "a loading CJK language keeps the current language; an unavailable one falls back to English")
# Key capture: Backspace cancels, menu keys cannot be bound, the prompt is shown alone.
need("capture_cancelled()" in main and "bindable_key(" in main, "key capture must be cancellable and filtered")
# The game's own fonts are used when the companion language equals the game language.
need("if (language == gameLanguage) return false;" in fonts, "the game's own language needs no extra library")
# Only one game function is hooked; store slots are found by reading memory.
need(fonts.count("MH_CreateHook(") == 1, "only the add-font-library function may be hooked")
need("vtable_entry(store" not in fonts, "store methods must not be called through a guessed vtable slot")
# Every game function comes from a unique pattern and must lie inside RDR2.exe.
need("count == 1 ? found : nullptr" in fonts and "inside_main_module(" in fonts, "patterns must be unique and checked")
# Font names end up in markup, so they are sanitised.
need("usable_face(" in fonts, "font names must be sanitised before use in markup")

# Menu fonts come from the game's own fontmap.xml, per language, and the choice is per language.
need("load_font_map();" in main and "menu_fonts_for(g_fontMap" in main, "menu fonts must come from fontmap.xml")
need('"MenuFont.%s"' in main, "the menu font must be saved per language")

with tempfile.TemporaryDirectory() as tmp:
    for test in ("test_font_catalog",):
        exe = Path(tmp) / test
        subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-I", str(ROOT / "src"),
                        str(ROOT / "tests" / f"{test}.cpp"), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
print("Native font integration checks passed.")
