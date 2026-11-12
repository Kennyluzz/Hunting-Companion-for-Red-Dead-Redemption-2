cmake_minimum_required(VERSION 3.20)
project(HuntingCompanion VERSION 1.20.0 LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Configure with cmake/mingw-w64-toolchain.cmake to target Windows x64.")
endif()

# Third-party sources are vendored under third_party/ (see third_party/README.md).
set(HC_IMGUI_DIR    ${CMAKE_SOURCE_DIR}/third_party/imgui)
set(HC_MINHOOK_DIR  ${CMAKE_SOURCE_DIR}/third_party/minhook)
set(HC_VULKAN_DIR   ${CMAKE_SOURCE_DIR}/third_party/vulkan-headers/include)

foreach(required
        ${HC_IMGUI_DIR}/imgui.cpp
        ${HC_IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
        ${HC_MINHOOK_DIR}/src/hook.c
        ${HC_MINHOOK_DIR}/include/MinHook.h
        ${HC_VULKAN_DIR}/vulkan/vulkan.h)
    if(NOT EXISTS ${required})
        message(FATAL_ERROR "Missing ${required}\nSee third_party/README.md for what to place where.")
    endif()
endforeach()

set(HC_THIRD_PARTY_SOURCES
    ${HC_IMGUI_DIR}/imgui.cpp
    ${HC_IMGUI_DIR}/imgui_draw.cpp
    ${HC_IMGUI_DIR}/imgui_tables.cpp
    ${HC_IMGUI_DIR}/imgui_widgets.cpp
    ${HC_IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
    ${HC_MINHOOK_DIR}/src/buffer.c
    ${HC_MINHOOK_DIR}/src/hook.c
    ${HC_MINHOOK_DIR}/src/trampoline.c
    ${HC_MINHOOK_DIR}/src/hde/hde64.c)

add_library(HuntingCompanion SHARED
    src/main.cpp
    src/localization.cpp
    src/official_strings.cpp
    src/cjk_overlay.cpp
    src/cjk_overlay.hpp
    src/overlay_queue.hpp
    src/embedded_languages.hpp
    src/embedded_official_strings.hpp
    src/embedded_challenge_objective_hashes.hpp
    src/embedded_hunting_recommendations.hpp
    src/embedded_cjk_fonts.hpp
    ${HC_THIRD_PARTY_SOURCES})

target_compile_features(HuntingCompanion PRIVATE cxx_std_17)
target_include_directories(HuntingCompanion PRIVATE
    src
    ${HC_IMGUI_DIR}
    ${HC_MINHOOK_DIR}/include
    ${HC_MINHOOK_DIR}/src
    ${HC_VULKAN_DIR})
target_compile_definitions(HuntingCompanion PRIVATE
    WIN32_LEAN_AND_MEAN NOMINMAX WINVER=0x0601 _WIN32_WINNT=0x0601
    # Vulkan functions are resolved through the game's own loader at runtime;
    # nothing links against vulkan-1.
    VK_NO_PROTOTYPES
    IMGUI_IMPL_VULKAN_NO_PROTOTYPES
    IMGUI_IMPL_VULKAN_USE_LOADER
    IMGUI_USER_CONFIG="hc_imconfig.h")
target_compile_options(HuntingCompanion PRIVATE -Wall -Wextra -Wpedantic -Wno-missing-field-initializers -Os -ffunction-sections -fdata-sections)
# Vendored code: keep its warnings out of our build log.
set_source_files_properties(${HC_THIRD_PARTY_SOURCES} PROPERTIES COMPILE_OPTIONS "-w")
target_link_libraries(HuntingCompanion PRIVATE user32)
target_link_options(HuntingCompanion PRIVATE -static -static-libgcc -static-libstdc++ -Wl,--no-undefined -Wl,--gc-sections -s)
set_target_properties(HuntingCompanion PROPERTIES PREFIX "" SUFFIX ".asi" OUTPUT_NAME "HuntingCompanion")

# Companion UI phrases, official Rockstar item/material strings and the CJK overlay
# fonts are all embedded at build time. Nothing is read from or written to disk
# at runtime apart from the existing HuntingCompanion.ini.
