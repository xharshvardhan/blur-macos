# NativeHost: a small Cocoa window host

A C++17 Cocoa window/event/CAMetalLayer host, extracted from the authored native
macOS port. It builds and installs independently. The package API has no Core.h,
Mercury, Blur, original assets or parent-repository CMake dependency. Its MIT
license covers this directory only; see [provenance](PROVENANCE.md). It does not
license the rest of the repository.

This is a window host for source-based applications. It supplies no renderer,
game conversion, Windows ABI/API implementation, game loop, asset formats,
input mapping, text composition/IME, audio or network service. For clients
already using SDL, using SDL directly may be the better integration.

Build requirements: macOS, CMake 3.24+, an Apple Clang SDK/toolchain supporting
C++17/Objective-C++ and Cocoa/QuartzCore. The example also requires Metal. The
package does not invent a deployment target or claim another Mac/architecture
was tested. Set the normal CMake toolchain/deployment options for your target.

## Independent build, install and consumer

Configure this directory itself, without the repository's root CMake project:

```sh
cmake -S /path/to/native_host -B /tmp/native-host-build \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/tmp/native-host-prefix
cmake --build /tmp/native-host-build
cmake --install /tmp/native-host-build
```

Copy `client/` to a separate directory. It is a separate CMake project that
uses only `find_package(NativeHost CONFIG REQUIRED)`, its own authored source,
the installed prefix and system frameworks. It never reaches back to the parent
source tree. Relocating the install prefix before this step also exercises the
exported relative include/archive paths; framework link flags contain no
absolute build-SDK paths.

```sh
cmake -E copy_directory /path/to/native_host/client /tmp/native-host-client
cmake -S /tmp/native-host-client -B /tmp/native-host-client-build \
  -DCMAKE_PREFIX_PATH=/tmp/native-host-prefix -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/native-host-client-build
ctest --test-dir /tmp/native-host-client-build --output-on-failure
```

The consumer target is `NativeHost::Window`:

```cmake
find_package(NativeHost 0.1 CONFIG REQUIRED)
target_link_libraries(my_application PRIVATE NativeHost::Window)
```

Automated tests create minimized real windows, use hardware and produce no
audio. Serialize them with other hardware tests. They require a logged-in macOS
WindowServer session; they are not headless tests. `native_host_demo --show`
explicitly shows its finite three-second authored Metal triangle scene.
`--test` stays minimized and finishes after a two-second render interval. A
single final GPU readback checks triangle/background pixels; no images or
screenshots are written. This is a second independent host client, not a second
complete game port or proof of portability to arbitrary engines.

## Contracts

- Create, Destroy, PollEvents and owner destruction belong to the Cocoa main
  thread. Create rejects other threads. PollEvents and live Destroy throw on
  foreign-thread use. Getters do not promise concurrent access safety.
- Initial sizes are Cocoa content points. Resize callbacks and pixel getters
  are backing pixels; CAMetalLayer drawableSize follows them. A client owns
  reconciling its render graph and any asynchronous Ready resources with the
  latest dimensions before rendering. The host does not resize engine resources.
- Events report raw Cocoa physical key codes and modifier masks, UTF-8 text,
  repeats, separate flagsChanged events, mouse coordinates in backing pixels and
  Cocoa scroll units. There is no Windows virtual-key translation. Clients
  implement mappings, input state and held-key releases on focus loss.
- AppKit dispatch holds its state alive through a callback and preserves the
  first C++ callback exception for the next PollEvents. A callback may destroy
  its own owner. Initial resize runs after owner publication inside Create;
  its exception propagates directly and the owner remains available for cleanup.
- Close requests are advisory: CloseRequested becomes true and the close
  callback runs; the client performs resource retirement and calls Destroy.
  NativeWindow and MetalLayer are borrowed handles valid only for that owner.

## Authored validation

`native_host_conformance` tests actual host-created minimized windows, current
Retina backing dimensions, resize/close, main-thread rejection, raw key/mouse
fields, physical left/right modifier identity, callback first-error handoff,
self-destruction, initial callback failure and repeated teardown/recreation.
Its synthetic NSEvent/delegate notifications traverse actual host methods;
they do not claim physical user typing or hardware focus coverage. Held-key
release is explicitly the authored client's policy.

`native_host_demo` uses its own Metal shaders, queue, render targets and fixed
1/60 simulation with interpolation driven by real elapsed time. A worker creates
an old-size actual Metal target and withholds Ready publication across two host
resizes. The client catches up that target to the latest backing pixels before
its first render. A later resize tests continued rendering and clean retirement.
The test verifies actual triangle/background texels after presentation. This
client does not import any engine/game assets or timing/resize implementation.

The original engine consumes this same implementation through thin
`Framework::Mac` aliases. Existing full-engine/game acceptance remains a separate
gate. Installed-client test passes and another clean Mac require actual recorded
runs; building this directory alone does not establish them.
