# OVERDRIVE

An arcade combat racing game for Linux and Windows, built with C++17 and raylib 5.5/OpenGL 2.1. Work in progress, not a finished realistic-looking game or Blur/Asphalt equivalent.

## Current playable preview: 430cf64

[Preview release and downloads](https://github.com/linuxkiller9000/overdrive/releases/tag/preview-430cf64)

- [Game ZIP: Linux and Windows x86-64](https://github.com/linuxkiller9000/overdrive/releases/download/preview-430cf64/overdrive-game-preview-430cf64.zip)
- [Complete matching source ZIP](https://github.com/linuxkiller9000/overdrive/releases/download/preview-430cf64/overdrive-source-preview-430cf64.zip)
- [Preserved v0.4 hardware-test rollback](https://github.com/linuxkiller9000/overdrive/releases/tag/v0.4.0-hardware-test)

**Source status:** the attached preview source ZIP is authoritative for checkpoint 430cf64. Apart from this README update, the browsable repository remains the older baseline. This is not a full code-tree sync. The preview tag and GitHub-generated Source code ZIP/tar also point to that older baseline. Use the separately attached matching source ZIP to build the preview. It includes raylib source, assets, licenses and build instructions.

Extract the entire game archive before starting `sh run.sh` on Linux or `run.bat` on Windows. Linux needs a working desktop and OpenGL 2.1+ driver. Read `PREVIEW-README.md`, `README.md` and `TEST-REPORT.md` inside the game ZIP.

**Back up saves before testing.** Preview 430cf64 shares `~/.config/overdrive` on Linux or `%APPDATA%\overdrive` on Windows with v0.4. Do not run both together. Restore your backup before returning to v0.4. This preview does not have an isolated profile directory.

## What is in the preview

Six routes and six rule sets; 20 stock handling profiles plus a procedural custom-car creator; offline garage/progress; combat pickups; local two-player Street Race; keyboard and gamepad mapping; original synthesized audio.

The latest visual work adds CC0 authored buildings and distant building images to the fictional NEON DISTRICT route, native material/source-normal rendering and wheel animation for four imported cars, street furniture, and two graphics tiers. Most of the roster and other route art remain simplified. The creator edits the procedural custom body, not imported cars. This is not 20 finished realistic licensed models.

LOW is the default. HIGH is manually selectable and persists, adding city building/road shadows and postprocessing. Restart after changing quality to apply antialiasing. Single-player races have a skippable five-second intro. After finishing, R plays recent roughly 15-second visual replay at 0.75x. This is not saved replay, full sound playback or deterministic rewind.

Solo controls: arrows/WASD drive, Space handbrake, Shift boost, E or Right Ctrl fire, Escape pause, F3 timing overlay, F11 fullscreen. Menus use arrows/Enter and Escape. See the packaged player README for split-screen and controller setup.

## Verification and remaining gaps

Checkpoint 430cf64 passed Linux build, Windows cross-build, 31 simulation groups, complete scripted city laps on both tiers, alternating city quality/reload tests under software OpenGL 2.1, and LOW split finish/wait/reset. Extracted package launch and authored assets were checked. These are bounded automated/offline checks, not measured target-device FPS.

Windows execution, native Wayland execution, physical controllers, target Intel laptop and Nvidia/Ryzen device performance, and subjective audio quality remain unverified. No performance or visual-parity guarantee is made.

Repeated buildings, flat lighting, visible skyline transitions, crude gates/markers and mixed-quality cars remain. Collision is approximate and pileups can overlap. Split-screen is Street Race only and uses one controller mapping profile. Special-mode progression, route art, creator depth and the wider visual/product finish remain open. Audio is synthesized, not recorded realistic engines.

## Build the matching preview

Download the complete matching source ZIP above and follow `docs/BUILD.md` inside it. The browsable older tree can still be built separately with CMake and a separately built raylib 5.5, but it does not contain the preview features. System development headers and a working graphics driver are required.

## Licenses

Project code is GPL-2.0, see `LICENSE`. Third-party models, city assets and materials retain their own licenses and attribution in the source/game archives. Asset-file copyright licenses do not clear vehicle brands, badges or design rights. raylib is separately zlib-licensed. No paid resources are included.
