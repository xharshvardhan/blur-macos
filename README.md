# Blur for macOS — development port

A native Apple Silicon macOS port built on
[uKER/BlurRebuilt](https://github.com/uKER/BlurRebuilt), using Cocoa and Metal.
The game was originally created by Bizarre Creations. This work builds on uKER's
codebase, modern PC fixes and unlocked-frame-rate work; it is not
an official release.

**This is a development snapshot, not a finished game release.** Building from
source requires developer tools and native dependencies.

## Current status

- The original engine runs natively on Apple Silicon with Cocoa/Metal rendering,
  keyboard input, physics, Lua UI, audio and movie playback.
- A Brighton/BMW keyboard race completed its original checkpoint/lap/finish
  sequence and exited with code 0 ([slice evidence](docs/macos/PLAYABLE_SLICE.md)).
- A normal career race with 20 cars and three laps completed through ordinary keyboard input,
  returned through the original results UI, autosaved and exited through the
  desktop confirmations with code 0. The test finished 20th with no medal.
- A fresh relocated app loaded that save and retained its 60 fans/event/car record,
  with zero unread save bytes. All 21 non-system runtime libraries loaded from
  inside the app; its signature and dependency/notices manifest remained valid
  after ordinary exit ([career verification](docs/macos/career-cycle-acceptance.md)).
- Local development staging uses CMake BundleUtilities and includes app metadata,
  transitive libraries, notices and installed SPDX metadata. ZIP extraction also
  preserves the verified signature and library aliases.
- Real LAN/multiplayer play, medal/unlock progression, full content coverage,
  another clean Mac and notarized distribution remain unverified. This is a
  playable development port with bounded verification.

The original LAN menus also create a host lobby and leave through the normal
desktop confirmations with exit 0. The observed capacity is 20, with one local
player; remote joining and multiplayer racing remain unverified
([LAN evidence](docs/macos/lan-actual-readiness.md)).

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
versions. A local development bundle can include its runtime libraries; see
[development staging](docs/macos/development-bundle-staging.md). A notarized
installer and separate-Mac validation are not available yet.

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
