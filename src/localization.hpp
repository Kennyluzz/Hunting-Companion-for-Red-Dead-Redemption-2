#pragma once

#include <cstddef>
#include <cstdint>

namespace hunting_companion {

// Loads the embedded companion UI dictionary. -1 follows RDR2's detected
// language; 0-12 pins the companion UI to an independent locale.
void load_ui_localization(int gameLanguage, int uiLanguageOverride) noexcept;
int active_ui_language() noexcept;
const char* ui_text(const char* englishText) noexcept;
const char* ui_locale() noexcept;
const char* locale_name(int language) noexcept;
bool uses_cjk_font(int language) noexcept;
bool uses_body_font(int language) noexcept;
std::size_t loaded_ui_translation_count() noexcept;
const char* official_item_string(std::uint32_t labelHash) noexcept;
std::size_t loaded_official_string_count() noexcept;

} // namespace hunting_companion
