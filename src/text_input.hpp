#pragma once
// Typed text for the menu's search box, in any keyboard layout and through the
// Windows input method (IME) for Japanese, Korean and Chinese.
//
// The game window is subclassed once, the first time typing is enabled. While
// typing is off, every message passes straight through to the game.

#include <cstddef>

namespace hunting_companion::text_input {

// Script thread, once per frame: turns typing on or off. Turning it on
// subclasses the game window the first time and lets the IME compose text.
void set_active(bool active) noexcept;

// Script thread: the next typed character as UTF-8 (NUL-terminated, up to 4
// bytes plus NUL), or false when none is waiting. Control characters are never
// returned; Backspace, Enter and the arrows are read as keys by the menu.
bool next_character(char out[5]) noexcept;

// Script thread: the IME's text still being composed (not yet committed), as
// UTF-8, or an empty string.
void composition(char* out, std::size_t size) noexcept;

}  // namespace hunting_companion::text_input
