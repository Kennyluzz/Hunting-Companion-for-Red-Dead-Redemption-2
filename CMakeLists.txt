cmake_minimum_required(VERSION 3.20)
project(HuntingCompanion VERSION 1.0.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Configure with cmake/mingw-w64-toolchain.cmake to target Windows x64.")
endif()

add_library(HuntingCompanion SHARED src/main.cpp)
target_compile_features(HuntingCompanion PRIVATE cxx_std_17)
target_compile_definitions(HuntingCompanion PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX WINVER=0x0601 _WIN32_WINNT=0x0601)
target_compile_options(HuntingCompanion PRIVATE -Wall -Wextra -Wpedantic -Os -ffunction-sections -fdata-sections)
target_link_libraries(HuntingCompanion PRIVATE user32)
target_link_options(HuntingCompanion PRIVATE -static -static-libgcc -static-libstdc++ -Wl,--no-undefined -Wl,--gc-sections -s)
set_target_properties(HuntingCompanion PROPERTIES PREFIX "" SUFFIX ".asi" OUTPUT_NAME "HuntingCompanion")
