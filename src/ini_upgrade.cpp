#include "ini_upgrade.hpp"

#include "embedded_ini_template.hpp"
#include "native_fonts.hpp"

#include <windows.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace hunting_companion::ini {
namespace {

constexpr char kMissing[] = "\x01";

// Settings that no longer exist under that name; never carried over as-is.
bool deprecated(const std::string& key) noexcept {
    return key == "FontLog";
}

bool read_value(const char* path, const std::string& section, const std::string& key, std::string& value) {
    char buffer[512]{};
    GetPrivateProfileStringA(section.c_str(), key.c_str(), kMissing, buffer, sizeof(buffer), path);
    if (std::strcmp(buffer, kMissing) == 0) return false;
    value = buffer;
    return true;
}

std::vector<std::string> keys_in(const char* path, const std::string& section) {
    std::vector<char> buffer(32768);
    const DWORD length = GetPrivateProfileStringA(section.c_str(), nullptr, "", buffer.data(),
                                                  static_cast<DWORD>(buffer.size()), path);
    std::vector<std::string> keys;
    for (DWORD i = 0; i < length;) {
        const std::string key(buffer.data() + i);
        if (key.empty()) break;
        keys.push_back(key);
        i += static_cast<DWORD>(key.size()) + 1;
    }
    return keys;
}

// "key=value" or ";key=value" (a commented-out optional setting); not comments.
bool setting_line(const std::string& line, bool& commented, std::string& key) {
    commented = !line.empty() && line[0] == ';';
    const std::string body = commented ? line.substr(1) : line;
    const std::size_t equals = body.find('=');
    if (equals == std::string::npos || equals == 0) return false;
    key = body.substr(0, equals);
    for (char c : key) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '_' || c == '-')) return false;
    }
    return true;
}

}  // namespace

void upgrade(const char* path) noexcept {
    if (path == nullptr || path[0] == '\0') return;
    const DWORD attributes = GetFileAttributesA(path);
    const bool exists = attributes != INVALID_FILE_ATTRIBUTES;
    const int version = static_cast<int>(GetPrivateProfileIntA("General", "IniVersion", 0, path));
    if (exists && version >= kIniVersion) return;

    std::string output;
    std::string section;
    std::vector<std::string> templateKeys;  // keys the template has in the current section

    // The player's own keys in a section the template does not list (for example
    // MenuFont.<language> or a font override's Bold flag) are kept at its end.
    auto keep_extra_keys = [&]() {
        if (section.empty()) return;
        for (const std::string& key : keys_in(path, section)) {
            bool known = deprecated(key);
            for (const std::string& templateKey : templateKeys) known = known || templateKey == key;
            if (known) continue;
            std::string value;
            if (read_value(path, section, key, value)) output += key + "=" + value + "\r\n";
        }
    };

    const char* cursor = kTemplate;
    while (*cursor != '\0') {
        const char* end = std::strchr(cursor, '\n');
        std::string line = end != nullptr ? std::string(cursor, end) : std::string(cursor);
        cursor = end != nullptr ? end + 1 : cursor + line.size();
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line.size() > 2 && line.front() == '[' && line.back() == ']') {
            // A section's own extra keys go before the blank line that ends it.
            while (output.size() >= 4 && output.compare(output.size() - 4, 4, "\r\n\r\n") == 0)
                output.erase(output.size() - 2);
            keep_extra_keys();
            if (!output.empty()) output += "\r\n";
            section = line.substr(1, line.size() - 2);
            templateKeys.clear();
            output += line + "\r\n";
            continue;
        }
        bool commented = false;
        std::string key;
        if (setting_line(line, commented, key)) {
            templateKeys.push_back(key);
            std::string value;
            bool have = key != "IniVersion" && read_value(path, section, key, value);
            // Renamed setting: FontLog (under [Fonts] or [General]) is now Log.
            if (!have && key == "Log") {
                have = read_value(path, "General", "FontLog", value) || read_value(path, "Fonts", "FontLog", value);
            }
            if (have) {
                output += key + "=" + value + "\r\n";
                continue;
            }
        }
        output += line + "\r\n";
    }
    keep_extra_keys();

    std::FILE* file = std::fopen(path, "wb");
    if (file == nullptr) return;
    std::fwrite(output.data(), 1, output.size(), file);
    std::fclose(file);
    native_fonts::diagnostic("Settings file %s (layout %d -> %d)", exists ? "updated" : "created",
                             version, kIniVersion);
}

}  // namespace hunting_companion::ini
