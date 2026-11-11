# Companion UI phrase dictionaries

The `.lang` files are development source inputs for Hunting Companion's own UI phrases. `tools/embed_languages.py` compiles them into `src/embedded_languages.hpp`; the resulting ASI does not read this directory at runtime.

Official Rockstar recipe and material names are generated separately from the official language packs and embedded in `src/embedded_official_strings.hpp`. The final ASI does not require XML, language folders, or any other translation files beside it.
