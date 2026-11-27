// Host test for src/ini_upgrade.cpp: an old ini is rewritten in the current
// layout with the player's settings kept and FontLog renamed to Log.
#include "ini_upgrade.hpp"
#include "embedded_ini_template.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace hunting_companion::native_fonts {
void diagnostic(const char*, ...) noexcept {}
}

static std::string read_all(const char* path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}

static int g_failures = 0;
static void expect(bool ok, const char* what) {
    if (!ok) { std::printf("FAIL: %s\n", what); ++g_failures; }
}

int main(int, char** argv) {
    const std::string path = std::string(argv[1]) + "/HuntingCompanion.ini";
    // The ini of a player on an older version (the user's own, in short).
    std::ofstream(path, std::ios::binary) <<
        "[General]\nMenuKey=119\nFilterKey=219\nWeaponIconsKey=221\nShowWeaponIcons=1\nUILanguage=6\n"
        "MenuFont.en-US=$Font5\nMenuFont.pl-PL=$ledger\nTextColour=4\nHeadingColour=0\nHudLabelColour=0\n"
        "FontLog=1\n[HUD]\nAllX=12\nAllY=-30\nAllSize=120\n[Overlay]\nCjkTextScalePercent=110\n"
        "[Fonts]\nJapaneseFace=$title1\nJapaneseBold=1\n";
    hunting_companion::ini::upgrade(path.c_str());
    const std::string out = read_all(path.c_str());

    expect(out.find("IniVersion=" + std::to_string(hunting_companion::ini::kIniVersion)) != std::string::npos, "version written");
    expect(out.find("UILanguage=6\r\n") != std::string::npos, "UILanguage kept");
    expect(out.find("TextColour=4\r\n") != std::string::npos, "colour kept");
    expect(out.find("MenuFont.en-US=$Font5\r\n") != std::string::npos, "per-language font kept");
    expect(out.find("MenuFont.pl-PL=$ledger\r\n") != std::string::npos, "second per-language font kept");
    expect(out.find("AllX=12\r\n") != std::string::npos && out.find("AllSize=120\r\n") != std::string::npos, "HUD kept");
    expect(out.find("WeaponSize=100\r\n") != std::string::npos, "new HUD default added");
    expect(out.find("LowStarIcons=1\r\n") != std::string::npos, "new setting added with default");
    expect(out.find("Log=1\r\n") != std::string::npos, "FontLog=1 became Log=1");
    expect(out.find("FontLog") == std::string::npos, "FontLog removed");
    expect(out.find("[Overlay]") == std::string::npos && out.find("CjkTextScalePercent") == std::string::npos, "outdated section removed");
    expect(out.find("JapaneseFace=$title1\r\n") != std::string::npos, "font override kept (uncommented)");
    expect(out.find("JapaneseBold=1\r\n") != std::string::npos, "extra font key kept");
    expect(out.find(";KoreanFace=\r\n") != std::string::npos, "unused optional stays commented");
    expect(out.find("; Icons on one- and two-star animals") != std::string::npos, "explanations included");

    // A second run changes nothing (the file is now current).
    hunting_companion::ini::upgrade(path.c_str());
    expect(read_all(path.c_str()) == out, "current file left alone");

    // No ini at all: the full template is written.
    std::remove(path.c_str());
    hunting_companion::ini::upgrade(path.c_str());
    const std::string fresh = read_all(path.c_str());
    expect(fresh.find("Log=0\r\n") != std::string::npos && fresh.find("MenuKey=119\r\n") != std::string::npos, "fresh ini written");

    if (g_failures) return EXIT_FAILURE;
    std::printf("PASS: an old ini is updated in place, settings kept, FontLog renamed to Log.\n");
    return 0;
}
