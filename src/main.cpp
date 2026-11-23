#include "game.h"
#include "sim.h"
#include <cstring>
#include <cstdio>
#include <string>
#include <sys/stat.h>

int runTests();

static std::string settingsPath() {
    const char* home = getenv("HOME");
    std::string dir = std::string(home ? home : ".") + "/.config/overdrive";
    mkdir(dir.c_str(), 0755);
    return dir + "/settings.cfg";
}

int main(int argc, char** argv) {
    GameApp app;
    app.settingsPath = settingsPath();
    app.settings = sim::loadSettings(app.settingsPath);
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--selftest")) return runTests();
        if (!std::strcmp(argv[i], "--shots") && i + 1 < argc) return runScreenshots(app, argv[++i], 240, 120, 4);
        if (!std::strcmp(argv[i], "--low")) app.settings.quality = 0;
        if (!std::strcmp(argv[i], "--fps30")) app.settings.fpsCap = 30;
    }
    return runGame(app);
}
