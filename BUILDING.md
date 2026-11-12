# Building

Windows x64 target, built with MinGW-w64, CMake and Ninja. Third-party sources are
vendored in `third_party/`.

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --clean-first
```

The result is `build/HuntingCompanion.asi`. Use a fresh build directory each time.

To create a release zip: `python tools/package_release.py build/HuntingCompanion.asi`.

After editing a `.lang` file, run `python tools/embed_languages.py` and
`python tools/build_cjk_fonts.py --ttc NotoSansCJK-Bold.ttc`, then the checks in `tests/`.
