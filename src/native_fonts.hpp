#pragma once
// Text in any of the companion's languages through the game's own Rockstar
// font libraries, whatever language the game itself runs in.
//
// RDR2 draws text with Scaleform GFx. At start-up it adds the font library of
// its own language (font_lib_efigs, font_lib_japanese, ...) to the GFx font
// library; the others stay on disk, so text in another script shows as squares.
// This module asks the game's streaming system to load the missing library from
// the game's own archives and hands it to the game's own "add font library"
// function, the way RedM loads extra font libraries. Nothing is bundled and
// nothing is written to the game folder.

#include <cstddef>

namespace hunting_companion::native_fonts {

struct Settings {
    bool fileLog = false;              // [Fonts] FontLog=1
    const char* logDirectory = nullptr;
    const char* iniPath = nullptr;     // for the [Fonts] face overrides
};

// DllMain: starts a worker thread that finds the game functions and installs one
// hook. Must run before the game initialises Scaleform (the ASI loader's timing).
void install(const Settings& settings) noexcept;

// Script thread: the game's own language (GET_CURRENT_LANGUAGE), whose font
// library the game added itself.
void set_game_language(int language) noexcept;

// True when `language` (0-12) needs a font library the game has not loaded:
// a CJK language other than the game's, or Latin/Cyrillic on a CJK game.
bool needs_library(int language, int gameLanguage) noexcept;

// True while that language's library is still on its way (not ready, not failed).
bool loading(int language) noexcept;

// While that library is not available: true for CJK (show English instead),
// false for Latin/Cyrillic (keep the language in the game's basic font).
bool falls_back_to_english(int language) noexcept;

// Script thread, once per frame: advances any requested library load.
void update() noexcept;

// Script thread. Starts loading the library for `language` if needed and
// returns true once its fonts can be named in text.
bool library_ready(int language) noexcept;

// Font to use for `language` when the game's fontmap.xml could not be read: the
// [Fonts] ini override, or nullptr if none is set.
const char* fallback_face(int language, bool* bold) noexcept;

// The [Fonts] <Language>Face override from the ini, if one is set.
const char* override_face(int language, bool* bold) noexcept;

// Marks a loaded library as unusable (no font to draw it with is known).
void mark_unusable(int language, const char* reason) noexcept;

// Script thread. Reads a file through the game's own file system (for example
// "common:/data/ui/fontmap.xml"); NUL-terminated. Release with free_game_file.
char* read_game_file(const char* path, std::size_t* size) noexcept;
void free_game_file(char* data) noexcept;

// Writes one line to the diagnostics log (HuntingCompanion.fonts.log) when
// [Fonts] FontLog=1; does nothing otherwise.
void diagnostic(const char* format, ...) noexcept;

// Short English status line for the Settings page (not translated: it is only
// relevant while the interface has fallen back to English).
const char* status_line(int language) noexcept;

}  // namespace hunting_companion::native_fonts
