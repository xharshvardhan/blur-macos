# Blur for macOS — development port

A native Apple Silicon macOS port built on
[uKER/BlurRebuilt](https://github.com/uKER/BlurRebuilt), using Cocoa and Metal.
The game was originally created by Bizarre Creations. This work builds on uKER's
codebase, modern PC fixes and unlocked-frame-rate work; it is not
an official release.

**This is a development snapshot, not a finished installer or a complete game
release.** The current build still needs developer tools and native dependencies.

## Current status

- The original game engine builds and runs natively on Apple Silicon, with
  Metal rendering, keyboard input, physics, Lua UI, audio and movie playback.
- A Brighton/BMW keyboard race completed its original checkpoint/lap sequence,
  natural race finish and normal exit with code 0
  ([verification](docs/macos/PLAYABLE_SLICE.md)).
- Normal startup/menu navigation, local career profile creation and reload,
  autosave, the single-player hub and unlocked event selection have been
  exercised with original game data.
- A populated normal Debug frontend reached the owned-car garage and completed
  full application shutdown with exit code 0.
- The repaired normal career lighting pass reaches the HUD with 20 cars and
  responds to keyboard throttle and steering. The populated 20-car race shuts
  down normally with exit code 0 after the pickup ownership repair. Native
  alpha-to-coverage now supports the original crowd/mirror rendering requests.
  Debug and optimized builds each pass 19 affected integration checks; the
  worker quit-signal repair also passes ten fresh-process world rollback runs.
  A normal three-lap, 20-car career race has also reached its original finish.
  Its post-race photo exposed a scene-ownership gap, now repaired and verified
  through the original Photo Mode capture/return path. Full results/save-return
  validation is still in progress.
- The start-screen desktop quit confirmation exits normally using ordinary keys.
- Complete normal career race/results/save progression, full content coverage,
  real LAN/multiplayer play and portable distribution remain in progress.
  The verified one-track race is a narrower milestone than a complete port.

## Build and run

See **[README.macos.md](README.macos.md)** for the build requirements, dependency
list, native app products and current validation limits.

```sh
sh tools/macos/build.sh
_work/run/BlurNativeGame.app/Contents/MacOS/BlurNativeGame --data-root /path/to/complete/game-data
```

You must supply your own complete Windows game installation. The macOS port does
not include or download the retail game data, and those files are kept outside
the source tree and native application bundle. The executable runs natively;
Wine is not used.

The current dependency setup has been tested on Apple Silicon with macOS 26.
A lower deployment-target flag does not establish support for older macOS
versions. A self-contained application/installer is not available yet.

## Development

Native work lives alongside the original engine code. Source lists are derived
from Jam, and generated builds, local profiles, game files and runtime evidence
belong under ignored `_work/`. Focused native checks and the actual game run are
separate validation layers; compiling or passing a fixture does not establish
that every game mode works.

Useful references:

- [Native dependencies and reused projects](docs/macos/dependencies.md)
- [One-track acceptance scope](docs/macos/PLAYABLE_SLICE.md)
- [Normal menu flow](docs/macos/normal-menu-flow.md)
- [Task board](docs/macos/BOARD.md) and [verification ledger](docs/macos/LEDGER.md)

The native boundaries reuse existing projects including SDL3, SoundTouch,
FFmpeg, MojoShader, glslang/SPIRV-Cross and libunistring, alongside Apple's public
frameworks. Their individual notices and requirements remain applicable.

## Upstream and notices

This native source snapshot derives from uKER/BlurRebuilt
[commit 1965facd](https://github.com/uKER/BlurRebuilt/tree/1965facd3d4860447a043b1e0fea4d55c086d1f5).
The public snapshot is published independently of the local development history.
The original Windows README is preserved
at [UPSTREAM_README.md](docs/macos/UPSTREAM_README.md); its Windows installation
and online-play statements describe upstream, not the macOS development port.

Existing source and dependency notices are retained. This repository does not
assign a new blanket license to inherited code or game content.
