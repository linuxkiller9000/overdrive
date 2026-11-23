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
