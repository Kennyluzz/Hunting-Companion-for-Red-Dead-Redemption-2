#!/usr/bin/env python3
"""Search page: text matching (host-compiled) and its menu wiring."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
TEXT_INPUT = (ROOT / "src" / "text_input.cpp").read_text(encoding="utf-8")

with tempfile.TemporaryDirectory() as temp:
    exe = Path(temp) / "test_search_text"
    subprocess.run(["g++", "-std=c++17", "-O1", "-Wall", "-Wextra", "-I", str(ROOT / "src"),
                    str(ROOT / "tests" / "test_search_text.cpp"), "-o", str(exe)], check=True)
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    print(result.stdout, end="")
    if result.returncode != 0:
        raise SystemExit("search_text host test failed")

required = (
    # A root row of its own, results from both vendors, opened in the recipe detail.
    '"Pearson", "Trapper", "Search", "Crafting Uses", "Challenges", "Settings", "Close"',
    "case MenuPage::Search: draw_search_page(); break;",
    "g_menu.detailReturnPage = MenuPage::Search;",
    # Names in the menu language and in English, material names and their animal.
    "text.add(recipe_display_name(recipe));",
    "text.add(recipe.name);",
    "text.add(display_material_name(hash));",
    "text.add(find_material_name(hash));",
    "text.add(source_species(hash));",
    # Typed text, never game controls, while the box is open.
    "hunting_companion::text_input::set_active(typing);",
    "if (typing) invoke_native<void>(kDisableAllControlActions, 0);",
    "!(typing && key_types_text(g_iconsHotkey))",
    "!(typing && key_types_text(g_menuHotkey))",
    "if (g_searchQuery[0] == '\\0') { go_back(); return; }",
)
for token in required:
    if token not in SOURCE:
        raise AssertionError(f"Missing search integration: {token}")
for token in ("WM_CHAR", "GCS_RESULTSTR", "ImmAssociateContext", "CallWindowProcW"):
    if token not in TEXT_INPUT:
        raise AssertionError(f"Text input is missing {token}")
print("PASS: search page wiring, typed text and IME input.")
