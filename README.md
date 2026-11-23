# OVERDRIVE

An original arcade combat racing game for Linux and Windows. Fast, accessible racing with tactical power-ups, built for modest hardware (target: Dell Inspiron N5010, Intel i3-370M, integrated graphics, 1366x768).

Status: in development, not a complete release. Eight-vehicle garage milestone in progress. Native C++17 simulation core with a raylib 5.5 platform layer (OpenGL 2.1, X11 and Wayland backends).

## Build (Linux)

Requirements: CMake, a C++17 compiler, and X11/Wayland development headers. raylib 5.5 must be built separately from source. Its source and required system development headers are not included here.

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

Menus are keyboard driven (arrows + enter). Controls: WASD or arrows to drive, SPACE handbrake, SHIFT turbo, ESC pause, F3 FPS overlay, F11 fullscreen. Gamepads work too: left stick steers, right trigger accelerates, face buttons drive without sticks. Vehicle choice (8 original roster entries, 3 initially owned, 5 earned through race credits), AI difficulty, laps and key remaps persist in `~/.config/overdrive/settings.cfg`.

## Layout

- `src/sim.*` - pure C++17 simulation (driving, AI, race rules, power-ups), no engine dependencies
- `src/game.*` - raylib rendering, input, HUD
- `tests/` - headless automated tests
- `docs/DEVLOG.md` - development log

## Known issues

- Frame rate on the N5010 target hardware is not yet measured; use --fps30 and --low if needed.
- Native Wayland runtime untested (both X11 and Wayland backends are compiled in; X11 tested).
- Windows x86-64 targets cross-compile successfully with llvm-mingw. Windows runtime is NOT tested.

## Garage milestone

Open GARAGE from the title menu. Left/right browses eight vehicles; Enter (or
pad A) equips an owned vehicle or spends earned in-game credits to unlock one.
Escape/pad B returns. Credits are offline game rewards, not real money. A win
awards 120 CR, second 80 CR, another finish 50 CR. The first three cars remain
owned from the initial prototype; new cars cost 200-400 CR.

Physical tuning is in `src/vehicles.cpp`. Acceleration, speed, steering, grip,
drift grip, braking, maximum impact health, boost acceleration/duration and mass
feed the simulation, not only the garage bars. Durability currently changes
impact health only; combat destruction is a later milestone. The garage and
models remain work in progress, not final art.

Progress saves to `~/.config/overdrive/settings.cfg` on Linux or
`%APPDATA%\overdrive\settings.cfg` on Windows. No account or connection needed.
See `docs/RELEASE-CHECKLIST.md` for the outstanding complete-game work.

Windows cross-build: configure raylib 5.5 and this project with the same
`tools/mingw-toolchain.cmake`, updating the local toolchain prefix. Build raylib
with `PLATFORM=Desktop`, `OPENGL_VERSION=2.1`, examples off. Pass its build/raylib
folder as RAYLIB_DIR. `run.bat` launches the packaged Windows executable. This
recipe has compiled here but has not run on Windows.

## Licence

The OVERDRIVE source code (everything under `src/`, `tests/`, `tools/`, the build files and docs written for this project) is released under the GNU General Public License, version 2 (GPL-2.0). See `LICENSE` (text from https://www.gnu.org/licenses/old-licenses/gpl-2.0.txt). If you distribute modified versions you must do so under the same licence and provide the corresponding source. No copyright holder name is asserted in this repository yet; add one before relying on per-file notices.

Third-party 3D models in `assets/models/` are NOT GPL. Each keeps its own licence (CC BY 3.0 or CC0 1.0), listed with author and source in `assets/models/CREDITS.md` and shown in the in-game CREDITS screen. They are separate assets that this program loads at run time. raylib is not included; it is a separate zlib-licensed library you build yourself.

Car names such as DODGE CHARGER, CAMARO ZL1, NISSAN GTR, DODGE CHALLENGER and NISSAN 180MX are shown only when the matching model file loads. The model licences cover the model files only. They do not grant any rights in manufacturer names, logos or vehicle designs. This project is not affiliated with or endorsed by any manufacturer. That is a notice, not a clearance. OVERDRIVE is original software and contains no code or assets from Blur, Asphalt or any other game.

## Status

Work in progress. Automated headless tests pass on Linux; the Windows build has only been cross-compiled, never run; performance on the Dell N5010 target is unmeasured.
