#include "game.h"
#include "sim.h"
#include <cstring>
#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <cstdlib>
#include <filesystem>

int runTests();

static std::string settingsPath() {
#ifdef _WIN32
    const char* base = getenv("APPDATA");
    std::string dir = std::string(base ? base : ".") + "\\overdrive";
    std::filesystem::create_directories(dir);
    return dir + "\\settings.cfg";
#else
    const char* home = getenv("HOME");
    std::string dir = std::string(home ? home : ".") + "/.config/overdrive";
    std::filesystem::create_directories(dir);
    return dir + "/settings.cfg";
#endif
}

int main(int argc, char** argv) {
    GameApp app;
    app.settingsPath = settingsPath();
    app.settings = sim::loadSettings(app.settingsPath);
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--selftest")) return runTests();
        if (!std::strcmp(argv[i], "--uitest")) return runUITest(app);
        if (!std::strcmp(argv[i], "--uiscreens") && i + 1 < argc) return runUIScreens(app, argv[++i]);
        if (!std::strcmp(argv[i], "--shots") && i + 1 < argc) return runScreenshots(app, argv[++i], 240, 120, 4);
        if (!std::strcmp(argv[i], "--track") && i + 1 < argc) app.settings.trackIndex = std::atoi(argv[++i]);
        if (!std::strcmp(argv[i], "--fxshots") && i + 1 < argc) return runFxShots(app, argv[++i], app.settings.trackIndex);
        if (!std::strcmp(argv[i], "--low")) app.settings.quality = 0;
        if (!std::strcmp(argv[i], "--fps30")) app.settings.fpsCap = 30;
    }
    return runGame(app);
}
