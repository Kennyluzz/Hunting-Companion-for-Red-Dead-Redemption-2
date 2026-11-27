#pragma once
// Keeps HuntingCompanion.ini in the current layout.

namespace hunting_companion::ini {

// When the ini is missing, or its IniVersion is older than the mod's, rewrites
// it from the shipped template (src/embedded_ini_template.hpp): every setting
// the player has made is kept, new settings appear with their defaults and
// explanations, and outdated ones are removed or renamed (FontLog -> Log).
// Script thread, before the settings are read.
void upgrade(const char* path) noexcept;

}  // namespace hunting_companion::ini
