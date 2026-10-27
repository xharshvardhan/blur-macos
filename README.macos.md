# Native macOS port — development status

This local `macos` branch starts from `uKER/BlurRebuilt` commit
`1965facd3d4860447a043b1e0fea4d55c086d1f5`. The original game builds and runs
as an arm64 Cocoa/Metal application. Startup mounts the original PC archives,
verifies secure package signatures and loads the real game and UI resources.

The normal Master frontend has completed a career race with 20 cars and three
laps using ordinary keyboard input, processed the original non-medal results,
autosaved, returned to the SP menu and exited through the original desktop
confirmations with code 0 (MAC243). The test finished 20th; it does not establish
medal or tier-unlock progression. Manual and automatic results photo capture
also passed after repairing shared photo-clear scene ownership.

A fresh relocated app then loaded the same save, reached the owned/allowed
garage and exited normally with code 0. All 21 non-system runtime libraries
loaded from inside the app; its strict signature and dependency/notices hashes
passed before and after the run (MAC251). A local development ZIP also passed
extraction and integrity checks. These are same-host checks; real multiplayer
races, full content coverage, another clean Mac and notarized distribution
remain unverified. See [career acceptance](docs/macos/career-cycle-acceptance.md)
and [the initial playable slice](docs/macos/PLAYABLE_SLICE.md). This remains a
development port.

The public source snapshot lives at
[xharshvardhan/blur-macos](https://github.com/xharshvardhan/blur-macos).
It credits the upstream source and preserves inherited notices without assigning
a new blanket license. Private retail game data, profiles, build outputs and
inherited console payloads are excluded from the published snapshot. Original
PC game data must be supplied separately and is not bundled with the app.

The original LAN menus also create a host lobby and leave through the normal
desktop confirmations with exit 0. The observed capacity is 20, with one local
player; remote joining and multiplayer racing remain unverified
([LAN evidence](docs/macos/lan-actual-readiness.md)).

## Build and run

Requirements: Apple Silicon Mac, Xcode Command Line Tools, CMake 3.25+, Ninja,
Python 3 and the native dependencies listed in
[dependencies.md](docs/macos/dependencies.md). Current host-supplied FFmpeg,
libunistring and other dylibs require macOS 26; the historical 13.0 compilation
flag does not establish support for older systems.

```sh
sh tools/macos/build.sh
_work/run/BlurNativeGame.app/Contents/MacOS/BlurNativeGame --data-root /path/to/complete/game-data
```

The build script builds the actual game, runs the component tests, then checks
the separate diagnostic bootstrap. These checks do not establish gameplay
correctness. Manual launches without arguments choose and remember an existing game-data
folder and show the actual game window. Automated native game runs with
explicit CLI arguments use a minimized Cocoa window. Optional
`--capture /absolute/path/frame.png --capture-after 40 --quit-after 55` requests
one completed GPU-frame capture and an elapsed-time shutdown. An outer timeout
is still necessary when debugging a blocked callback or assertion.

Products:

- `_work/run/BlurNativeGame.app`: actual original Amax application, game layers,
  audio, UI/Lua and game logic, with native platform services and Metal graphics.
- `_work/run/BlurMacBootstrap.app`: separate archive-inspection/development UI.
- `_work/run/blur_mac_probe`: separate archive and Metal diagnostics.
- `_work/build/macos/blur_mac_*`: focused original-engine/native integration
  fixtures. Authored fixture images are not screenshots of the game.

All generated output belongs under ignored `_work/`. The Windows
`tools/build.sh` remains unchanged. Source lists are derived from Jam; generated
`cmake/targets.cmake` is not maintained by hand.

## Implemented integration

The actual game links its original Amax, GameLogic, UI, GameAudio, RaceBook,
Kanzen and networking modules. The native boundaries reuse Cocoa, Metal,
GameController, SDL3, SoundTouch, FFmpeg, MojoShader, glslang/SPIRV-Cross,
libunistring, zlib, libjpeg and LibTomCrypt/LibTomMath. All 693 Jam built-in
shaders have native compiled resource equivalents. See the dependency document
for upstream references and selected reuse decisions.

Component checks cover the original resource/PAK/Xt loaders, PC disk layouts,
UTF16/Lua, native file-save services, fixed simulation ticks with unlocked
rendering, Metal resources/shaders/FX/MSAA/MRT/occlusion, input mappings,
audio/movie decoding, scheduler/layers and the original network transports.
Actual retail-data checks separately verify secure game/boot PAK mounts,
66 UI bundle textures against their stored bytes and both original boot movies.
Those resource checks do not prove that a complete race works. The source
snapshot also lacks UITextBox: 11 text-entry widgets across seven account,
DLC, social/friends and custom-popup screens are omitted by the original
unknown-type importer
([source audit](docs/macos/ui-snapshot-gaps.md)). No replacement widget behavior
has been invented.

Native development instances can override their service bindings with
`BLUR_MAC_ARTERY_TCP_PORT`, `BLUR_MAC_ARTERY_UDP_PORT`,
`BLUR_MAC_LAN_DISCOVERY_BIND_PORT`, `BLUR_MAC_LAN_DISCOVERY_TARGET_IP` and
`BLUR_MAC_LAN_DISCOVERY_TARGET_PORT`. Defaults retain TCP6317/UDP6318 and
broadcast discovery50001. Discovery bind0 requests a client ephemeral port;
it does not make that client a migration-capable host. Invalid explicit values
fail initialization. Independent instances also need distinct absolute
`BLUR_MAC_PROFILE_STORE` files and `BLUR2PC_SAVE_ROOT` folders. These controls
support verification; they do not establish that multiple game players have
joined or completed a race.

Native networking preserves the existing LAN message layouts. Discovery and
socket fixtures include more than three synthetic peers; real lobby joins,
a complete multiplayer race and Windows interoperability remain unverified.
Hosted game services and voice are unavailable. Native spectral reverb follows
the original PC dry-signal path; wet spectral reverb is unavailable. The
original PC mixer remains24kHz/128samples at every output quality. Physical controller/haptics
verification also remains pending.

## Verification limits

Debug and optimized actual-game binaries build. The stream/audio/save/UI
ownership integration passed 19 affected checks per configuration, and the
lighting repair passed 14 renderer checks per configuration. The subsequent
alpha-to-coverage and private worker quit-signal integration passed 19 affected
checks in each configuration. Exact verification is recorded in
[LEDGER.md](docs/macos/LEDGER.md). Run game tests and component CTest processes
sequentially: the original Artery service owns localhost6317.

UBSan component configurations use `-DBLUR_MAC_BUILD_GAME=OFF`. The actual
full-game UBSan link still requires resolving the source snapshot's absent
`Components::C_PortalBlockerCreate` RTTI/key-function definition
([source audit](docs/macos/portal-blocker-audit.md)); the port has
not fabricated this component or disabled vptr checks to conceal that gap.
ASan currently hangs in dyld before main on this host, so it supplies no game
validation evidence.

The original Brighton/BMW keyboard slice completed its checkpoint/lap/finish
sequence and exited with code 0. The normal frontend created and reloaded a career save,
played tutorial movies, reached the owned-car garage and completed full shutdown.
Ordinary StartScreen desktop quit confirmation also exits0. A populated20-car
race completes normal shutdown after the pickup borrower lifetime repair.

A normal career race with 20 cars and three laps completes its original ordered splits/laps and
finish under human keyboard control. Its post-race photo, non-medal results
return, fresh autosave and ordinary desktop exit now pass (MAC243, exit0).
The test finished20th; it does not establish medal/unlock success.

A newly copied self-contained app reloads the same save (version 1378,
51,227 decoded bytes,0 unread) and retainsrank1/fans60/stars0, event4597671/state
none and ownedcar331339493. With dyld override paths unset and a minimal runtime
PATH, all 21 non-system libraries load from inside that app. Strict signatures
and notice/dependency hashes pass before and after ordinary exit0 (MAC251).
The copy preserves all seven library aliases; an earlier private copy that flattened
them is retained as a failed signature baseline. See
[career-cycle-acceptance.md](docs/macos/career-cycle-acceptance.md).

Real LAN/multiplayer races, medal/unlock progression, broader content coverage,
another clean Mac and notarized distribution remain unverified. See
[BOARD.md](docs/macos/BOARD.md) for the active LAN work.

Native alpha-to-coverage preserves original crowd/mirror requests at existing
attachment sample counts. Focused GPU checks cover1x/2x/4x, MRT, depth/stencil,
blending and state restoration; see
[alpha-to-coverage.md](docs/macos/alpha-to-coverage.md).

The earlier optimized World rollback timeout was reproduced with live thread
capture and traced to resource/layer workers sharing a private quit-event name.
Their native constructors now use private signals; general named-event sharing
and Windows code remain unchanged. The original-code regression fails, while
the repaired worker fixture and ten fresh World processes pass without a test
timeout increase. See
[resource-worker-retirement-audit.md](docs/macos/resource-worker-retirement-audit.md).

CMake BundleUtilities stages a local development app with meaningful metadata,
all transitive libraries, native/package notices and installed SPDX records.
Strict signatures, hashes and relocation are checked. The current libraries
require macOS 26; another Mac and notarized distribution remain unverified. See
[development-bundle-staging.md](docs/macos/development-bundle-staging.md) for
commands and the exact acceptance limits.
