# OVERDRIVE release acceptance

This is the production checklist, not a claim that the work is finished.
The existing C++17/raylib/OpenGL 2.1 project and working features are preserved.

## Current verification
- Linux build and 11 simulation test groups pass.
- Full scripted three-lap game rendering completes under X11/Xvfb.
- Windows x86-64 cross-build succeeds with llvm-mingw, not runtime-tested.
- Physical keyboard/gamepad polling, native Wayland, and actual N5010 FPS unknown.

## Milestones remaining
- [ ] Eight original vehicle silhouettes with previews, meaningful acceleration,
  speed, steering, grip, drift, brake, durability and boost differences.
- [ ] Functional garage comparisons, unlocks and persistent progress.
- [ ] Neon District street/tunnel art and route polish.
- [ ] Coastal Run: independent highway/bridge/tunnel layout and ocean scenery.
- [ ] Industrial Zone: independent technical route, cargo/factories/machinery.
- [ ] Desert Circuit: independent long-straight route, rocks and desert scenery.
- [ ] Track previews, collision boundaries, AI routes, checkpoints, start/finish.
- [ ] TURBO, SHIELD, EMP, OIL SLICK, DECOY, MAGNET, DISRUPTOR, REPAIR with clear
  feedback, cooldowns, AI usage and offensive/defensive counterplay.
- [ ] Street Rush, Last Driver Standing, Checkpoint Chaos, Time Attack,
  Free Drive, Elimination with real rules, HUDs and win/end conditions.
- [ ] Better avoidance, overtaking, recovery, pickup and combat AI.
- [ ] Main Play/Garage/Tracks/Modes/Multiplayer/Settings/Credits/Exit interface.
- [ ] Keyboard/gamepad menu navigation, remapping screen, button-only pad support.
- [ ] Two-player split-screen with independent inputs and per-player HUDs.
- [ ] Original engine/brake/drift/collision/pickup/powerup/menu/countdown sounds
  and music; separate persisted SFX/music volume.
- [ ] Offline profile/rewards/unlocks/results/personal-bests/statistics/settings.
- [ ] Resolution/quality/30-or-60 FPS options and measured optimization.
- [ ] Runtime verification and regression tests for every vehicle/track/mode/
  powerup/save/input/split-screen path, with explicit unverified hardware gaps.
- [ ] Source, assets, Linux/Windows builds and launchers, README, controls,
  build instructions, changelog, test report in a compact release ZIP.

No copied proprietary assets, accounts, subscriptions, paid dependencies,
microtransactions or public publication. Four-player support only if measured
performance supports it. Do not call the project complete before the checklist
is implemented and honestly tested.
