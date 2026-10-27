# Native dependency reuse

The native port keeps the original engine/game code and replaces platform/vendor
services at their existing boundaries. Current choices:

| Dependency | Native role | Evidence |
| --- | --- | --- |
| SDL3 3.4.18 | CoreAudio PCM output; media playback output | Original audio mix and authored Movie audio fixtures |
| SoundTouch 2.4.0 | Replace bundled SoundTouchZ pitch/time processing | Original audio/DSP fixture |
| MojoShader ad5dff84830c2863c841f4b1f4e3df78c705b383 | D3D SM1–3 bytecode to Metal source | Actual native shader loader and file-backed FX GPU draws |
| glslang, SPIR-V Tools, SPIRV-Cross | Original source HLSL to validated MSL | 693 Jam built-ins translated/loaded earlier; focused application shaders built normally |
| GNU libunistring | PC UTF-16 formatting with native unsigned64 and Unicode support | UTF-16 and actual Amax text fixtures |
| Foundation/CoreFoundation | Window/platform paths and strict UTF8/UTF16 conversion | Native application and UI file-save/text fixtures |
| FFmpeg9 / libavformat63, libavcodec63, swscale10, swresample7 | Movie demux/decode/color conversion/PCM resampling | Authored streaming/preload/bounded-memory movies and exact GPU pixels |
| zlib | Existing PAK and UI script compression formats | Original PAK/UI Lua callbacks on authored data |
| libjpeg-turbo through libjpeg API | Original photo JPEG encoder/decoder | Actual original JPEG source, known-color round trip fixture |
| LibTomCrypt 1.18.2 / LibTomMath 1.3 | Original Demonware cryptographic operations on Darwin | Original vendor sources, independent 3DES vector and ECC peer fixtures |
| Apple SDK libcurl | Original network HTTP boundary | Native vendor link; hosted game services remain unavailable |

ICU was evaluated for wide formatting. Its ustdio unsigned64 limitation does not
match the game's formatting contract; libunistring covers the mixed integer64
and UTF8/UTF16 case without a custom varargs decoder. MoltenVK is useful for a
Vulkan renderer, but this engine has a direct Metal backend, so adding another
whole graphics translation layer would add work here. FNA3D demonstrates the
MojoShader integration pattern; the port uses the translator rather than
replacing the original renderer with the FNA engine.

The additional reuse review considered
[Valve GameNetworkingSockets](https://github.com/ValveSoftware/GameNetworkingSockets)
and its [macOS build presets](https://github.com/ValveSoftware/GameNetworkingSockets/blob/master/BUILDING.md).
It supplies reliable/unreliable messages, fragmentation and optional ICE peer
connectivity. It is a candidate for a later transport enhancement. The current
port first preserves the game's existing LAN messages and discovery layout;
changing transport requires coordinated peers and game-lobby validation. The
library is not integrated and does not establish that the old three-player
lobby problem is fixed.

[FNA3D](https://github.com/FNA-XNA/FNA3D) is a useful implementation reference
for D3D9 effects and cross-platform graphics. Its XNA-oriented interface would
require another engine adapter; the reusable shader translator is already
integrated directly. [MoltenVK](https://github.com/KhronosGroup/MoltenVK) layers
Vulkan over Metal. Its current upstream notice recommends KosmicKrisp for
Apple Silicon Vulkan applications. Neither layer simplifies this port's
existing direct Metal renderer enough to justify replacing it.

Pinned fetched libraries keep their notices. FFmpeg/libunistring currently come
from the host package manager. Their installed binaries require macOS26 despite
the project's historical13.0 compile flag. Current-host verification establishes
Apple Silicon/macOS26 behavior only. A portable distributable must build/bundle
these dependencies for its chosen deployment target and remove host paths.
The local 0.1.4 development bundle has passed same-host ZIP extraction, relocation,
strict signatures and dependency/notices integrity. Another clean Mac and older
macOS deployment remain unverified.

Primary references: [SDL macOS](https://wiki.libsdl.org/SDL3/README-macos),
[MojoShader](https://github.com/icculus/mojoshader),
[FNA3D](https://github.com/FNA-XNA/FNA3D),
[SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross),
[GNU libunistring](https://www.gnu.org/software/libunistring/manual/libunistring.html),
[ICU formatted I/O](https://unicode-org.github.io/icu/userguide/io/),
[FFmpeg](https://ffmpeg.org/).


The 2026-10-28 [optimization/reuse audit](optimization-and-reuse-audit.md) reviews
SDL3, the existing shader tools, optional Tracy, bgfx/wgpu and DXVK Native/Vulkan
portability. These alternatives are not newly integrated and are not measured
performance improvements. Preserve the working direct Metal path while testing
bounded changes; generic platform/rendering libraries do not remove engine or
asset-schema adaptation.

The authored [NativeHost package](../../code/mercury/library/gamesys/framework/native_host/README.md)
now isolates the Cocoa window/event/CAMetalLayer implementation. Its directory
has a scoped MIT license; it does not change inherited engine/source terms.
Blur uses that same implementation through compatibility names. A separate
installed-package consumer exercises a Metal scene without engine headers or
assets. This is a source-port building block; rendering, game logic, data
formats, input mappings, audio and network policy remain the consumer's work.
Its license is included in staged game notices as `NativeHost.txt`.
