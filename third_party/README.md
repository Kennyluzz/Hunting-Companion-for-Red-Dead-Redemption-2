# Vendored third-party dependencies

These source dependencies are included in this source archive and are compiled
into the single `HuntingCompanion.asi`. End users do not install them separately.
Versions are pinned because the overlay is written against these APIs.

- Dear ImGui v1.91.9 — MIT
- MinHook v1.3.4 — BSD-2-Clause
- Vulkan-Headers v1.4.368 — Apache-2.0 OR MIT (headers only)

The source files are under `third_party/`. Their licence texts and notices are
included in the top-level `LICENSES/` directory and `THIRD-PARTY-NOTICES.md`.

## Fonts

The generated regional Noto Sans CJK subsets and the Latin/Cyrillic subset are
embedded in `src/embedded_cjk_fonts.hpp`. Both font families are distributed
under SIL OFL 1.1. The Latin subset is a modified Liberation Sans derivative;
the build script renames its family records to `Hunting Companion Sans` to
respect Liberation's Reserved Font Name. See `LICENSES/OFL-1.1.txt` and the
detailed font notices.
