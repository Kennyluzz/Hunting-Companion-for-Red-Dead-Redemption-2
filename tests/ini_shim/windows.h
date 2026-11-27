// Host stand-in for the few Windows ini functions src/ini_upgrade.cpp uses, with
// the same rules: case-insensitive sections and keys, values trimmed, a null key
// lists the section's keys separated by NULs.
#pragma once
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>

using DWORD = unsigned long;
using UINT = unsigned int;
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)

namespace ini_shim {
inline std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
inline std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r"), e = s.find_last_not_of(" \t\r");
    return b == std::string::npos ? "" : s.substr(b, e - b + 1);
}
struct Entry { std::string section, key, value; };
inline std::vector<Entry> parse(const char* path) {
    std::vector<Entry> out;
    std::ifstream in(path, std::ios::binary);
    std::string line, section;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == ';') continue;
        if (line.front() == '[' && line.back() == ']') { section = line.substr(1, line.size() - 2); continue; }
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        out.push_back({section, trim(line.substr(0, eq)), trim(line.substr(eq + 1))});
    }
    return out;
}
}  // namespace ini_shim

inline DWORD GetFileAttributesA(const char* path) {
    struct stat st{};
    return stat(path, &st) == 0 ? 0 : INVALID_FILE_ATTRIBUTES;
}
inline DWORD GetPrivateProfileStringA(const char* section, const char* key, const char* fallback,
                                      char* buffer, DWORD size, const char* path) {
    const auto entries = ini_shim::parse(path);
    std::string result;
    if (key == nullptr) {
        for (const auto& e : entries)
            if (ini_shim::lower(e.section) == ini_shim::lower(section)) { result += e.key; result.push_back('\0'); }
        const DWORD n = static_cast<DWORD>(std::min<std::size_t>(result.size(), size - 1));
        std::memcpy(buffer, result.data(), n);
        buffer[n] = '\0';
        return n;
    }
    bool found = false;
    for (const auto& e : entries)
        if (ini_shim::lower(e.section) == ini_shim::lower(section) && ini_shim::lower(e.key) == ini_shim::lower(key)) {
            result = e.value; found = true; break;
        }
    if (!found) result = fallback;
    std::snprintf(buffer, size, "%s", result.c_str());
    return static_cast<DWORD>(std::strlen(buffer));
}
inline UINT GetPrivateProfileIntA(const char* section, const char* key, int fallback, const char* path) {
    char buffer[64];
    GetPrivateProfileStringA(section, key, "", buffer, sizeof(buffer), path);
    return buffer[0] ? static_cast<UINT>(std::atoi(buffer)) : static_cast<UINT>(fallback);
}
