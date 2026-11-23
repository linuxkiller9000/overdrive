# OVERDRIVE

An original arcade combat racing game for Linux and Windows. Fast, accessible racing with tactical power-ups, built for modest hardware (target: Dell Inspiron N5010, Intel i3-370M, integrated graphics, 1366x768).

Status: Phase 2 playable prototype. Native C++17 simulation core with a raylib 5.5 platform layer (OpenGL 2.1, X11 and Wayland backends). Private until you say otherwise.

## Build (Linux)

Requirements: CMake, a C++17 compiler, and X11/Wayland development headers. raylib is built from source (vendored in the build tree, no system packages needed).

```
cmake -B build -DCMAKE_BUILD_TYPE=Release -DRAYLIB_DIR=/path/to/raylib-5.5/build/raylib
cmake --build build
```

## Run

```
./build/overdrive            # play (title menu: start, vehicle, difficulty, laps, quality, fps cap)
./build/overdrive --fps30    # cap at 30 FPS for weaker hardware
./build/overdrive --low      # minimal visuals
./build/overdrive --selftest # headless simulation tests
./build/overdrive --uitest   # headless runtime loop test (window + input mapping + race)
```

Menus are keyboard driven (arrows + enter). Controls: WASD or arrows to drive, SPACE handbrake, SHIFT turbo, ESC pause, F3 FPS overlay, F11 fullscreen. Gamepads work too: left stick steers, right trigger accelerates, face buttons drive without sticks. Vehicle choice (STANDARD / SPRINTER / HAULER), AI difficulty, laps and key remaps persist in `~/.config/overdrive/settings.cfg`.

## Layout

- `src/sim.*` - pure C++17 simulation (driving, AI, race rules, power-ups), no engine dependencies
- `src/game.*` - raylib rendering, input, HUD
- `tests/` - headless automated tests
- `docs/DEVLOG.md` - development log

## Known issues

- Frame rate on the N5010 target hardware is not yet measured; use --fps30 and --low if needed.
- Native Wayland runtime untested (both X11 and Wayland backends are compiled in; X11 tested).
- Windows cross-build planned, not yet produced or run.
