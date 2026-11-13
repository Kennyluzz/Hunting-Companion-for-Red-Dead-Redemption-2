// Host test for src/localization.cpp, driven by tests/test_localization.py.
// Reads "<language>\t<english phrase>" lines from stdin and prints ui_text().
#include "localization.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

int main() {
    using namespace hunting_companion;
    // Pointers must survive language switches made on another thread.
    load_ui_localization(1, -1);
    const char* french = ui_text("Back");
    const std::string frenchCopy = french;
    std::thread([] { for (int i = 0; i < 2000; ++i) load_ui_localization(i % 13, -1); }).join();
    if (frenchCopy != french || frenchCopy == "Back") return 2;

    std::string line;
    int current = -2;
    while (std::getline(std::cin, line)) {
        const auto tab = line.find('\t');
        if (tab == std::string::npos) continue;
        const int language = std::atoi(line.substr(0, tab).c_str());
        if (language != current) { load_ui_localization(0, language); current = language; }
        if (active_ui_language() != language) return 3;
        std::cout << ui_text(line.c_str() + tab + 1) << '\n';
    }
    return 0;
}
