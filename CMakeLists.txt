cmake_minimum_required(VERSION 3.20)
project(HuntingCompanion VERSION 1.30.1 LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Configure with cmake/mingw-w64-toolchain.cmake to target Windows x64.")
endif()

# MinHook is vendored under third_party/ (see third_party/README.md).
set(HC_MINHOOK_DIR  ${CMAKE_SOURCE_DIR}/third_party/minhook)

foreach(required
        ${HC_MINHOOK_DIR}/src/hook.c
        ${HC_MINHOOK_DIR}/include/MinHook.h)
    if(NOT EXISTS ${required})
        message(FATAL_ERROR "Missing ${required}\nSee third_party/README.md for what to place where.")
    endif()
endforeach()

set(HC_THIRD_PARTY_SOURCES
    ${HC_MINHOOK_DIR}/src/buffer.c
    ${HC_MINHOOK_DIR}/src/hook.c
    ${HC_MINHOOK_DIR}/src/trampoline.c
    ${HC_MINHOOK_DIR}/src/hde/hde64.c)

add_library(HuntingCompanion SHARED
    src/main.cpp
    src/localization.cpp
    src/official_strings.cpp
    src/native_fonts.cpp
    src/text_input.cpp
    src/text_input.hpp
    src/search_text.hpp
    src/native_fonts.hpp
    src/font_catalog.hpp
    src/embedded_languages.hpp
    src/embedded_official_strings.hpp
    src/embedded_challenge_objective_hashes.hpp
    src/embedded_hunting_recommendations.hpp
    ${HC_THIRD_PARTY_SOURCES})

target_compile_features(HuntingCompanion PRIVATE cxx_std_17)
target_include_directories(HuntingCompanion PRIVATE
    src
    ${HC_MINHOOK_DIR}/include
    ${HC_MINHOOK_DIR}/src)
target_compile_definitions(HuntingCompanion PRIVATE
    WIN32_LEAN_AND_MEAN NOMINMAX WINVER=0x0601 _WIN32_WINNT=0x0601)
target_compile_options(HuntingCompanion PRIVATE -Wall -Wextra -Wpedantic -Wno-missing-field-initializers -Os -ffunction-sections)
# Vendored code: keep its warnings out of our build log.
set_source_files_properties(${HC_THIRD_PARTY_SOURCES} PROPERTIES COMPILE_OPTIONS "-w")
target_link_libraries(HuntingCompanion PRIVATE user32 imm32)
target_link_options(HuntingCompanion PRIVATE -static -static-libgcc -static-libstdc++ -Wl,--no-undefined -Wl,--gc-sections -s)
set_target_properties(HuntingCompanion PROPERTIES PREFIX "" SUFFIX ".asi" OUTPUT_NAME "HuntingCompanion")

# Companion UI phrases and official Rockstar item/material strings are embedded at
# build time. Japanese, Korean and Chinese text uses the game's own font libraries,
# loaded from the game's archives at run time. Nothing is read from or written to
# disk at runtime apart from HuntingCompanion.ini (and the optional font log).
