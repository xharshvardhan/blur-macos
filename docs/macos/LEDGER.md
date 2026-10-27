# macOS ledger

## 2026-10-26 — MAC001 claimed

Cloned the public upstream and switched to a local macos branch. Read inherited AGENTS.md, build/workflow references, architecture, and DX11 phase records. Ran the required context helper; it does not recognize this new phase. Native build tools: Apple Clang and SDK present; installed CMake and Ninja because neither was available on PATH.

The standard tools/build.sh is Windows-only (hard-coded Visual Studio/MSBuild). macOS needs its own platform target; no Windows executable can be delivered from this host. Exact native verification and remaining blockers will be appended after implementation. Original game data is still on the user's separate Windows laptop; a local path was requested while independent platform work proceeds.

## 2026-10-26 — MAC001 native bootstrap built and verified

Changed: Apple-only CMake routing before the Windows build; a guarded macOS
core ABI branch; a read-only native Pak adapter using existing disk-layout
definitions; native Metal compile/draw/readback check; Cocoa bootstrap bundle;
command-line probe; independent serialized archive regression fixtures;
reproducible build script and native phase documentation. No generated Windows
source list was edited. These native targets have their own source list in
cmake/macos.cmake, following the existing separately authored backend-target
pattern; they do not pretend to build the Jam-selected Amax game.

Verified: sh tools/macos/build.sh returned 0 on Apple M4 Pro. CTest passed
mac_core_abi, mac_pak_regression (15 test methods including v1/v2, all three
supported codecs, secure variants, block boundaries, corruption and bounds),
and mac_metal_readback. The real GPU produced the expected triangle and clear
pixels. file identifies both deliverables as Mach-O arm64; otool -L shows only
native Apple/system dependencies. The delivered bundle's --self-test passes.
git diff --check passed. Logs: _work/build/macos/{verification.log,
final-verification.log,bundle-final.log}. GPU verification image:
_work/build/macos/metal-verification.png.

An experimental offscreen AppKit snapshot did not reliably include composited
text. That optional snapshot mode and its image were removed; no GUI screenshot
is claimed as verification. Manual folder-picker operation remains untested.
The Metal PNG is explicitly a synthetic hardware probe, not game imagery.

Status: MAC001 is complete as an initial bootstrap milestone. The user's full
native-game objective is not complete. No Amax game executable or native game
renderer has been built. Windows runtime checks are unavailable on this host;
legacy core include-case/formatting warnings remain. Nothing was committed,
pushed, relicensed or published.

Next: obtain the user's actual installation folder path and run
_work/run/blur_mac_probe --data-root against it, then validate known entries
before integrating the filesystem/resource layer. Full Display/Display2 and
shader porting, platform/audio/input/network implementations and gameplay/LAN
verification remain tracked in MAC002–MAC005.

## 2026-10-26 — MAC006 source-only core port complete

User steering: keep building from BlurRebuilt rather than waiting for a copy of
the Windows installation. Recorded that game content is a later runtime-test
dependency, not a gate on current platform compilation. The repo contains
authored shaders and some test/tool artwork; no runtime PAKs were found under
code/. A brief search found the publisher's in-store demo announcement, not an
authorized downloadable PC runtime-data package. No game package was downloaded.

Changed: compiled the actual original engine CoreMemoryAligned.cpp,
CoreMemoryNonTracked.cpp and utility/endian.cpp into blur_mac_core_runtime.
Added Darwin posix_memalign allocation with valid small-alignment handling and
NULL/EINVAL on invalid requests, plus malloc_size queries. Non-math translation
units include their required core headers directly on Darwin instead of
depending on the Windows math PCH. No allocation success stub or replacement
endian implementation was used. The source files already belong to the original
core jamfile; no generated Windows source list needed changing.

Verified: native CMake configure/build succeeded; CTest passed all four suites,
including the new engine runtime check. Checks cover three actual aligned
allocation APIs, read/write/usable sizes, null and zero-size requests, invalid
alignments, byte swaps/format records, active source debug assertions and 8,000
threaded allocations. The standalone UBSan build/run passed with recovery
disabled. git diff --check passed. Logs: _work/build/macos/mac006-{configure,
build,ubsan-build,ubsan-run}.log.

Limit: the optional ASan executable stalled inside sanitizer initialization
before main, as established by _work/build/macos/mac006-sanitized-sample.txt.
Its owned process was terminated. ASan verification is unavailable on this host
configuration; no ASan pass is claimed. Original header case/formatting warnings
remain and Windows compilation was not exercised here.

Next: continue asset-independent platform/rendering work from source. Retail
content will be needed when validating actual menus/vehicles/tracks, not before
starting or continuing native compilation. No further installation-transfer
request is pending as a gate on this work. The full native game objective is
still incomplete. No commit, push, public fork or release was made.


## 2026-10-26 — MAC004 continued: full core/native runtime interfaces

User explicitly requested sustained source-only work and an active goal for the
complete running native game. Created that goal; it remains active/incomplete.
No permission question or repeated original-file transfer request was issued.
MAC004 remains owned/in progress, rather than marked done for these components.

Changed: original core target generated from Jam (50 files accounted for: 45
original native objects, five replaced thread/platform objects); whole-archive
link verification; scalar math/64-bit allocation/stream/Clang fixes; real Darwin
FP environment, MT synchronization, pthread/TLS/atomic/thread-ID APIs, debugger
and heap checks. Ported the original Time/Address/Socket objects. Added native
Sys::I_File disk/memory streams and a PAK-to-I_File bridge; the existing archive
probe now reads decoded payloads through the engine interface. Extended build
options to isolate normal, Release and UBSan outputs. No changes to generated
Windows cmake/targets.cmake or the inherited Windows launcher.

Verified: tools/macos/build.sh succeeds, including all eight CTest suites and the
bundle's offscreen Metal self-test on Apple M4 Pro. Separate Release and UBSan
builds also pass all eight suites with undefined-behavior recovery disabled.
The 15-method archive suite covers versions/codecs/security variants through the
engine stream and independently compares SHA-256. Core checks force-load every
object, test matrix/vector math and stream copies/FP setup, 40,000 contended
fetch-adds, recursion/ownership/deadlines/events/semaphore limits/RW exclusion,
TLS and worker restart/errors/QoS. Platform fixtures exercise UTC conversion,
monotonic microseconds, eight concurrent TCP clients, UDP/peek/EOF/close, buffered
and direct file IO, sparse >4 GiB seek, null skips/zero writes and memory streams.
Mach-O inspection confirms native arm64; otool lists Apple frameworks/system
libraries, no Wine. git diff --check and Python byte-compilation pass.

Evidence: _work/build/macos/mac004-final-build.log,
mac004-{release,ubsan}-{configure,build,tests}.log,
metal-verification.png. Source inventories are generated into each build's
`generated/` directory. A broader diagnostic syntax audit passes 30/93 original
system sources; its report is system-audit.{json,log}. This audit is not a linked
system build and includes ordinary unity/header dependencies among the failures.

Limits: no actual game startup, menu, track, race, save or LAN lobby tested;
Display/Display2 is still not connected to Metal. Names share MT state only in
one process; QoS is a coarse Windows-priority mapping. Native disk streams are
not yet mounted through I_FileSystem/game factories. Original RGBA TODO and
case warnings remain. ASan remains unavailable due to the previously sampled
pre-main runtime initialization hang; no new ASan attempt was made. Source and
asset publication provenance remains unresolved; nothing was committed/pushed.

Exact continuation: keep MAC004 active. First port remaining low-level platform
contracts revealed by system-audit: core scheduler TL platform/thread APIs
(mercury_to_tl.h, tl_defs.h, tl_thread.h and jobqueue_xenon.cpp replacement),
Xt/Artery fixed-width platform branches, C_FilePath native separators/case and
Clang dependent type declarations in sortedbinarytree.h. Replace VirtualMemory's
incorrect Linux reserve/free/commit fallback with real native mappings, then
integrate filesystem/async services and continue renderer/shader bring-up. Do
not count the existing Linux fallback's successful syntax as working memory
services. Continue source work without requiring retail data at this point.


## 2026-10-26 — MAC004 continued: actual scheduler/VM/filesystem interfaces

Previous goal turn classified as progress from the inspected worktree and test
logs. This turn ports additional real engine interfaces and keeps the complete
running-game objective active. MAC004 remains owned/in progress. No retail
installation transfer is requested as a gate on source work.

Changed: native TL platform and worker lifecycle; original scheduler execution,
wide job parameters and pointer/count operations; source queue mutation/lifetime
synchronization; worker events/stop/role handling; native VM backend dispatch
with real reserve/commit/decommit/release and protections; original BlockAllocator
64-bit ownership/allocator statistics; actual C_FileSystem backend/default
interface; path/metadata/mutation operations; packed scalar streaming;
zero-buffer audio/scene/rampak read-through; core singleton/map portability;
Xt/Artery fixed-width platform branches. Source lists continue to derive from
Jam. Sixteen original system entries now compile in addition to the core set.

Verified: standard tools/macos/build.sh and its native bundle self-test succeed.
All eleven suites pass in Debug, independent Release and UBSan builds (recovery
disabled). New behavior tests cover TL suspend/resume/exit/join/stale handles,
8,000 concurrent original-queue records, real scheduler worker/dependency/wide
parameter/ungrouped execution and repeated shutdown; actual VM protection/zero
recommit/sparse ownership/full-page and concurrent allocation; default engine
filesystem, independently specified packed bytes, zero-buffer streaming,
original-case paths, file metadata/permissions/time, directory creation and
rename-without-overwrite. Active allocation statistics are checked as well as
zero post-free totals. git diff --check and Python byte-compilation pass.

Failures resolved during work: scalar ALIGN128 left a packed atomic misaligned
(the owned crash report proves EXC_ARM_DA_ALIGN at AtomicAdd); packed alert lane
order needed explicit interpretation; an ungrouped job indexed group -1;
Release exposed an uninitialized allocation-statistics accumulator; broader
map instantiation required corrected type friendship/base lookup. No debugger
completion was claimed: the owned lldb/debugserver/test processes stalled and
were terminated; the crash report supplied the actionable stack instead.

Evidence: _work/build/macos/mac004-platform2-{debug,tests}.log,
mac004-platform2-{release,ubsan}-{build,tests}.log,
mac004-{scheduler,filesystem}-tests.log,
mac004-scheduler-crash.ips and mac004-scheduler-lldb.log.
The final broader syntax audit passes 70/93 sources, recorded in
system-audit-platform2-final.{json,log}; this is diagnostic and not a full engine
link/runtime result. ASan remains unavailable for the previously recorded
pre-main host runtime issue; no new ASan attempt was made.

Limit: actual Amax/Blur startup, menu, race, save/load, controllers and LAN lobby
remain unverified/unimplemented as a complete native application. Metal is still
only the existing offscreen bootstrap probe, not Display/Display2 integration.
Full async services, audio/input, game allocator bootstrap, debug transport and
renderer/shaders still require work. Queue mutation uses a native mutex and
profiling is explicitly disabled for bring-up. No commit, push or publication.

Exact continuation: keep MAC004 and the full-game goal active. Use the final
system audit to port ArterySync/ArteryService and native BSD socket selection
(WsSocket.cpp is the Windows selection; BkSocket.cpp is the existing POSIX
starting point), plus async/Io services. Finish remaining GetNode/Min/Max
Clang-dependent lookup issues as their actual callers instantiate, then
MemoryTracker/global allocator startup and CPU timing. Promote verified objects
into the generated native system target; do not include the obsolete C_File
backend alongside NativeFile or disable its failures by returning success.
Proceed to complete engine/framework compilation and actual Metal display and
shader integration. Runtime content is still only needed at the actual
menu/track/race-validation boundary, not before continuing these source tasks.


## 2026-10-26 — MAC004 continued: original Artery service on Darwin

Goal continuation classified as progress. MAC004 remains owned/in progress;
the complete running-game goal is active, with no new asset-transfer gate.

Changed: native ArterySync and BSD socket branch; generated source selection
from both actual PC and PS3 Jam rules; original service/peer/message execution
in Speed; atomic ownership and status publication; restart/error cleanup;
source-faithful scalar bytes and bounded message handling. Twenty-two original
system entries compile. Native Ut/Btl tree storage now gives the sentinel a
real Node lifetime without constructing its payload; GetNode no longer returns
a reference to a temporary iterator member. Broader instantiated map/set lookup
and friendship issues were corrected. Initial native malloc/path/format work in
MemoryTracker remains incomplete, not presented as a working tracker.

Verified: standard sh tools/macos/build.sh succeeds with 12 Debug suites and
headless native bundle self-test. Independent Release passes 13 suites,
Debug UBSan passes 12, optimized Speed UBSan passes 13; recovery disabled.
Artery checks cover thread lifecycle, recursive/timed locks, conditions,
manual/auto-reset signal semantics, eight sockets, partial transfer, EOF,
SO_NOSIGPIPE, UDP, real HELO/framing/eight service connections/restart and
80,000 concurrent shared-message copies. Optimized sanitizer checks exposed and
resolved invalid sentinel downcast; subsequent erase coverage exposed and
resolved dangling GetNode references. Non-default payload construction and
release counts also pass. No debugger completion claim or ASan retry.

Evidence: _work/build/macos/mac004-artery-*.log; mac004-tree-{build,test}.log;
system-audit-artery-final.{json,log} (84/93 standalone Debug syntax only).
No full game link or game-frame screenshot exists. No commit, push or release.

Exact continuation: port actual async IoDevice/IoService platform bodies, fix
IoRequest/IoFile reference ownership and wait/cancel semantics, integrate
remaining debug/network/MemoryTracker/global allocator startup. Use the final
audit for concrete remaining Clang failures. Do not confuse syntax-only IO
success with a working implementation. Then compile the wider engine/framework
and implement the actual Display/Display2 Metal backend, shaders, audio/input.
Retail assets are required for actual scenes/race validation later. Full
startup/menu/race/save/load/game-LAN criteria remain unfulfilled.


## 2026-10-26 — MAC004 continued: native async and wider engine system

Previous goal turn classified as progress from current source/log evidence.
MAC004 remains owned/in progress and the complete running-game goal is active.

Changed: actual queued native IoFile/IoRequest/IoDevice/IoService backend, request
completion/cancel/release/exception ownership, joined shutdown and restart;
original AsyncFile integration including the real C_FileSystem read entry;
source-generated wider system/messaging target and real build metadata/logger
registration. All 92 selected original system entries compile and force-link.
Resolved whole-library startup faults in virtual debug-root lifetime, local CPU
timing counters, XML root casts, nested list ownership and messaging ADL.
Optional profiling/event instrumentation is explicitly disabled; no success
stubs replace file operations or engine startup.

Verified: standard sh tools/macos/build.sh succeeds, including native bundle
self-test and 15 suites. Independent Release, Debug UBSan and optimized Speed
UBSan pass all 15. New async checks exercise eight callers, 64-bit sparse-file
offsets, callback completion/release and failures, pending cancel without buffer
access, close ordering, real stream buffering/EOF/seek/skip, filesystem routing,
worker reentry, concurrent shutdown, queued failure callbacks, restart and zero
native I/O resources after shutdown. Wider integration force-loads the entire
system set and executes logging/XML/CPU timing/messaging/build identity in
Debug and Speed. ASan remains unavailable as previously recorded; no new try.

Source-only continuation also removes initial compiler failures from resource,
logicalFs and Xt. Final audit: 6/6, 29/29 and 8/8 syntax pass, respectively;
framework 0/17 still needs Win32/Direct3D PCH/platform port. These are diagnostic
results, not runtime validation or native game completion.

Evidence: _work/build/macos/mac004-async-*.log,
mac004-system-integration-*.log, engine-audit-system-integration.{json,log}.
No game frame screenshot exists; Metal remains the synthetic hardware probe.
No commit, push, release or original runtime-data download.

Exact continuation: generate/build/link the real resource/logicalFs/Xt targets
from Jam and test actual resource handles/managers/mounts/schema loading. Preserve
PC binary layouts. Port FrameworkPch/window lifecycle and actual display APIs to
Mac/Metal; then game shaders, audio/input, remaining networking/global allocator
bootstrap and full Amax startup. Actual scenes, one race, saves and game LAN are
still unimplemented/unverified. Source work continues without another request
for the Windows installation.


## 2026-10-26 — MAC004: original loading and framework services

The active complete-game goal remains in progress. Prior turn evidence was
classified as progress. No real game startup/menu/race/save/LAN run exists.

Jam-derived native targets now compile and force-link all resource (6),
logicalFs (29), Xt (8) entries and six framework services (parameters, logger,
timer, settings, layer and layer manager). No manual source list was added to
cmake/targets.cmake. The inventory explicitly records eleven pending framework
entries; UserControls/MemoryDumpers syntax success is not runtime integration.

Original resource handles/managers now load authored files on their actual
worker, notify success/failure, deduplicate copies and repeated requests, run
main-thread post-load, and free on final release. The same path runs through
all six actual PAK mounts: v1/v2 x raw/zlib/packet-zlib, including original packet
scheduler jobs over native async disk reads. Original memory/overlay mounts
and Xt annotated schema export/import/CRC run over authored fixtures only.

Native fixes: AtomicMutex ownership uses acquire/release atomics, recursive
owner count and monotonic timed waits. PakEntryFile uses actual final-buffer
bounds, valid refill markers and EOF-safe positions; unbuffered skips/short
reads maintain fulfilled byte counts. Packet-zlib reads spanning 64 KiB append
to the output cursor and consume remaining bytes instead of the original
request length; premature packet exhaustion reports failure. Fragmented reads
caught this fault after full-buffer reads had passed. The original compressed
backward-physical-seek assertion remains active; no retail seeking behavior is
claimed. Windows-selected implementations remain intact.

Framework native PCH now selects standard/core dependencies without Win32 or
implicit Direct3D includes. Native HRESULT preserves signed 32-bit status.
Layer manager shutdown joins its worker before destroying owner/event state,
releases pending requests, promotes ready layers and completes deferred
cleanup. Repeated shutdown and destructor are safe. Tests exercise real
elapsed/forced/paused/scaled timers, debug settings save/load, layer priority,
enabled filtering, pause IDs, sync/async callbacks and pending cleanup.

Verification: standard sh tools/macos/build.sh, Release Speed, Debug UBSan and
optimized Speed UBSan pass all 17 suites. Standard bundle headless Metal check
passes. Logs: _work/build/macos/mac004-loading-framework-{standard,release,
ubsan,speed-ubsan}.log. ASan remains unavailable; no pass claimed. Synthetic
Metal remains disconnected from engine display; debug settings are not game
save validation. No original data downloaded, commit, push or release.

Exact continuation: port actual Display2/Display backend selection, native
resource/format/context interfaces and Metal device/window lifecycle. Eleven
remaining framework entries depend on application/render/input integration;
use mac004-framework-audit.json for concrete compile failures. Then authored
shaders/effects, audio/input and full game allocator/network/bootstrap. Preserve
fixed simulation cadence and unlocked rendering. Runtime scenes, race, game
saves and game LAN need real content later; source-only work continues now.


## 2026-10-26 — MAC004: native Display2 Metal buffer implementation

Previous goal turn classified as progress from source and verified loading/
framework evidence. MAC004 remains owned/in progress; the full game goal is
active and incomplete. No actual game launch/menu/race/save/LAN evidence exists.

Added the engine-facing C_VertexBuffer/C_IndexBuffer interfaces and the native
C_MetalDevice resource portion under graphics/display2. Actual Apple GPU shared
buffers implement allocation, flags/size/format, CPU lock/unlock, retained
aliases and queued GPU blits. Locks wait for prior GPU commands and reject
nested/range-invalid use; unlock enforces CPU owner identity. Blits reject CPU
locked resources, invalid ranges/alignment and overlapping self-resource use.
Pending work retains Metal resources; native destruction waits for completion.
GPU queue submissions serialize, including cross-queue prior-use waits. Device
lifetime must be serialized with callers; upload workers are joined before
device shutdown. External memory adoption/resizing is explicitly rejected,
rather than silently reporting a metadata change without GPU allocation.

Source selection is derived from the new MACOS branch of the existing Display2
Jam file. Generator records the seven original shared Display2 entries as
pending; it does not count them as compiled or stub their dependencies. The
complete C_Device/C_Context selection and rendering interfaces are not yet
bound to this partial native implementation. No game shaders, textures or draw
calls are claimed by the buffer test. Existing bootstrap triangle is unchanged.

Verified all 18 suites in Debug, Speed, Debug UBSan and Speed UBSan. New hardware
suite checks 4 MiB transfers and byte readback, index format widths, alias release,
source destruction with pending work, CPU/GPU access ordering, invalid ranges,
eight concurrent worker uploads and device restart. Logs under _work/build/macos:
mac004-metal-final-{debug,ubsan,speed,speed-ubsan}.log and
mac004-metal-buffers-{build,test}.log. Standard native build/bundle check passed.
Python generation compiles and git diff --check passes. Windows Display2 Jam
sources/defines/includes compare identically with HEAD (only input-file metadata
differs in the isolated comparison). No inherited Windows source list edits.

API authorities: Microsoft D3DFORMAT documentation establishes the retained
INDEX16=101 and INDEX32=102 wire identifiers:
https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dformat .
Apple shared-storage and buffer-blit documentation establish shared CPU/GPU
storage and macOS four-byte blit alignment:
https://developer.apple.com/documentation/metal/choosing-a-resource-storage-mode-for-apple-gpus
https://developer.apple.com/documentation/metal/mtlblitcommandencoder/copy(from:sourceoffset:to:destinationoffset:size:)
Engine flags and public buffer signatures are taken from the local D3D11Context
and PCD3D11Device sources. No retail binary data was inferred from the fixture.

Exact continuation: extend the real Metal context/types with vertex layout and
resource bindings, shader/texture/surface interfaces and render-state command
encoding; then bind the engine Display2 C_Device/C_Context selectors and upper
Display device, Cocoa window/presentation and remaining framework callers.
Authored shaders/effects, audio/input, complete Amax allocation/bootstrap and
actual game/LAN integration remain required. Source work can continue without
retail files. No commit, push, publication or runtime-data download occurred.


## 2026-10-26 — MAC004: authored Metal shader resources and built-ins

Previous goal turn classified as progress from native buffer source/hardware
results. The complete-running-game goal remains active/incomplete. No game
startup, menu, race, game save/load or game LAN evidence exists.

Added an actual native shader resource implementation beside Display2: versioned
little-endian BMSL blobs contain generated MSL, entry point and register/semantic
reflection. C_MetalDevice creates real MTLLibrary/MTLFunction objects through the
engine-style vertex/pixel shader interfaces. Parameter name lookup, register
spans, vertex input semantics/locations, flags, retained aliases, explicit
release, wrong-stage/truncated/overflowing-section rejection run natively.
Original private device state is shared through MetalNative.h; full C_Device/
C_Context selection and render commands are still unbound.

New tools/macos/compile_shaders.py derives 693 built-in variants from the actual
Jam manifest, regenerates ignored CP/CV/SP headers, normalizes Windows include
paths in ignored shadow copies, preprocesses the original HLSL and preserves
row-major constant-register storage. glslang generates SPIR-V, spirv-val checks
it and SPIRV-Cross emits MSL. Installed build tools: glslang 16.6.0 and
SPIRV-Cross/SPIRV-Tools 1.4.363.0. No game binaries/data downloaded.

Fidelity corrections found beyond compiler success: half/float overload helpers
are normalized to the existing SM5 32-bit convention and only token-identical
resulting helpers merge. Legacy POSITION/PSIZE/DEPTH map to actual raster
built-ins; DepthReplacing is declared for depth writes. Interstage locations
follow HLSL TEXCOORD/COLOR semantics rather than independent declaration order.
Cloth retains signed VFACE through the native boolean front-facing input. The
existing PC alpha-test transformation is reused at b13; its packed backend
fields remain in reflection and are bound separately from authored register
parameters by the future render context.

An explicit audit found sixteen originally unconverted variants with extern,
struct and arithmetic-sized array constants. Those now retain the source's
register offsets and HLSL packing. No legacy register(cN) declarations or
automatic global constant buffers remain in the 693 generated programs.
Focused checks cover ocean c215/c229 wave/position structs, crowd c207 SH data,
Perlin settings, DoF array extents, matrix c0/c4 and original input semantics.
These checks do not prove actual scene/pixel behavior.

The generator emits native C++ arrays for original built-in symbols. Mac-only
EXTERN_SHADER/REF_SHADER declarations now use typed byte arrays, explicit byte
counts and full-width addresses. The native archive force-links every generated
object; original reference macros load native shaders without Windows blobs.
Windows-selected macro definitions are unchanged. Generated native source lists
still come from Jam; no hand edits to cmake/targets.cmake.

Verified: standard native build and bundle headless check pass. All 19 suites
(with shader tools; base remains 18 without them) pass Debug, Speed, Debug UBSan
and Speed UBSan. All 693 programs pass translation, SPIR-V validation and actual
engine Metal resource loading; complete load set also passes Debug UBSan. The
built-in archive/macro integration passes Debug and UBSan. A fresh isolated
macos-shader-clean build proves the generated-source dependency graph from an
empty build directory. Python compilation and git diff --check pass. ASan
remains unavailable; no new attempt/pass. No game frame screenshot exists.

Evidence: _work/build/macos/authored-shaders/{report,metal-check}.json;
mac004-shader-final-{debug,ubsan,speed,speed-ubsan}.log;
mac004-shader-final-full-ubsan.log; mac004-builtin-shaders-final.log;
mac004-builtin-shaders-ubsan.log; mac004-shader-clean.log. Explicit build target:
cmake --build _work/build/macos --target blur_mac_builtin_shaders_test .

Authorities: original D3D11Context/PCD3D11Device/ShaderUtils interfaces and the
original authored shader declarations; existing dx11_shadercompile/alpha_test.py.
Half scalar convention:
https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-scalar
Raster/front-face semantics:
https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-semantics
SPIR-V semantic annotations/opcodes:
https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html
https://github.khronos.org/SPIRV-Registry/extensions/GOOGLE/SPV_GOOGLE_hlsl_functionality1.html
Compiler authorities: https://github.com/KhronosGroup/glslang and
https://github.com/KhronosGroup/SPIRV-Cross .

Exact continuation: implement actual engine vertex element/declaration/format
interfaces and map shader semantic locations to Metal vertex descriptors. Build
render pipelines/context state around the now real buffer/function resources;
add texture/surface/sampler resources and constant banks, including private
alpha/HDR buffers. Metal::Shader and Metal::Buffer native definitions currently
live in their .mm implementation files; expose shared private definitions when
render command encoding needs them. Bind full Display2 selectors/upper Display,
Cocoa application/window presentation, remaining framework, audio/input,
allocation/bootstrap and networking. Data-driven material effects/runtime shader
assets remain outside the built-in set and need their own integration. Retail
content is required for real scenes/races later, not this next source work.
No commit, push, license assignment, publication or original runtime-data request.

## 2026-10-26 — MAC004 actual engine Metal draw/context and texture milestone

MAC004 remains in progress, owned by Codex /root. Added native vertex layouts,
surfaces, context command encoding and 2D/3D/cube textures beside Display2.
Ported actual shared Context.cpp and selected the Metal Context.h backend.
The native Jam selection now contains six implementation files plus the shared
context source. Generated source inventories remain build products. Rendering
uses actual engine shader/buffer APIs, real pipelines/constants, retained command
resources, synchronized staging and explicit unsupported-path failures.

Added offscreen authored-shader GPU regressions for offsets, padded/split/packed
vertices, indices/fans, non-symmetric matrices and per-draw snapshots, dynamic
rename/NOOVERWRITE, depth/alpha/blending, viewport clears, texture addressing,
filtering, gamma/mips, BC decoding, channel swizzles, render-to-texture, volume
Perlin output, all cube faces and CPU/GPU ownership. All comparisons use authored
fixtures and CPU references; there is no game frame or gameplay screenshot.

Shader linking now uses the original D3D9 branch rather than mixing inherited
DX11 register-order signature edits with semantic linkage. MSL varying registers
have a consistent four-component interface with original component accesses;
this permits original float2 pixel reads from float4 vertex outputs. All 693
Jam built-ins translate, SPIR-V validate and load as native Metal resources on
Apple M4 Pro after these corrections. Compiler/Metal success does not imply all
shader pair pipelines or actual scene rendering are verified.

Verified latest changes: standard sh tools/macos/build.sh passes all 20 suites
and the native bootstrap bundle self-test. All 20 also pass Speed, Debug UBSan
and Speed UBSan. Logs: _work/build/macos/mac004-native-render-final-{debug,
ubsan,ubsan-tests,speed,speed-tests,speed-ubsan,speed-ubsan-tests}.log;
mac004-varyings-full.log; authored-shaders/{report,metal-check}.json. ASan remains
unavailable; no new attempt or pass is claimed. git diff --check passes.

Exact next work: finish native texture/state/device wrappers and selectors,
remaining shared Display2 sources and upper Display. Add real window/presentation
and application integration, then continue framework/audio/input/game sources.
D24/stencil/MSAA, nonzero sampler bias, arbitrary borders, anisotropic quality,
partial-block BC root resources and broader formats remain unsupported. Actual
menu/race/save/LAN validation needs later runtime content; no data request is
needed for these next source changes. No commit, push, license assignment or
public release. The complete-running-game goal remains active and incomplete.

## 2026-10-26 — MAC004 shared renderer resource infrastructure continued

Added actual Display2Logger.cpp and BaseDevice.cpp to the Jam-derived native
build; now three original shared Display2 files compile with six native files.
Added native render-texture metadata wrappers and resource-format log strings.
Checked resource insertion/naming/exact-address lookup/sorted dumping/removal with
a real native vertex buffer. Corrected core debug iterator relational operators
needed by libc++ sort and exercised a 513-element permutation. Optional null index
arguments now preserve the existing binding as the authoritative PC bridge does.

The preceding native-shader built-in archive and macro gate also passed Debug and
Debug UBSan; all 693 resources loaded under UBSan. Evidence:
mac004-native-render-builtins{,-ubsan}.log. Latest resource work has a passing
mac_engine_render targeted run in mac004-resource-tracker-test.log; full Debug,
Speed and both UBSan regressions are running into mac004-tracker-final-*.log.
Native full device/window integration and other limits in the previous entry
remain pending. MAC004 and the complete-game goal remain in progress.

## 2026-10-26 — MAC004 native drawable presentation

All preceding tracker changes passed 20 suites in Debug/Speed and both UBSan
configurations (mac004-tracker-final-*.log). Added MetalPresent.mm and native
host-layer attachment, real engine-surface copy/presentation, resize/detach and
submission counters. mac004-present.log records six actual drawable submissions
with exact pixel readback and ownership/dimension checks. This is a layer-only
check; no NSWindow/game/menu/race is claimed. mac_engine_present is now a standard
hardware CTest, bringing the base count to 19 and count with shader tools to 21.
The expanded Debug/Speed/UBSan matrix is running in mac004-present-final-*.log.
Next: connect an actual Cocoa application host while continuing full engine
state/device integration. The goal remains active; no publication or commit.

## 2026-10-26 — MAC004 actual minimized Cocoa host

The 21-suite presentation matrix passed in Debug, Speed, Debug UBSan and Speed
UBSan (mac004-present-final-*.log). Added Framework MacWindow host and a native
Jam-derived host library. The host owns a real NSWindow/view/delegate/Metal layer,
physical resize dimensions, keyboard/modifier/mouse callbacks, event pumping and
engine-controlled close. Lifetime is safe across a callback releasing the host;
callback failures return at the C++ boundary. No original game startup or full
input/controller integration is claimed.

Targeted Cocoa/presentation checks pass in mac004-window-final.log, including six
engine presents from an actually minimized window. Tests wait for asynchronous
miniaturization and never call AppKit's activating finishLaunching. The new
mac_cocoa_window CTest is RUN_SERIAL and makes totals 20 base / 22 with shaders.
The full four-configuration matrix is running in mac004-window-matrix-*.log.
Next source gate: native C_Device/C_DeviceManager selection and actual shared
Device.cpp lifecycle, then full state/upper Display and C_Application linkage.
The active complete-game goal remains incomplete. No publication or commit.

## 2026-10-26 — MAC004 native engine device/manager and shared lifecycle

All 22 Cocoa-host suites passed in Debug/Speed and both UBSan configurations
(mac004-window-matrix-*.log). Added native C_Device/C_DeviceManager parameters,
real GPU selection/ownership, create/volatile/destroy stages and host attachment.
Device.h selects these types and the original shared Device.cpp now compiles.
Targeted Cocoa/presentation verification passes in mac004-engine-device.log,
including failed volatile setup cleanup and exact original global registration
and teardown. The native backend has eight files and four shared original files.
The latest 22-suite matrix runs into mac004-engine-device-matrix-*.log.

Next: complete the engine state/draw convenience API and texture/render-target
wrappers, then remaining shared Display2/upper Display and actual C_Application
startup. Native window and factory checks are platform infrastructure, not Blur
startup/menu/race. Full source/asset provenance constraints and later runtime
content requirements remain unchanged. No commit or public publication.

## 2026-10-26 — MAC004 native state records, scissor/fill and device forwarding

The engine-device lifecycle matrix passed all 22 suites in Debug/Speed and both
UBSan builds (mac004-engine-device-matrix-*.log). Added original-layout native
state records/getters/setters and engine C_Device state/draw forwarding. Actual
GPU output verifies scissor draw/clear intersection, render-target viewport/
scissor resets, wireframe/solid, colour masks, whole blend/alpha state and
anisotropic magnification. Stencil draw, point fill and nonzero depth bias are
explicitly rejected while their native implementation is pending. Zero/default
state records can be carried by the engine without fabricated GPU support.

Targeted engine-render/Cocoa tests pass in mac004-state-device.log; extended GPU
coverage is also recorded in mac004-state-draws.log. The latest full 22-suite
matrix runs in mac004-state-matrix-*.log. Next remains full engine compatibility,
render-texture/read-write/resource helpers, upper Display and C_Application/game
startup. Native platform windows are not a running Blur game. MAC004/goal active;
no commits, publication, license assignment or original-data request.

## 2026-10-26 — MAC004 PC read/write render targets and gamma compatibility

The state matrix passed all 22 suites in Debug/Speed and both UBSan configurations
(mac004-state-matrix-*.log). Added native render-target creation/destruction,
PC double read/write slots and Resolve switching, single-slot resolves and cube
face/mip targets. Source-selected native Display2 count is nine, with four shared
original sources. Fixed native tracking to use the current ReadWrite enum and
corrected gamma sampling/write policy against the actual PC view code.

Targeted engine GPU checks pass in mac004-render-targets-test.log; gamma checks
also pass in mac004-gamma-compat.log. They cover history/mip rendering, preserved
bound data after target destruction, resource naming, all cube faces/mips and
subsequent authored shader sampling. The full four-configuration 22-suite matrix
runs into mac004-render-texture-matrix-*.log. No game startup/race screenshot or
completion claim. Next: remaining original Display2 helpers/memory/interfaces,
full context compatibility, upper Display and C_Application/game integration.
Goal and MAC004 remain active; no commit, push, release or original-data request.

### MAC004 — tracked engine resources, original Utils/Memory and all shared Display2 sources

Native C_Device factories now register/destroy actual engine resource metadata;
render textures include their children. The original Utils implementation now
loads bounded DDS/BMP/TGA fixtures into actual Metal storage, clones via GPU
blits, and loads/refreshes native shader files. Fixed native pixel reload and
DoNotTrack paths. All seven shared Display2 Jam sources compile (Buffer is an
empty translation unit); twelve native implementation files compile beside them.
The original memory manager runs on native owned/borrowed host arenas. Regressions
reproduced and fixed native shutdown/counter state, stale skip-list links and a
system-tracker publication/address-reuse race. Windows implementations unchanged.

Verification: mac_display_utils passes actual image/mip GPU readback and clones,
red-to-green shader reload, malformed-file rejection and resource cleanup; arena
checks include 257 permuted allocations, full merge/exhaustion/recreation and
8,000 concurrent allocation/free cycles with the actual system tracker enabled.
The full sequential matrix passes 23/23 in Debug, Speed, Debug UBSan and Speed
UBSan; logs mac004-memory-matrix-{debug,speed,ubsan,speed-ubsan}.log under
_work/build/macos/. Prior helper-only matrix also passes all four configs in
mac004-utils-matrix-*.log. No ASan or successful LLDB run is claimed. The debugger
launch stalled, and the owned process tree was stopped; an OS crash report
provided the actual skip-list stack. git diff --check passes.

No actual game startup, playable race, save/load, LAN, retail scene or approval
screenshot yet. No public commit/push/release. Goal and MAC004 remain active.
Next: native input device bindings and original game-input mapping, full context
compatibility, upper Display and original C_Application/game integration. Retail
validation remains later, after the source-only integration available here.

### MAC004 — original action mapping receives native Cocoa keyboard/mouse input

Compiled/force-linked all six common Input Jam entries plus the Jam-selected
native event backend. The manager has four logical user slots; native keyboard
and mouse snapshots feed original device and C_GameInput interfaces. Cocoa
responder checks now verify actual key-down/up and click actions in the original
mapping layer. Events remain window-local, weak-lifetime callbacks; no global
input injection. Native hold/repeat uses original elapsed-time policy with the
monotonic engine clock. Relative mouse axes preserve original sensitivity,
time scaling and upward-positive Y. Disabled-device checks exposed and fixed
inverted enable state/getter semantics under PLATFORM_MACOS only. Controllers,
raw/captured mouse and IME remain pending.

Verification: full 24/24 suites pass sequentially in Debug, Speed and both UBSan
configurations (mac004-input-matrix-*.log). Latest upward-positive mouse direction
checks rebuild/retest only the two affected input/window suites in all four
configurations (mac004-input-direction-*.log). The earlier standard 23-suite
build and native bundle self-test passed in mac004-memory-standard.log; the
24-suite standard refresh follows this entry. No actual game startup/race or
completion screenshot. No commit/push/release. Goal and MAC004 remain active.
Next: native gamepads, complete unchecked context/upper Display interfaces and
original C_Application/game loop integration; audio, scenes and runtime data
validation remain outstanding.

### MAC004 — upper conversion bridge and lifetime-checked context views

Added Jam-selected original DisplayToDisplay2 compilation, preserved PC metadata
branches for native headers, and filled missing enum identifiers from the
original context declarations/Microsoft constants. Unsupported D24S8 GPU
allocation remains rejected and is checked explicitly. Immediate-context views
now expose original reference-return device getters and forward actual native
GPU operations. Copied views expire at device destruction and require explicit
rebind after recreation. Original helper shader rendering uses the views plus
upper state/primitive conversions.

Verification: targeted mac_display_utils/mac_metal_buffers pass in
mac004-context-view-test.log; full 24/24 suites pass sequentially in Debug,
Speed, Debug UBSan and Speed UBSan (mac004-context-view-*.log). The preceding
bridge-only full matrix also passes in mac004-display-bridge-*.log. Standard
24-suite build and bundle self-test passed in mac004-input-standard.log before
this context change; the standard refresh follows. git diff --check passes.
No game startup/race/save/LAN or completion screenshot. MAC004 and goal remain
active. Next: native controller binding, full upper device/context/frame
composition and original application/game loop, then remaining runtime systems.

### MAC004 — native GameController and handle-haptics implementation

Added a second Jam-selected native Input implementation for stable controller
slots, coherent Apple profile snapshots and actual CoreHaptics handle engines.
The original C_Pad360 API now runs on this backend; original deadzones and game
mapping are retained. Focus/disabled state suppresses actions and motor output.
Haptic reset/stop callbacks use safe per-generation atomic state; unsupported
actuators and errors do not report successful output.

Verification: actual authored Apple snapshot objects pass native slot and
original per-user action checks, including four local slots, reorder/fifth-pad
handling, disconnect/reassignment, analog values/deadzones, release and focus
loss. Snapshots without haptics reject nonzero motor output. mac_input,
mac_controllers and mac_cocoa_window pass targeted; full sequential 25/25 matrix
passes Debug, Speed and both UBSan configs in mac004-controllers-matrix-*.log.
The context milestone's standard 24-suite build and native bundle self-test
passed in mac004-context-standard.log; the standard 25-suite refresh follows.
git diff --check passes. No physical gamepad/haptics or LAN evidence, no actual
game startup/race, no completion screenshot. Goal and MAC004 remain active.
Next: complete upper Display device/context/frame composition and original
application/game loop. Native audio and remaining runtime systems remain
outstanding; runtime scene/race validation will later require game data.

### MAC004 — force-linked original upper CPU Display services

Compiled/force-linked ten original upper Display sources plus scoped native CPU
device binding. Original projections/frustum and renderer registration/lifecycle
run against real C_DeviceBase state; no upper GPU device or successful-rendering
stand-in is created. Native-only PCH removal/access binding and PC dynamic-aspect
selection preserve shared algorithms. Missing/duplicate/stale binding checks and
renderer partial-failure cleanup run in mac_display_base.

Verification: targeted mac_display_base passes. Full sequential 26/26 matrix
passes Debug, Speed and both UBSan configurations in
mac004-upper-base-matrix-*.log. Earlier controller milestone standard 25-suite
build and bundle self-test passed in mac004-controllers-standard.log; the
26-suite standard refresh follows this entry. git diff --check passes. No game
startup/race/save/LAN screenshot, physical gamepad/haptics verification, commit,
push or release. MAC004/goal active. Exact next step: native upper resource
selection and actual C_Device adapter over Display2, full context/frame/window
composition, then original C_Application/game loop and remaining audio/runtime
systems. Original game data will be needed for real scene/race validation later.

### MAC004 — upper resource types select actual Metal wrappers

Added native resource-header selection and the exact PC vertex terminator,
plus three original shared upper source entries (Texture, TextureParams and
empty VertexFormat). Thirteen original upper entries now compile/force-link.
Existing GPU checks use the upper shader/surface/declaration types and original
upper conversions through real native context views. Metadata declarations do
not imply runtime shader/constant-lock implementation.

Verification: targeted upper CPU/GPU checks pass in
mac004-upper-resources-test.log; sequential full 26/26 suites pass in all four
configs (mac004-upper-resources-matrix-*.log). The prior upper CPU standard
26-suite build and bundle self-test passed in mac004-upper-base-standard.log.
git diff --check passes. Goal and MAC004 remain active; next is the actual
upper C_Device adapter, resource/frame/window composition and original
application/game loop. No game startup/race/save/LAN, physical haptics evidence,
completion screenshot, commit, push or release.

### MAC004 — shared upper GPU device, stencil and layer publication race

Actual upper C_Device now creates native GPU resources/default framebuffers and
presents minimized Cocoa drawables. Fifteen original upper entries/two native
adapters compile and force-link. Opaque D24S8 surfaces map to native combined
depth/stencil (float32 depth on M4); GPU stencil/partial-clear checks pass.

Verification: earlier 27-suite upper-device matrix passed all four configs. New
28-suite matrix passes Debug/Speed and both UBSan configs in
mac004-stencil-matrix-*.log. Standard 28-suite build and native bundle self-test
pass in mac004-stencil-standard.log. Full suite found a real native layer worker
publication race; signalling after queue insertion fixes it. A control build
without that signal fails with the original three-second timeout; fixed fixture
passes sixteen repeats. See analysis and ignored framework-publication-control/.
No timeout expansion or Windows behavior changes. git diff --check passes.

Goal/MAC004 remain active. No actual game startup/race/save/LAN, completion
screenshot, physical controller proof, commit, push or release. Next: original
upper context registration/binding and state/draw paths, frame/window composition
and original application/game integration, followed by remaining audio/runtime
systems. Game data is needed for real scene/race validation later.

### MAC004 — original context registry/state/draw integration

Seventeen original upper Display entries and three native entries now compile
and force-link. Original DeviceContext registry and Context_Base conversions
drive actual GPU state, attachments, shader parameters and indexed/nonindexed
draws through native contexts. Cloning and expired-device checks pass. Unsupported
coverage/point expansion/deferred buffers report errors. Native per-slot sampler
anisotropy preserves upper sampler maxima.

Verification: targeted 4/4 passes; sequential full 29/29 suites pass Debug, Speed
and both UBSan configs (mac004-game-context-matrix-*.log). Standard 29-suite
refresh is running after the four-config pass (mac004-game-context-standard.log).
git diff --check passes. No actual game startup/race/save/LAN, screenshot,
physical controller proof, commit/push/release. Goal/MAC004 active. Next: original
dynamic vertex buffer and BeginVertices path, frame/windows/application/game
binding and remaining runtime/audio. Runtime scenes later need game data.

### MAC004 — original dynamic vertices drive real GPU geometry

Original DynamicVertexBuffer.cpp now compiles/force-links (18 original upper
entries/three native). Native Begin/EndVertices, original PC budgets and
double-buffer rotation drive actual GPU data; native locks provide safe
discard/no-overwrite reuse. Creation/map failure and mapped teardown are cleaned
up without silent resource/lock retention.

Verification: six-frame independent left/right GPU colour output plus actual
vertex attributes, nested/overflow/zero-stride rejection, budget/exhaustion,
fake-end and mapped volatile recreation pass. Sequential 29/29 passes in
Debug/Speed/both UBSan configs (mac004-dynamic-vertices-matrix-*.log). Standard
29-suite build and bundle self-test pass (mac004-dynamic-vertices-standard.log);
prior context standard also passed. git diff --check passes. MAC004/goal active.
No game startup/race/save/LAN/screenshot, physical haptics, commit/push/release.
Next: actual BasicRender/shader helper integration, frame/window composition,
original application/game loop and remaining runtime/audio.

### MAC004 — actual BasicRender/shader helpers and PC device blend state

Twenty-two original upper Display entries/four native entries now compile and
force-link. Actual BasicRender creates six source-built shaders and draws real
coloured/alpha/textured 2D and textured 3D geometry through native transient
resources. Native packed colours and original view/register/fallback semantics
are verified. Native device uses the original upper PC blend table, including
alpha factors and invalidating stale mode assumptions when other immediate
contexts change state. Failed shader creation cleans up and retries.

Verification: targeted 4/4 and sequential full 30/30 pass all four configs in
mac004-basic-render-matrix-*.log. Final exact PC fallback coordinate coverage
passes affected GPU check in all four rebuilt configs
(mac004-basic-render-fallback-*.log). Final standard build/all 30/native bundle
self-test pass (mac004-basic-render-standard.log). git diff --check passes.
Goal/MAC004 active; no actual game startup/race/save/LAN/screenshot, physical
haptics, commit/push/release. Next: opaque depth render textures and sampling,
actual upper window/frame composition, original application/game loop and
remaining runtime/audio systems. Game scene validation later needs game data.

### MAC004 — actual GPU-only depth render textures

D24S8 native 2D depth/stencil textures and render-texture aliases now allocate
real Metal storage, preserve nominal PC size metadata and reject CPU locks.
Native depth identity views also cover D16. Colour/volume/cube D24 combinations
that are not integrated reject.

Verification: actual GPU-cleared/rasterized depth sampling, mip selection,
read/write slots, GPU copy/preserved read data, D16 sampling and attachment
feedback rejection pass. Targeted 4/4 and sequential full 31/31 pass Debug/Speed
and both UBSan configs (mac004-depth-textures-matrix-*.log). Final standard
31-suite build and bundle self-test pass (mac004-depth-textures-standard.log).
git diff --check passes. Goal/MAC004 active; no game startup/race/save/LAN,
completion screenshot, physical haptics, commit/push/release. Next: original
Quad/frame drawing and window composition, original application/game loop and
remaining runtime/audio. Real scene validation later needs game data.

### MAC004 — original fullscreen/partial/split drawing

Twenty-six original upper Display entries/four native entries compile and
force-link. Original Quad geometry/default shader registries/declarations draw
real colour/texture/flip/split-viewport/partial-screen output; factories roll
back failed native shader loads. Original default/cull/depth selections feed
actual GPU/CPU viewport state.

Verification: targeted 5/5 passes. Speed UBSan found an uninitialized cull-flip
cache field; native constructor/default reset now calls the original base
InvalidateState, matching the required PC reset ordering. Pre-fix log retained.
Final sequential 32/32 passes all four configs in mac004-quad-matrix-*.log;
standard build/all 32/native bundle self-test pass (mac004-quad-standard.log).
git diff --check passes. No actual game startup/race/save/LAN/screenshot,
physical controller validation, commit/push/release. Goal/MAC004 active.
Next: original window resources/scene composition/masks, original application/
game loop and remaining runtime/audio. Real scene validation later needs data.

### MAC004 — real per-view resources and frame composition

Added native back-buffer/current-target ownership and original PC-style window
metadata/resources (26 original upper entries/five native). Actual original Quad
composes two views and stencil masks; matching offscreen surfaces use GPU blits,
while partial colour masks use shader drawing. Scene Reset/Unbind now follows
original PC ordering. Native derived device destruction cleans up before
clearing the singleton used by shared helpers. Volatile partial failures roll
back and support retry.

Verification: targeted 5/5 and sequential full 33/33 pass Debug/Speed/both UBSan
configs (mac004-game-windows-matrix-*.log). Standard build/all 33/native bundle
self-test pass (mac004-game-windows-standard.log). Checks cover actual split-view
colour/mask pixels, offscreen copy/channel preservation, render/screen sizing,
attachment identity, reset/rebinding, MSAA rejection/recovery and implicit native
owner destruction. git diff --check passes. Goal/MAC004 active; no actual game
startup/race/save/LAN/screenshot, physical haptics, commit/push/release. Next:
display-manager and original application integration, remaining runtime/audio
and game compilation. Actual scene validation later needs runtime data.

### MAC004 — original display-manager factories and native capability service

Original shared manager/base/window-metadata sources plus real native Metal/
primary-display capability service now compile/force-link (29 original upper
entries/six native). Factories load actual source-built shaders and manage
actual device ownership; failed loads roll back and retry. Only implemented
windowed/no-MSAA formats are advertised; modes use physical pixels and real
refresh values. Native equal-mode comparison is strict for sorting.

Verification: targeted 4/4 and sequential full 34/34 pass Debug/Speed/both UBSan
configs (mac004-game-manager-matrix-*.log). Standard build/all 34/native bundle
self-test pass (mac004-game-manager-standard.log). Checks include real adapter/
mode metadata, failed creation/retry, actual GPU drawing, owned/indirect/group
cleanup and adapter restart. git diff --check passes. Goal/MAC004 active; no
actual game startup/race/save/LAN/screenshot, physical controller proof or
commit/push/release. Next: original framework CoreSystems/application startup,
remaining runtime/audio and game compilation. Actual scene validation later
needs runtime data.

### MAC004 — actual framework CoreSystems startup/update/shutdown

Seven original framework entries now compile/force-link. Original CoreSystems
starts actual I/O/resource/debug services; native startup validates the root,
checks I/O startup, rolls back partial ownership and reports initialized only
after success. Native cleanup preserves borrowed I/O and retires owned services
before singleton destruction. Original framework/rendering application header
passes a private native syntax probe; application bodies remain pending.

Verification: owned/borrowed services run in separate processes, reading actual
authored payload bytes through workers and updating actual service connections.
Invalid root/retry, duplicate initialization and clean/idempotent shutdown pass.
Targeted 3/3 and full sequential 35/35 pass all four configs
(mac004-core-systems-matrix-*.log). Standard build/all 35/native bundle self-test
pass (mac004-core-systems-standard.log). git diff --check passes. Goal/MAC004
active; no actual game startup/race/save/LAN/screenshot, physical haptics,
commit/push/release. Next: original application body/render/camera dependencies,
remaining native runtime/audio and actual game compilation. Data is required
for real scene validation later, not for the source work still available.

### MAC004 — original application compilation and complete camera archive

Original ApplicationBase now follows PC view/render/input paths on Darwin with
explicit dependencies and correct native bool-to-HRESULT failures. Sixteen
original framework sources compile (seven force-linked services; nine separate
application archive entries). Nine original render support sources compile.
All 34 original camera sources compile and force-link; native capture owns an
actual completed GPU snapshot. Native upper renderer ownership/idle wait/view
setup/presentation interval behavior now runs, including CommonRender timing
updates within a scene. Windows branches remain selected only on Windows.

Verification: original animation/camera interpolation at nine fractions within
one fixed simulation tick, unchanged simulation state, debug feed, exact GPU
capture bytes, rejected unsubmitted/nested captures and snapshot survival after
GPU owner destruction pass. Ownership recursion/timeout/wrong-thread release/
transfer, host dimension defaults, real CAMetalLayer vsync, implicit/explicit
view setup and actual six minimized presents pass. Sequential full 36/36 all
four configs pass in mac004-camera-matrix-*.log. Application archives compile
all four in mac004-application-archive*.log. Standard build/all 36/bundle Metal
self-test pass in mac004-camera-standard.log. A real private application link
probe records remaining dependencies in mac004-application-link-probe.log.
Goal/MAC004 remain active; no running game, game screenshot, race/save/LAN,
physical haptics, commit/push/release. Next: original Display debug/font/timing/
resolution and Render view/primitive dependencies, shader/texture-stream linking,
then native application host and actual engine/game startup. Runtime assets
remain unnecessary for this source work.

### MAC004 — PC texture resources, linked application body and SDL3 reuse

Native PC TextureDesc/face/mip upload now uses actual tracked Metal textures.
Original UploadTexture2D/container loading preserves tight PC rows across native
staging; upper standard-image calls reuse the previously verified native decoder.
Eighteen more original Display helpers and eleven Render sources compile.
ApplicationBase links/runs in an instance-only private probe; actual native host,
startup and game remain pending. Per-scope GPU timing is marked unavailable.

Following the user's open-source reuse instruction, SDL3 3.4.18 is fetched from
the official release with a pinned checksum and retained notices. Native PCM
output uses its real CoreAudio backend/queue/conversion/ownership APIs; the
original mixer/DSP/decoders remain pending. No hardware output backend is rebuilt.

Verification: actual PC 2D/cube/all-mip/BC1-3/RGBA32F GPU bytes, prefix streaming,
short-read nonmutation/whole-mip boundaries and full-load rollback pass. Original
texture params/container plus reused DDS/BMP/TGA origin/offset/stride/truncation
checks pass. Real silent PCM is consumed by CoreAudio at three game rates with
stereo/surround input; pause/clear/restart/borrowed SDL ownership pass. Full
sequential 37/37, 38/38 and 39/39 pass all four configs; final logs are
mac004-sdl-audio-matrix-*.log. Standard build/all 39/native bundle self-test pass
(mac004-sdl-audio-standard.log). Shader compiler/693 inputs are unchanged.
git diff --check passes. No game startup/menu/race/save/LAN, game screenshot,
physical haptics, commit/push/release. Goal/MAC004 stay active. Next: actual native
application host/transactional startup/render loop, original audio mixer/DSP
integration through SDL output, remaining engine/game compilation. Use existing
open-source components where they solve a real missing capability; retain original
PC data/simulation/wire mechanisms for compatibility. Assets are still unnecessary
for these source tasks.

### MAC004 — native application runs original framework rendering

The actual Framework::C_Application now selects a native Cocoa host and invokes
original ApplicationBase startup, user callbacks, CommonUpdate/CommonRender,
original renderer services and Metal presentation. A focused set of 29 actual
Jam shaders is built through the existing compiler using its new optional
--symbols selector; the full 693-program translation mechanism is unchanged.
The original PC accumulated fixed-step cadence (1/60 s, at most four ticks per
update) remains separate from unlocked rendering. Camera render interpolation
runs; optional world interpolation callbacks are present but are not yet bound
to the actual WorldRender game layer.

Initialization propagates real renderer failures and rolls back partial owners.
Native application retry restarts the original joined layer worker and reuses
the capture service within the same core-services owner. This does not establish
whole-game/core-services restart. Native physical-pixel resize recreates per-view
framebuffers/renderers. Shutdown during a render callback defers teardown until
the loop unwinds. Callback exceptions release render ownership and retire any
recording scene before returning failure.

mac_application uses the actual framework, minimized Cocoa window, renderer
services and source-built shaders. It checks missing core services, invalid
window/retry, failed user initialization, missing shader rollback, duplicate
initialization, eight presents with exact GPU color, one/four/two split views,
physical resize, balanced tick/interpolation callbacks, repeat initialization,
deferred shutdown and a real rendering exception. Separate original layer tests
verify a restarted worker performs actual asynchronous create/destroy work.
Sequential 40/40 pass Debug, Speed and both UBSan configurations in
mac004-application-matrix-*.log. Standard build/all 40/bundle Metal self-test pass
in mac004-application-standard.log. Earlier failure logs are retained. This
supersedes the instance-only application probe; it remains an authored engine
fixture, not Amax, a menu or a playable race. Original game/WorldRender/audio
mixer integration and runtime-data scene/save/LAN verification remain pending.

### MAC004 — real audio integration, world interpolation and further open-source reuse

Original audio archive now force-links and actual original renderer buses/shared
PC mix feed SDL/CoreAudio. Worker/counter/quality/ownership/shutdown checks pass;
full 41/41 all four configs and standard build/bundle self-test passed in
mac004-audio-core-matrix-*.log and mac004-audio-core-standard.log. UBSan-exposed
Xt base-layout/static debug registration faults were fixed; prior failures kept.

Next changes connect the actual original world interpolation implementation to
the native application and replace seven old bundled SoundTouchZ sources with
public pinned SoundTouch 2.4.0. Native audio selection is now 174 original sources
plus output/engine adapters. Eleven original world bodies plus a native wrapper
compile; full scene/FX dependencies and cubemap handle remain pending. Focused
audio, world and application checks pass, including offline stereo pitch/bounds,
actual default world callbacks and matrix/skin/hook behavior. Full 42/42 suites pass in all four configurations
(mac004-world-soundtouch-matrix-*.log). Standard build/all 42/native bundle Metal self-test
also pass (mac004-world-soundtouch-standard.log). git diff --check and Python
syntax checks pass.
No game menu/race/save/LAN, game screenshot, original banks, commit/push/release.
Continue remaining actual RenderingData/SceneGraph/FX and game compilation after
verification. Runtime data is not required for this source work.


### 2026-10-26 — Expanded original engine and parallel native ports (ongoing)

User explicitly requested open-source reuse and coding subagents. MAC007–014
own disjoint physics, dynamic/crowd, actor, UI/Lua, native query/photo, Movie and
Amax startup work. Root integrates shared ABI/compiler fixes and Jam/CMake.
MojoShader pinned ad5dff84830c2863c841f4b1f4e3df78c705b383 now feeds the actual
Display2 Metal shader loader (SM1–3 float registers/semantics/samplers); original
FX file loading exercises it through actual GPU output. Unsupported register
classes remain explicit errors. Original RenderingData24, SceneGraph32, FX19,
full Render87, World13 including native interpolation, Gravity116, Racing42,
Dynamic85, Crowd30 and actor/model/resource modules compile in the integrated
Debug build. This is compilation/component evidence, not a running game.

Expanded 50-suite run:49 pass; mac_scene times out after successful assertions
because the original persistent stream worker has no exit path. Native
cooperative drain/stop/join now added before scheduler teardown; verification
is ongoing. Log: mac004-expanded-engine-test.log. Earlier scene fixture lacked
Scheduler group pool setup; it now uses actual Init/SetGroupPoolSize/Start.

Public GNU libunistring formatting plus Foundation UTF conversions replace
Win32 wide formatting while preserving16-bit WCHAR/serialized text width.
ICU was evaluated, but its unsupported unsigned64 formatting and installed
minimum deployment version made it unsuitable here. Full mixed UTF8/UTF16,
surrogate, numeric64, truncation and locale preservation fixtures are running.
FFmpeg native Movie work uses actual engine file streams and SDL audio. No
original game assets have been acquired or needed for these source changes.

Next: integrate UI/Lua/save/Movie/Amax closures, rerun Debug and the four-config
matrix, and continue actual game linkage. No menu/race/savegame/LAN validation,
public release, commit or push is claimed.


### 2026-10-26 — Native UI, scripts, save and Movie integration

Full integrated Debug build includes original UI99 plus NativePaths, actual
Lua5.1.4 interpreter26/shared3/helper1 and Amax141 startup/application sources.
Module-private PCH roots now precede Jam includes: UI previously accidentally
selected Amax/game/stdafx.h. Old Lua/current headers now resolve to the actual
Jam-selected interpreter's source directory, preventing mixed private Lua ABI.

All55 integrated Debug suites pass (mac004-original-modules-test.log), including
scene worker shutdown, UTF16/Amax formatting, original compressed UI script PAK
callbacks, actual async UI file saves and FFmpeg native Movie decoding/GPU
sampling/PCM queues. These authored component fixtures do not establish game
menu, actual savegame, race or LAN behavior. Source changes exposed first-save
and stale Lua error-stack issues; native-only fixes are tested by fixtures.

MAC015/016/017 now cover game-specific UI, MRT transitions and GameLogic229.
MAC018 compiles remaining actual vehicle/shared actor/debug/effect utilities.
Integrated four-configuration verification remains pending these changes.
Dependency decisions and current macOS26 binary minimum are documented in
dependencies.md. No game data acquisition, commit, push or release.


### 2026-10-26 — Original game archives and next native runtime gaps

GameLogic229 and Amax UI59 source probes pass; integrated source build now
includes these and the original GameAudio19, RaceBook7, Kanzen4, vehicle, actor,
effect and debug/support archives. Abstract/nondefault Xt reflection now uses an
explicit existing concrete layout-probe type; it neither fabricates a gameplay
constructor nor casts a fake pointer. Authored loader fixture covers a nonzero
base offset before main under UBSan. Remaining abstract creator declarations
are being integrated with appropriate concrete probes.

NativeMain.mm and an excluded actual game bundle/link target now delegate to
AmaxApplicationMain using a supplied data-root. This target is not delivered or
boot-verified. Full application linking still depends on active network/audio
and native graphics integration.

Native maintained libjpeg ABI/channel conversion replaces the old binary JPEG
dependency; an original codec fixture is added. Current native dependencies
require the installed macOS26 runtime; portable packaging remains pending.
Native multiple-render-target and MSAA GPU tests are authored separately from
game execution. Nonzero UNORM depth bias remains explicit unsupported where
exact mapping is unverified. Full Audio::System/bank checks exposed original
queue sizing/locking/alignment and streaming-worker shutdown issues beyond the
earlier renderer fixture; fixes and runtime matrices are in progress.

Native networking reuses285 original Unix vendor sources and maintained
crypto/curl. Parent/agent verified a real MSVC/ARM64 raw-profile packet layout
difference; native reserved padding/assertions are being added before claiming
wire compatibility. Synthetic socket peers do not prove a real game lobby,
fourth-player joining, or Windows interoperability.

Next: finish all archives, link actual game, run the integrated regression
matrix and bootstrap delivery check, then actual startup/race/save/LAN as data
becomes necessary. No original asset download, public release, commit or push.

### 2026-10-26 — Actual game links, complete private data and real startup

Debug and optimized BlurNativeGame link the original Amax application and full
game modules, not a replacement demo. NativeMain delegates to
AmaxApplicationMain. All693 native built-in shader resources are supplied in
the bundle. The standard native build now includes this actual application.
The Windows generated source file was regenerated through the Jam tooling;
new --declare-generated-shaders supports declaration without Windows fxc.

The authorized runtime acquisition completed privately:117 original members,
7,806,764,660 bytes, independently reread for CRC32/SHA256. Ignored provenance
and final audit reside in _work/runtime/research. No original game executable,
DLL or installer was executed or added to tracked files. The supplied archive
has no optional cache/models.pak. Original assets remain outside the app.

Real startup exposed and fixed native SHA1 no-op authentication, original
exporter absent compressed mip tails, empty Movie volatile startup, original
PC FX preload pointer initialization and game automation subscriber ownership.
MAC030 independently verifies original RSA-PSS signatures and secure game/boot
PAK mounts. MAC029 compares66 real UI GPU texture payloads. MAC026 plays both
actual boot BIKs. These are resource/component checks, not a playable race.

The actual original game now mounts root/shaders/root_streaming/gamedata,
completes boot global resource caching (~21s) and constructs C_UiManager.
Latest bounded run (_work/runtime/verification/game-startup-diagnostics.log)
reports `Native presentation interval is unsupported`, then asserts because
application shutdown asks to destroy async UI/audio layers before they run.
MAC036 graphics mapping and MAC037 creator quiescence own these next fixes.
Native --capture/--quit-after diagnostics are integrated, but no actual game
frame has yet been captured. Do not present fixture PNGs as gameplay evidence.

The first integrated69-suite run had67 successes and two port6317 conflicts
from an overlapping owned game process. Both failed suites passed on a
separate rerun. New auth/frame/automation fixtures require a fresh sequential
matrix. Actual Debug/Speed links succeed; full-game UBSan retains the absent
C_PortalBlockerCreate key-function/RTTI source gap. Component UBSan builds keep
all checks and explicitly exclude the actual Game target. No vptr suppression
or fabricated component definition was added. No public release, commit or push.

### 2026-10-26 — Actual rendering path advances; native automation and allocator checks

MAC034/MAC039 original Automation service/client fixture passes all four
configurations, including clean disconnect with another in-process Artery owner.
Client now waits for callback completion before freeing its context; no shared
Artery shutdown patch was needed. MAC035 native Lua Tap/RaceState helpers pass
all four configurations through actual keyboard/action mappings and elapsed
150ms hold, including minimized input and physical held-key ownership.

The pre-MAC036 optimized build passes72/72 integrated checks. Both UBSan
component runs pass70/71, exposing an actual misaligned intrusive light-pool
link (size12/align4 value versus native align8 pointer). MAC040 changes only
native allocator slots/block accounting to satisfy both alignments; actual
value/disk layouts are unchanged. Its focused four-configuration matrix passes,
including deliberately skew byte allocation, over-aligned objects, growth,
reuse and lifetimes. Parent scene/header-dependent rebuild is pending.

MAC036 implements genuine drawable minimum-duration scheduling for multi-refresh
presentation, using the actual screen refresh interval and original enum
mapping. Genuine Metal drawable fixtures pass Debug/UBSan with validation.
A private snapshot of the completed actual-game binary (before the later pool
ABI change) passes the previous presentation failure and reaches original UI
OnCreateVolatile. It then LOGFATALs because UiRenderBuffer selected console
D24FS8 under its PC-only format guard. MAC043 restores that original PC branch
for native PC-data rendering without changing samples or substituting a format.
Actual stack/status/log are _work/runtime/verification/mac036-*; owned process
group was cleaned and slot released. No screenshot, input, menu or race success
is claimed. Automation greeting connected but commands could not complete while
the game stalled in creation, so no blind keys were sent.

MAC041 manual native launch now chooses/validates/remembers an existing game
folder through Cocoa; explicit CLI remains noninteractive. Manual visibility
uses a narrow native game-launch flag; automated explicit CLI stays minimized.
Four syntax probes pass, while full integration/manual GUI remain pending.
Root's manual-hook include was initially placed before platform identity was
defined; moved after StdAfx before retrying the actual build. The final running
product must contain all pool/UI/platform fixes, not the private probe snapshot.

### 2026-10-26 — PC UI route integrated and actual model/audio blockers

MAC044 restores nine exact original PC UI selections in six source files:
postprocess D24S8, frontend/in-game None samples, PC script platform, PC
localized text exports/fallbacks, and linked Simple2d six-vertex TriangleList
paths.24 syntax checks pass. Dormant depth-texture code was left unchanged.

MAC046 native light handles now check independent runtime lifetime ownership
before reading an object whose vptr has been overwritten by the pool free
link. Same-address salt0 reuse cannot revive old handles; light copies receive
fresh ownership, while assignment preserves destination identity. All four
focused matrices pass. The integrated latest Debug UBSan scene, handle, pool
and original UI render-buffer suites pass4/4 (mac046-integrated-ubsan-tests.log).
No serialized value types or nonnative pool/handle behavior was changed.

MAC047 actual minimized game with these fixes reaches further initialization,
then reports205 PC resources rejected as wrong platform and a spectral-reverb
48000 assertion. GetPlatform had no native return path at all; MAC049 restores
explicit PC asset identity without bypassing the resource validator. The
actual original PC mixer is24000Hz/128samples independently of output quality
(documented retail T144); MAC048 preserves that cadence and selects original
PC dry spectral-reverb processing instead of console FFT bodies. Native wet
spectral reverb remains unavailable as in the current original PC processing
path; no replacement effect/rate was invented. Source fixes await parent run.

The original ui.bin additionally contains11 UITextBox objects across seven
screens. MAC045 confirms their implementation/schema is genuinely absent from
this snapshot; importer nulls these unknown objects but loads the other UI.
Affected screens include account login/register, DLC, friends/social and
TheCustomPopup. No missing Jam member/class was found, and no fabricated
replacement was inserted. Main menu/race impact remains to be observed.
No actual game frame/race/save/LAN success yet; all owned probe processes
were cleaned up and native service queue slot released.

### 2026-10-26 — Exact shared-depth contract and original UI query

MAC053 exposes only the existing portable Amax.GetScreenResolution body and
registration to native; original PC-only settings functions remain separate.
MAC055 confirms the actual GUIBank3283 nil-function error is gone. The next
native failure is precisely1280x720 A8R8G8B8 colour with2560x1440 D24S8 depth,
both single-sampled. This is valid original D3D9 larger-depth reuse; strict
native equality was narrower than the original API. MAC052 bounds Metal render
area to colour dimensions, retains format/sample/device/bounds checks, and
uses scissored GPU depth/stencil clearing to preserve the unused shared region.
Private Debug Metal Validation covers D16/D24S8, None/2x/4x, exact pixels/query
counts and rejection of smaller/mismatched attachments. Parent rebuild/run
pending. All owned MAC055 processes were cleaned; no frame captured yet.

The latest integrated Debug UBSan audio-core suite passes after MAC048
(mac048-integrated-ubsan-audio-tests.log). Native unknown UI widget recovery
limits are now described in ui-snapshot-gaps.md with affected screens and actual
importer behavior, without including proprietary extracted bytes.

### 2026-10-26 — Actual UI sentinel and postprocess allocation integration

MAC058 reflection-based constants pass integrated real source UI GPU shader
validation (mac058-integrated-uniform-validation.log). Native Tap/Hold uses
elapsed time and original keyboard/action paths; all four focused matrices pass
including independent throttle/steering keys, finite bounds, conflicts,
physical ownership and teardown. No OS injection or game-state substitution.

MAC061 restores original PC blend Unknown cache invalidation and Default state
semantics, which original Simple2D/DepthOfField explicitly request. Count and
unsupported states remain rejected with numeric diagnostics. Real device and
shared-context GPU state/pixel tests pass Debug/UBSan with Metal Validation.

MAC062 progresses beyond these errors, loads the actual showroom stream,
then ReducedRender/PostProcess volatile creation fails and a null bloom
receiver crashes. Actual crash report shows x0=0 at SetFeedbackFade. MAC063
finds two original-PC contract differences: Reduced manager selected console
D24FS8; PostProcess passes null address plus a nonzero computed allocation
budget, while native wrongly treated size alone as an external-memory request.
Native now selects PC D24S8 and allocates real owned Metal storage for a null
address regardless of that informational budget, retaining rejection of actual
external-memory aliasing. Detailed native allocation errors are added.
Parent actual rebuild/run pending. No captured gameplay/menu/race yet.

MAC064 actual startup reaches UI/showroom loading but fails on vector index4369/size200; secondary EndScene exception obscures it. MAC065 bounded LLDB captures the primary before unwind: Simple2D::Render at1498 indexes the pre-transform matrix pool with a sprite index0x1111. MAC066 private reproduction proves the existing Windows64 full-size sprite clearing branch is also needed for native136-byte LP64 sprites; the console aligned128-byte branch leaves exact0x1111 indices and can touch adjacent records. Owner assigned the narrow guard extension and adjacent-sprite regression. A second actual debugger run exposes Default texture-address requests in deferred lighting; MAC067 owns matching the existing PC contract. No completed actual frame/race/save/LAN evidence yet. MAC068 audits the original automation/launch flow while these disjoint coding tasks proceed.

MAC066 full136-byte native sprite clear passes four private configurations and root Debug adjacent-object regression. MAC067 Default sampler UVW and filter triples now restore source-defined Wrap/Wrap/Wrap and Linear/Linear/None transactionally; private Debug/UBSan Metal Validation and root Debug pass. MAC069 actual game advances beyond both but stops on a pixel slot0 unbound2D texture. MAC072 before-unwind __cxa_throw stack proves original motion-blur downsample, not a generic metadata hypothesis. Native had skipped originalPC SetViewport/chain publication and selected console stencil/depth setup. MotionBlur and GameWorld restore the existing PC paths; ApplicationBase already correctly publishes the real window mask and is unchanged. Actual next game build pending. Debugger-wide require conditional was too expensive and aborted; bounded throw breakpoint captured exact primary without per-call overhead. One immediate retry hit original Artery startup failure, revealing base startup rollback dispatched derived GameSystems::Shutdown before resource manager creation; MAC073 qualifies base-owned rollback, with root actual occupied-port/borrowed-worker/retry regression passing.
MAC070 compressed-profile format selects PC SAVE/BLURSAVE and little-endian framing on native, and matches two new[] buffers with delete[]. This is source/format-boundary work, not career-save proof. Its UBSan envelope test exposed an actual packed U32 store at byte1 in BinarySerialiseAutoEndian. MAC075 replaces typed unaligned accesses and aliasing bitcasts with exact-size memcpy, preserving both endian modes and wire bytes; independent packed-byte regression added to filesystem fixture. MAC074 restores originalPC Options keyboard remap, steering-only PC axis calculation and dedicated forward/back fire buttons on native; source syntax all4 configs passed. No completed actual frame/race/save/LAN yet.

MAC076 actual startup passes the missing texture point but fails the same motion-blur draw with pixel shader null, while actual colour/vertex/declaration are valid (bounded before-unwind MAC076 LLDB). No completed diagnostic capture, no menu inputs sent. After caught failure the app also crashes on return in BootLayer::Destroy because OnShutdown cleared SystemLayer while deferred layer destruction still needs it; MAC079 assigned native dependency-aware teardown. MAC078 ReplayDirector source/runtime ownership is separately fixed: source12configuration probes and actual full-type Debug/Speed lifecycle tests pass; full-game UBSan fixture correctly remains blocked by known absent PortalBlocker RTTI. MAC080 probes actual effect shader initialization before unwinding. Native DDS loader audit finds original ghost_mask.dds is luminance8, currently rejected, and PostProcess OnCreate hides bCreated false while short-circuiting later creation. MAC072 owner is implementing exact luminance format loading and truthful graph failure propagation. No substitute textures or game states. Root integrated save-envelope/options2tests and packed auto-endian filesystem test pass. Expanded UBSan build succeeds; sequential runtime matrix pending while actual debugger owns service slot.

MAC081 actual app still rejects original ghost DDS despite format support. It then crashes in async world debug initialization because AmaxWorldLayer ignored world Create(false); this is separately being corrected. Real-file GPU fixture reveals stale pitch512 for a64x64L8 DDS whose exact file payload is4096 tight bytes. The original PC loader derives row size from format and ignores pitch. Native now accepts this precise tight complete chain and rejects ambiguous/truncated input. Actual all4096 GPU bytes and LLL1 sampled pixels pass private Debug/Metal Validation; UBSan pending. MAC079 native targeted synchronous layer retirement and RaceBook child teardown source12configuration probes pass; actual full-app teardown remains next-runtime verification. Root actual full-type replay lifecycle CTest passes. MAC082 confirms game transport already probes100 ports from3074 and derives real common-address identity; four local games need distinct Artery bindings and configurable discovery bind/destination, with profile/save/data roots isolated. MAC083 implements explicit native settings preserving defaults and unchanged wire/security; this does not yet prove four lobby players.

MAC079 source and actual LayerManager lifetime/cancellation fixture pass all4 configurations. Native shutdown now joins the creator before targeted synchronous retirement, keeps app/RaceBook borrowed pointers live during each real callback, retires RaceLogic before world then UI/RaceBook/audio/debug/system, and prevents reentrant world/race creation after shutdown begins. Actual Boot/world/UI cleanup and a clean full-app exit still require the next run; the fixture does not stand in for them.

MAC083 strict per-instance Artery TCP/UDP and discovery bind/destination settings source12probes and all4 focused actual service/discovery runtime variants pass. Four distinct real framed Artery services and one original host/three ephemeral clients exchange unchanged nonce/security/common-address bytes. Defaults unchanged, invalid settings/occupied binds fail; NetRace now reports discovery initialization failure. This is not joined-game lobby evidence. Root optimized build was interrupted incrementally to prioritize the actual Debug app; root Debug Game, replay lifecycle and layer shutdown targets now link successfully with MAC079/85, tight-pitch L8 and MAC083. MAC084 owns the fresh minimized actual probe; root holds shared DebugMap archives for any required debugger evidence. MAC086 native failed-layer no-promotion/error-handoff implementation is proceeding separately after the frozen game build.

User explicitly re-scoped immediate work to a grouped assumptions/init-graph audit and one playable slice before expanding menus/save/LAN/distribution. MAC088/89/90 split the audit across PC format versus OS and LP64/ownership, actual startup/vehicle failure propagation, and complete real renderer/effect graph. MAC087 confirms HDR luminance1x1R32F read/write alias. Existing original PC DX11 compatibility backend snapshots active colour targets before sampling, so Metal needs that whole ownership contract rather than guard suppression. Source audit finds original Ui.NewLaunchMode Debug directly initializes local debug profile and DebugScreen, while DebugRb passes StartScreen/showroom. Private root now uses supported Debug entry, NumCars1, Brighton1/BMW/controller0/1lap; original DebugCarScreen permits1 and RaceLogic has a dedicated single-car spawn branch. No multiplayer AllowSingleRacer or race-state substitution. Next real probe uses only observed DebugScreen normal Return to launch the actual configured race.

MAC093 grouped ownership fix: actual original-PAK GameWorld postprocess fixture
completed fourteen temporal HDR/Bloom GPU frames and explicit graph/device
cleanup, but failed static exit. LLDB proves allocator destructor precedes native
named-registry weak-control-block free. All registry allocation classes now use
the platform libc lifetime domain and are normally released; unnamed objects
avoid unused registries. Actual original allocator/static-lifetime + existing MT
behavior regressions pass Debug/Speed/DebugUBSan/SpeedUBSan (8/8). Private
previous-implementation differential fails retained-block check then SIGABRT.
Logs `_work/runtime/verification/mac093-static.log`,
`_work/build/mac093-registry-tests.log`, `mac093-registry-matrix.log`,
`mac093-before-result.log`. Full graph exit and game slice remain pending.

MAC090+93 actual original root.pak complete GameWorldPostProcess graph now
executes fourteen HDR/Bloom frames and exits0 under strict Metal validation,
including graph/device/PAK/static destruction. Private log
`_work/build/macos/occlusion-agent-graph/validation.log`. Disabled original PC
prepass remains the explicit fixture limit; motion/DoF execution awaits actual
game prepass. Root integrated resource/FX shutdown fixtures pass2/2; original
allocator + MT regression passes all4 configurations8/8. Newly expanded colour
snapshot fixture exposed its own upper BeginScene bindingreset and correctfully
failed; graphics corrected fixture to sample outside-scene snapshot with actual
lower command path and explicit owners, verificationpending. No gameframeclaim.
MAC094 fullDebug gameclosure rebuild underway; agents hold productionWorld/view
and lowerRenderer transaction edits until root snapshots this coherent build.

MAC094 actual fullDebug app built all1033changed objects, source-frozen snapshot
`verification/mac094-probe.app`, strict Metal minimized run. Actual DebugScreen
renders~60fps; long root inspection command exceeded original128byte UI message
capacity and did not execute. Shortqueries correctlyobserveDebugScreen/None and
StreetRace enum1; existingAmax.SetupRace selectsStreetRace, normalReturn attempts
LoadingGame. Primary then shader708 pixel0unbound, after route-not-found and nil
LoadTextureClone warnings. MAC096 same frozenapp captures real completed GPU
DebugScreen2560x1440 to`verification/mac096-debug-frame.png`; root visually
inspected. ActualLuaqueries prove configuredcity/route valid, but levelRoute and
GameData.GetRoutes IDs positive4112414174 vs UIselector/CurrentRoute negative
-182553122. This is the original PC32 integer-bitpattern contract crossing
native Lua ptrdiff64 and signedUIitems, not missingarchives. MAC097 owns grouped
producer/consumer numericID restoration withoutpointertruncation/scriptbypass.
Both actualruns still hit retained-GPU-list assert onexit despite cache drains;
MAC098 boundedLLDB isolates menu-only owner lifetime. No actualtrack/car/finish
claim. MAC099 client now rejects>=128byteLua command beforedispatch, failed
nativeUIcommand constructor initializesempty ratherthanexposinguninitialized
bytes; privateharnessshortenedstatequeries andvalidatesbytelength. Rootautomation
buildpass, runtimefixturequeuedafterowneddebugger.


## 2026-10-27 — MAC091 full World transaction and bounded failure verification

Native World creation now owns its actual loading-worker pauses and resumes on
bool failure and exceptions. It records acquired services/publications, flushes
world work, detaches the actual message forwarders, and retires actors before
their visibility/shadow/physics services. Shadow owners retire before prepass
and scene owners. The DynamicRenderer remains the sole owner of its
Light/Particle/Track managers. Native ViewManager publishes nullable child slots
before their Create callbacks; Main/Mirror/CubeMap/View cleanup clears actual
owners for partial and repeated Destroy. Original nonnative flow and timing
policy remain unchanged. All six actual source objects compile in all four
configurations (24 probes); the final World recheck passes all four. Scoped
diff whitespace validation passes.

The full-type Debug fixture `tools/macos/world_transaction_test.cpp` uses the
actual root.pak, source-built shaders, original complete postprocess graph,
original Resource loading worker and real GPU resources. Poisoned storage
verifies never-created World/Main/Mirror/CubeMap ownership; original
ViewManager also tolerates never-created repeated cleanup. Authored postprocess
Create callbacks allocate a real GPU payload then return false or throw. Both
paths retire it once, retain no later World owners or GPU delta, and admit a
new typed worker load after rollback. Final Debug process exits0; log
`_work/build/macos/world-agent/fixture-bootstrap-first.log`.

The passing link explicitly prepends the freshly compiled original
`blur_mac_amax.dir/code/amax/game/Main.cpp.o`, then fixture/revised World/View
objects, generated original Lua and the unchanged actual-game library closure.
Without that explicit native bootstrap order, the same assertions pass but
static RayGroup vector destruction aborts after the small-block allocator.
Mach-O init_offsets proves RayGroup constructor index82 versus Main
PreStatic101 index101 and allocator102 index102: init_priority is locally
emitted but does not sort extracted translation units globally. MAC102 owns
the production link/lifetime resolution; no RayGroup workaround was applied.
Full UBSan fixture linkage still encounters stale resource-drain/absent
PortalBlocker RTTI closure. These checks prove the pre-physics failure boundary,
not every later acquisition failure, actual racing or a completed gameplay
slice. Production World/View source is frozen for root integration.


## MAC102/MAC104/MAC108 — coherent startup integration, 2026-10-27

The actual frozen MAC104 Debug process traversed original Brighton level,
physics/render/stream files and UK_Brighton_1 AIR data through the normal debug
menu Return handler. At41.38s it crashed inside BMW DamageLayer::CreateVolume:
the native single-buffer branch had never assigned its required volume pointer.
This is track/car startup evidence, not a playable frame or a completed race.
The vehicle owner is correcting the whole damage volume/pool/model acquisition
class against the original PC material representation (MAC105).

The World transaction fixture exposed a separate cross-translation-unit static
lifetime failure: native init_priority orders constructors within an object,
but Mach-O archive extraction did not globally put Main's allocator bootstrap
before retained RayGroup vector owners. CMake now force-loads a dedicated
archive of the original Main.cpp first in the actual game's library closure;
all actual-game fixtures inherit that same closure. All four bootstrap library
builds passed. The existing actual allocator fixture now also holds a real
small-block allocation in a default-priority static owner until destruction;
all four configurations pass. Actual coherent World/game static exit still
requires the new integrated run. No RayGroup-specific capacity-clearing fix or
allocator lifetime leak was introduced.

MAC100's focused actual UiLayer callback fixture passed Debug and Speed with
real Sparkle GPU buffers acquired/released through two owner lifetimes. The
required UiManager initialization API is called before Bloom creation; native
message dispatch now scopes both the device lease and memory tracker across
required-resource exceptions. MAC099 command capacity and actual allocator
registry fixtures passed the remaining three configurations (six checks).

MAC108 builds the actual Debug game plus World, original-archive graph, UI
required-resource, Simple2D, damage, PC32 Lua-ID and real owner fixtures together.
The private original root.pak is only enabled via an optional CMake data path;
no original game files are added to source or required by source-only CI.
Next: run those integrated checks under the exclusive runtime slot, then a
fresh frozen actual Brighton/BMW process with complete gameplay rendering and
normal keyboard control. Natural AIR/checkpoint finish and normal clean exit
remain mandatory and unverified.


## MAC109 — Darwin allocation-domain retirement, 2026-10-27

Integrated MAC108 Debug had five passing fixtures and three functional passes
followed by a static-exit SIGSEGV in PageUsage::GetBit. Bounded LLDB on the
actual damage fixture proved a late libc++ string deletion after SBA retirement:
the pointer had malloc_size32. Separate late AGX memory-pool worker deletion was
also observed. Original global delete queried destroyed SBA page metadata even
for genuine system allocations. The preceding three deletes belonged to the
allocator's own malloc-backed lock/state/area metadata (sizes128/16/384).

Native BizSmallBlockOwns now classifies actual Darwin malloc allocations before
querying any game allocator metadata. Original SBA pages are native AllocVirtual
mmap regions (malloc_size0), remain on the original allocation/free mechanism,
and a non-malloc allocation after SBA retirement still explicitly aborts.
No game-owner cleanup was skipped, no bitmap metadata leaked and no tracked
resource force-freed. The real allocator regression now deliberately retains
both an SBA game-static block (must die before allocator) and an early malloc
system-static block (must free through real global delete after allocator).

Debug, Speed, DebugUBSan and SpeedUBSan real-allocator regression runs all exit0.
The coherent Debug integrated required-UI, Simple2D, UI GPU ownership, damage,
original-PAK postprocess and World transaction checks all pass under Metal
validation, seven of seven including the allocator check, with full static exit.
These supersede the earlier partial pass/static-crash outcomes; full-game UBSan
remains separately limited by the original missing PortalBlocker RTTI.

The actual frozen MAC108 app now gets through original Brighton/BMW resources,
World and RaceLogic Ready and Running, then Lua's real Race loaded message.
The first physics worker collides with its stack guard: EXC_BAD_ACCESS/SIGBUS,
Thread stack size exceeded, Gravity::CollideMesh SPUScene.cpp1464 through
jqWorkerLoop/NativeTLThread/MT::Thread. MAC111 owns the CPU scheduler stack
contract. No gameplay frame or completed race has yet been established.


MAC109 differential follow-up: without the new malloc-domain check, the same
late-system-delete fixture can accidentally pass if the freed bitmap storage
has not been overwritten. With Darwin MallocScribble=1 the private old-helper
variant reliably exits245/SIGSEGV, while the production-helper fixture exits0
and prints the late-deallocation PASS. The registered allocator regression now
sets MallocScribble=1, so stale freed metadata cannot accidentally satisfy it.
Private copies and result logs stay under _work/build/mac109-before*.


## 2026-10-27 — MAC110 read-only current vehicle and authored-road diagnostics

UiManager's existing native non-Gold Blur2PC Lua library now registers
VehicleState(). The helper is confined to automation code/registration,
coordinated disjointly with MAC103 UI resource ownership. It returns nil off
the native main thread, while original scheduler jobs remain active, for
loading/UI/teardown states, or without a current loaded live vehicle whose
handle/model/physics/configured slot0 car identity matches the current World
and RLC. It does not flush jobs, update the race or control the car. Existing
public lifetime/loaded/state APIs are used; no RLC private field was exposed.

Returned values copy actual VehiclePositionInfo/LapsInfo pose/+AZ/signed speed,
mph, lap/time/progress/distance and current level/control flags. Raw configured
AI, AI-toggle and automated-human flags are reported separately because the
original IsVehicleAIControlled deliberately disguises automated-human cars.
Optional AIR data comes from existing nullable RouteInfo getters: split-volume
occupancy, current authored route/segment/direction/racing-line point and
centre offset. The adjacent authored racing-line sample/direction wraps only
explicitly looped routes, otherwise clamps at the last valid segment. No
accumulated checkpoint count or inferred finish is invented. Existing real
SplitCrossed Automation notifications remain the ordered split evidence.

Actual UiManager object probes pass Debug/DebugUBSan/Speed/SpeedUBSan (4/4),
logs under `_work/build/<config>/telemetry-agent/`; scoped diff whitespace
validation passes. Source is frozen for parent integration. Native runtime
telemetry has not yet been queried in an actual race. The bounded driving
protocol and exact source/field authority are documented in
`docs/macos/keyboard-race-telemetry.md`; all sample Lua commands fit the actual
less-than128-byte original message capacity. No actual game/service run or
shared build was started, and no input/progress/AI mutation was added.


## MAC111 — Original desktop stack commit versus native total reserve

Actual MAC108 reached Brighton/BMW World and RaceLogic Ready/Running, then JQ
Worker0 hit a stack guard at the first nonrecursive Gravity::CollideMesh frame.
Original PC TL/MT pass the size to CreateThread without the reserve-size flag;
the 65536 scheduler request is initial commit within the executable default
1MiB reserve. Native had incorrectly made this the entire pthread stack.
No source executable stack-reserve override was found. Microsoft documented
CreateThread/default linker behavior is linked in thread-stack-contract.md;
retail PE header stack size was not independently measured.

NativeThreads.cpp now preserves that available desktop reserve for all native
MT and TL users, with whole-MiB rounding for larger requests and Darwin page
alignment. No physics algorithm, local buffer, Windows implementation, job
policy or fixed simulation timing changed. Actual Debug compiler stack usage:
CollideMesh61904 fixed bytes plus dynamic arrays; its source-defined native
EPA/pointer/facet/result storage totals78208 bytes.

New tools/macos/thread_stack_test.cpp checks real MT requests/restart, suspended
TL requests/exit code, source-sized volatile storage and two real scheduler
lifecycles calling the actual Gravity::CollideMesh zero-pair entry. Debug,
Speed, DebugUBSan and SpeedUBSan all exit0. Exact pre-fix native stack handling
rebuilt privately makes the same fixture fail safely at the capacity assertion
(exit1) before touching an undersized stack. No original assets/CLI/GPU/service
required. Root CMake integration needs gravity + system_runtime dependencies.

Private builds/executables: _work/build/macos/physics-agent-stack-{macos,
macos-release,macos-ubsan,macos-speed-ubsan}; private compiler assembly/stack
usage: _work/build/macos/physics-agent-stack/SPUScene.{s,su}. All owned runs
ended and exclusive slot is free. Source frozen for parent integration;
actual next Brighton/BMW run remains necessary for collision/race proof.


## 2026-10-27 — MAC113 required AIR failure and real Debug input/finish audit

Read-only flow audit confirms original direct Debug loading waits LevelDumped,
the0.5s load delay, actual RLC/AI/actor readiness, RaceLoaded and completed
streaming. LoadingGame then sends the original RaceStarted while selecting HUD.
RaceBook forwards that message into RLC, which opens its controls gate and
broadcasts VehicleRaceStarted; the actual actor enables controls and StreetRace
relinquishes AI for the configured human. Control1=0 selects player slot0;
INVALID_CONTROLLER selects AI. Direct Debug UISetup locks/primary profile0,
disables all-pad menu input and applies profile controls through ClearGameProfile
and SetSystemDefaults. The real keyboard/mouse0 feed original layout0 Q/A/arrows.
RLC also rejects paused/debug-camera-controlled input; accepted key injection
alone is not proof of bound physical controls or actual motion.

Concrete initialization gap: both AmaxWorldLayer and RLC treated AIR LoadFailed
as AllLoaded, allowing readiness despite missing required human lap/split
authority. The two CPP handlers now publish the existing native first startup
error and return before readiness/state promotion for nonUI native races.
Optional UI and other-platform fallthrough behavior is unchanged. All eight
actual object probes pass (two sources x four configurations); whitespace check
passes. Production source is frozen. The actual-handler callback fixture
`tools/macos/air_startup_failure_test.cpp` compiles; parent owns coherent
full-game closure integration/runtime. It tests callback-thread error handoff
without owner-state promotion and blocked pending Create/Ready, then actual
cancellation/application/static cleanup. RLC static zero-fill is an explicit
fixture precondition, not a claim about arbitrary-storage constructor coverage.

For genuine one-lap finish, retain the MAC095 protocol: ordered real splits and
LapInc:0:1 plus RaceFinished; immediately dispatch the existing
UISystem.SystemQuitGame(true). Plain Debug's HUD reaches MpFinished, which
at >1 UI second calls multiplayer result access without an initialized MP
result set. A one-second Finished polling loop is therefore unsuitable; keep
the real notification watcher attached before launch and react promptly.
Actual process exit0 must still verify timely legitimate shutdown; no finish
interception or synthetic progress was added.


## MAC112/MAC114 — original PC render preset integration, 2026-10-27

Restored original PC graphical-detail propagation to native World/view hierarchy,
including prepass scheduling independent of visible-light count, original model
masks and Lights/LSplitScreen subtechniques, view format/MSAA propagation,
SceneGraph window depth/stencil metadata and native MRT/slot13 detach at the
existing PC pass boundary. Native Application owns original no-settings detail2
and change-only ApplyGraphicalDetail semantics; graphics persistence is deferred.
The actual GameApp hook handles an absent acquired world; settings changes during
concurrent world creation are not a supported new flow. Required view allocation
failures continue through actual partial-owner rollback.

All13 graphics production units plus view-policy fixture compile under four
configurations; root ApplicationMac, actual GameApp and application fixture
compile12/12 probes. Integrated Debug build passes; fourteen focused checks
pass under Metal validation, including actual application lifecycle/preset,
MT/TL/JQ/Gravity stack, scheduler/core threads, original archives/World, damage,
UI, PC32 IDs and static cleanup. Enhanced view-policy fixture additionally
passes with a real published R32F depth/mask resource and resolved GPU pixels.
It does not execute a complete enabled game prepass; MAC114 real frozen app is
running for that gate.

New required-AIR failure fixture's first integrated attempt exposed a fixture
prerequisite: real LayerManager.Update requires an actual upper Display device
for its lease. It failed truthfully with Native upper Display device is not
bound; it has not yet verified the new first-error callback. Owner MAC113 is
adding the real minimal device prerequisite, with production sources frozen.


MAC113 actual-handler Debug fixture now exits0 with its actual native Display
lease surrounding both callback/application lifetimes. The original World
handler runs on a real callback thread, and the original RLC OnMessage consumes
the actual LoadFailed type. Both leave the tested owner's readiness/ownership
representation unchanged; the original first-error handoff throws on main
manager.Update before pending Create/Ready. Cancellation, full application,
actual device and static destruction complete. Private compile/link reused the
integrated target's exact original bootstrap/Lua/full-game closure; runtime
queued through run_exclusive.py. Log
`_work/build/macos/air-failure-agent/fixture-summary.log`, result exit0. An
unregistered message-debug-info warning is expected because this bounded
fixture does not execute game message-registration startup. No assets, AIR
import, actual race or progress/finish mutation are involved. Source frozen.

MAC115 companion audit / MAC116 native worker storage: source authority confirms
GetWorkerID is the logical processor slot, main0 plus enabled workers1..7,
not the ordinal/thread count. The only additional active fixed-slot consumers
found beyond MAC115 particle random storage were original SceneGraph contexts
and Audio::ObjectManager scratch. Native now provisions8 independent elements
for each and checks ownership IDs; no clamping, remapping or worker-count/policy
change. Legacy platform capacity/path remains7. Current Amax audio policy uses
slots4/5, so its old ID7-to-main alias was latent, while original scene policies
can select7. Particle block m_WorkerID has source writers but no indexed-reader
use; existing scheduler alerts8/jobnames10 fit; proprietary six-context XMem
paths are not active native compile paths. Unused jqAssistWithJobs's external
caller TLS0 contract is recorded without introducing usage or speculative fixes.
Detailed bounded audit: docs/macos/worker-storage-audit.md.

Three changed original source objects (SceneGraph, RenderTask, ObjectManager)
compiled in Debug/Debug+UBSan/Release/Speed+UBSan,12/12PASS; diff check is clean.
Production source frozen before root coherent MAC117 rebuild. New
worker_storage_test.cpp uses original scheduler/SceneGraph/ObjectManager with
real Display/FX lifetime and all seven concurrent pinned logical worker IDs,
plus main0. It verifies context/scratch uniqueness and actual sample isolation,
not gameplay. Root owns integrated full-game/bootstrap closure and queued test
runtime; runtime result pending at this entry.


## MAC114/MAC115/MAC116/MAC117 — worker identity is an ownership index

Actual MAC114 now passes the original physics stack boundary, enters running
world/race layers and starts streaming, then dies in a real particle spawn task.
Crash registers show Util::C_Random this=null; native mti offset0x1380 explains
the fault address. Scheduler logical ID7 indexed particle arrays sized7. The
same native0..7 contract also reaches SceneGraph's seven-context arrays, and
Audio's scratch API aliases ID>=7 to main0 (latent under current audio masks).
MAC115/116 restore eight independently owned native slots with checked access;
legacy platform capacities/policies remain intact, no worker cap/remap added.

Full GetWorkerID audit finds those three active fixed-slot domains. Particle
block IDs are U16 with no source readers; source alert slots8/job names10 already
fit. Dormant XMem converter paths and unused external jqAssistWithJobs TLS0 are
recorded separately, not silently enabled or patched. The scalar original MT
random generator matches independent32-bit vectors across50000 outputs and
copy/range audits; LP64 storage size alone is not a proven algorithm fault.

Coherent MAC117 Debug rebuilt all352 required objects and actual game/fixtures.
Sixteen integrated checks pass, including original emitter update+SpriteRotating
spawn on real worker7, World/original archives, AIR first-error callbacks and
all previous GPU/static-exit checks. New scene/audio parallel storage fixture
timed out60s; owner is diagnosing its actual prerequisites/stack before that
check is accepted. Enabled gameplay frame/motion/natural finish/clean exit are
still unverified. Remaining three CMake allocator+stack configuration checks
also pass after rebuild; private optimized game fixture closures remain stale
and cannot establish a Speed game runtime pass.

Two MAC114 automation warnings were audited without treating them as new
resource failures. NoDObjMesh's success-bit setter exists only inside commented
legacy collider code, while its diagnostic remains active; main/new-car
colliders have separate actual success bits. AIBROKEN queries AllOK during RLC
POST_INITIALISE before streamer InitRace enables safeToSimulate. It does latch
a legacy analysis flagfalse; it does not prove every later AIR consumer ready.
Post-streamer airReady and actual AIR lap/splits still require runtime proof.

MAC118 / MAC116 runtime verification complete. Initial integrated worker-storage
fixture timed out60, and a diagnostic direct run timed out45 AFTER all context,
scratch and owner-cleanup checks. Actual owned process sample shows main joining
JQ Worker0 in Scheduler::_jqStop, while that worker is blocked in Event::Signal
at jobqueue_kernel.cpp346. Group executing count was decremented before the
last event notification; Flush then DestroyGroup could destroy/recycle that
native event. LLDB timing hid the race once, so a debugger exit0 was not used
as sole proof. Private original hang sample and runtime log are retained under
_work/build/macos/worker-storage-agent/.

Native CompleteNativeBatch now uses the existing batch-pool lock across group
count decrement/final event Signal; publishes global completion after grouped
event work. Both normal and temporary worker execution use it. Native
DestroyGroup waits outside that lock, then retires the event before publishing
the free slot under the same lock. SetGroupPoolSize already waits then locks;
no lock inversion was found by independent physics-agent review. Windows/console
paths remain unchanged. No sleeps, ID clamping, thread-count reduction or fixture
bypass. Callers still own group/pool management and must quiesce other waiters or
late task publication; this does not add general multi-waiter auto-event support.

Two original scheduler objects compiled four configs8/8PASS. Actual scheduler
and thread-stack regression executables were privately linked with exactly those
two corrected objects and the actual per-config source-derived library closures;
Debug/Debug+UBSan/Release/Speed+UBSan runtime8/8 exit0. Final full original
SceneGraph/Audio fixture, coherent Debug full-game/bootstrap closure plus those
two scheduler objects, passed five independent non-debugger processes exit0.
Each run held seven actual scheduler workers concurrently in their distinct
contexts/audio regions and verified main0 isolation, then immediately retired/
reused256 real event groups before full owner/device/FX/scheduler shutdown.
First corrected process1.02s, remaining0.23–0.24s. Results:
completion-probes.json, completion-runtime.json, completion-regressions.json
under worker-storage-agent. Production/fixture source frozen, no owned processes;
root integrates/delivers and performs the actual Brighton/BMW race gate.

Root integration confirmation for MAC116/118: coherent Debug scheduler rebuild,
actual game/fixture links passed; full integrated Debug CTest17/17 passed under
Metal validation, including worker_storage's actual seven concurrent workers,
256 event-retire/reuse cycles and clean scheduler shutdown. Root has launched
actual frozen MAC117 game for the remaining natural one-track/car race gate;
no completed-race claim follows from these engine regressions.


## 2026-10-27 — MAC119 particle queue corruption traced to native base tail padding

Actual frozen MAC117 reached the original Brighton/BMW race layers and streamer,
then asserted on a null `SpawningEmitter::pEmitter`. Bounded before-unwind LLDB
recorded the real queue count as4 before `AtomicVector::Update`,4 immediately
before radix sorting, and186 after sorting. The failing record was a newly
exposed tail entry with a null effect, rather than evidence that the retail
emitter resource failed to initialize. The diagnostic inferior was explicitly
killed; it was not a successful game-exit test. The first helper's per-record
reader used an unavailable SBFrame API; those reads failed, while count/pointer/
stack captures succeeded. The helper was corrected for any later diagnostics.
Evidence: `_work/runtime/verification/mac119-game.log`.

Independent compiler layout inspection and an authored exact queue reproduction
confirmed the cause: `Btl::Vector<SpawningEmitter>` has sizeof24, data-size20 and
alignment8; its derived `AtomicVector::m_Size` occupies reused tail padding at
byte20. `Btl::Swap` used a raw sizeof(base) copy during radix sorting, overwriting
that derived count. The pre-fix private queue reproduction failed with lost
records; the semantic fix preserves the same layout, without padding changes,
local counter restoration, null skipping, worker reduction or substitute data.
The actual pointer key mapper already returns INTPTR; the radix pass count comes
from its sizeof, so no separate pointer-key narrowing change was required.

Native-only `btl/Containers/vector.h` now exchanges owned members/storage for
array, static, dynamic and fixed-size policies. Dynamic storage swaps pointer,
capacity and active size; TrimCapacity/SetCapacity use the same safe path.
Static policy swaps only live values, avoiding inactive indeterminate scalar
slots used by original crowd records. Its unequal-size transfer stages a default
reset before mutation and assigns into a still-live source object, then commits
the active count. This avoids the destroy/reconstruct exception gap in the first
active-only draft. A fault fixture failed on that draft and passes after the
staged-live correction. Windows/console byte-swap paths remain unchanged. The
allocator is a stateless type with static operations, not omitted instance state.

Permanent `tools/macos/vector_swap_test.cpp` links the existing dynamic-atomic
fixture closure (dynamic include directories plus system runtime), takes no
arguments and starts neither the GPU nor CoreSystems/Artery. It verifies owned
nontrivial values, unequal/empty/self swaps, derived-tail canaries, growth/trim/
retry, default-constructor failure with live-count/error preservation, and the
exact particle AtomicVector shrink/radix/regrow path. Stable full-record checks
include keys with equal low32 bits and distinct high32 bits. Final Debug, Speed,
Debug+UBSan and Speed+UBSan builds/runtimes all exit0; sanitizer runs use
halt_on_error. Fresh original Manager/GenericAll/CrowdStaticRadix probes passed
all4 configurations, and final staged-reset CrowdStaticRadix probes also passed
all4. Logs/status: `_work/build/macos/occlusion-agent-particle/vector-*` and
`final-static-source-status.json`. Scoped diff-check passed.

Final production/fixture source is frozen. Root's bounded actual MAC120 probe
uses an earlier coherent binary containing the primary particle queue fix, but
predating the generic static-constructor-failure refinement. Final-header full
Debug game rebuild/integrated tests remain pending, as do actual enabled game
rendering, keyboard movement, natural race finish and clean game shutdown. No
complete-running-game or completed-race claim follows from these container tests.

## MAC120/MAC122 actual integration boundary, 2026-10-27 — /root

MAC120 coherently rebuilt the Debug actual game and focused fixtures (1,894 build steps), with 18 integrated checks passing under Metal validation. Its binary included the primary semantic-vector fix, but predates MAC119's final generic static-constructor-failure refinement. The actual authenticated Brighton/BMW startup passed the former particle queue assertion and then exited245 after49.84seconds in original UpdateTrails/GetSalt. The faulted pointer was 0xcdcdcdcd00000000. This is failed gameplay evidence, not a completed frame or clean exit. Private log/status/crash: `_work/runtime/verification/mac120-*`.

MAC121 source audit independently establishes transient C_Particle_E grows from PC8 to native16 bytes while its original runtime block manager still advanced8; adjacent record writes overwrite full pointers. Graphics owns the sizeof-based runtime stride and complete raw-slot pointer/salt initialization. These records are neither disk nor GPU formats.

MAC122 now integrates the final frozen MAC119 header and MAC121 source together, including the actual super-effect trail regression in CMake. Root owns shared build directories; agents audit/source-probe only while archives rebuild. Pending gates remain actual enabled full game frame, real keyboard motion, ordered AIR splits/lap/rule finish, and normal complete shutdown.

MAC122 coherent Debug rebuild completed all1,897 steps, including final MAC119 generic static swap exception handling and MAC121 runtime trail layout/initialization. Strict Metal integrated CTest passed20/20 in22.37seconds, including final vector, actual trail spawn/update/retire, logical worker ownership/lifecycle, original PAK-backed graph/world rollback, and required UI/AIR boundaries. This is focused component evidence. A frozen actual MAC122 game probe is now running; no playable result is claimed from those tests. MAC123 separately owns source-level renderer bool and creation-completion propagation gaps found by the ongoing startup audit; current frozen binary predates those checks.


## 2026-10-27 — MAC121 transient particle trail references

The bounded actual MAC120 playtest advanced beyond the emission radix queue
failure, then crashed in original UpdateTrails/GetSalt on a worker. Its emitter
pointer was 0xcdcdcdcd00000000. The actual trail record is C_Particle_E: a native
pointer plus U32 salt, sizeof16/alignment8 on this host, whereas its manager still
reserved the original PC32 stride8. A salt write and the next pointer therefore
overlapped adjacent records. These records are transient runtime metadata only;
the source and independent companion audit found no disk serializer, Xt schema
or GPU vertex format using C_Particle_E. Other particle/strip fixed-size payload
managers remain unchanged; generic effect handles already initialize whole
pointers and were not the cause.

Native Manager.cpp now derives trail storage stride from sizeof(C_Particle_E).
Both SuperSpawn raw creation helpers explicitly initialize pointer and salt
before attempting child-emitter allocation, including exhaustion. All original
UpdateTrails, FreeParticle, effect-reset and emitter-generation ownership rules
remain in use. Windows/console retain their original8-byte branch. Production
and permanent fixture source are frozen; scoped diff-check passed.

The new tools/macos/particle_trail_test.cpp takes no arguments and links the
same actual Game/bootstrap-first closure as particle_worker_test. It uses an
actual manager, SpriteRotatingSuper effect, two actual child-trail definitions,
original BeginCreateSuperNew/NewParticle/NewParticles(num1)/CreateTrails/
UpdateTrails/FreeParticle/Reset, live emitter pools and a real display lease.
It poisons adjacent raw metadata, checks complete pointer/salt identity after
creation/update, exhausts the owned cache, verifies empty metadata, expires a
real emitter salt, moves a live last record during interior erase, reuses
retired emitters, then verifies all parent/trail blocks and active children
retire over two cycles. No retail asset or substitute renderer is required for
this focused ownership regression. Its first poison setup incorrectly indexed
size0; it was corrected to acquire raw slots through PushBack before poisoning
and Clear before original creation.

All8 affected/companion source objects and the fixture compile in Debug, Speed,
Debug+UBSan and Speed+UBSan (32/32 probes). Root's coherent final119+121 Debug
build and strict-Metal integrated20-test run passed including this fixture with
full exit0. Private Speed and Debug+UBSan full direct processes also exit0 under
Metal validation; the sanitizer uses halt_on_error. Their private closures
replace the stale bootstrap with current Main.cpp first. Speed+UBSan remains
unvalidated at runtime: an older shared Portal::C_DeferredBlocker archive stops
before main on its previously fixed StaticPool alignment issue. This was not
suppressed or counted as a particle pass. Earlier private stale-bootstrap
closures completed the trail checks but failed late allocator teardown; current
private bootstrap resolves that separate class for the two successful configs.
Logs are under _work/build/macos/occlusion-agent-particle/trail-* and per-config
trail-fresh-runtime.log. Actual enabled gameplay, real keyboard movement,
natural race completion and clean actual-game exit remain root integration gates.

Actual MAC122 frozen Debug startup passes the prior particle failures and loads original Brighton/BMW into in-game UI and BeginFrame rendering. It then asserts in FX::CheckRenderMutex (lock typeNone) while BlobShadows::PreRender -> RenderBlobTexture -> SceneGraph context -> shared sampler-state binding executes. The live owned sample is `_work/runtime/verification/mac122-live-sample.txt`; no gameplay PNG was completed. Original assert reporting waits for Automation, so root terminated only its owned exclusive wrapper after collecting the stack; exit130 is a failed clean-exit gate. The old probe classifier missed the exact ` assert :` log syntax (status primary:null); the private harness now recognizes it for future runs. MAC124 graphics owns source-backed render ownership audit/fix across the actual pre-render path. No fake finish/input/graphics bypass was used.


## Post-MAC121 — Bounded runtime element-stride companion audit

Read-only inventory of dynamic particle/strip ElementVector sizes/copies and
scenegraph result/visible-node storage found no additional concrete LP64
pointer-stride mismatch beyond the actual C_Particle_E16-versus8 case owned by
MAC121. Other literal buckets carry numeric pointer-free records; original
particle format sizeof assertions remain authority. Runtime pointer arrays,
job parameters and result transfers use their actual type sizes. The legacy
U32 FetchBits alignment is SPU-only, not active native code; fixed disk/GPU
numeric widths are not host-pointer widths.

Added bounded findings and limitations to slice-assumptions.md. Scratch search
inventory: _work/build/macos/physics-agent-element-audit.txt. No production edits,
shared builds or runtime runs. No gameplay/race/clean-exit milestone advanced.


## 2026-10-27 — MAC124 actual render FX ownership

The actual MAC122 game reaches BeginFrame's dynamic model blob rendering, then
asserts FX RenderMutex enum0 through SceneGraph/RenderingData shared texture
binding and SamplerStateManager. The original GameApp already owns the whole
OnRender/photo/CommonRender lifetime in normal gameplay, but skips it when the
boot UI-stall flag is false. Boot Create sets false and only Boot Destroy
restores it. Debug launch waits for a frontend world that it does not create,
so actual Boot remains Running/false alongside the real race world.

Native GameApp OnRender now always owns the existing render mutex with a local
RAII scope. The original nonnative conditional policy is preserved; sampler
assertions, serializer exclusion and scenegraph worker model are unchanged.
The audited actual normal-game scope contains no new nested serialise lock or
resource wait in BeginFrame/blob/target rendering. Failure releases the owner
while retaining the original draw exception. No lower-pass lock, type flip,
blank texture or skipped shadow was added.

New game_render_ownership_test.cpp invokes actual C_GameApp::OnRender with an
actual false-flag SystemLayer. Its virtual render callbacks run original
file-loaded sampler FX against a real Metal target, verify yellow pixels, block
a concurrent original serializer/cache mutation until rendering exits, inject
a draw exception, and retry successfully. Fixture-only private member access
installs the actual system owner without a production API/layout change. The
fixture starts the original scheduler, releases its live FX before application
teardown, creates its own scratch directory, and completes strict-Metal Debug
with full exit0. GameApp and fixture syntax probes pass all4 configurations.
This is ownership/GPU evidence; enabled actual BMW model shadows/prepass and
race progression remain the next actual-game gate. Logs:
_work/build/macos/occlusion-agent-render-owner/. MAC126 separately audits the
reachable PC blob channel-policy gaps before any edits to that subsystem.


## MAC125 — Required native physics container ownership and status

Audit recovered a single unconditional true return in original Gravity Import;
malformed native scalar reads throw. Discarded bools alone were not evidence of
a reachable false-return importer. No fake false implementation or shape-level
policy change was added. Native PhysicsLevel now stages scene ownership, checks
original Import results, initializes route slots, requires Main/exact declared
routes and validates original block extents/counts before publication. Caller
version metadata and prior successful ownership survive false/throw. Windows
parser body, PC record widths and legacy raw import remain unchanged. Header
remains optional with original zero-route default; this is explicitly tested.

Owned production: gamesys/game_resource/source/PhysicsLevel.cpp and its header
(private native method only; no data member/layout change). Added focused fixture
tools/macos/physics_level_failure_test.cpp, scratchDir [optionalOriginalPhysicsPath],
link game_resource + gravity. It uses the actual block writer and Gravity parser:
missing Main, missing/excess routes, negative/impossible counts, short Header,
duplicate Main, truncated route scalar, short/root-overrun headers, legacy raw,
Header-less Main, successful replacement, failed retry and first-load cleanup.

Debug, Speed, DebugUBSan and SpeedUBSan all compile/link/execute exit0. Each also
loads the complete private original Brighton Level.levelphysics (four routes),
imports actual Main bytes and rejects a final-scalar truncation while restoring
version/ownership. Private logs under _work/build/macos/physics-agent-level-failure-<config>/
runtime.log. No original bytes tracked. Source/fixture frozen for root MAC127;
no gameplay/simulation/contact/finish milestone advanced. Shape-level assertions,
unknown-type behavior and uncommitted descriptor cleanup remain original limits,
recorded in slice-assumptions.md; this is not arbitrary malformed-parser safety.

MAC125 differential: the same authored fixture linked privately with the exact
previous PhysicsLevel loader exits1, `Malformed original physics container
reported success`, while corrected four-config runs exit0. Pre-fix execution
finished normally before the root MAC127 actual probe acquired the slot; no
owned process remains. No additional Debug linking occurred during root build.

Root integration MAC127: coherent Debug game/fixture build succeeded and all
24 integrated checks passed under strict Metal validation in22.82s, including
physics_level_failure authored and physics_level_failure_original complete
Brighton container/Main truncation tests. The actual frozen MAC127 game probe
was then launched by root; its outcome is separate and not claimed here.

MAC123 required startup completion source/fixture frozen and integrated. Native
AmaxWorldLayer consumes actual registered-renderer Create/CreateVolatile false
or exception through its production helper and existing main first-error channel,
before level instance/AIR/RLC acquisition. Failure unwinds debug initialization,
RaceIntro subscriber registration, fully created transactional World, then level
handle/LUT/advertising with original tracker/perf balance. Registered renderer
ownership already rolls back its failed acquisition; no invented renderer state.

Native level actor wrapper no longer publishes success from its destructor.
Runtime-only side interface permits C_GameActorList to call actual completion
only after successful original preload/postload. Native false/exception retains
the first error and destroys the owned list without success publication; pending
cancellation retires lists before base actor-list destruction. Actor.cpp85 stores
its own strong DataRef creator copy; Ptr.inl Claim/Release preserves it after the
list releases its references. No C_GameActorList/I_ActorCreationList layout or
serialized representation change. Original default E_OnFail_Continue deliberately
allows optional actor Create=false, and remains intact; no source-backed mandatory
classification for every level actor was recovered. Required BMW/AIR/RLC checks
remain separate. See startup-publication-audit.md for grouped remaining/deferred
status gaps and the stream-pending-bool nonfinding.

Two production CPPs compiled Debug/Debug+UBSan/Release/Speed+UBSan8/8PASS.
startup_completion_test.cpp uses real original full types/factory/renderer GPU
acquisitions and the production native renderer gate, callback threads and main
LayerManager handoff. All base/volatile false/throw cases roll back their own
GPU allocation; first error prevents pending Create/Ready. Actual preload and
pending Create exceptions do not escape the worker or publish completion. Real
Priming-actor cancellation retains creator data until actor destruction, then
creator dies exactly once; original optional Create=false still completes under
Continue. Private Debug fixture body passed initially but static teardown exit250
exposed private override-object placement ahead of bootstrap, not a source actor
creator lifetime failure. With actual original Main.o first, true process/static
exit0 passed. Mach-O init-order.json proves bad private GameActorList global init
index11 before PreStatic/SBA12/13, versus good bootstrap0/1 then GameActorList14;
ActorFactory table init958 occurs after SBA in both. Fixture factory registration
happens during main, not namespace preregistration. No allocator/engine lifetime
change was proposed for that private link placement.

Root coherent MAC127 Debug actual-game/fixture links passed; strict Metal CTest
24/24 passed including startup_completion true process/static exit0 in normal
CMake bootstrap-first closure. No owned processes or pending source changes.
Actual MAC127 frozen game is root's next separate enabled-graph/keyboard/natural
race/clean-exit gate; these boundary regressions do not claim a completed race.

## MAC127–129 actual enabled game boundary — /root

MAC127 coherently integrates123/124/125/initial126 and passes24/24 focused Debug checks under Metal validation (22.82s). Actual original Brighton/BMW emits RaceStarted and RaceLoaded and enters the enabled lighting prepass; no completed gameplay frame exists. It fails the native adapter guard requestedDepthEnable-with-null-attachment. Fatal cleanup was externally stopped after its bounded grace; exit130 is not clean shutdown.

MAC128 per-draw conditional LLDB diagnosis was too slow to reproduce before its120s normal quit. Its cleanup sample instead captures an actual SceneGraph/renderer view-index assertion in CubeMap removal; MAC130 owns this registration/removal class audit. After LLDB timed out the separate inferior process group remained; root explicitly terminated only owned PID42020. No non-owned process was touched. The private harness now retires any leftover inferior only when its executable path exactly matches that probe's newly-created app.

MAC129 uses a filtered __cxa_throw breakpoint, avoiding per-draw debugger overhead. It captures the original failure before unwind: Deferred::C_Light::RenderBox/RenderSpotlightVolumeSimple -> LightingPrepass::RenderSpotLights/RenderLights1 -> MainView::RenderPrepass1. Actual depth pointer is null, requestedEnable1/write0/function2. Original D3D APIs permit absent-attachment tests; MAC128 adapter companion owns effective state translation, preserving cached requests. Inferior44428 was explicitly killed through LLDB and debugger quit; diagnostic exit is not a game pass. Evidence: verification/mac129-game.log278 onward.

Runtime orchestration adds an optional run_exclusive --started-file handshake, published atomically after acquiring the slot, so private harness checkpoints begin at the owned launch rather than while queued behind another fixture. Default callers are unchanged. Python compile and actual true-command handshake exit0.

## MAC128 PC attachment-conditioned depth/stencil adapter — /root/physics_port

Official D3D9 NULL-depth and D3D11 missing-aspect contracts make absent depth/stencil tests pass while requested state remains cached. Actual MAC129 primary confirms the reachable deferred spotlight-volume draw requests depth enable1/write0/function2 with a null depth pointer. Native MetalContext now derives effective per-draw depth/stencil from actual bound aspects; it preserves requested state, real attachment validation and explicit clear errors. No header/layout or Windows changes. See [depth-attachment-contract.md](depth-attachment-contract.md) for exact source and primary-document authority.

New `depth_attachment_test.cpp` uses real D16/D24S8 GPU surfaces, detach/rebind with retained requests, GPU colour/depth and stencil-content inference, exact detached-depth byte preservation, rebound writes and invalid explicit clear controls. Private Debug, Speed, DebugUBSan and SpeedUBSan compile/link/runtime all exit0 under MTL_DEBUG_LAYER=1 (UBSan halt-on-error). Exact pre-fix adapter fails the same fixture safely exit1 at its absent-stencil guard. Combined exclusive run completed and released its slot; no owned processes remain. Production and fixture frozen; actual full-frame/keyboard/natural-finish/clean-exit proof remains parent-owned and unclaimed.


## 2026-10-27 — MAC126 coherent original PC blob and default-depth policy

Reachable blob-shadow source selected its console single-texture branch on
native, despite original PC source implementing RGBA packing and already-built
channel shaders. BlobShadows.h/cpp now select the existing PC chain for native:
channel ownership/packing, four shader declarations/create/parameter layouts/
destruction, actual stencil texture at s2 and matching sampler, channel selection,
CW volume culling, and final texture unbinding. All PS3 Cell/GCM remap/control
branches remain PS3-only. Original target formats and geometry are unchanged.
Native target creation propagates every required CreateRenderTarget failure;
its tracker uses existing MemoryTrackerContext RAII. A failed required box VB
map also reports false, allowing existing renderer rollback.

The real PreRender GPU fixture initially produced RGBA coverage0/0/0/0. Audit
then identified the coherent device-depth policy gap: original
PCD3D11GameDevice constructor calls SetDepthFlip(true,false). Its implementation
sets comparison flip=true, viewport flip=false, default comparisonGreaterEqual.
Native had left Base's false/LessEqual defaults, yielding depth clear1 while
blob projection explicitly tests Greater at .5. Native MetalGameDevice's
constructor now establishes those exact original PC fields after cache
invalidation, before GPU state exists. CreateVolatile/DefaultStates publishes
the correct comparison; SetView restores it. Existing View.cpp native projection
and RenderState depth reconstruction read GetDepthFlip; clear reads
1-m_DepthClear. Both projection modes thus map near1/far0 while viewport Z
remains uninverted. No local blob clear, compare inversion or viewport hack was
introduced. No GetZScaleOffset symbol exists in this source; View.cpp computes
its Q/offset directly.

Permanent blob_shadows_test.cpp takes original-data-root and authored-shader-root
and uses the full Game/bootstrap-first link closure. It mounts actual root.pak,
sets the original resource manager's filesystem to that overlay, and waits for
the actual imposter texture's valid readiness. Four authored raw-geometry
casters run original C_BlobShadows::PreRender, packing differently sized
silhouettes into one RGBA target. GPU coverage is1600/1024/676/324. It also checks
actual perspective and orthographic near1/far0 and verifies near-over-far GPU
ordering with the default clear/comparison in both draw orders.

An actual GPU stencil clear generates a2048-pixel left-half car mask through
RenderWindowMask. Original ScreenRender reads that mask's blue channel, real
full-size R32F receiver depth and each packed channel shader: all four passes
alter1920 unmasked pixels and zero masked pixels. Receiver setup was corrected
from1x1 to64x64 because original pixel offsets divide by depth extent; depth.9
places it inside the smallest authored caster. These are fixture scene inputs,
not altered shaders or fallback game resources. The initial fixture also needed
SetFileSystems on its own resource manager to read the mounted PAK.

All4 BlobShadows source, native device constructor, and final fixture probes
pass. Focused Debug with strict Metal validation completes full resource/GPU/
PAK/stream/scheduler teardown with exit0. Evidence is under
_work/build/macos/occlusion-agent-blob/, especially macos/runtime.log. Root's
integrated24-check run already passed MAC124 ownership and the prior frozen blob
chain; the latest depth constructor and this complete fixture require final
root integration. Actual MAC127 advanced beyond the mutex assertion and reached
race start, then exposed the separate color-only lighting pass/no-depth adapter
contract (MAC128/129). Real BMW enabled rendering, keyboard motion, natural
finish and clean actual game exit remain unproven. This authored geometry
fixture is not a completed race or BMW-rendering claim.

## MAC131/MAC132 integration and actual lazy-view snapshot — /root

MAC131 coherently rebuilds final126 PC default reversed-depth constructor plus128 effective attachment state and all selected GPU/startup tests (101 build steps, exit0). Broadened strictMetal CTest is39/41 in34.21seconds: new depth-attachment and actual PAK-backed four-channel blob tests pass, as do the other startup/worker/physics/world/graph checks. Two failures are retained: mac_application loses expected BasicRender GPU output after the PC depth-default correction (MAC133 graphics audit); mac_engine_render still expects an absent-stencil draw to be unsupported. Root replaces that obsolete assertion with actual PC-compatible draw/pixel verification, leaving the unsupported point-fill guard. Do not report41/41 until repaired/rebuilt/retested.

MAC132 is a bounded diagnostic using the frozen MAC127 app and a55second normal quit. Before first ViewManager removal, pure LLDB field/memory reads capture scene11views versus dynamic renderer10slots. Main index0 is initialized; LightTrails index10 is not initialized and has no renderer views. This proves MAC130's lazy registration mismatch before the nonordered removal permutation. Full JSON is verification/mac132-game.log312 and stack directly afterward. The callback makes no target calls or expressions; it stops only during actual ViewManager::Destroy. Inferior53038 was explicitly killed through LLDB and debugger quit. Exit0 from this diagnostic is not a clean game-exit result. Frozen app debug-map warning concerned regenerated Lua bundle object; the relevant unchanged scene/view/renderer types and fields were read successfully.


## 2026-10-27 — MAC133 application fixture owns its final depth precondition

The broadened MAC131 checks found the framework application fixture's final
BasicRender rectangle black after the corrected original PC reverse-depth
policy. Private diagnostics reproduced the first frame: renderer ready, full
192x128 viewport, depth enabled/write enabled/GreaterEqual, zero bias, all pixels
black. The final frame also failed across224x160. Actual source-built BMSL, not
only HLSL, confirms vsBasicRenderColoured2D outputs clip z1/w1. Original
BasicRender binds shaders/blend state and leaves depth policy to its caller.

Original CommonRender clears the selected display windows' depth in
RenderSplitscreenViewWithAA. RenderSplitscreenWithoutAA restores their colour
to the back buffer; the PC RenderFullscreen path does not clear that separate
backbuffer depth. The fixture's extra final draw calls DefaultStates, enabling
the PC comparison, then draws into that attachment without owning an initialized
depth value. Undefined attachment contents are not a valid finite[0,1] test
precondition. The exact initial depth bits were not sampled or claimed to be NaN.

Only application_test.mm changed: that authored draw initializes its own depth
with Device::Clear(E_Clear_Depth), using actual GetDepthClear (zero), after the
existing DefaultStates/SetView(NULL). Depth remains enabled and the original
GreaterEqual/default projection policy is preserved. No production renderer,
framework, shader, default or attachment-translation code changed. Failure
readback now includes the first bad pixel/coordinates, dimensions and mismatch
count; temporary per-frame state logging was removed.

Complete final Debug native host fixture under Metal validation exits0, retaining
all previous startup rollback/retry, missing required shader, eight frames,
window resize/split layouts, GPU pixels/presents, restart, deferred shutdown,
renderer exception and owner-retirement checks. Final fixture probes pass Debug,
Speed, Debug+UBSan and Speed+UBSan. Evidence:
_work/build/macos/occlusion-agent-application/runtime-final.log and
source-probe-status.json. This verifies the actual framework host fixture; it
makes no Amax gameplay, input-motion or completed-race claim.

## MAC130 — Native scene/renderer view slot lifecycle — /root/dynamic_port

Actual MAC132 JSON proves scene11 versus Dynamic renderer10, with final
LightTrails uninitialized and first Main removal initialized/index0. Native
registration now reserves typed NULL slots before publication, materialization
uses the parent stable index, and removal preserves the same nonordered
permutation for lazy and created slots. Registration and materialization
rollback retain exact owners; Dynamic native acquired groups initialize NONE
and retire once. Windows/console branches and simulation timing are unchanged.
The only typed family is Dynamic; untyped LightTrails/AuxFlares/CrowdFlags have
no child-view storage. Native virtual hooks require coherent consumer rebuild.

Production source frozen with16/16 private object probes across all4 configs.
Real SceneGraph/T_Renderer fixture tools/macos/view_slots_test.cpp compiles and
links against real full-game bootstrap-first closure plus all4 changed typed
objects. Exclusive Debug runtime exits0 after lazy/out-of-order/disabled/
nonordered cancellation, reserve/Create failure rollback/retry, real Dynamic
groups and actual layer first-error/no-Ready/no-Render checks. Evidence:
_work/build/macos/view-slots-agent/{matrix.log,runtime.log}; full rationale and
limits in view-slot-lifecycle.md. Root coherent integration and actual clean
game exit remain pending; no race completion claim. No shared builds/runtimes
or unowned process kills were performed by this task.

## MAC136 — Actual post-layer subscriber ownership audit — /root/dynamic_port

Read-only source audit maps MAC131 secondary subscriber-pool failure to static
boot/race/world owners retaining bindings beyond C_Game, plus explicit heap
audio subsystem owners whose pointer table is released without deleting them.
All unknown message IDs decode to original AMDA Dialogue and DTRC Destruction
subscriptions. Do not detach Sticker's boot lifetime in its per-race Shutdown;
do not delete generic borrowed Audition subsystem entries; do not clear the
pool. Known owner/acquisition/final-release matrix and proposed binding-aware
multi-forwarder retirement are in subscription-owner-lifecycle.md. Optional
pure-read owner snapshot script is syntax-checked but not runtime-verified.
No production edits, builds or game runs. Next: root approves exact lifecycle
scopes/ownership; implement named owner release and repeated real lifetime
regressions, then verify actual clean exit after primary LOD fix.

## 2026-10-27 — MAC134 PC sampler bias shared shader integration frozen

Found in / belongs to: native macOS runtime sampler adapter. MAC135 captured
actual MirrorView pixel sampler2 encoded16, linear min/mag/mip, anisotropy1.
Added one load-time adapter for authored BMSL and MojoShader output, independent
per-stage float2[16] state at an actual free uniform binding, explicit PC LOD
addition/clamps, isotropic gradients and precise unsupported provenance/
anisotropic-gradient diagnostics. Vendor pristine; exact-context build-copy
patch retains MojoShader TEXLDL source.w, previously discarded upstream.
No source-built693 blob regeneration or asset modifications.

Verification: modified real engine loader compiles693/693 authored shaders on
Apple M4 Pro; all4 configurations compile three modified/new lower source
objects and focused fixture/profile. Full Debug, Speed, DebugUBSan and
SpeedUBSan focused strictMetal GPU binaries exit0, including pixel/vertex
legacy TEXLDL, signed/fractional mip selection, separate slots, buffer ownership,
helper rejection/retry and negative/None clamps. Private logs
`_work/build/macos/occlusion-agent-lod/`. Full semantic/evidence/limit detail in
sampler-lod-contract.md. Production and fixture frozen. Root owns CMake
integration; next is coherent existing regressions and actual MirrorView/race
rerun. No complete game/race/clean-exit claim from these focused checks.

## MAC136 — Native binding-aware entered owner retirement frozen — /root/dynamic_port

Native nonvirtual Single/Multi detach(expectedPool) removes only an owner's
actual binding/strong forwarder and preserves unrelated pools. RaceBook
entered-service bits and mode prefix pair partial Init with final Destroy/
destructor; resources/Xt remain alive for Audio. AIR pairs per-world binding
retirement; world intro/rules stages retain quick restart and retire at final
Destroy; RaceLogic constructor failure/destructor retires entered Pickups/
start-line bindings, preserving quick Reset and Sticker per-race Shutdown.
Root's MAC137 live read confirms172 subscriptions/121 lists/27 types match
the complete named owner matrix, with no debugger read errors.

Source24/24 probes pass across4 configurations. Real Debug owner fixture
compiles/links and exclusive runtime exits0:3 repeated two-pool lifetimes,
partial Init throw/retry, preserved borrowed current/noncurrent forwarders,
actual unentered RaceBook,2 real C_Game/StreetRace/MP/Sticker lifetimes and
true static exit. No fake pool clearing or arbitrary borrowed-owner cleanup.
Evidence: subscription-owner-agent/{matrix.log,runtime.log}; integration
noargs actualGame/bootstrap-first closure, GameLogic includes before Amax.
Production/fixture frozen; no private archive reads pending. Coherent full
RaceBook/AIR/race unwind and separate Audio owner teardown remain pending
actual-game validation. See subscription-owner-lifecycle.md for precise limits.

MAC134 root-review extension: unsupported sampler provenance must also reject
non-default MaxLOD0 (mip-filter None) when bias0; restricting the guard to
nonzero bias left an unintegrated state accepted. New actual helper GPU fixture
verifies explicit level1 with0/linear→mip1, rejects0/None andnonzero bias,
then retries direct integrated level1 with0/None→mip0. Pre-fix M4Pro selected
mip0 for NotMipmapped despite helper acceptance; no wrong-pixel claim for that
host case. Production change is solely the unsupported-state guard; shared
MSL adapter/corpus unchanged. Final all4 source reprobes and full focused
strictMetal GPU binaries exit0, both UBSan runs halt-on-error. Coherent actual
Game integration/rerun remains root-owned.

### MAC139 integration started — root
MAC131 actual startup reached RaceStarted/RaceLoaded and the complete renderer, then failed required MirrorView pixel sampler2 bias (signed16 => +1mip); normal exit251 also exposed final message-pool subscriptions. MAC135 before-unwind proof confirms original PC sampler state, and MAC137 maps all172 remaining subscriptions/121 lists/27 owner types without SB errors. MAC136 no-args diagnostic was invalid (no data-root); MAC137 used explicit data-root and owned inferior cleanup. No game frame/input/finish/clean-exit claim.

Integrating frozen MAC130 stable scene view slots/rollback, MAC133 authored fixture depth initialization, MAC134 per-stage shader sampler LOD contract and exact-context legacy TEXLDL profile correction, MAC136 entered message-owner retirement, MAC138 audio owned-subsystem/load-thread lifecycle. Added native MetalSamplerLod.cpp through authoritative display2 Jam; configure generates source lists, original vendor source remains pristine. Added focused no-args sampler/view/subscription/audio fixtures through their actual dependency closures. Native ABI/layout consumers require a coherent rebuild.

## MAC138 explicit native game-audio subsystem ownership — /root/physics_port

Actual MAC137 owner dump retains exactly UI10/System3/Dialogue2 subscriptions after all layers Destroyed. GameAudioLayer's eleven heap subsystems were never deleted by original InterfaceController::Release. Native opt-in registration now owns only those explicit allocations; borrowed Audition registration remains unchanged. Both Music/UI loaders join before any dependent owner retires, Music/UI clients are destroyed with live table/System/Wave/Xt dependencies, and System goes last; each table slot remains live through its destructor then clears. Existing system/output/wave/table teardown follows. Never-started/repeated loader retirement is safe; controller resets only safe table/pointer/count state. No callback/bank reload behavior was invented.

Four native source configurations/16 actual object probes pass. New no-args `audio_subsystem_ownership_test.cpp` passes private Debug and Speed full-game/bootstrap-first links and true process/static exit0, including actual Music/UI thread and scheduler-group ownership and real subscriber-pool cleanup of fifteen source-derived UI/System/Dialogue subscriptions. Root coherent build/real fresh-game clean exit remains separate; no loaded bank or reload proof is claimed. See [audio-subsystem-ownership.md](audio-subsystem-ownership.md). Production and fixture frozen, no owned runtime remains.

MAC139 root coherent Debug actual-game/fixture build passed, and the integrated `audio_subsystem_ownership` check passed along with view_slots/startup_completion/game_render_ownership. The larger45-check batch was still running at handoff. Actual populated-world cleanup/clean exit remains unproved; no additional runtime was queued after the integrated result.

MAC139 coherent Debug build exit0 (949 steps; original Jam source list regenerated; generated legacy Metal profile leaves vendor source pristine). Actual engine shader loader accepts693/693 authored shaders. Strict Metal integrated checks45/45 exit0 in66.62s, including final app-depth fixture, original Brighton physics, real postprocess/FX/blob graph and new sampler/view/subscription/audio fixtures. Actual fresh MAC139 game run now in progress; no game-frame/input/finish/exit claim from those checks.

## 2026-10-27 — MAC140 required native FX defaults freeze

Read-only MAC139 companion audit identified a concrete required resource
invariant: FX System published the3D default before checking allocation, ignored
failed mapping and unconditionally unlocked. Native System::Initialise now
stages actual2D/3D owners, checks Create/IsCreated/map and publishes only on full
success; local owners roll back false/throw, null/repeat shutdown is safe and
native GetDefaultTexture returns absent after failure. FX::Initialise uses
existing tracker RAII. No shader/dimension policy, Windows or console changes.

All4 configurations compile System.cpp/Fx.cpp and extended realFX fixture.
Debug strictMetal full original file/cache/context/GPU fixture exits0, including
actual uncreated-device allocation rejection, no publication/tracker balance,
real retry, initialized2Dwhite/3Dzero maps, actual default-white GPU sampling,
duplicate-live rejection, texture-count baseline, repeat shutdown and reinit.
DebugUBSan strictMetal full fixture also exits0 with halt-on-error, using a
private current Manager object because its shared archive lacks pre-existing
NativeShutdownCache; no shared build.
Production and fixture frozen. Root owns coherent integration. Late3D/map
allocation failure has not been fault-injected; do not claim that differential.

Separate source-backed boundaries and selected/inactive effect policies are
recorded in render-startup-boundaries.md; MAC141 owns actual vertex sampler
dimension selection, unchanged by140.

MAC139 actual run: original Brighton/BMW reaches RaceStarted/RaceLoaded/StreamerStarted, passes prior Mirror sampler LOD boundary, then required vertex slot1 expects2D(type3) but has3D(type4), shader589. Capture deadline70 occurred at DebugScreen, so PNG is menu evidence only. No actual keyboard motion/finish. All layers now destroy without prior view-slot/subscriber assertions, archives close, then processSIGSEGV (wrapper245); actual IPS identifies deferred UI payload-pool assertion during later static gButtons destruction, with retired logger secondary. MAC142 owns actual queue cancellation/lifetime fix, no logger/allocator bypass.

MAC141 LLDB confirms first failing MainView RenderWithAA material draw, shader589 texture reflection slot0=3D/slot1=2D, actualslot1=3D. Qualified global lookup used wrong frame-variable command; LLDB abortedsource before explicitkill, private harness retired exact owned inferior and verified no leftover. Exit1diagnostic, no gamepass. MAC143 retries pureSB global/pointer capture without target expression calls; MAC140 required FX allocation/map publication transaction remains separate frozen source work.

MAC143 actual before-unwind proof complete: pureSB structuredcapture errors[], shader589 reflection3Dslot0/2Dslot1, actualslot1 is1x1x1 RGBA32Fvolume; storage pointer exactly equals initialized FX defaultvertex3D global. Shaderblob5072bytes retained privately. MainView RenderWithAA draw. Exactdefaultpointer identity proves inappropriate generic substitution rather than a native enum mismatch. Existing PC NativeFX11 limits substitution to named damage-volume policy, while native direct FX applies it to every default vertex binding. MAC143 native reflected-dimension fix/actualmixedvertex2D+3D GPUfixture assigned; explicit mismatched resources must remain rejected. This does not by itself establish every selectedcar instance binding. LLDB explicitprocesskill+quit exit0 is diagnostic only; no ownedinferior remains.

## 2026-10-27 — MAC144 selected graph failure contracts frozen

Concrete source gaps: native World postprocess Resize ignored required volatile
creation false; manager wrapped throwing acquisitions in manual tracker scope;
paint allocation false popped a context whose Push is commented out. Native
Resize now propagates real failure after existing I_Renderer rollback; manager
uses tracker RAII, clears borrowed cached/active state when retiring actual
owners and checks both creation stages before Begin. Native paint false retains
its original bool/owner cleanup without the unmatched pop. Windows/console
paths preserved, no graph/shader substitutions.

Actual original-PAK graph fixture registers an I_PostProcess which owns a real
GPU texture before false/throw, verifies caller propagation, owner rollback,
absent volatile readiness/target list, restored tracker and rejected rendering
of the retired graph. Each failure is followed by a successful resize and
actual HDR/Bloom GPU frame. The fixture also exercises real uncreated-device
paint allocation false, tracker balance, repeated cleanup and actual-device
paint retry. Full Debug strictMetal fixture exit0 including existing14temporal
frames and static exit. All16 probes pass (3production+fixture ×4configs;
UBSan fixture probes use actual Game-source compile flags because GameOFF
configs do not register that fixture target). Production and fixture frozen.
Root owns integration and actual enabled-prepass race validation; focused
fixture remains explicitly disabled-PC-prepass and is not complete-race proof.

## MAC142 — Actual post-layer deferred UI queue ownership — /root/dynamic_port

Actual139 IPS identifies retained C_DeferredUiMessageStore pool entries at
gButtons static destruction; the pool assertion then logs through destroyed
static string state, causing observed strlen(0xfefefe) SIGSEGV. Native UI now
claims deferred delivery after uniqueness validation, closes/cancels on init
failure and before gadget/Lua destruction, rejects late producers and supports
legitimate reopen. Queue/controller destructor fallback retires real entries/
copied buffers. Native acquisition/callback RAII closes popped/partially
acquired entry leaks; original POD memcpy/borrowed-pointer semantics remain.
No logger/pool-assert/Core workaround or tick change. Native owner fields require
coherent rebuild. Source/fixture frozen.

Production12/12 probes pass across4 configs. Actual Debug fixture noargs, real
full-game/bootstrap-first closure: copied-byte independence, actual malloc-zone
4x64KiB acquisition/reclamation, exact cancellation, callback throw/reentrant
two-buffer cleanup, late rejection/3 reopen lifetimes, concurrent producer
close, rejected real UiManager owner,2 actual missing-required-resource init
failures closing before destruction, and retained static queue all PASS with
true process exit0. Engine tracker was inactive, so actual malloc-zone stats
verify payload reclamation. Evidence deferred-ui-agent/{matrix.log,runtime.log};
full mechanism/limits in deferred-ui-message-lifecycle.md. Root coherent actual
game exit/full successful UI reinit remain pending. No shared build or game
launch; exclusive fixture processes were owned and completed normally.

## MAC143 — Reflected native vertex-sampler default dimensions (2026-10-27)

Owner: Codex `/root/physics_port`. Found in actual MAC139/MAC143 native main-view material draw; belongs to the native FX binding adapter. Pure-SB evidence proves slot1's storage is exactly the owned zero3D default while shader589 declares volume3D at0/info2D at1. Existing PC NativeFX11 applies the zero-volume replacement only to its damageVolume parameter; native FxContext applied it to every vertex sampler.

Added a const native `C_VertexShader::GetTextureType(slot)` query backed by actual loaded reflection, without object layout or shader format changes. `Context::SetVertexTexture` now invokes the existing default-volume helper only for declared3D slots. Generic2D retains initialized white; all nondefault explicit resources and lower strict mismatch/attachment validation remain intact. Windows and System.cpp initialization/shutdown are unchanged. New authored `vertex_fx_texture_test.cpp` uses actual original FX loader callbacks, normal/immediate binding and real GPU readback. It needs the existing `blur_mac_fx_test` include/link closure, no arguments/assets/service/window/files.

Verification: changed shader implementation/context plus fixture compile in Debug, Speed, DebugUBSan and SpeedUBSan. Private Debug strictMetal fixture exits0 with white2D/zero3D defaults, black/green2D and blue3D explicit resources, both wrong explicit dimension rejections and a successful retry; normal/static teardown completes. An isolated old FxContext reconstruction fails with the same `stage=vertex slot=1 expectedType=3 actualType=4` primary, then its incomplete exception unwind hits the resource-list ownership assertion (wrapper251). This is a failing differential, not a clean baseline exit. Both private links use a frozen8-archive FX-test snapshot and do not touch shared build outputs. Speed private link is blocked by older shared archive missing prior SamplerLod::Integrate; UBSan checks are compile-only. Logs: `_work/build/macos/physics-agent-vertex-default-*`. Initial authored setup mistakes (missing original serialise lock and precomputed binding offset) were corrected before final verification; production was not adjusted to tolerate them.

Source and permanent fixture frozen. Root MAC145 owns coherent build/integrated runtime. See `vertex-texture-defaults.md` for required car-info binding limits: the actual fallback error is proven, but the reason this particular model instance still reads defaults is not established. No actual race frame, keyboard motion, natural finish or clean full-game exit is claimed.

MAC145 coherent integration in progress (1284 dependent steps): frozen MAC140 FX required-default allocation/map publication/tracker transaction; MAC142 copied UI-payload owner cancellation and initial/final failure-close including static fallback; MAC143 reflected per-slot 2D/3D default policy with explicitresource mismatch rejection; MAC144 required postprocess reinitialization status/rollback/readiness/tracker and unmatched paint failure pop. New deferred_ui_messages uses actual bootstrap-first game closure/GameLogic include priority, vertex_fx_texture uses actual FX/device/force-loaded FX registrations, both no args/assets. Focused new checks pass before integration; full integrated47/actualgame remain pending. Root sole sharedbuilder; agents released private archive reads beforebuild.


### MAC142 / MAC145 — Corrected authored queue exit lifetime

MAC145 integrated build passed46/47 checks; deferred_ui_messages reached all
queue/owner assertions, then the authored namespace-static retained queue
outlived the allocator. Mach-O __init_offsets proves the standard initializer
order: fixture1, PreStatic1012, SBA1023, production ButtonController/gButtons895.
The earlier explicit-bootstrap-first private link placed allocator0/1 before
fixture4 and masked that invalid authored owner contract. Production gButtons
ordering is valid; no production, allocator or standard link-order changes made.

Fixture now holds only a zero-initialized static pointer; main creates the real
queue/payload and registers an atexit callback which deletes the ENTIRE queue
while the allocator is live. Cancelling entries alone would leave pool blocks
and vector capacity, so complete owner retirement is required. A private link
reused the exact standard CMake target linker edge, replacing only the fixture
object. Strict Metal validation exclusive runtime passed with true process
exit0. Evidence deferred-ui-agent/{initializer-order.json,standard_fixture.py,
standard.runtime.log}. Fixture source frozen; parent integrated leaf rerun
pending. No arbitrary post-allocator queue guarantee or gameplay/clean-game-exit
claim is made.

MAC145 coherent Debug build exit0 (1284steps), actual shader loader693/693, integrated46/47 strictMetal pass. Authored deferred_ui_messages staticfixture owner ran before PreStatic/SBA initializers understandard link order; its retained queue outlivedallocator. Original productiongButtons initializesafterSBA and hasvalidallocatorlifetime. Testonly correction owns entireheapqueue via main-registeredatexit retirement (payload+pool+vector beforeallocator), standardCMakeleaf rebuilt andstrictMetal rerun1/1 pass4.70s; no production/allocator/linkorder workaround. All47 relevantchecks now pass across fullrun+correctedleaf.

ActualMAC145 progressed to shader589MainWithAA, then opposite slot0default2D where3Dexpected. MAC146 source+actualpureSB proves logicalsharedbindingappliedbyRD::SetNode beforepassPrefetch (previousshader0/403), cachedsharedbits then skiprebinding aftercurrentVP589; actualslot0 exactlyFXwhite2D. MAC146 sourcefix tracks actualpass sharedsampler materialization/cacheconflicts, preservinglocalimmediate values andconstant/register cache.

MAC145 fatalgrace30s externallyterminated afterlayers (exit130); notcleanexit. MAC146 debuggercontinued realcleanup untilallarchivesclosed andlatestatic std::system_error (invalidmutex) during ComponentCreatorDataRef pool dtor, thenSIGABRT. No pooledcounter/underlyingthrowproofyet; assertion handler itselfownsstaticlock thatmayberetired. MAC147 auditsmissingglobalXtDb/callback/schemaownership; rootMAC148 filteredbefore-unwind system_error proof running, no assertion/logging bypass. Debugger0diagnostic is not gamepass. No actualframe/keyboardmotion/finishyet.

## MAC146 — Native shared vertex texture materialization and pass cache order (2026-10-27)

Owner: Codex `/root/physics_port`. MAC145 actual coherent game failed on the same shader589's opposite slot0 (expected3D/actual2D) after46 focused checks. Root MAC146 pure-SB capture and bounded trace prove that RenderingData::SetNode binds shared default textures with no selected vertex shader, then preceding shader403, before eventual shader589 selection. Register/source-valid bits then skip the physical texture update. This confirms the source-order hypothesis, rather than requiring another texture type guess.

Native FxContext.cpp only: paired InvalidateState now clears its selected vertex shader pointer; a local BeginPass lambda reapplies pass-used shared vertex samplers after actual shader selection, including a conflict-free cached entry. Before the cached return, actual current-shared/pass-mask conflicts require the normal full pass bind, restoring overwritten local parameters according to the existing original reapply comment. No new header/storage/layout, Windows behavior or unrelated constant/register cache changes. A nonconflicting cached local immediate value is intentionally preserved; shared immediate values are rematerialized from their original context-owned logical data.

Extended permanent vertex_fx_texture_test with actual original SharedEffect registration and authored usage/conflict masks over its registered entries: bindings before any shader and after preceding2D shader, cached shared values, foreign-node conflicts, cached local immediate preservation, local restoration after actual clobber, strict explicit mismatch/retry, and paired invalidation. Cached sequences stay in the same real scene; new scenes perform original FX invalidation because the native lower BeginScene resets bindings. Source/fixture compile all4 configurations; final Debug strictMetal expanded GPU fixture and process/static teardown exit0. MAC143-only adapter differential reproduces exact opposite primary `slot=0 expectedType=4 actualType=3`, then its incomplete unwind hits resource-list assert (wrapper251). Logs under `_work/build/macos/physics-agent-vertex-default-macos{,-pre-shared-fix}/runtime.log`; all links use private archive snapshot, no shared build. The first expanded driver incorrectly carried a cached pass across lower BeginScene reset; corrected fixture respects the actual per-scene cache contract, with no production tolerance added for that setup mistake.

Production/permanent fixture frozen; root owns next coherent integration and actual race attempt. Required per-instance car damage binding closure, actual frame/keyboard motion/natural race finish, and clean full-game exit remain separate gates. No owned processes remain; exclusive slot released. Detailed source authority in vertex-texture-defaults.md.

MAC148 filtered before-unwind exception proof errors[]: stdsystemerror comes from MT::RecursiveState::Enter through helper_screens::BizAssertHandler; actual primaryexpr is ComponentCreator SorceryPool free-list fullness. Exact msg m_list.Size(97) PoolTypes<T>::N(128) m_pool.Size(1), hence31live entries. Actualassert handler global lock isalreadyretired atatexit; no PointerSlot32 path. Rootdiagnostic explicitkill/quit exit0, noownedinferior remains. MissingglobalXt pack/database ownership is the source-backed MAC147 repair; preserveassertion/lock/allocator validation. MAC146 expanded original SharedEffect+FX GPU fixture exits0 strictMetal; differential143-only adapter reproduces exact slot0 expected3D/actual2D class. Source/fixture frozen, coherent integration pending147.

MAC147 audio companion: exact Amax path acquires two pack opens on the app DB,
flushes EntitiesPack after runtime definition construction, retains AudioPack,
and never acquires a separate schema-init reference through C_XtAudioDb::Create.
XtHandles are nonowning. Audio pack destruction callbacks log/no-op and do not
call engine/subsystem/wave/controller APIs; raw proxy consumers must retire
first. Delegated native helper NativeIsCreated/NativeReleaseBorrowed and narrow
AudioLayer Destroy call now close actual packs+singleton after joined/retired
subsystems, before ReleaseSystem. Absent/partial unentered ownership no-ops;
missing borrowed DB with live packs and standalone DB misuse reject truthfully.
No broad schema decrement or runtime entity-type drain. All8 source probes pass,
production frozen, no companion shared build/runtime. Details in
audio-xt-owner-order.md. Dynamic owns combined global Xt integration/tests.


## MAC147 / MAC148 — Actual component-creator record owner retirement

MAC146 later static termination initially exposed only noexcept DynamicPool
unwinding. MAC148 filtered before-unwind proof confirms31 live Sorcery entries
(free97/N128/blocks1); installed visual assertion handler then takes retired
g_AssertLock and throws system_error before message output. No PointerSlot32
path and no evidence that every live entry is an external component claim:
StorageSorcery registers live Xt records even with refcount0.

Native application/global loader, optional MultiplayerData, DynamicEffect and
RacingPhysics owners now pair acquired pack handles, borrowed-vs-owned databases
and successful schema references. App local creation transaction publishes an
exact base callback context/DB; final record callbacks run after borrowers stop,
while C_Game/factories/context/schema live. Loader replacement retains old on
failure and closes old only after successful new open. Native sparse DbDestroy
uses live index rather than physical prefix. Audio companion closes acquired
pack/singleton after subsystems and before engine release, no schema decrement.
No layout, pool/assert/logging bypass, fixed-tick or Windows flow change.

Focused actual inherited TranslatorMovementComponent PC32 pack fixture compiles
and links standard full-game closure, runs strict-Metal exclusive and exits0.
Covers sparse close/retained DataRefs, duplicate/failed/retried Dynamic/Physics
loads, actual AudioPack release, successful+failed loader replacement, two app
owner lifetimes and final pool/static retirement. One explicit fixture schema
lease preserves original schema across repeated owners: fully zero→schema
regeneration is an independently documented original annotation-mutation limit,
not claimed as supported. Initial24/24 production+Audio8/8 four-config probes
pass; final owner/fixture28/28 rerun passes. Source/fixture frozen, private archive
reads released to root for coherent MAC149. See component-creator-lifetime.md,
audio-xt-owner-order.md and xt-owner-agent/{matrix-final.log,fixture.log,runtime.log}.
Actual coherent game exit remains pending.

MAC149 coherent Debug build exit0 (457dependentsteps) includes final146 cache-order fix and147 completeglobalXtowner lifecycle (including matching concrete callback allocation). Native CMake adds no-arg xt_owner_lifecycle with standardbootstrap/full-game closure, GameLogic-first and explicitprivateaudio includes. Rootfull48 strictMetal checks nowrunning. No newclasslayout introduced147; test schema lease explicitlydoesnot claimin-process fullzero→schema regeneration. Actualcompletedframe/keyboard/naturalfinish/normalexit pendingthiscombinedgame.

## 2026-10-27 — MAC150 read-only colour mask audit

No production edits. Native/PC bits are1/2/4/8 and U32; no enum LP64 change or
selected complement-mask caller found. Upper Default=UINT_MAX resolves to
per-device RGB7, whereas lower Metal rightly accepts literal0..15. Native
Device setters resolve it; native Context_Base directly forwards it. Original
PC singlethreadContext and DX11 bridge both route through Device resolution.
SplashCamera504 explicitly restores Default through pDisplay context and is a
concrete candidate; the added native indexed context overload has the same
unresolved-sentinel surface but no current Default caller found. Do not remove
lower guard or blindly mask to15 (wrong alpha default). Actual151 numeric
value/caller proof pending; source contract/proposed GPU tests recorded in
colour-mask-contract.md.

## 2026-10-27 — MAC149 real-path result and MAC151 diagnosis

Coherent Debug457-step build succeeds and all48 selected strictMetal CTests pass (71.68s). Real original archive startup passes prior shared vertex-sampler error and reaches Brighton/BMW RaceStarted/RaceLoaded, live human control/AIR and road telemetry. Q pulse returns true but no after-motion evidence is claimed. Actual complete rendering next rejects Invalid colour write mask; fatal cleanup reaches scene light manager with one allocated point light. The owned217s run is externally terminated after120s cleanup grace (exit130), with no completed gameplay PNG, natural finish or clean exit. MAC150 audits upper sentinel policy; MAC152 audits grouped UI scene borrowers; MAC151 captures actual value/caller and pointer ownership before either production fix.

## 2026-10-27 — MAC151 actual particle topology boundary

Before-unwind diagnostic stopped at actual ToPrimitiveType2(E_PrimitiveType_QuadList), via DynamicVertexBuffer::DrawPrimitiveBlock and SpriteRotating particle renderer after a finite realQ1 pulse. The file named mask-proof.json therefore contains a topology proof, not a numeric colour-mask proof. Controlled continuation reaches same scene-light destructor with1 live point light; script cached before reload omitted exact gadget-handle field, so matching that owner is not claimed. Explicitprocesskill/quit ends diagnostic0 and ownedinferior isgone. MAC155 reuses frozen149 with corrected explicitfields. MAC150 may fix source-proven upperDefault contract independently; MAC156 will audit/fix whole reachable PCparticle topology policy without lowerQuad reinterpretation.

## 2026-10-27 — MAC150 upper Context default mask contract frozen

Native scalar and indexed Context now forward actual upper Device setters,
matching original PC policy resolution/cache rather than truncating sentinel.
Lower literal mask/target guards remain unchanged. Broader Default inventory
records depth/fill latent unexercised Context gaps, context-local cull and distinct
shared BlendMode semantics; no blanket state reroute. Actual155 captured exact
UINT_MAX through SplashCamera504 after151 instead stopped at particleQuadList.
All12 private probes across4configs pass; extended existing blend_mode_test
Debug andDebugUBSan strictMetal both exit0: all16 scalar/indexed GPU masks,
four changed per-device defaults, invalid bits/target cache preservation and
lower sentinel rejection. No CMake/CLI additions. Sources/fixture frozen; root
coherent actual integration pending. See colour-mask-contract.md and
occlusion-agent-colour-mask logs. Sequential next task156 owns original PC
particle geometry/map policy, separately proven actual151.

## 2026-10-27 — MAC155 exact default and light-owner proof

Actual frozen149 before-unwind primary is lowerMetalSetColourWriteEnable(value4294967295), upperContext flags4294967295, SplashCamera::Render504. This is the actual upperDefault policy sentinel, supporting MAC150. Continued actual cleanup stops before C_LightManager destructor: numPointLights1 and activefirst0x0000000c40f1d678 exactlymatch UIPerks2 staticlocalcar gadget handle m_pObject0x0000000c40f1d678, managedtrue salt0, scene0x0000000c3f80b800. PureSBcaptureerrors[]; controlledkill/quit diagnosticexit0 (not normalgameexit). MAC152 owns synchronous quiescent scene-borrower retirement and idempotent detach; pool/assert remain. Rootprepared fullgame ui_world_borrowers CMakeclosure+noargs45hardware test and nextcoherent50checks, pending frozen production/fixtures.

## 2026-10-27 — MAC158 final selected PC view handoff

MAC154 identified two omitted originalPC final-view guards. Root traced native ApplicationBase RestoreWindow/globaltargets chain and PC-selected PostProcessLayer, with source-retailT232 authority; graphics owner reports no intentionalMetal exception. Native now executes existing ViewDependentFXUpdate before final postprocess and existing singleview/detail>1 prepass depth/stencil restoration for HUD after. Only2guardselectors/comments changed, no inventeddepth/cullvalue/surface or layout. All4private GameWorld source probes PASS (Debug/Speed/DebugUBSan/SpeedUBSan). Standard coherentbuild/actualstrictMetalframe stillrequired. Reclaimed15owned obsolete/duplicate probe appbundles afterexactprocessabsence and SHAmanifest, retainingalllogs/proofs and139/145/149frozenbaselines; disk5.2GBfree.

## MAC152 — Main-owned UI world-borrower and scene-light retirement (2026-10-27)

Owner: Codex `/root/physics_port`. Actual MAC149 world shutdown reached SceneGraph light-manager assertion with one point light retained. Bounded direct-owner inventory found all scene point-light acquisitions in vehicles, wheel effects, SpecialFX light components and UIPerks2. Root MAC155 pure-SB evidence proves the actual final active pointer equals the attached-car perk widget's handle exactly (`0xc40f1d678`, owner scene`0xc3f80b800`, managedtrue, salt0); errors empty. No pool/assert changes or global layer reordering were made.

Native-only changes in UIPerks2.cpp, UICarNode.cpp and UIHudDummy.cpp subscribe to the existing ClearStoredActorPtrs protocol, run even for inactive widgets, clear cached actor/physics/pickup pointers and permit later player rebinding. Perks Exit is idempotent and removes its light through the actual live light's GetSceneGraph owner rather than a potentially replaced/absent global world; its handle/static interpolation borrower are reset. GameWorld NativeDestroy's existing post-job-flush clear block dispatches synchronously on the recorded application main thread, covering the final native callback after creator join. Normal creator-worker destruction keeps original deferred UI dispatch. No header/layout, Windows, fixed-step gameplay, UI resource/screen-lifetime or normal loading flow changes. Root separately owns later GameWorld MAC158/159 renderer guards/readiness edits outside this block.

New no-argument ui_world_borrowers_test.cpp uses actual three gadget Init/message handlers, public activation API and the real gButtons subscriber/deferred queue. Two actual SceneGraph lifetimes acquire the perk light via original AddPointLight, then validate queued clear remains pending versus synchronous original message retirement, inactive handlers, borrowed pointer reset, repeat clear/Exit, independent pending-message cancellation, real pool capacity reuse/stale generation handling, original scene-manager destructors and final Exit after scene destruction without a global GameWorld. Test-only private field inspection follows existing lifecycle fixture pattern; opaque borrowed markers are never dereferenced and no actors/race are fabricated. Debug full-game/bootstrap-first + original Lua blob link, strictMetal runtime and complete process/static exit0. All4 configurations compile production4 + fixture. An informational missing message-name debug registration in this no-game-boot fixture does not affect original subscriber routing. Root CMake uses full-game closure/GameLogic include priority; no arguments/assets/Artery/window. Logs `_work/build/macos/physics-agent-ui-borrower-*`.

Production/permanent fixture frozen; shared archive reads released, no owned processes. See scene-light-ownership.md for exact ownership/thread authority and limits. Root owns coherent integration and actual game exit proof; the fixture does not exercise full gadget Setup through real Lua/actors or actual main-versus-worker world dispatch, and does not establish a natural completed race or clean full-game exit.

## 2026-10-27 — MAC156 complete original PC dynamic triangle policy frozen

Actual151 before-unwind QuadList belongs to native host-vs-PC geometry policy.
Native selects existing PC paired primitive/vertex counts and duplicated writers
in11 particle headers/13 CPU sources, six light header/inline writers and
Minimal debugcube; also actualDVB PC Lock/Unlock and originalPC UseSoft=false.
No lower topology reinterpret/guard relaxation, asset/layout change or newshape.
Track/tracer contain noQuadList producers; remaining dynamicQuadList tokens
are console-only. New no-arg particle_geometry_test uses actual bootstrap-first
fullGame closure: metadata, actualeffect reserved6vertices, originalsorted+block
CPUposition/colour/UV/animation andsentinelbounds, actual lightQuad3d writer,
three actualmapped dynamicGPU draws with independent rectangle/gap pixels.
All72 private source probes4configs pass; standard Debug strictMetal fixture
exits0 inclstaticretirement. Initial fixture precondition failures corrected
Scheduler/camera/existingTextureCoords ownership; no production workaround.
Other3Game archives stale, no runtime matrix claim. RootCMake sameclosure as
particle_trail, noargs. Production/fixture frozen for coherentGame integration.
See particle-geometry-contract.md and occlusion-agent-particle-geometry logs.

## 2026-10-27 — MAC159 required world texture false-result guards

All4 combined real GameWorld translation-unit probes PASS after adding native required cube creation checks before assert/map and face/depth null-map checks beforememset/Unlock. Successful exactcontents remainunchanged; partialownership stays in existingNativeDestroy IsCreated loops. No injectedlatemapfailure claim. Native explicitblackcubemap initialization retained pendingseparatePCskipfirstuseproof. Root153 CMake nowwires fullbootstrapGameclosure noarg ui_world_borrowers +particle_geometry, extends selectedchecks to51 includingexistingblend_mode. Coherentbuild waits154fixture/readrelease.

## 2026-10-27 — MAC153 coherent grouped integration

Frozen150uppermask,152UIworldborrowers,154requiredXt/defaultmaps,156wholePCdynamicgeometry/map,158finalviewhandoff and159worldmapguards are coherentlybuilt into standard Debuggame (549initialsteps exit0). All50selectedstrictMetalchecks excludingstill-changingXtfixture pass74.94s; finalfrozenstandardxt_ownerfixture rebuildandCTest PASS1.31s, completing51checks withoutunnecessary50rerun. Newui_world_borrowers/particle_geometry usefullbootstrapGame/Luaclosure GameLogic-first, noargs45hardware. Releasegamecoherentbuild inprogress; MAC160 actualDebug330s/onecapture180/finiteQ1 startedfromfreshstandardcopy tovalidatecompletegraphandpopulatedcleanup beforeoptimizednatural-laprun. Actualacceptancegates remainpending.


## 2026-10-27 — MAC154 required configuration and renderer-default startup boundaries

Bounded Brighton/BMW preset2 source audit found original required boot Xt and
Multiplayer configuration results consumed before checking, and Dynamic white/grey
allocation accepted despite null map. Native RaceBook now returns required failures
before consumers; real resource pause/tracker ownership balances on false/throw.
Native Multiplayer rejects a missing actual strong config root before InitSubsystems,
retires rejected actual metadata/transferred input and stages replacement metadata;
no arbitrary internal Xt parser mutation rollback is claimed. Offline required
configuration is separate from online provider support. Dynamic maps now reject
failure through existing I_Renderer rollback, preserving exact original texels.

Four production units pass all 4 configuration object probes (16/16); corrected
existing Xt owner fixture passes4 object probes. Standard coherent MAC153
mac_xt_owner_lifecycle CTest passes strictMetal and true process/static exit0 in
1.31s. It covers missing configured input and worker resume, missing/wrong-root
required Multiplayer imports and transferred-buffer rejection, actual owned
registered-manager failure after acquired defaults, rollback and two successful
exact-texel retries/retirement. Device precedes real graph and outlives borrowers;
no injected Metal null-map or full successful authored MultiplayerConfig/UI boot
is claimed. Other 50 selected checks pass 74.94s, completing coherent 51/51.

The same audit led root MAC158 to restore original PC final-view FX/depth-stencil
selectors, and MAC159 to reject failed world cube/depth allocation/maps before
use. Both are integrated, not outstanding deferred fixes. Only cube skip-clear
content-policy comparison remains deferred pending first-use evidence. No new
LP64 defect was found in this bounded pass. Actual MAC160 frame, natural keyboard
lap/finish and clean full-game exit remain separate pending runtime evidence.

## 2026-10-27 — MAC160/161 actual next required-shader and self-pool classes

Actual coherentDebug160 acceptsfiniteQ1 but fails IncompleteDrawState beforecapture; no inputdisplacement/steer/naturalfinish proof. Actualcleanup now retiresWorld/UI/alllayers andclosesarchives withouttheoldlightassert, thenSIGSEGV (wrapperexit245),140.64s. Root161beforeunwinderrors[] proves ParticleSpriteRotating draw hasNULLVS+PS withvaliddeclaration/targets. MAC162sourcefinds entireParticle CreateShaders guard excludesnative yetreturns success, a required-resource falseReadyclass. FinalcrashpureSB+IPS prove GetEffectiveLevel→MessageSubscriberSystemdtor→RaceBookManagerdtor→staticfinalization; MAC163sourcefinds selfhandler ownsitsmemberpool butDestroydoesnotdetach beforememberdestruction. Controlledkill/quitdiagnostic0notnormalexit. CoherentSpeed1855-stepgamebuildexit0;5selectedSpeedGPU/lifecyclechecks PASS3.32s, butactualgamependingnewclasses.

## 2026-10-27 — MAC162 actual particle shader owner readiness frozen

Actual161 beforeunwind: colour/depth/declaration live, vertex+pixel backingNULL;
SpriteRotating actualstates7 includesBindEffect. ParticleManagerCreateShaders
excludedDEVICE_MACMETAL entirely thenreturnedtrue; notFXcacheloss. Native now
executes existingoriginalPC16VS/7PS selected initialization, verifies actual
requiredset andretireseverypartialshader onfalse/throw. Unusedenumplaceholders
2DRefract/AlphaAndAdditive remainabsent (noinitializers/consumers). Native
Recreate propagatesfalse, nofalseReadycontinuation. Otherdynamicmanager
shadercreationunconditionalboolchecked. SeparateauthorizedSplashCamera381
selector nowtakesexistingrecoveredT107PCsixvertexwriter; noquadreinterpret.
All12finalprobes4configsPASS; extendedexistingparticle_geometry Debugstandard
fullGame strictMetal exits0: actualuncreateddevicefailure/samemanagerretry,
16VS/7PStracking/recreate, realManagerVS_UVW+PS_DefaultoriginalCPUgeometry
GPUdrawandownerretirement. TrackerchecksconditionaloptimizedAPI, nooptimized
runtimeclaimorlateallocationfaultinjectionclaim. Source/fixturefrozen; same
noargtarget, noCMake/header/layoutchanges. See draw-readiness-contract.md and
occlusion-agent-particle-{shaders,geometry} logs. Coherentactualgamepending.

## 2026-10-27 — MAC162/163/165 frozen class handoff and MAC164 integration

MAC162requiredParticle16VS+7PS creation/validation/false+throwrollback/recreate failure propagation andSplashCameraPCsixvertexpath frozen12/12probes; actualManagerGPUshader/drawretirementDebugexit0. MAC163ownedRaceBook selfpool/profile/dialogue/vehicle lifecycle frozen12/12, faithful-library-position privatefixturetrueprocess/staticexit0; initialdirectoverride-object earlySBAplacementdiagnosticnotproductionfault. MAC165wholePCtrail/flareexclusions+paireddebugboxselectors frozen12/12probes; no inventedtriangles. RootCMakeaddsnoarg fullbootstrap/GameLogic-first racebook_owner and52selectedchecks. CoherentDebug+Release164buildsnowrunning; actualframe/input/naturalfinish/cleanexitremainpending. MAC166bounded75backendguardinventoryclassifiesselectedvsunselectedwithoutblanketPCdefine.

## 2026-10-27 — MAC164 coherent validation PASS / actual167

Debug152stepbuildexit0 includes162requiredparticleShaders+SplashPCpath,163RaceBookownedselfpool/finalprofiles and165crossenginePCquadproducerpolicies. All52selectedstrictMetalchecks PASS74.29s. Speed81stepbuildexit0; changedactualParticleManagergeometry+RaceBookowner checks PASS1.51s (other5Speedchecks prior153 PASS). StandardCMake racebook_owner fullbootstrap/GameLogicfirst closure trueexit0, confirmingprivateinitializationplacementdiagnosticnotproductionworkaround. Actual167freshSpeedcopy nowrunsrealarchives/fullgraph, onePNG90s, max1500s, initialthrottle0 soordinaryfinitekeyboardcalibration/operatorownsinput; naturalRaceFinishedwatchownslegitimatequit. Allfiveactualgatesstillpending.


## 2026-10-27 — MAC169 original VAG mono/stereo full-pointer alignment

Actual optimized MAC167 audio-worker memmove fault has a low32-only source.
Both original decoder stream readers selected console U32 pointer alignment on
native while their existing PC branch preserves the whole host address. Two
native/PC guard selectors now restore that paired original PC contract; no
format, predictor, cadence or PCM algorithm changes. Authored actual-reader
fixture exercises both real decoder classes above4GiB at all16 alignments,
partial/cross-packet reads, distinct channels, loops/fader reset, EOF and bounds.
All4 configurations compile/link/run exit0; no GPU/output/service init or shared
build/game run. Exact scope/limits in adpcm-stream-pointer-contract.md. Parent
coherent actual game is the next gate; MAC170 owns separate runtime scratch
prefix alignment, not part of this decoder change.

## 2026-10-27 — MAC167/168 actual audio pointer class

Actual coherentSpeed167 startsdebugrace at20s andloadsfullworld~6.24s, passespriorrender-state failures butworker9SIGSEGV27.49s (wrapper245), noPNG/keyboardmotion/naturalfinish/cleanexit. OwnedIPS+root168pureSB errors[] show memmove←ReadVagStreamStereo←StereoFillBuffer←Wave/Mixer/Scheduler; x1source0x00000000fa9b0080 islow32, x0destfullnative0x16b879780, x2copy128. Optimizedbuildhasnolocals, sofulloriginalsourcepointerisnotclaimed. Controlledkilldiagnostic0. MAC169pairedoriginalPCU64alignmentguards fixesactualdecoderclass, all4actualhighpointer/16alignmentmono+stereostreamfixturespass. MAC170companionauditsupstreamnativepointersandseparate4byteprefix/8bytealignscratchgap; sourcecoordinatedrepair inprogress. RootCMakeaddsnoargs adpcm_stream CPUaudioleaf withoriginalaudio closure/noforce-load.

## 2026-10-27 — MAC171/172 actual native image and keyboard gates

CoherentDebug217/Speed218stepsbuildexit0;10affectedrealAudio/worker/subsystem/Xt/RaceBook/entrypointchecks PASS14.19sDebug/9.75sSpeed. Newadpcm_stream/audio_scratch leafsnoforce-load useoriginalaudio closure, noassets. Actual172nowcompletesoriginalBrighton/BMWfullGPUframe(mac172-game-frame.png), realQ.25 movescarfromstandstill, acceptedRight.1 changesyaw+.0395367rad; read-onlyhuman/AIRflagsallvalidnoAI. Actualfinitekeyboardoperator advancesonroute tospeed8.55m/s and~42mhorizontal displacement. ThenstrictMetalattachment-samplinghazard stopsrun; normalerrorcleanup ALLlayers+archives exits1at150.57s withoutoldlateSIGSEGV/assert. Naturalfinishandnormalexit0stillrequired. Root173Debugbeforeunwinddiagnostic+agent174snapshot/target/bindingclass auditactive, nofallback/guardrelaxation.

## 2026-10-27 — MAC173 before-unwind real water/loaded-audio evidence

Debug173 pureSB errors[] proves pixel slot10 shader622 water ColourBufferSampler samples2D BGRA8 2560x1440 surface0x105940718 equalsto activecolour0, throughactual static RenderingData primitive/View::RenderWithoutAA. Depth0x1059407f8 isdistinct; no depthfailureclaimed. Lower strictguard retained. Actualcleanup retiresworld/UI/layers/archives thenasserts nonempty Audio::PagedArray<EntityDataPollInfo*> inInterfaceController staticdestruction. Thusoptimized172errorExit1 isnotcompleteownershipproof;175auditactualpollproducer/consumer/finalretirement. Diagnosticcontrolledkill/quit0notnormalgameexit.174ownssource-backed Context→existingDevice snapshots;177coherentintegration +sameactualkeyboardnaturalrace follows.

## 2026-10-27 — MAC174 actual water colour feedback route frozen

Actual173 noerrors capture confirms water pixelColourBufferSampler10 texture
BGRA8 2560x1440 surface0x105940718 identicalcolour0, distinctdepth, actual
RenderingData staticprimitive→RenderWithoutAA. RecoveredContext_D3D11 routes
pixelSetTexture→DeviceSetNativeTexture snapshot; nativeContext bypassed it.
NativeContext h/cpp nowusesexistingactualDeviceSetTexture snapshot route, no
newcopyalgorithm/lowerguardrelaxation/substitutetexture. Auditretains binding
timefrozencontents, explicitrefresh, allMRT/cube/resolve ownership; vertex/depth
andlate-targetaliasareseparatecontracts. Existingcolour_feedback extendedboth
Context/Device allformats/samples/cubefaces/mips/sRGB/source-destroyed/noscene,
MRT1..3, late-targetreject+explicitrebindretry, active-depth/rawreject.
All8privateprobes4configs and all4standardfixtureclosures strictMetal GPU
exit0 inclstaticowners; UBSanvariants halt_on_error. No CMake/argchange, no
headerlayoutchange. Sources/fixturefrozen, noownedruntime orsharedarchive
reads left. See attachment-feedback-contract.md and occlusion-agent-attachment.
Actualcoherentgamecontinuation remains root integration gate.


## 2026-10-27 — MAC176 original route adjacency and operator evidence audit

Read-only VehicleState next-sample arithmetic wrapped to0 unlike real AIR
GetIndexOffset forward navigation1. Native telemetry now calls that original
helper, with >1 sample bound; no gameplay/input/progress changes. All4 real
UiManager/helper compilations pass; tiny original-route CPU checks exit0 in all4
(private core closure, not coherent177 archive validation). No shared build/game
run. New keyboard-slice-operator.md records route/volume/progress distinctions,
conservative operator recovery limitations, realistic lap budget, and concrete
watcher false classification: original StreetRace emits RaceFinished on wreck
and failed rules too. Parent owns observer classification update. Actual172
proves frame/motion/steer only; natural lap/finish and exit0 remain separate.


## 2026-10-27 — MAC175 audio observation final-owner repair frozen

Actual MAC173 late static assertion names PagedArray<EntityDataPollInfo*> inside
InterfaceController, after layers and four archives closed. Source traces
SystemSubsystem's48 Low/High oscilloscope poll acquisitions and empty destructor;
actual remaining count was not captured. Explicit subsystem raw entity deletion
bypassed normal controller ReleaseEntity observation cleanup. Native now retires
exact owned-subsystem polls while live entity refs remain, retires borrowed/orphan
observations before table clear, and balances orphan Untrack/exception-safe Track.
Strict normal getter remains unchanged; retirement uses a bounded live-slot lookup.
Final callback recycling waits until actual Audio System is stopped, preserving
live action-producer retirement and normal ReleaseEntity callback behavior.

Controller-only native retained-capacity entry release preserves original per-slot
generation history across >32 shrink/regrow/reinit. Payloads and occupied entries
retire normally, metadata stays at bounded high-water until original destructor;
PC/default generic policy and1..254 per-slot wrap semantics unchanged. No pool or
assert/logger bypass. Four production plus four fixture object probes PASS; existing
full-game audio_subsystem_ownership fixture extended with40 real TrackValue owners,
owned/borrowed isolation, orphan Untrack, normal release, departed callback retirement
and stale handles across3 retries. No fake script/action producer/playback claim.
Coherent177 standard runtime and actual game shutdown remain root verification.
See audio-observation-ownership.md; production/header/test frozen, no archive reads.

## 2026-10-27 — MAC178 bounded native context/device source audit

Read-only inventory of every selected upper context texture/sampler/target/shader,
geometry and inherited state route against original Context_D3D11, Context_PC,
Display2D3D11Bridge and actual Device ownership. Source differences requiring
focused evidence: native scheduling surface publication changes physical targets
without updating the Device target cache/absent MRT slots (PC setter is empty);
cached borrowed declaration/index replay differs from PC overrides; local GetView
differs from PC actual Device view; raw context depth/fill Default and cull aliases
have no selected caller proven here. Actual window/prepass/reduced-render owners
remain distinct from schedule metadata. MAC174 pixel snapshot route is already
fixed; vertex/depth/late-target strict semantics are intentionally separate.

See context-device-contract-audit.md for complete setter inventory and exact
source owner paths. No production edits, archive reads, builds or runtimes for
the source audit. Root authorized a subsequent bounded mixed-context/device GPU
regression in existing game_context_test.cpp; no speculative target/view/default
repair is authorized by that follow-up.

Follow-up baseline under strict Metal reproduced seven mixed-route failures:
declaration/index A replay rendered left128/right0 instead of Device B's right128,
including clones; null/retired declaration failed draw completeness and retired
index failed bound-index validation. Direct Device and explicit context-rebind
controls passed. Root approved native-only preserving overloads in Context_Base:
four-argument VB bind and six-argument indexed draw, retaining bound-context device
generation checks and existing cloned metadata. No public layout or lower guard
change; target/view/default follow-ups remain untouched.

Eight production/fixture object probes and four private standard fixture links
PASS. Debug, Speed, Debug UBSan and Speed UBSan GPU runs all exit0 with strict
Metal; UBSan halt-on-error enabled. All mixed cases now render right128/left0,
alongside the original registry/dynamic-storage/stale-generation test suite.
Source and fixture frozen for root coherent integration. Exact baseline/after
logs under _work/build/macos/occlusion-agent-context-bindings. No actual game or
completed-race claim; no active agent process or archive-read lease remains.

## 2026-10-27 — MAC177 coherent classes PASS / actual179 Debug exit0 / actual180

Coherent Debug580/Speed708stepsbuildexit0 integrates174Contextpixelbinding adapter,175exactaudioobservation ownership+controllerretainedgenerationrelease and176originalAIRadjacency telemetry. All14affectedchecks PASS19.79sDebug/12.11sSpeed, includingactualPAKeffectgraphGPU and40realTrackValue/reinit/departedowner callbackrecord. Actual179Debugfullworld loadsBrighton/BMW, finiteQ5+ordinarykeyboardoperator advances~80.73mhorizontal beyondpreviouswaterhazard, thenexplicitordinaryUISystemQuitGame(true) exits0at196.19s afteralllayers/archives/staticowners; noassert/error/timeout. ItsPNGisDebugmenu(preworld), notgameplayproof. Noactualnaturalfinishclaimed. Actual180freshSpeedcopy nowqueued/running withoriginalarchives/fullgraph,onePNG45sec,max1800sec,initialthrottle0,privatefinitekeyboardoperator1650sec starts onlyactualRacing1car. WatcherqualifiesfinishonlyoriginalLapInc:0:1 +RaceFinished; sameeventonwreckclassifiednonlapandstillordinaryquit. NoAI/pose/progresswrites.

## 2026-10-27 — MAC180 actual start/split and PC cube/2D class

Actual177Speed180 renderscompleteBrighton/BMWPNG45sec andordinaryfinitekeyboardoperator advances~1km withhumanflagsfalse, crossesoriginalLapInc:0:0 thenSplitCrossed:0:1:1. No naturalcompletedlap. At216.18s exits1 cleanly afterstrictpixel slot8 expectedCube5 vsactual2D3 shader387; allworld/UI/audio/archives retirewithoutoldcrash.181sourcefinds exactalreadyrecoveredPCNativeFX11cube/2Dadapter withPC67/native54 hardwarecases: actualbound2D cube samplingRGBA0000 includingwhite/nonzero/allfaces, distinctfromnullD3D9alpha1.182willintegrate181upperFXbothstagecompatibility+185requiredinitializedzeroCube ownertransaction, preservelowerguard.178separateboundedmixedContext/Devicefixture reproduced7 staleVB/index/declaration failures; nativeonly originalPCpreserving overload correction passesall4strictMetal/UBSan configurations, sourcefrozen. No target scheduling/GetView/default changes withoutproof.


## 2026-10-27 — MAC185 required FX PC 2D-to-cube zero owner frozen

Source-measured original PC 2D-bound-to-cube sampling returns RGBA0000; MAC181
owns the discriminating upper adapter/reflection and mixed-stage GPU checks.
Native System now owns a required actual1x1 cube with all6 zero RGBA faces,
transactionally alongside existing white2D and zero float4 volume. Allocation/map
failures stop before publication; correct unique device owners unwind even on
final-face failure. Getter null before/after, repeated init rejected, repeated
shutdown idempotent. Strict lower guard, validcube/unbound/other-type semantics
and nonnative code unchanged. No production failure hooks or fake GPU creation.

All8 object probes across4configs pass. Private standard FX archive-position
fixture runs with Metal API validation and true process/static exit0: real
uncreated-device rejection, successful retry, all6 exactzero faces/type, unchanged
white/volume bytes, tracked+3 defaults and complete baseline retirement, repeat
init/shutdown and existing shader/cache/GPU draws. Late allocation/finalface-map
failure is source/RAII evidence, not injected runtime proof. Production/test frozen,
shared archive reads released; coherent182 and next actual game remain root gates.
See fx-zero-cube-owner.md and fx-zero-cube-agent/runtime.log.

MAC175 follow-up: coherent177 Debug and Speed builds both exit0; Debug all14 affected
standard checks pass19.79s, including real40poll observation/reinit/orphan ownership.
Actual populated game exit must be established independently through root runtime evidence.

## 2026-10-27 — MAC182 coherent PC sampler/current-binding contracts PASS / actual183

Debug/Speed1178stepgamebuilds complete, thenfinalcached-pixel amendmentincremental and41stepaffectedfixturebuilds exit0. All13selectedstrictMetalchecks PASS15.85sDebug/9.30sSpeed, includingactualPAKpostprocessgraph, exact6facezeroowner, mixedcurrentVB/index binding, bothstagecube/2D zeroRGBA andcachedunchangedpixel snapshot preservation.181normalnewpassrematerializesbothstages afteractualshaderselection; cachedconflict-free entryretains146vertex-only behavior, existingdirtysharedupdates remain. No automaticpixelresnapshotpercachedBeginPass. Actual183fresh182Speedcopy nowfulloriginalkeyboardrace,max1800sec,privateoperator1650sec,onePNG45sec,initialthrottle0. ActualnaturalLapInc:0:1 +RaceFinished +normalexit0 remainsrequired; nofinishclaimfrom180firstsplit/179earlyquit.


## 2026-10-27 — MAC181 reflected both-stage original PC cube/2D semantics

Actual180 PSslot8 expectsCube/receives2D. Reused existing NativeFX11 measured
PC67/native54 cube+2D sampling RGBAzero policy, including nonzero2D; no guessed
whitecube or arbitrary missing-resource fallback. Bothstage reflection/source
type checks choose185requiredzeroCube only forCube/actual2D, preserving real
cubes/othermismatch/null guards. Normalnewpass rematerializes used shared
samplers aftershaderselection; cached unchangedpixel preserves174binding-time
snapshot, retaining146vertex/dirty/conflict behavior. No lower guard changes.

Expanded originalFXGPUfixture coversbothstage sixfaces/zeroalpha, default+
explicit2D/cube/2D transitions, sharedprepass/cache/conflict/localimmediates,
strictbadtypes/null/retry andcachedactivecolour snapshot. All4 source/fixture
probes PASS; privateDebug strictMetalGPU/staticexit0. Omittednormalpixel rebind
reproduces slot8 expected5actual3; blanketcachedpixel rebind failssnapshot. Both
failingcontrolsunwindintotracked-resourceassert(signal5), notcleanbaselines.
Private8archivecopyavoidsrootbuildrace. cube-sampler-contract.md recordsscope
andlimits. Root182 coherentchecks/actualrace remain separate completiongate.


## 2026-10-27 — MAC187 normal career save/restart source contract

Read-only audit follows ordinary Start→real Offline primary profile→Boot→AsyncLoad. Missing BLURSAVE requires observed MenuNext confirmation before actual AsyncSave. Documented SAVE/BLURSAVE compressed PC format, separate durable local OS identity, required normal completion/status and optional hosted upload. Existing GameInfo/TierInfo/EventInfo/options getters supply real before/after/restart assertions; debug_save.amax validation functions do not inspect normal saves. Explicit BLUR2PC_COMPLETE_CAREER=0 prevents the preexisting opt-in completion cheat; no cheat/progress calls performed.

career-save-reload-flow.md distinguishes prior native identity/storage/authored-envelope fixtures from production career serialization. Bounded proposals identify missing lazy profile/photo-interface retirement and unchecked corrupt-envelope framing/decompression; no production fixes or runtime claims made. MAC189 normal menu run remains parent-owned. No shared build/archive reads, game runs, state/config/save changes, commit or push. Exact next step: observe genuine initial creation, change one normal profile-backed option through its menu, await actual autosave/exit0, then relaunch same identity/save roots and require real load Okay/zero unread plus matching state; genuine career event evidence remains separate.


## 2026-10-27 — MAC186 normal menu source map / camera snapshot owner class

The legitimate normal launch is original default Master: Legal/StartScreen,
real local profile load (fresh FileDoesNotExist confirmation needs ordinary
Return and AsyncSave), SP hub Races/tier/event/owned allowed car, PrepLoading /
LoadingSpGame, original event-derived rules, intro/HUD, natural SP results and
awards/save/LoadingUi. Optional online stats/social requests do not authorize
fabricated service success or profile/progress mutations. Normal career is
separate from actual183's verified one-car debug lap. Exact source boundaries,
original keyboard actions, reachable generic-popup missing widget and limits are
in normal-menu-flow.md;187 persistence and188 resource audit remain disjoint.

Actual189 camera destructor crash exposed owning-pointer value-copy: showroom
SetEffectInfo → DebugTools local → NetworkLink local assigned the same effect
pointer, while every camera destructor deletes it. Native Camera.h/.cpp now stage
independent C_EffectInfo/C_ShakeCamera clones, recursively closing nested shake
owners while retaining full base-feed/scalar/projection/interpolation values,
borrowed world/name policy and self-assignment. Windows/CopyFrom/object layout /
PC disk formats unchanged; no null/teardown bypass or special callsite workaround.

Camera/DebugTools/NetworkLink + authored fixture compile in all4 current native
configurations (16 objects). Private Debug full-game closure with original Main
bootstrap links; original camera/debug/network nested-owner, source-before-copy
retirement, overwrite/empty/self-copy and repeated client-only Artery lifecycle
fixture passes through static process exit0 under exclusive45. No assets/device
arguments needed; no owned process remains. Production/fixture frozen. Next:
root coherent integration and actual normal Master early-quit/profile/menu flow;
no actual Master clean-exit/career/results claim from this focused check.


## MAC183 actual completed native keyboard slice; normal-menu phase189/190

2026-10-27. Coherent Speed182 actual original Brighton/BMW run passed the slice:
completed GPU gameplay frame,705 observed human Racing records with noAI flags,
3309.574m sampled travel, ordered actual SplitCrossed1/2 then LapInc:0:1 and
RaceFinished. Original ordinary application quit only after lap-qualified finish;
process exit0 after571.51s, no primary error or external kill. Binary/hash and
controls/events are recorded in ignored verification/mac183-playable-slice-proof.json.
No game pose/progress/result writes. Normal Debug MP results were avoided by
quitting after actual finish; this does not establish normal menu/results flow.

Continued into independent Master flow with real offline account and save root.
Actual189 idle hit missing AmaxAttract movie then incomplete draw; late camera
snapshot owner fault is covered by186. Actual190 ordinary PressStart and missing
save confirmation created real saves/save/blursave (2027bytes, magicABCDE337,
raw50903/compressed2015) with BLUR2PC_COMPLETE_CAREER=0. Subsequent existing
BlurAttract.bik failed engine filesystem Open before decoder;191 investigates.
No successful reload/menu/results claim yet.186/187/188 class repairs integrate192.

## 2026-10-27 — MAC188 menu audit and failed-movie readiness correction frozen

Normal FE required UI manager/BPR/font/schema and full-size RGBA8+D24S8/Bloom
owner graph audited against original PC. Eleven missing UITextBox objects remain
a real documented snapshot gap; generic TheCustomPopup missing `textbox` is not
its title/message/options. Existing unknown-type skip behavior preserved, no fake
widget. Shape LoadScene bool returnsfalse even on success; no naive required bool
check or speculative178 target/view/default policy edit. See menu-renderer-assumptions.md.

Actual root189 reached Master showroom/Start then unavailable AmaxAttract movie
openedfalse but UI markedactive and added its graphic, reaching strict unbound
PStexture0 guard. Approved native UI Movie staged decoder/GPU publication, exact
existing MovieFinished once on failed request, true texture readiness/last genuine
frame preservation, initialized owners, device RAII and elapsed pause forwarding.
Native Lua InitMovie returns actual readiness and rejects invalid/duplicate owner
requests; SetupMovie omits only explicit-false movie graphic, retaining Windows
and preview nil behavior. No lower GPU guard or asset-name substitution.

All12 exact objects compile across4configs. Debug/Speed private bootstrap-first
UI movie+actualLua helper+deferred queue+decoded GPU fixture exit0 under strict
Metal, missing/corrupt/valid retry3cycles, one completion, pause/end/retirement and
preview. Descriptor collector tests actual helper publication, not full gadgets.
Default authored video-only isolate; optional BLUR_MAC_UI_MOVIE_AUDIO=1 Speed
passed but DebugSIGSEGV on asynchronous CoreAudio globaldelete→PageUsage, owned
IPS retained for193. No all-audio or fullGameUBSan runtime claim. Logs/private
objects in _work/build/macos/occlusion-agent-ui-movie. Frozen for192; no active
processes/archive readers. Actual190 existing BlurAttract Openfalse belongs191 IO,
separate from original absent Amax filename and this failed-open owner contract.


## 2026-10-27 — MAC187 scoped native save owner and envelope repair frozen

Parent approved production after the normal Master path reached real profile boot. Native lazy profile/photo ownership now closes new requests, joins actual save/load/delete worker callbacks while services live, detaches only exact owned borrows, retires concrete interfaces/pending streams, and rejects late End-callback reacquisition until actual Lua library registration reopens a fresh UI generation. Setup is transactional, failed empty preparation cannot report Okay, and malformed framing/extent/zlib decode fails before mutable deserialization. Existing PC Z_FULL_FLUSH footer and wire bytes preserved; no invented size cap (INT_MAX follows actual MemoryStream storage). Whole nested career parser transaction remains outside this narrow repair.

Four production TUs×4configs16PASS, three fixtures×4configs12PASS. First small actual compressor/native worker format roundtrip true exit0. Final fixtures cover >32KiB original encoded payload, malformed no-deserializer gates, failed/abandoned preparation/retry, active worker join, borrowed isolation and actual Lua lazy interface generations. Production+fixture source frozen/read lease released to parent MAC192 coherent build; no shared archives mutated or further private links. Parent MAC190 genuine initial save produced2027 bytes/raw50903/compressed2015 and save Okay; read-only file inspection confirms original footer/marker/version, no file changes. Fresh-process reload still pending following separate188 attract movie issue. Exact next step: coherent192 standard tests, then ordinary existing-save load with same roots and real saved-state equality; no fake progress or overwrite.


## MAC192 coherent camera/profile/movie class integration

2026-10-27. Debug and Speed complete native game builds pass after186/187/188
source freeze, including regenerated original Lua bundle. All12 affected checks
pass in each configuration with strictMetal: camera_ownership, ui_movie,
profile_save_owner, ui_save, amax_save_envelope, ui_world_borrowers, racebook_owner,
deferred_ui_messages, ui_required_resources, movie, replay_lifecycle and game_entrypoint.
Debug checks29.04s; logs standard builddirs/mac192-{game,build,tests}.log.
Actual194 starts original Master flow with genuine190 save and careercheat0;
not yet restart proof. Optional Debug CoreAudio asynchronous/static-disposal
stress failed in188 private audio variant;193 audits that distinct class.


## 2026-10-27 — MAC187 coherent checks and normal result follow-up

Parent MAC192 coherent Debug/Speed game builds succeeded; affected12checks PASS in each configuration, including ui_save, Amax envelope and actual Lua lazy-owner fixtures (standard mac192-tests.log). Normal194 restart using preserved190 real save is parent-owned and pending. Read-only mandatory normal race audit confirms live actors→owned C_LevelResult→value Lua snapshots→DoResults one-shot actual award→synchronous profile byte snapshot→asynchronous file write→real UI/audio/unlock/save-context completion. Original normal event AI/population/unlocks remain mandatory. Postrace snapshot precedes derived unlock screen; load runs original unlock processing again, so compare persistent event/fans/time fields at equivalent settled stages rather than assuming all animation/new flags are byte-stable. Popup value1 returns to event select; failed-result default0 retries. No new production edits, shared reads/builds or runtime actions. Exact next step remains genuine normal-event result autosave and fresh-process real load equality after parent194 base reload.


## MAC194 actual Master career reload and normal-menu path; audio shutdown failure

Actual Debug192 correctly reloaded genuine190 BLURSAVE version1378,50903bytes,
0unread, FINISHEDLOAD0; normal BlurAttract and47.38s firstCareer movies opened,
decoded and completed. Real firstCareer sequence autosaved without external
profile writes. Ordinary Left/Return reached SP hub, Races, first unlocked tier
and actual event4597671 (class3/very-easy/race,0stars/noresult), fresh rank1/fans0.
Finite Escape/Return backed out toStartscreen; CLI elapsed480guard initiated
normal app shutdown before final desktop popup. UI/World/Racebook retired, then
C_GameAudioLayer teardown asserted nonempty PagedArray40: exit250 after484.33s.
No clean-exit claim; original slice183 remains separately proven. Derived profile
proof/backups are ignored verification/mac194-normal-profile-proof.json and
mac194-after-intro-save.bin.195 audits recurring audio-owner class;196 captures
exact pool beforeassert. Prior harness final polling missed an assertion emitted
immediately before exit; now it scans final log as well as running output.

## MAC193 CoreAudio asynchronous disposal audit and bounded dependency experiment

Owned optional Debug188 audio IPS shows AudioQueueQueue system object disposal
calling actual engine global delete/BlockAllocator::Free while main is in process
static teardown. Current pinned SDL3.4.18 and inspected upstream main use async
AudioQueueDispose(0); SDL releases the callback device mutex before backend
close, supporting a synchronous-disposal candidate without that mutex deadlock.
No SDL dylib is present in the IPS, so the separately installed Homebrew3.4.14
was not proof of the failed binary's version. Allocator routing remains intact.
Private clean pinned3.4.18 baseline/candidate links each passed12 real UI movie
processes (36 cycles) plus12 immediate actual Movie decoder/Metal/PCM owner
processes (144 cycles), strictMetal and realCoreAudio, no timeout/crash. Both
immediate variants print runtime3004018. No controlled negative reproduction;
original rare teardown failure remains unresolved. Immediate disposal may cut
pending audio, whose audible tail these tests do not measure. The checked helper
is an unhooked proposal only, not recommended for production integration from
green-only samples. Exact sources, API contract, limits and integration proposal:
coreaudio-disposal-audit.md; private logs/archives under ignored
occlusion-agent-coreaudio. The actual194 frontend Audio PagedArray assert is a
separate195/196 owner investigation, not attributed to this optional stress.


## MAC196 exact before-assert stream owner proof

Actual Debug192 Master/profile restart underLLDB reproduced the194 shutdown
assertion at FileStreamManager destruction. __assert_rtn stack identifies
PagedArray<Audio::Core::FileStream*> → FileStreamManager → WaveServer →
Audio::System → C_GameAudioLayer::Destroy. This is distinct from175's late
static poll-observation table. Loader thread stop/join alone leaves real owned
stream objects.195 final-owner proposal preserves accepted-read buffer/file
lifetime, settles consumer/worker dependencies, then retires real graphs,
streams and refcounted wrappers; no assertion/pool-clearing bypass.196 wrapper0
is diagnostic-only: LLDB stopped beforeassert and killed its ownedinferior;
not a successful game exit. Full stack retained ignored mac196-game.log.


## 2026-10-27 — MAC191 bounded existing-movie Open audit/diagnostic

Actual190 Speed182 engine Open failed beforeFFmpeg for existing readable
112311932-byte BlurAttract; actual194 Debug192 samefile opened viaAsync131072
andplayed. No reproducedorcorrectedIOrootcause claim. Source distinguishes
originaloverlay/native translation, pre-Async construction/route exceptions,
sync path/OS/allocation errors andpost-entryqueue errors.190hasno unconditional
AsyncFile entry, so normalqueued-open/generation attribution isunsupported.
VideoStarted only game-stream flag; it doesnot selectsync orstopnativeIO.

NativeFileSystem.cpp existingOpencatch nowlogs failedoperation,selectedroute,
path/access andwhat(), adding capturedcode/category onlyforstd::system_error.
Noambienterrno attribution, reroute/retry/fallback orcritical/null policychange.
All4configproduction+unchangedfilesystemfixture compilePASS(8objects); private
Debug standardclosureexistingcompletefixturePASS/staticexit0, realmissingfile
logs sync/native-open/code2 thenoriginalnull/error. Initialrelativeargvfailed
fixtureworkingdirprecondition; absoluteignoredscratchpathpasses. No sharedbuild
orgamerun. Sourcefrozen/archive readreleased. file-open-failure-audit.md records
exactboundaries/limits; nextrootactualSpeedfailure(ifany) shouldusethisdiagnostic
beforeanyIOrepairproposal. Separateaudio195/movie188lifetimes remainownedelsewhere.


## 2026-10-27 — MAC200 normal career setup/config authority audit

Read-only audit of independent menu189431-variable XML and source normalMaster
→SPmenu→PrepLoading→SetupRaceFromEvent: debug_frontend onecar/Brighton/lap1 is
onlyGameModeDebug Start data, normalSPbeginsClear and rebuilds route/rules/cars
from real event/profile. Actual global lap/minor-character/force-split/AIthrottle
and classoverride gates audited neutral0/false; Shop override absent/defaultfalse,
C++Game.DevMode onlyHUD and MasterLuaDevModefalse. Profile/nativecontrols use
actualsavedkeyboardlayout(0Q/A/arrows,1WASD), notdebugcontrollerrecords; noforced
layout/progress. OriginalAI/eventdifficulty/ownedallowedvehicle remainauthority.
career-race-config-audit.md records exactscope/defaults/limits andread-only
GetLevelData comparison beforedriving. No production/config/save writes, builds
orruns. No correction justified. Nextrootactual198 normalcareer with authored
population/laps/realhumancontrols andnaturalresults/save; debug183slicePASS is
separate, additionalAIassets/combat/results notclaimed fromsource audit.


2026-10-27 MAC195 /root/dynamic_port: actual194 genuine career reload/movie/menu
normal shutdown failed; root196 exact before-unwind pool is Core::FileStream*
in FileStreamManager→WaveServer→System::Release, not earlier175 poll owner.
Native System.Shutdown now joins renderer then actual loader before graph/source/
stream/wrapper owner retirement; logical cancellation only after physical join,
no invented successful IO/readiness. Repeated partial-owner destructor fallback,
closed-generation acquisition guards, exact real DoStreamDestroy refcounts/handles
and unopened-wrapper allocation-scope RAII preserve original Windows/wire behavior.
Rejected reads no longer publish logical progress; rejected chunk-free enqueues
retain actual pointers for ordinary retry or joined final deletion. Native manager
layout requires coherent rebuild. New real System/blocked forwarding-file/queuefull
fixture tests actual acquired owners and borrowed-file isolation; no fakepoolclear.
16 production +4 fixture source probes all4configs PASS. Production/fixture frozen,
shared archive reads released; root197 standard coherent runtime pending. No actual
menu clean-exit claim. Full source/fixture limits in audio-frontend-retirement.md.

MAC195 final accepted-free refinement: native loader continues actual chunk
deallocation despite read-suppression debug flag (reads still obey it). Fixture
toggles flag from loader after two actual reads, before accepted queued frees,
then restores prior flag after joined retirement. Changed TU+fixture all4configs
re-probed8/8PASS; source/fixture re-frozen, no shared archive reads.

MAC195 MAC197 initial integrated leaf hit authored GateFs/C_FileSystem singleton
double-construction, beforestream exercise. Corrected adapter implements
I_FileSystem and forwards alloperations toexisting actualnativeFS, onlywrapping
real returnedfiles; restoresdefaultFS. Correctedfixture probes4/4PASS, frozen;
production unchanged. Rootstandardleafrebuild/runtimepending.

MAC195 further fixture/runtime review: initialize hostdefaultFS beforeasserting
GetDefault; actual FileStream::Setup forcesinitial1, so acquiredchunkcounts
followthat sourcecontract. Fixture exceptionowner nowunblock/joinsSystem before
scratch/FSdies. Private runtime exposed invalidnewcallerassert on original
DoStreamDestroy(bool alwaysfalse evenafterrealretirement); nativecaller now
verifiesactualhandleinvalidation afterunchangedDoStreamDestroy. No original
returnbehaviorchange/noassertbypass. Manager+fixture8sourceprobesPASS4cfg;
source/fixturefrozen, rootcoherentincremental+runtimepending.


## 2026-10-27 — MAC203 independent195 final-owner review

Read-only review of SystemShutdown→WaveServer/nativeFileStreamManager and195
real blocked-read/fullqueue fixture. No additional blockingproductiondefect
identified after correctednewcaller checksactual DoStreamDestroy invalidation
(legacyfunctionreturnsfalse evenonsuccess). Other checkedfunctions legitimately
returntrueafteractualgraph/source/wrapperretirement. Loader join precedeslogical
pendingcancel andownedgraph→source→stream→filewrappers; firstSystemShutdown
runswhilequeues/locks/Systemlive, laterdestructor repeat isempty/inert. Accepted
read metadata advancesonlyafterqueueaccept; rejectedchunkfrees retainmanager
ownership, retryorpostjoinrealdelete; closedgenerationrejectsandborrowedfile
isolation aremeaningfulfixtureboundaries. Noproduction/build/archive/runtime
operations byreviewer. Limits: focused195fixture deliberatelynorendererInit;
existingrealbankrenderercheck +actual198 mustprovepopulatedcallbackshutdown.
Noclaimofgeneralconcurrentnewclients, allocatorfailureorIOexceptioncoverage.

MAC195 exactstandardabscratchargv host correctedscopedcwd+shortbanknames with
original64limit retained; explicitScheduler/defaultFS/System fallbacks. Actual
partialCreate/rejectedoversizedsource/neveractivatedloaderShutdown PASS. Next
private firststreamcycle reachedallownerchecks butLLDBcaught ObjectManager
staticm_pfMixerFrames doublefreeonsecondSystemlifetime: dtorfreedbutdidnotnull,
Initonlyallocatesifnull. NativeexactownedpointerresetaftermatchingFree approved
andimplemented; otherdtorstatics auditednoadditionalstaleskipowner. Current
ObjectManager+manager+fixture12sourceprobesPASS4cfg, total24distinct5TUs+fixture
probes. Production/fixturefrozen, noarchivesread. Rootcoherent197runtimepending,
noactualmenucleanexitclaim. Proofmac195-agent/lldb.log, inferiorownedkill.

MAC195 final focused runtime: root197 rebuiltcoherentDebug+Speedapp/fixtures.
CorrectedstandardDebug mac_audio_stream_retirement PASS actualprocessexit0
0.69s (mac197-stream-tests.log). Truepartial/rejectedsource plusqueued/blocked
reads/fullqueuerejection/deferredfree/retry/postjoindelete/readflagfalse/borrowed
isolation/staticownerrecreate nowverified. Full19affectedchecks eachconfig
queuedbyroot; actual198 Master/Garage/ordinarymenuExit stillrequired, notclaimed
fromleaf. Production+fixturefullyfrozen; nofurthercodework.


## MAC197 final coherent stream and mixer final-owner integration

Final Debug and Speed games/affected fixtures build successfully;19/19 selected
checks pass in each configuration (standard mac197-tests.log). Included new
real stream fixture covers partial System acquisition, blocked native-file read,
queue-full acceptance rejection, retained/retried/free-after-join chunks,
closed logical generation, actual graph/source/stream/wrapper retirement and
borrowed isolation. It preserves original64byte bank-name and initial1chunk
policies. Earlier authored-host singleton/defaultFS/absolute-name/exception
scheduler cleanup mistakes were corrected without widening those policies.
Focused runtime identified original DoStreamDestroy's unconditional false return;
native caller verifies actual slot retirement. Multiple real System lifetimes
also exposed static ObjectManager mixer pointer freed but not nulled; matching
native reset now passes repeated acquisition/release. No pool/assert bypass.
Binary hashes/test times are ignored verification/mac197-integration-proof.json.
Actual198 now follows full Master/default profile through normal SP frontend;
no successful populated frontend exit claim until actual process0.


## MAC198 actual populated Master frontend and clean Debug shutdown

Actual coherent Debug197 reloaded real version1378 profile with0unread, followed
ordinary keys through SP hub/tier/event/pre-race and tutorial movie/autosave,
then original garage. The real selected car331339493 was owned with allowed~=false,
and no synthetic event/car/unlock/transition setters were used. Source QA207
separately distinguishes raw career lookup key4013244786 from resolved gameplay
eventId4597671; neither number was assigned externally. Private199 helper's
SpPreRace name typo was corrected, and startup IPC was gated after actual ready
UI because repeated loading queries could exhaust original debug peer slots.

Finite Escapes backed through the menus to SP exit popup. The private quit
observer expected a separate popup Lua context, but this popup broadcasts through
Main; desktop quit confirmation was not reached. The elapsed600 application
guard requested actual normal shutdown, all layers/Audio/static owners retired,
and the real process exited0 after605.95s with no primary error. No external
kill/timeout termination; do not claim desktop quit route or career results.
Runtime logs and snapshots remain ignored verification/mac198-*.

User subsequently authorized a public personal GitHub repository and initial
source snapshot. Created xharshvardhan/blur-macos under verified personal login.
Curated snapshot excludes private runtime/build data and inherited console
payloads/developer credential literals; local upstream/macOS history stays intact.
Public snapshot publication remains pending the final tree scan at this entry.


## MAC206 personal public source snapshot

User explicitly authorized a public repository and asked to use the personal
login. Verified authenticated GitHub user xharshvardhan/id90440920 and created
https://github.com/xharshvardhan/blur-macos. Prepared an independent initial
source tree, preserving the active local macos branch, ordinary index and
upstream history. Excluded inherited prebuilt/payload/log/private-key files,
the upstream release workflow and private runtime assets; credential getter
overrides are redacted only in the publication copy. Both Debug/Speed real
profile source compile probes accept that redaction. No blanket license added.

The curated source-only checkout configures the complete arm64 Debug game and
all native check targets successfully, reusing the existing pinned dependency
sources. The initial 19520-file tree contains no unexpected binary/private-key
material, no file above10MB and no excluded history ancestors. Public author
identity is the personal GitHub no-reply address. Destination main is reserved
for this independent publication history; local origin and macos remain intact.

Actual201 normal career event reached IntroVista, then failed strictly on
"Stencil clear has no stencil attachment" and exited1 with clean cleanup.
MAC208 audits pass-depth inheritance; MAC209 captures its exact before-unwind
caller. Actual183 and198 acceptance claims remain scoped as described above.


## 2026-10-27 — MAC209 actual pre-unwind stencil-clear proof

FreshhashcheckedDebug197/realMaster189data+profile+save, CompleteCareer0 /
ObserveOnly1; ordinaryStartScreen-gatedhelper navigatesactual unlocked event /
ownedallowedcar. Exactfiltered__cxa_throw at167.18s: RenderStencilBox→spotlight
volume→LightingPrepass→MainView Prepass1; actualClearflags4(stencilonly),
actualdepthSurfacepointer+controlblockNULL, colour0A8R8G8B8/2560x1440/sample1,
command+encodernonnull. PureSB JSONerrors[]. Supports208nativeBegin owner-policy
candidate, notwrongdepthformat orlegitimateignoredclear. No source/guardrepairby209.
ExplicitLLDBprocesskill owned26484(status9)+quit, wrapperdiagnostic0,
qualified_game_exitfalse/slotreleased, exactprivateprocessleftoversnone. Operator
stoppedonownedexit. Harnessprimarynullbecausebeforeapplicationcatch; exactthrow
capturedinignoredmac209/stencil-proof.json/log. stencil-clear-runtime-proof.md
recordsfields/caller/limits. No screenshot, sharedbuild/archivewrites, progression
mutations ornaturalrace/normalgameexitclaim.208repair/integrationisnextrootstep.


2026-10-27 MAC211 /root/dynamic_port: source-audited normal desktopquit after
198exit0proved elapsedguard cleanup, notpopupconfirmation. Repaired ONLYignored
quit_master_frontend.py: CustomPopup/PopupGUI/SCUI_Popup observedthroughMain0,
underlyingSpMain/StartScreen remainsGetCurrentScreen; noseparatepopupctxsearch.
OriginalExitApplication1 options/default/selectedYes verified, Kill204nooptions
onStartScreen, originalmodecameraBackguard; freshcontext/type/selectionfence.
FiniteordinaryESC/ENTERnativeTap only, uniqueACK, 16key/180sbound, exact127byte
maxcommand at mffff, no blindrepeat/directquit/profile/progress/selectionsetter.
After204ACK log/statusonly(no teardownIPC), exit0+noprimary requiredbyhelper.
Puredecisions/builders/selftest+py_compilePASS; no game/client/runtime/shared
build/archive/GPU/prodsourceactions. Sourceauditnormal-desktop-quit.md records
nonatomicsnapshot/concurrentoperatorlimits; actualrootUIquitverificationpending.


## MAC214 actual ordinary StartScreen desktop confirmation PASS

Fresh unique frozenDebug197 app, real Master189 data/profile/save roots and
explicit CompleteCareer0. The211 observer waited for actual StartScreen before
IPC, read original Main0 CustomPopup204/no-options/unfinished state, and sent
only finite ordinary ESC then ENTER. Both actual key acknowledgements were true.
After204 acceptance no further IPC was sent during teardown. The actual process
exited0 after79.09s with primary null, before elapsed180 application guard; no
external kill or timeout. Slot released, operator exit0. This verifies the
StartScreen desktop route; SP ExitApplication1 confirmation after populated
career play and full career results are separate. Actual198 already proved
populated Master shutdown via the elapsed application guard.
Private evidence verification/mac214-game.log, mac214-probe-status.json and
mac214-quit-operator.jsonl; no screenshot was required for the exit contract.


## 2026-10-27 — MAC212 independent deferred-depth review

Read-only210Manager Begin/current deferred_depth_test review complete; no blocking
issue identified. Sole actual manager producer LightingPrepass pairs same-extent
None-sample normal D24S8 + R32F and lighting Unknown-depth; actual normal-to-light
pass sequence retains owned D24S8 while separately detached R32F is sampled.
Native MRT retirement and requested depth-disable policy remain; strict lower
clear/format/extent/samples/feedback guards unchanged. Actual209 stencil-box
caller supports owner-policy restoration. Fixture source exercises original
manager/light mask/depth pixels, detach, invalid extent/samples, current-binding
replacement and two resource-baseline cycles. It restores default before manager
Destroy, so live-bound Resize/Destroy is not proven. deferred-depth-review.md
records source authority and limits. No production/fixture/CMake edits, builds,
runs or sharedarchive reads. Root integration/actual career retry next.

## MAC210 actual normal-career large-light stencil owner contract

209 before-unwind confirms201 primary: stencil-only clear4, null bound depth,
RenderStencilBox→RenderSpotlightVolume→LightingPrepass RenderLights1. Original
PC Manager::Begin replaces only a supplied owned depth target; native's explicit
null bind had dropped the prior normal-pass D24S8 that large lights borrow.
Native Begin now retains actual existing depth when this view owns none, while
retiring stale MRT slots and preserving requested depth-disable state. No new
surface, forced-simple-light policy, clear flag filtering or lower guard change.
Sole production setup client is Prepass: normal+lighting ±view pairs have same
extent/sample count; light shaders sample retired R32F MRT1, not borrowed D24S8.
Eight four-config private source/fixture objects PASS. Identical final Debug
fixture with197 manager exits1 at exact stencil-clear guard; corrected Debug and
Speed strictMetal GPU runs exit0 after2 lifecycles, actual volume-light stencil
mask520/1024, preserved depth, explicit detach rejection, owned rebind, allowed
larger depth and rejected smaller/mismatched samples, no extra resource owner.
New mac_deferred_depth CTest leaf registered (fullGame/non-UBSan hardware45s).
Private source/link/runtime evidence occlusion-agent-deferred-depth; scope and
live-bound-destruction limit in deferred-depth-ownership.md. Coherent actual
normal-career rerun remains required; fixture is not race proof.


## MAC213 coherent PC borrowed depth/stencil integration PASS

Full standard Debug and Speed native game builds succeeded with210 Manager
retention and the actual deferred_depth leaf. Both coherent configurations pass
14/14 affected manager/renderer/FX/resource/entrypoint checks under strict Metal
validation, serialized through run_exclusive. Native MRT retirement remains;
original PC absent-owned-depth retention is restored, not a lower guard bypass.
Agent controlled baseline reproduces exact missing-stencil failure; corrected
Debug/Speed actual manager/large-volume-light regressions each pass two full
lifecycles with520/1024 stencil-constrained GPU pixels, preserved depth, explicit
detach failure and extent/sample rejection. Actual normal-career run215 follows;
fixtures do not establish gameplay completion. Logs mac213-build/tests.log in
each build dir; binary hashes recorded in ignored mac213-integration-proof.json.


## MAC215 actual20-car normal career: startup/movement PASS, shutdown FAIL

Coherent Debug213, genuine Master189 profile/save/PC data, CompleteCareer0 and
ordinary menu/event/owned-car keys. Actual game progressed Loading20 → IntroRace
Starting20 → HUD Racing20. Original RaceLoaded and RaceStarted notifications
observed; completed native GPU frame captured once. Primary human controlType0,
configuredAI/aiToggle/automatedHuman false; 92 Racing telemetry records,
91 with accepted finite ordinary keyboard actions and 137.91m observed
travel. Real car331339493, genuine event configured3 laps; no checkpoint/lap/AI/
pose/event setup/progress writes. Menu observer retired onceRacing to reduce IPC
peer pressure; no gameplay/driver/process termination requested externally.

Elapsed300 application guard requested normal exit; actual process returned245
after302.76s, no textual primary error, last layer transitions RaceLogicLayer4
then0. This is a shutdown failure, not a clean gameplay pass or race completion.
No natural lap/finish/results verified. Before-unwind diagnostic216 and source
class audit217 own the unknown shutdown cause. Exact ignored proof/log/PNG
mac215-career-smoke-proof.json, mac215-game.log, mac215-keyboard-drive.jsonl and
mac215-game-frame.png.


## 2026-10-27 — MAC216 populated-career shutdown fault captured

Fresh Debug213 hashchecked private app/real Master189profile+save, CompleteCareer0
ObserveOnly1; ordinary realevent4597671/ownedcar navigation. Actual180s HUD/Racing20;
owned menuobserver retired. Elapsed240 shutdown exactmainthread EXC_BAD_ACCESS0x10:
GetActorPtr(thisNULL)171 ← GetLogicVehicleActor(i12)1123 ← PickupBase dtor147 ←
Mine/Bomb typedpoolretirement ← Pickups Delete/KillAll/Clear ← RaceLogicLayer
dtor220 ← NativeDestroyLayerNow185 ← NativeShutdownChildren481 ← OnShutdown781.
ActualBOMB unique17 firing12, samestillpublishedRaceLogic; confirms217 dependency
clearedbeforeconsumerretirement. All29threads pureSB, topcaptureerrors[]. Stale
unexecutedlocals explicitlynotusedasproof. Ignoredfull+compactJSON/logs and
career-shutdown-pickup-proof.md records evidence/limits. LLDBexplicitowned54667
kill9/quit, wrapperdiagnostic0/248.64s/slotreleasedtrue, noexactapp leftovers.
No production/build/progress writes, no normalgameexit or careercompletionclaim.
Parent/dynamic217 received exactstack; repair/integration/retry remain next.


## 2026-10-27 — MAC220 independent217 shutdown owner review

Current one-TU native217 candidate reviewed against exact216fault; no blocking
issue found. Native actorlistborrow survives Destroy through derivedtypedpickup
retirement/bodyremoval/deletednotifications; nativehelper Flush actualpickupgroup
beforeClear/poolfree, thenborrowcleared, expectedstaticbindingsdetached, basemessage
handlers retireagainstlivegamepool. Shockrecords/membermaps/taskgroup ownership
coherent; constructorNULLborrow/enteredflag/nonegroup partialcleanup checked.
Windows/quickReset retainedoriginalpolicy; no getterfallback orskippedcallbacks.
race-shutdown-owner-review.md records sourceauthority andboundedlimits. No code/
fixture/CMake/build/archive/runtime actions. Emptyfixturewouldnotreproduceactual
AI-firedbomb dependency; root218coherent+219populatedshutdown nextacceptance.


2026-10-27 MAC217 /root/dynamic_port: sourceaudit narrowed actual21520car
shutdown afterRaceLogic state0 toderiveddtor. Actual216 confirms ActorList
GetActorPtr(thisNULL)←GetLogicVehicleActor12←PickupBase/Mine/Bombtypedpool
retirement; liveBOMB17, SBerrors[], diagnosticinferiorownedkill9 (notcleanexit).
OneTUnativefix retainsborrowedactorListthroughDestroy→deriveddtor, flushes
actualpickupgroup BEFOREunchangedtypedpool/shock/body/deletedmessage cleanup,
clearsborroweronlyaftercleanup. NativeCtor initializeslookupNULL, enteredpartial
ctorcleanup uses samepreflush. Windows/quickReset/handles/getter/physicscallback
policyunchanged; noABI/nullskip/forcedpoolclear. Fourconfigsyntax4/4PASS, scoped
diffcheckclean; MAC220 independentreadreview noblockingissue. Sourcefrozen/root
readleasereleased. Noassetfreefake/no-pickupmirrorfixture: actual219controlled
20car AIpopulatedshutdown requiredaftercoherent218+10ownerchecks.
Sourceaudit race-pickup-retirement.md. No217game/debugger/build/archive/GPUrun.


## MAC218 coherent pickup dependency-retirement integration

Both standard Debug and Speed game builds succeeded with217 one-TU repair.
Debug10/10 affected layer/world/borrower/particle/entry checks passed. Speed9/10
passed initially; real world_transaction timed out90s at second failure boundary
afterworker-1 fileclose. Its focused strictMetal retry passed1/1 in0.28s; the
original timeout output remains in mac218-tests.log and audit222 investigates
its timing/worker handshake. Do not rewrite initial result as10/10PASS.
No production resourceworker or timeout changes made on this evidence.
Exact hashes/results in ignored mac218-integration-proof.json. Controlled
actual219 follows216 normal20car run with elapsed240 and no driver.

## MAC222 bounded resource pause/rollback audit; initial timeout retained

218Debug selection10/10PASS; initialSpeed9/10 with mac_world_transaction90.04s
TIMEOUT remains failure in macos-release/mac218-tests.log. Separate focused222
Speed retry1/1PASS0.28s does not erase it (LastTest nowcontainsretry). The fixture
fails an actual owning postprocess effect beforephysics/RaceLogic creation and
validates real worker/file reads afterfalse/throw rollback. Native owned pause
is source-balanced in both branches; no217RaceLogic runtime method invoked.
Final AsyncFile::Closing worker-1 log means closebegan, beforependingOps/native
closeWait, not completion; no timeout thread stack wascaptured. Read-only audit
records checkpoint/status-event, blocking-mutex and acceptedIO completion
contracts plus Stop/Join-vs-main-owned-blocking-mutex candidate, not a diagnosed
cause. No production/test/timeout edits, newruntime or weakenedfailureclaim.
resource-worker-pause-audit.md specifies needed all-thread/queue/IO state if it
recurs; root219 actualruntime remains separatelyowned.


## MAC219 actual normal20-car populated shutdown PASS

Fresh coherent Debug218 with217 final pickup task/lookup owner repair, same real
Master189 profile/save/PC data, CompleteCareer0, normal unlocked event/ownedcar
keys, elapsed240 guard and no driver as216. Actual original game progressed to
HUD/Racing20, emitted RaceLoaded/RaceStarted and captured one completed native
GPU frame at210s. Owned menu observer retired after Racing to reduce IPC.
Elapsed240 requested actual application shutdown; full RaceLogic/pickups/world/
Audio/debug/system/static/package cleanup completed and actual process exited0
after253.46s with no primary error. No debugger, external kill or timeout.
This verifies the exact populated owner class that crashed216 in bomb17's final
borrow of vehicle12. No natural race finish/results were requested or claimed.
Keyboard movement137.91m was separately measured215 before the teardown fix.
Screenshot verification/mac219-game-frame.png; logs/status and scoped acceptance
proof mac219-populated-shutdown-proof.json remain ignored. Normal career race
completion/results/save progress, full content, real LAN and portable packaging
remain separate work. The222 initial optimized resource-rollback timeout remains
recorded despite a passing focused retry; no speculative resource-worker change.


## 2026-10-27 — MAC224 private finite keyboard pacing operator

Ignorednew drive_keyboard_paced.py imports unchangedold telemetry/Hold transport;
original183adjacentpoint steering7/11m/s policy retained. Actual215 Qrequested
22.75/94.93swall(~24percent), median3.18m/s vs1839.47m/s; dutyaddressedfirst.
Q<=.75default, eachholdfinite<=1; independentaxes, .08pollsleep. ACKdoesnotstart
releaseclock: fresheligibleadvancingpostACKsample anchorsfullrequestedduration
wallfence+.08; unknown/rejectedHold stopsoperator. PLAYER0/nonAI/controls/AIR/
onroute/fresh/finite/advancing/actualseamguards; explicitlayout0confirmedgate
becauseoriginaloptionsgetterlackskeyboardlayout. Pure852recordreplay745eligible
plusguard/expiry/duration/seam testsPASS, pycompile/layoutCLInegativePASS; noIPC
ornativegame/build/productionwrites. keyboard-operator-pacing.md recordsinterface
andlimits. Root226ownsactualliveperformance/natural3lapcareerresultsverification;
replaydoesnotpredictdynamics/finish.

## MAC225 timeout reproduction/thread proof preparation only

Prepared ignored optimized world_transaction repeated-process runner +data-only
LLDB capture. Defaultplan dispatchesno runtime. Explicitrun nonblocking-owns the
native service slot, real original data/optimized shaders/isolated scratch,
strictMetal, atmost10s totalcycle budget withslow-cycle all-thread sample and
optionalbounded ownedPID SB snapshot before termination. First nonpassing or
diagnosedcycle stopsseries andkeeps logs/status/sample/availableJSON; sampled
exit0isnot untouchedpass. Source/test/CTesttimeout/sharedarchives unchanged.
CPU-only Python surrogate termination/deadline selftest andsyntax/plan PASS;
noGPU/Artery/Automation/Worldfixture launched, no queuedstress while rootlong
career ownsruntime. resource-worker-stress-plan.md lists exactfutureCLI/limits;
actualreproduction pendingrootrelease. Initial218 Speedtimeout remainsunexplained.


## MAC226 normal career first startline and new alpha-coverage feature boundary

Frozen Speed218 on genuine189Master/profile/save roots, CompleteCareer0,
ordinary menus to original20car3lap event. Independent realstate backup stored
before launch. Paced224 finite Q/A/Left/Right actions preserved primaryPLAYER0
with configuredAI/aiToggle/automatedHuman false; actual startline rawlap0→1
(completedlap0, not a completedrace) observed. Newly visible geometry then
failed strictly: Native alpha-to-coverage requires multisample integration.
Actual process1 after234.79s; full original error cleanup/layers/owners clean,
no externalkill/timeout. No natural3lap finish/results claim. Baseline actor
retirement217 remains intact through this error path.227 owns exactcaller
diagnostic;228 owns originalPC-to-Metal coverage state class. Private logs/
input/notifications/proof under verification/mac226-*.225stress begins only
after226slot release, then227 actual run follows.


## MAC223 — Genuine normal-career results/save-return source audit

Read-only original StreetRace/rules/AIR/UI/SP/save authority is recorded in
career-results-flow.md. Global RaceFinished is also emitted on wreck/failure;
normal3lap acceptance needs ordered completed LapInc0:1/2/3 (rawlap4), real human
Racing/control flags and actual HUD terminal UI pass, then original results.
Post-finish forced AI camera control is original behavior. Outside-top3 result
state none can physically finish and earnfans but grants no medal/timeBest;
postrace options159 defaults to retry0, while desired return is actual value1.
Added ONLY ignored career_results_operator.py, reusing passive tagged Observer.
Finite Enter/Up awaits actual photo/animation/options identity, selects return
through original keys, observes fresh autosave and SP menu, never calls result/
progress/save/transition/quit setters. Twelve-key/300sec bounds, fresh fences,
127-byte source envelope, unknown-popup refusal. Pure decisions/builders,
expired/refused keys and py_compile PASS; no game/IPC/build/archive/GPU used.
Helper exit0 proves only ordinary return/newsave/menu observation, explicitly
not physical finish or disk/restart qualification. Root226 genuinecareer run,
actual award delta and sameidentity saved-record reload remain separate.
No production code or original data changed.


## MAC230 — Actual worker stop ownership after optimized World timeout

229 fullsample/LLDB proves main LayerManager::NativeQuiesceCreation Join, live
LayerManagerThread EventWait, no ResourceLoadingThread and idleIO, not a file
close/blockingResourceMutex diagnosis. Manager.h19 defines REMOVE_BLOCKING_
RESOURCE_MUTEX;222 candidate is dormant. Real Resource/Layer workers both
constructed same named EventSignalQuit; native named desktop auto-reset event
sharing allows one private owner to consume the other's quit. Original PC
CreateEvent name contract remainsunchanged; native's actual join/return final
lifecycle requires private signals instead of originalPC TerminateProcess.
Changed only2 native constructor branches to unnamed per-owner events; noABI
or genericMT policy/Windows change. New real worker_stop_ownership fixture on
untouched218Debug baseline isolation exits1 'Layer quit was consumed by Resource
worker'. Fixed2TUs+fixture compile4configs12/12; private readonly-oldarchives
with explicitchangedobjectoverride link4/4 and isolation/fullCPU runtimes8/8
exit0 inclbothUBSan. Fullactualmanager callbacks:24resource payloads/12layer
restartlifetimes +32earlyquiesce/restart, reciprocal/same-family isolation and
genericnamed-event contract. NoGPU/Artery/game/sharedwrites. Source/fixture
frozen, archive lease released; root owns standardleaf/coherent app builds and
unchanged originalPAK/GPU World stressacceptance. Initial218/225/229failures
retained; no timeout extension/check removal. Focused source/proof/limits in
resource-worker-retirement-audit.md; private logs/linkcommands build/mac230.


## 2026-10-27 — MAC227 actual mirror crowd alpha-to-coverage proof

Freshhashchecked Debug218/Master189profile+save/CompleteCareer0 observeonly strict
Metal diagnostic afterexplicitroot229slotrelease. Firstmenuhelper refusedbefore
logexisted/noIPC; retryafterStartScreen normal event4597671/ownedcar. Actual255s
HUD/Racing20; retryobserver retired,224finiteordinarykeys. Exact__cxa_throw:
ContextBase SetAlphaToMaskEnable True485 ← CrowdFlags RenderMain527/Render513 ←
SceneGraph ← MirrorView Render239. ActualupperMSAANone; colourA8R8G8B8/depthD24S8
both1152x192/samples1, otherMRTnull; livecommand+encoder, VS1086/PS1088, oneflag.
PureSBerrors[]/allthreads; no unexecutedgeometrylocalclaims. Ignoredfull+compact
JSON andalpha-coverage-runtime-proof.md record limits. Owned91179explicitkill9/
LLDBquit, wrapperdiagnostic0/367.93s slotfree/noexactapp leftovers; helpersended.
No careerfinish/gameexit0 claim, source/build/statebypass. Parent+228 exactfacts
sent; featureintegration+actualretry next.

## MAC228 — native alpha-to-coverage (27 October2026)

Actual227 before-throw proof identifies `CrowdFlags::RenderMain`527 explicitly settingTrue inMirrorView,1152×192 A8R8G8B8/D24S8 sample1, no MRT1..3; raw enum valid, no missing-resource or wrong-platform inference. Lower requested coverage bool now reaches Metal pipeline descriptor and cache key independently of blend/test, preserving alpha-to-one=false. Upper Context forwards strict valid enums; upper Device retains its original PC state cache and false DefaultStates reset. No target ownership, samplecount, asset, clear flag or cutout changes. Existing PC/GCM branches preserved.

Private source probes24/24 (four implementation objects and two fixtures across four configs), fixed GPU4/4 Debug/Speed/DebugUBSan/SpeedUBSan exit0 under strictMetal. Real1x/2x/4x MRT coverage, preserved resolved alpha, independent blending/alpha test, real covered depth/stencil writes, partial-clear/default/target cache restoration and invalid-request retry pass; return-to1x also passes. Controlled old-Context baseline exact guard fails1 clean. Artifacts `_work/build/macos/occlusion-agent-alpha-coverage/{runtime-results.json,...}`. First fixture alpha assertion failure was RGB-only default write mask; explicit All-channel test setup fixed it without production change. ExplicitSV_Coverage parity remains outside current693source shaders and is documented, no speculative parser. Shared218 archives untouched, runtime slot released. Root coherent231 and normal-career acceptance still pending.


## MAC225/229 actual repeated stall and successful thread proof

Unchanged Speed218 real World rollback fixture:225 first3 freshprocesses passed
~.21s, fourth stalledsameboundary1 and owned10s watchdogcleaned it; first
threaddiagnostics timed out beforecapture. Dedicated229 unchangedfixture had
onepass then stalled. Its30s diagnostic budget allowed successful native sample
and data-only LLDB allthreads/SB capture before ownedwatchdogkill29.56s.
Main was joining LayerManagerThread during application destruction; that worker
waited on itsEvent, resource loading worker hadalreadyretired and bothI/O
workers wereidle. No resourcepause/close/blockingmutex diagnosis selected from
staleClosing stderr. Source230 proves two actual workers consumed the same
named EventSignalQuit: cross-owner stoptheft, not main-ownedblockingmutex
(the latter is compiledout). Native privatequit ownerrepair and deterministic
realworker baseline/control+async lifetime probes frozen;231coherent integration
follows. Original21890s timeout remainsbaselineevidence, not erasedbyretry.


## MAC231 coherent alpha coverage/private worker quit integration PASS

Full standard Debug and Speed games built with228/230. Both pass19/19 affected
coverage/renderer/deferred/context/layer/worker/world/entrypoint checks under
strictMetal. Unchanged optimized World fixture then passed10/10 freshprocess
cycles without diagnostictrigger/watchdog ortimeoutincrease, retaining the
originalfalse/throw rollback and actualworker resourceproof. Native individual
quitEvent owners fixthe stack-proven shutdownstoptheft; genericNamed contract
andWindows path remain. AlphaCoverage isrealpipeline/cache state at1x/2x/4x,
with no forcedsamplecount/asset edit. Exacthashes/timing underignored
mac231-integration-proof.json and perbuild mac231-build/tests.log. Original
218failure and captured225/229baselines retained; fresh232normalcareer follows.


## 2026-10-27 — MAC233 private guarded transport resilience

Newignored drive_keyboard_guarded.py imports224planner, retainsfullCLIexit/stdout/
stderr/classification andeachonekeycommand carriesuniqueexecutiontag+raceTime
+fresh1.5simsecdeadline within127bytes. Definitebootstrap/AddPeerfail retriesonly
newfreshdecision; ambiguoussend/reply/disconnect/timeout neverblindlyreissued.
Uncertaininput freezesallkeys untilfreshadvancingpostdeadline sample +fullhold
walltime+.08+anotheradvancing sample,15swithoutproofstop. Acceptedkeys retain
postACK/postpollaxisfence; prematurefalseHold stopsunknownowner. Originalhuman/
route/freshness/layout/finiteguards unchanged, noengineprogress/AI/posewrites.
852actualrecordpurechecks+7mockrealtransportclassifiercases+syntaxPASS/noIPC.
Source/knownclient16362–16375correlation supportslegacy232KJ247preLuaAddPeer
rejection (oldrawCLIstatusnotretained); parentreviewedandowns232continuation.
First117liveguardedrecordsread-onlyaudit nohelperfault; actualtransportrecovery
notyetobserved. keyboard-transport-resilience.md recordsprotocol/limits/CLI;
no production/sharedbuild/GPU/gameexecutionby233. Rootruntimefinish/results
remainrequired, helper0notgamefinishproof.

MAC233 bounded follow-up: first411live guarded records include244accepted
commands+19explicitguardskips; no capacity/deadline/control violation, no adverse
transportrecoveryevent observed. LastobservedRacing/onroute/wrongWayfalse,
distance1109m/rawlap1of3; nofinishclaim. Source/helper remainedfrozen.


## MAC234 personal public integration follow-up

Curated separate-index update includes accepted228 Metal coverage,230 private
worker quit ownership, standard fixtures/CMake leaves and bounded audit docs.
Coherent231 Debug/Speed each19/19 affected checks; ten fresh optimized World
rollback runs pass without a watchdog or timeout change. Public source-only
checkout synchronized from the exact curated tree and full CMake configure
passes. First configure used a stale checkout missing the earlier public
follow-up deferred-depth fixture; exact-tree extraction corrected that staging
checkout, with no production-source or active game change. Original working
branch/index/history and publication credential mirror retained. Personal
author/login verified; no private game/save/build payload added.

MAC232 is still running separately: first genuine completedlap1 notification
and rawlap2 observed under finite ordinary human control. Complete3laps/results
and save-return are not yet established. Existing300s capture was taken while
parked beside wall/ledge and has severe foreground occlusion. Independent
read-only review cannot distinguish camera/nearby geometry from a placement
defect; it is not evidence of an alpha-coverage fault or full visual fidelity.
No guessed rendering change or extra capture was made.


## 2026-10-27 — MAC235 relocatable app distribution audit

Read-only actualSpeed231 appSHAa2502036…df2c6: arm64exe minOS13 but9direct/
21nonSystemdylibs from16Homebrewpackages ALLarm64/minOS26; no unresolvedhost
refs, noFrameworksbundled. BlankbundleID/versions/minOS; linkeradhoconly
plistunbound/resourcesunsealed.693bundleshaderfiles5.3MB/NSBundlepath, original
datachooser/externalprofiles+saves recovered. Existing stage_bundle.cmake already
reuses maintainedCMakeBundleUtilities fixup_bundle/verify_app/transitiveclosure,
derivedminOS andadhocsign; recommendfinishthatpathnotnewrewriter. Noticesfrom
SDL/SoundTouch/Mojo currentlybesideapp lostbyapp-onlystage; actualHomebrew16
SBOMs available, FFmpegGPL/version3/x264/x265 exactconfig documented. Primary
CMake/Apple sources checked; optionalCPackDMGlater. distribution-readiness-audit.md
recordsmetadata/notices/signing/minOS/relocation/testplan/limits, ignoredclosure
JSONinpublication/mac235. No package/install/build/GPU/input/source changes.

## MAC238/240 — original photo-start native scene owner

232genuine3-lap normal20-car completion reachesSpFinished then strictlower BeginScene error. Source audit: GameApp.OnRender callsPhotoCapture.OnAppRender beforeRenderNow/BeginRender; RenderSingle callsAppPhotoCapture.RenderStart→AccumulateStrip.Start→ClearTextures beforefirstRenderAddSample.BeginScene. Actual237before-unwind proof pending. Priorphoto fixture manuallybeganScene beforeallStartcalls.

240native owned-scene policy movedfromvolatileCreate-only tosharedClearTextures; preserveborrowedactiveScene onnormal/error, requirednewBegin failuretruthful, ownercleanupEndScene onthrow, volatileallocationrollback remainsacquisitionboundary. OnlyAccumulateStrip.cpp+photo_test.cpp changed (sourceASCII/Latin1preserved); Windows unchanged. NewfixtureoutsideStart1/2sample +realCPU-mapconflict/ownedversusborrowedthrowcleanup/retry joins originalGPUstrip/readback/recreate/teardown checks. Privatefour-configsourceprobes initiated; noGPU/sharedarchivewrites while237ownsdiagnostic. Ordinarysingle-playerPause→Photo→MenuNext isshorterfaithfulapprepro; noforcedresults/progress.


## MAC239/241 — Postrace photo owner audit and ordinary reproduction operator

232 actuallastUI SpFinished/Finished20 after genuine3laps; errorBeginScene then
exit1, noresultsoperatorinput. SourceGameApp OnRender callsqueuedPhotoCapture
beforeRenderNow/BeginRender; RenderSingle→RenderStart→AccumulateStrip.Start
clearsoutside scene beforeRenderAddSample BeginScene. ExistingCreateVolatile
hasownedsceneclear wrapper; photo fixture begins scene beforeStart andmisses
actualapporder. Exact237firstthrowpending;240graphicsownsclassrepair. Source
audit doesnotclaim unseenawards/timelinecallactuallycaptured. Ordinaryoffline
Racing HUD F1→Paused actualunlockedPhoto carousel→PhotoModeCamera Enter reaches
sameAppPhotoCapture withoutresult/finish/selectionsetter; Garagehasnosuchroute.
Onlyignored photo_mode_operator.py added; twelvekey/180s finite F1/Right/Enter,
Main0 actualscreen/mode/branch/pause/readiness/freshnessfences,127bytebuilders,
unknown/refusedinputstop. CameraEnterACKthenALLIPCstops; helper0onlyacceptance
notphoto/process/diagnosisproof. Pureguardtests+py_compilePASS, noIPC/build/game/
GPU/productionchange. Parent237ownsactualruntime, postrace/save remainsseparate.
postrace-scene-lifecycle-audit.md records sourcepaths/testgap/CLI/limits.


## MAC232 genuine normal career finish and post-race scene failure

FrozenSpeed231 actual20car3lap normalMaster event4597671/car331339493
completed original ordered split1/2 crossings and LapInc0:1/2/3, then
RaceFinished/rawlap4/vehicleState1.2996 observed Racing records retain
configuredAI/aiToggle/automatedHuman=false. No pose/checkpoint/lap/result writes.
Early224 operator stopped on source-proven pre-dispatch peer rejection; guarded
233 resumed ordinaryfinite input and reached naturalfinish. OriginalSpFinished
screen then exposed Draw operation requires BeginScene; actualprocess exited1
after1762.3s with full error cleanup. No resultsoperator ran and savebytes
remain unchanged. mac232-career-proof.json records the narrow pass and failed
results/save/exit gate; wrapper lap_qualified_finish=false is one-lap autoquit
bookkeeping disabled in observe-only mode, not a contradictory physicalproof.
238/239 identify automaticphoto RenderStart before firstsampleBeginScene;240
sharedphoto-clear class repair and237actualPhotoMode diagnostic follow.

MAC241 actual-root passive-query correction: original helper bareIsNetworkRace
was Paused-only global, nil inHUD; ProtectedLoadRunString error beforeanykey.
Privatehelper now actualregisteredAmax.IsGameModeSplitScreen/IsNetworkRace on
all3screens, per-screen fieldaudit, sourcebinding checks +terminalfreshLuaerror
classification ratherthan repeatedtimeoutretry. No productionfault or actual
photo proof inferred. Correctedpuretests/syntaxPASS; noIPC byagent, root237
owns correctedactualoperator/capture. Sourcefrozen again.

MAC240 validation afterexplicit237release: actual237pureSB errors[] confirms ClearTextures→Start→RenderStart→RenderSingle→OnAppRender→GameAppOnRender beforefirstsampleBeginScene. Diagnosticonly ownedkill, notcleanGameexit. Privateold231baseline exactBeginScene diagnostic exits1clean with realbuiltinshader/devicecleanup. FixedDebug/Speed/DebugUBSan/SpeedUBSan4/4 strictMetal+haltUBSan GPU exit0,0.56–0.79s;8/8 sourceprobes. OutsideStart1/2samples andborrowedStart, realmappedtargetowned/borrowedthrowcleanup/retry, exact34×18pixels/partialstrip/RW/recreate/teardown allpass. Logs+runtime-results.json underocclusion-agent-photo-scene. SourceLatin1preserved, Windowspathunchanged, lowerguardnotrelaxed; allprivatearchiveleases/runtime released. Root242coherentbuildsdone; integratedchecks andfixedapp/results acceptance pendinghandoff.


## MAC237 actual ordinary PhotoMode class proof / MAC242 integration

FrozenDebug231 normalMaster20car race, finiteF1/right/Enter throughactual
unlockedPausedPhoto/Camera. No progress/result setters or syntheticfinish.
Before-throwpureSB237 errors[] confirms require→Impl::check(active=true,
command=nil)→Clear→AccumulateStrip::ClearTextures216→Start256→
AppPhotoCapture::RenderStart182→PhotoCapture::RenderSingle155→OnAppRender
→GameApp::OnRender1056. This confirms the real application class viaPhotoMode;
232's exactpostraceUIcallback was not itself captured. OwnedLLDBinferior killed
afterproof, wrapperdiagnostic0/573.02s and slotreleased, not clean gameexit.
First241helper stoppedbeforekeys afterbareIsNetworkRace querynil; corrected
registeredAmax getters reachedtheoriginalcapture with7finitekeys andnootherIPC
afterACK.

240class repair/fixture8of8sourceprobes, old231baseline exactguard exit1clean,
fixedfourconfigGPU4of4 understrictMetal/UBSan. Coherent242 standardDebug+Speed
apps build andeachpasses8 affected photo/coverage/context/feedback/renderer
transaction/graph/engine/entrypointchecks. No lowerguard relaxation, samplecount
change, originalPCflow change or resultscallbackshortcut. FrozenSpeed242 actual
243 manualphoto→resume→normal3laps/results/save-return follows.


MAC241 explicitreturnextension sourcefrozen: --return-to-race preservesdefault
captureACKstop, optin waits2freshconsistent Viewing/UI-restored negativeflag
snapshots thenoriginalESC Viewing→Camera→Paused→HUD/unpausedRacing viaactual
MenuBack handlers/EndPhotoCam/UnPause. Separatephasepolicy neverreshootsCamera,
neverEnterViewing/photo-save, max16keys/180s,127bytes andoriginalsourceguards.
Puredecision/mode/readiness/actiontests+syntaxPASS/noIPCbyagent. Exit0labels
render/UI lifecycleonly, notphotoGPU/progress/finish/process success. Root243
ownsactualcombinedSpeed242 photo/return/humanraceacceptance.


## MAC243 fixed real PhotoMode lifecycle / MAC244 local stage

FrozenSpeed242 actualnormalMaster20car race. Ordinary10-key F1/carouselPhoto/
CameraEnter andEsc return sequence reachesoriginalViewing twicewith
enable_render_in<0, healthyfreshstable snapshots, thenCamera→Paused→HUD/
Racing/unpaused. No saved-photoEnter/result/progress setters. Exact240missing
scene class no longer recurs; fixture supplies exactGPU pixels separately, UI
lifecycle alone isnotclaimed as a pixelreadback. Guardedfinitehuman driving
resumes toward original3lapfinish/automaticpostracephoto/results. Private243
launcherlogmarker was overescaped, so its automaticobserverretirement marker
didnotmatch; actualfrontendobserver hadalreadyexited and root confirmed no
competinginputhelpers beforePhotoMode. Game unaffected; runtimeproof records
manualhandoff.

Unchangedexisting CMake BundleUtilities stagedfrozen242 into newignored244app,
rewrote actual transitiveMachOclosure andpassed strictadhocsignaturevalidation.
Allnon-systemabsolute dependencies absent, actualbundled minimumOS26.0.
This replaces236old231binary as candidateforlaterrealrelocatedreload. Metadata/
notices/distributionsigning/cleanmachineacceptance remain235audit work; no
retaildata orrelease included.

MAC245 source-only diagnostics audit: NoDObjMesh missingbit1 check remainsactive butonlyobjectcollider setter iscommentedout; activeworld/newcarphysicscolliders separatebits, no missingrendermesh proof. InvPartSC licenseplate front_hull/back_hull failsexactlocationname lookup, then skipsper-part dynamicmaterial parameterassignment; SetupDynamicMaterials3calls explainsrepeatednotifications. NoOSbranch atparser/guard; actualhullprimitive/physicsmetadata notinspected, so required/optional classification remainsunproven. AIBROKEN usesAllOK (includesm_safeToSimulate) atRLC POST_INITIALISE beforeStreamerStarted/InitRace; historicalMAC114samefinding. Legacyanalysisfalse latch/laterAIconsumer correctness notcertified. Existingrequiredmodel/wheelselection/renderlayerstatus contracts distinct fromdiagnosticlabels. No production/fixture/build/runtime/input/capture changes; visualfidelity remainsbounded/unverified. Seevehicle-diagnostic-contracts.md.


## MAC246 personal public photo ownership snapshot

Curatedsourcefollow-up includes sharednative photo-clear scene ownership and
meaningful outside-scene baseline/control fixture, acceptedfourconfig GPU
checks, coherent242 Debug/Speed builds/eightchecks each and actual243original
PhotoMode capture/return. It also records actual2323lapfinish/postracefailure,
245bounded legacydiagnostic audit and235localdistribution limits. Current243
fullcareerresults/save-return remains explicitlypending; no finishedinstaller
orfullgameparity claim. Sourcepathsunchanged/CMakeintegrationalreadybuilt, no
redundantfullconfigure. Separatepublicationindex retains exactredactedprofile
mirror and sourceonlyexclusions, originalbranch/defaultindex/historyintact.


MAC223/243 source interpretation correction: UIterminal25870352 is actual
RaceFailed (LuaVariableHelper32–33), not necessarily physicalDNF. RaceRules-
Manager388–406 resolvespass/fail AFTER rawlap>configured; unsatisfiedloadedpass
rules maySendFail atgenuinecompleted3laps. FinishInXPosition39 evaluatesactual
position againstloadedthreshold; StreetRace375→UIFailed/globalRaceFinished;
HUD762 accepts RaceFinished ORRaceFailed intooriginalresults. Position20→medal
none matchesfailedobjective, notwinning/unlocks. Earlier223UIpass-only physical
qualification wasTOOSTRONG, doccorrected. VehicleState1=RACING notFINISHED(2),
so rootorderedlaps1/2/3/raw4/nonwreck/human/RaceLayerFinished supplyphysicalproof.
Actual223helper alreadyordinaryreturned159value1/newsave0/SPmenu; emittedfans60
butattributablegainrequiresbeforebackup. Sourceaudit makesno process/reload claim;
rootownsacceptance, faultyUIassert/shellcontinuation mustremaininrecord. No source/
helper/progress/runtime/build change bythiscorrection. career-results-flow.md.


## 2026-10-27 — MAC247 bounded development bundle staging

Existing stage_bundle.cmake now retains BundleUtilities as sole resolver/rewriter,
new/outside-source destination and raw-payload preflight; meaningful validated
development metadata defaults/overrides. New bundle_notices.py consumes original
CMake resolved keys and copies3adjacentnative notices +actual Homebrew package
notices/installedSPDX intoResources/ThirdParty, relative hashedmanifest; missing
provenance/SPDX/notices failsclear. Inspect arm64/minOS/noexternalruntime refs,
signnested dylibs thenapp, finalstrictsignature+manifestverification. No blanket
licenseclaim. stage_bundle_test.py actual frozenSpeed242 aedb9756…60c56 PASS
newignoredmac247-stage:21deps/16packages/57noticeSPDXfiles+3native, derived26.0,
metadataoverrides, strictsignature, sourceapptreeSHAunchanged2a4ce175…9a41,
new-destination/missingSPDX/notice/invalidID rejection. Addedauthoredforbiddenpak
preflight alsoPASS; noactualstage repeatneededasproductionunchanged. Syntax and
whitespace checksPASS. development-bundle-staging.md records usage/scope/limits.
No game/GPU/Artery/build/sharedarchives/engine ororiginaldependency/sourceapp
mutations; MAC253 cleanmachine gameplay/reload/quit +publicsigning/notary remain
unverified. Parenthasexactstagedpath, localadhoconly.

### MAC247 follow-up — preserved relocated bundle seal

Parent MAC249 found its private default copytree flattened seven Frameworks
symlinks; original MAC248 stage remained valid. Production bundler unchanged.
stage_bundle_test.py now copies a signed stage with symlinks=True, compares
symlink topology plus contents/modes, verifies strict deep signatures and full
manifest/notice/signeddependency hashes on both copies, and checks the original
staged app remains unchanged. Added --staged-app mode for this bounded check;
normal raw-app regression runs the same relocated-copy check automatically.
tree_digest now records aliases as aliases before following regular files.
Actual MAC248 copy into new ignored mac247-followup-copy PASS, seven aliases,
no re-signing, original unchanged; verification.json and separate log preserve
prior247 proofs. Python syntax and scoped whitespace checks PASS. Staging doc
requires a symlink-preserving copy (Finder/ditto/copytree symlinks=True), not
re-signing as repair. CLI only: no game/GPU/service/build/archive changes.


## MAC250 — Actual LAN host/join readiness source audit

Boundedreadonly Master→Multiplayer→LAN/offlinepopup86→browser→ordinaryButtonY
→ModeSelectCustomRace2→actualhostLobby source mapped inlan-actual-readiness.md.
Join mustselectactualenumeratedcompatible row+originalasyncpopup8/handshake.
Read-only registeredUI/router/lobby/realSlots queries identified. Noengine/code/
protocol/account/room/ready setters, build/GPU/runtime/input. NativeEntryRequire-
ments(false) stillreturnsunknown4(PCguardexcludesMac); LANUIonlyspecialcases1
soitcontinues—recordnotfake success. OfflineRouter reallyrequiresbdNetDONE,
actualkeys/players beforeCreateLanRunning; popupsuccessalone insufficient.
Sourcebounds20networkvehicles/8party/1local; browser10servers notplayers;
6syntheticdiscoveryquerytests not4gamejoins. Onehostsmokecanproveoffline
realhost/roster/advertise/leave/exit, normalraceStartrequiresremote>0 with
AllowSingleRacer false. SameMac endpointsettings/freegameports existbutcurrent
Artery/serviceflockstillserializesactualapps; no bypass/multiapp launch. Future
coordinator underonelease separate, real4independentclients/machines required
for>3 historicalissue acceptance. Root249keptservice/inputownership. Scoped
diffcheckclean, noactualLANproof/newconnectivityrepairclaim.


## MAC243 normal career cycle PASS (non-medal result)

FrozenSpeed242 actual20car3lap event4597671/car331339493,2881 observedRacing
records withallAIflagsfalse, orderedoriginalstart/splits/laps1/2/3/RaceFinished,
rawlap4. OriginalUIRaceFailed25870352 reflects20thplace/unmetcareerobjective;
this isfullphysicaldistancewithstate'none', not medal/unlocksuccess. State1
isRACING/nonfatal snapshot, not itself finishproof. Resultsphoto nowcompletes;
originalawardsNext +popup159Up/Return1 returnsSPmain, freshFINISHEDSAVING/
local_error0 +FinishedSuccess +inactiveLoadSave. On-disk save changes2028→2207
bytes/hash603efcf7…, liveGameInfo rank1/fans60/stars0. No earned-fansdeltaassert
fromunknownprebackuprecord. Original7keySPexit/Startdesktop204 confirmation
endsactualprocess0/1546.35s/no primary, wellbefore3300guard. FirstrootUIpass
assert wasoverstrong forfailedmedal andshellcontinuednextcommand;223audit
corrected andfullphysicalproof retained. No progression/result setters.

## MAC248/249/251 relocated profile reload and signed-copy verification

247tool default-stage248 hasmeaningfuldevelopmentmetadata,21actualdylibs,
16packageSPDX/notices +3native notices andactualminOS26.0. Actual249 reload
readsversion1378/51227bytes/0unread, retainsrank1/fans60/stars0/eventnone/car
331339493, realownedgarage,21runtimebundledlibs/noexternalnonsystemlibraries
underminimalPATH/unsetdyldpaths; ordinaryexit0/246.35s. BUTprivatecopytree
flattened7libraryaliases, invalidatingthatcopy'sresource seal (original248valid).
No re-sign repair: privateharnessesnowpreservesymlinks and251repeatsactual
reload/records/Garage/dyldproof withstrictsignature+manifestbeforeANDafter
ordinaryexit0/196.35s. All21non-systemlibraries loadfrom251app, noHomebrew.
247CLIregressionnowalsochecksrealrelocatedcopy/topology/originalimmutability.
Theseare same-host relocatedruns, not anothercleanMac validation.

## MAC254 local development ZIP delivery

Signature-preserving default248app copiedintoignored _work/dist development
folder withlocalREADME, ZIPcreatedandextractedviaApple ditto. Extractedapp
passesstrictdeep signature/notice/dependencyhashmanifest unchanged; rawupstream
removedcredentialliterals absentfromnativeMachO (valuesneverprinted). ZIP
27422927bytes, SHA295f1fceee30a0432b8a487e02c1c2d8015610065eb6e85362c8246da69e6384.
No retaildata/profile/save included andnopublicbinaryrelease. Thisisarm64/
macOS26 adhocdevelopmentbundle; cleanmachine/LANmultiplayer remainunverified.


MAC252 — Prepared ignored lan_host_operator.py; no IPC, game, build or GPU work.
Pure ordinary-key decisions, refusal/stale/127-byte builders and py_compile PASS.
Source-gated Master stage0 Enter → original profile boot/stage2 → MP/LAN →
actual offline router Complete (popup fallback alone refused) → RControl Host →
actual CustomRace2 → real host roster/start policy. Known native entry ret4 is
recorded separately, not auth/router success. Requires actual one-local/zero-
remote roster with matching actual configured capacity<=20 and both real
CanStartCustomRace/HostCanPressStart false; never enables single-racer bypass.
Optional original leave: lobby Escape/popup4 Yes/automatic6/browser Escape/
MP root Escape/popup2 Yes/StartScreen2; no direct disconnect/transition/setter.
Corrected250 documentation typo ExitRaceLobby2→4 before any runtime. Private
helper frozen and exact CLI handed to root253; max24 finite keys, fresh actual
contexts/selection/popup/local profile gates, unknown query/key outcomes stop.
Doc lan-actual-readiness.md records source contracts and proof limits. Actual
LAN host/join, desktop exit and4-player acceptance remain unverified by252.


## MAC253 LAN entry and operator axis correction

OriginalMaster mode2/MPcarouselLAN throughfinitekeys reaches actualofflinerouter
Complete3/error0, onlinefalse andrealprimaryprofile0. Native entryrequirements
returns4 fromunknownplatformbranch; originalLANUI ignoresallbutret1. This
knownnativeguardgap isnot relabeledsuccess, andrealrouter proof isseparate
frompopupcallback. Actualbrowser/nogamespopup3ack, HostRCONTROL reaches
originalMpModeSelect. Firsthelper'sDOWN assumedverticalnavigation; actual
UINodeListdefaulthorizontal and screenshot/source showtwoside-by-sidechoices.
GetSelectionreturnsactual1/2payloads, notrowindexes. Noenginefault inferred.
Corrected252resumeobservespayload1/index0/twoauthorednodes andusesoneRIGHT,
thenpayload2/index1authorizesEnter. Firstdeadline/axisbaseline retained.


MAC252 follow-up — Actual root253 reached offline router Complete3/error0 and
ModeSelect5; authored Down was ACKed but selection payload stayed1, with no
confirm/host attempted. Corrected helper-only axis assumption: source creates
two payload nodes1/2; GetSelection returns payload, GetSelectionIndex row,
UINodeList horizontal default advances viaRight. New RO index/node-count gates
require payload1/row0/two nodes before finiteRight, and payload2/row1 before
Enter. Explicit --resume-selector freshly validates original ready offline
selector/local profile/no existing session; it cannot navigate/set that state.
Pure tests + py_compile pass, no IPC/build/GPU/source change by author. Root
owns resumed253 actual selection/host/leave proof; audit records prior typo
and navigation assumption transparently, not a native/protocol defect.

MAC252/253 follow-up — Root253 real local-host smoke PASS: actual offline
router Complete3/error0, SystemLink CustomRace Running/Host true, UI/network
max20 and copied20-slot roster1local/0remote, both start getters false. Original
popup4 Yes/automatic disconnect/NoGames3/browser Escape also completed. Return
observer's extra CarouselActive=true condition then stalled, though source
MenuBack only checks controller lock and active_selection branch; elapsed guard
exit0/601.95s is not desktop exit proof. Removed only that authored WAIT_MP_LEFT
presentation gate; actual false-CarouselActive snapshot now yields ordinary
Escape, with negative liveapp/subscreen/locked/running/notReady refusals tested.
Pure decisions/127-byte builders + py_compile PASS; helper frozen handed to
root256 fresh host/leave + original desktop verification. No engine/source/
protocol/build/GPU/IPC change or four-peer claim. Initial navigation gates stay.

### MAC256 — Actual LAN host and ordinary desktop cleanup

Fresh staged Speed242 arm64 app, original Master profile, normal offline
SystemLink flow: router Complete3/error0, CustomRace host, capacity20, local1,
remote0, both start gates false. Twenty finite original UI keys create and leave
the session through popup4, automatic6, browser, MpMain and popup2 to Start2.
Separate original three-key desktop204 route exits0/120.24s/no primary, before
420s guard. Post-run strict signature and dependency/notices manifest pass.
Proof: ignored mac256-lan-proof.json and original logs. This closes253's helper
presentation-gate desktop gap; no engine change. Entry classification4 remains
recorded separately from actual offline router success. No remote join/race or
historical three-player issue repair is claimed.

MAC257 independent bounded source/doc review found two stale documentation
claims (career cycle still pending; clean-host label253); both corrected.
MAC258/259 copiedited the acceptance/staging/LAN documents without altering
engine or input mechanisms. MAC255 publication retains the separate curated
index, original credential override exclusions and personal GitHub identity.

### MAC255 — Verified career and development staging publication

Curated source-only follow-up includes original normal career non-medal
results/save/exit243, signature-preserving saved-career reload251, BundleUtilities
metadata/notices/alias-preserving staging247/248 and local ZIP integrity254.
Fresh actual LAN host/leave/ordinary desktop256 is documented with remote play
still unverified. No game data, apps, saves or ignored verification outputs
enter the public source. Independent packaging/docs review257 found no further
blocking defect after two stale claims were corrected. Personal author/account
verified; credential-redacted profile mirror and empty default index preserved.


MAC260/261 — Audited all three original CheckEntryRequirements callers and
confirmed the prechange function exactly matches pristine1965fac source. Native
unknown-platform ret4 was nonblocking for actual253/256 LAN;256 normal host/
leave/desktop exited0 in120.24s. Narrow native guard now shares the original PC
branch: offlinefalse leaves0, onlinetrue still requires credential pointers and
returns2 on native's actual null getters. No credentials/account/router/provider
identity/transport/player-limit changes. Existing hosted NoServiceProvider and
OnlineConnectionPossiblefalse stay intact. Valid online caller outcomes mapped
in native-network-entry.md, including dormant invite ret4-to-popup-1 gap; no
invites/provider success fabricated. Exact guard removal restores pristine
function bytes; scoped diff check PASS, original source encoding/LF preserved.
Production frozen; root owns coherent Debug/Speed build and actual registered
Lua offline0/online2 + ordinary LAN host/leave/desktop integration. No build,
archive/GPU/IPC/runtime by author; no four-peer or connectivity repair claim.

### MAC262/263/265/266 — Native entry integration and local delivery

Both coherent Debug/Speed apps build with261; native profile and actual
game-entrypoint checks pass2/2 each. Independent263 review confirms the original
PC offline/online distinction and unchanged invalid-input/debug/provider paths.
Actual264 returns0/2/4/4 and hosts, but its private popup reader indexed retired
CurrentData during leave, stopped before further input, then game elapsedguard
exits0/422.55s/no primary. This is a retained failed-helper baseline, not desktop
acceptance. Guarded reads and identity/type/status/selection fences now retry
only passive observations; early/late-retirement/refusal/127-byte tests pass.

Fresh266 repeats actual0/2/4/4, offline routerComplete3/error0, original
SystemLink CustomRace host capacity20/local1/remote0/both-startfalse, ordinary
20-key leave to Start2 and separate3-key desktop204 confirmation. Actual
exit0/147.65s/no primary before420guard, helper0/quit0, signature/manifest pass
afterexit. False→0 proves allthree debug entry overridesfalse atthatcall; source
defaults alone would not prove it because startup XML can load values. No
credential/provider/identity/session/player/progress setter or bypass.

MAC265 stages 0.1.1/build262 through unchanged BundleUtilities tooling and
verifies copied/ZIP-extracted app signatures, dependency/notice hashes and all7
aliases. No original game/profile data, re-sign repair or previous0.1.0 mutation.
Proofs: ignored mac262-integration-proof.json, mac264-entry-proof.json,
mac266-lan-proof.json, mac265-delivery-proof.json. Four removed upstream login
literals remain absent from the native executable. Personal public follow-up
preserves curated source-only history/redacted profile/empty default index.
Remote joins/races, >3-player behavior, clean-Mac and notarization remain open.
