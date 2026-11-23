# OVERDRIVE development log

## 2026-11-23 - Phase 1 technical design + Phase 2 prototype

- Environment inspected: Linux x86-64, GCC 11.4, CMake, no GPU/controllers/Windows available; no-root .deb path set up for X11/Wayland/GL headers.
- Engine evaluated and chosen: raylib 5.5 + pure C++17 sim core, OpenGL 2.1 profile for the N5010 Ironlake target. Godot 4 ruled out (needs GL 3.3); Godot 3.6 and SDL3+custom rejected on risk. Plan published as a File and PDF for the user.
- raylib 5.5 built from source, GLFW with BOTH X11 and Wayland backends; smoke binary renders under Xvfb + Mesa llvmpipe.
- Implemented: spline track (original Neon District-style loop, 8 checkpoints, walls), arcade car model (accel/brake/reverse, speed-sensitive steering, handbrake drift), chase camera with speed FOV, lap/checkpoint rules with placing, TURBO pickups (collect, activate, respawn), 3-car grid with pure-pursuit AI, minimap, HUD, pause, FPS overlay, settings save/load with corrupt-file fallback, remappable keys, gamepad support with button steering.
- Automated tests: 7 groups passing (track integrity, acceleration/top speed cap, AI full race, wall containment, boost pickup/activation/respawn, determinism, settings roundtrip).
- Visual verification: rendered frames under Xvfb inspected; start-gantry alignment and building offsets fixed.
- Verified by measurement: binary links only libc/libstdc++/libgcc/libm; GL and windowing load at runtime.
- NOT yet done: Windows cross-build, real-hardware FPS, native Wayland runtime test, audio, menus beyond pause, more power-ups/tracks/modes (Phase 3+).

## v0.1.1 - camera and visibility fixes (2026-11-23)

- Chase camera now targets 3m ahead of the car and the demo draws every 3rd
  frame so the smoothed camera actually tracks (previously snapshot frames
  caught the camera far behind the car, hiding it).
- Buildings rejected within halfWidth+55m of any track point and placed at
  halfWidth+62..102m; the corner camera corridor is now clear (was occluded
  by buildings at the hairpin).
- Start/finish gantry thinned and raised to y=9 so it no longer blocks the
  speed HUD or the view.
- Added car-car collision test (separation + velocity exchange); 8/8 test
  groups pass.
- Demo evidence re-captured: grid start, corner traffic, turbo activation,
  pause overlay, finish banner (finish=1).

## v0.1.2 - dt-based camera, shared input mapping, HUD contrast (2026-11-23)

- Chase camera now updates every sim step with dt-based smoothing (1-exp(-6dt))
  and bounded distance (8 + speed*0.08): the player car stays in frame through
  acceleration and turbo, independent of render cadence.
- Input handling split into pure mapKeys/mapGamepad/mergeInputs (sim layer,
  unit-tested); the demo drives the player through mapGamepad with scripted
  analog axes, exercising the same mapping the interactive path polls.
- Finish banner, countdown, BOOST! and WRONG WAY now draw with black outline
  for contrast against bright track/car colors.
- 9/9 test groups pass (added input mapping tests).

## v0.2.0 - Phase 3 core game flow (2026-11-23)

- Title menu (start, vehicle select STANDARD/SPRINTER/HAULER, AI difficulty
  ROOKIE/PRO/ACE, laps 1/3/5, quality, fps cap, quit), pause menu
  (resume/restart/quit to title), results screen with race time, best lap and
  persisted track records.
- AI difficulty scales AI target-speed skill; player vehicle choice applies the
  roster spec to the player car.
- Settings persist aiDifficulty/laps/playerVehicle/keys to
  ~/.config/overdrive/settings.cfg.
- --uitest runtime smoke test: full windowed race driven through the shared
  input-mapping path. Note: XTest keyboard injection does not reach GLFW under
  Xvfb (probe-verified), so physical key polling is covered by unit tests up to
  the GLFW boundary and still needs on-hardware verification by the user.
- 9/9 headless test groups pass.
