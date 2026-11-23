// OVERDRIVE game layer: raylib rendering, input, HUD. Sim lives in sim.h (pure C++).
#pragma once
#include "sim.h"
#include <string>

struct GameApp {
    sim::Settings settings;
    std::string settingsPath;
    bool showFps = false;
};

int runGame(GameApp& app);                       // interactive
int runScreenshots(GameApp& app, const std::string& prefix, int warmFrames, int shotEvery, int shotCount);
int runUITest(GameApp& app);
int runUIScreens(GameApp& app, const std::string& prefix);                       // headless XTest input-path test
