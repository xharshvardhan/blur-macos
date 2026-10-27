# Native macOS findings

## Build baseline

- Source base: `1965facd3d4860447a043b1e0fea4d55c086d1f5`.
- Host: Apple M4 Pro, arm64, macOS 26.6.2, Apple Clang 17.
- CMake/Ninja configure and native target builds pass. Linked frameworks are
  Cocoa/AppKit, Metal, CoreGraphics, ImageIO and standard system libraries.
- The full upstream build starts by requiring MSVC in `cmake/blur_common.cmake`
  and requires Windows SDK tools and libraries in `cmake/dx11.cmake`. Those
  dependencies are not made portable by the separate Mac bootstrap target.
- The Windows build was not run on this host. Existing Windows build flags,
  generated targets and shaders were not regenerated or altered.

## Core ABI

Before changes, directly compiling Core.h fails at the missing Darwin
`malloc.h`. Its platform selection also falls through to Linux/32-bit for
arm64 Darwin, and its GNU typedefs make INTPTR 32-bit and DWORD 64-bit under
LP64. The guarded macOS branch corrects those definitions, uses Darwin's
malloc header and a native debug trap, and preserves Clang's override keyword.

The ABI executable includes the actual Core.h and PakHeader.h and checks the
platform defines, primitive widths, pointer round-trip and PC archive struct
sizes. It does not establish that the rest of core or the whole engine builds.
Legacy header include-case warnings and deprecated formatting calls remain;
case-sensitive-volume support has not been established.

## PAK source authority

The native adapter uses the following existing source definitions:

- `pakfs/impl/PakHeader.h`: 12-byte preheader, 40-byte v1 header, 32-byte v2
  header and 40-byte v2 FAT entry. Compile-time layout checks guard ABI drift.
- `PakRead.cpp` and `PakFatEntry.h`: v1 32/56-byte normal/secure entries; v2
  40/64-byte entries; strings and sector padding; v2 stored length in the FAT.
- `PakUtil.cpp` / `PakUtil.h`: string-table decoding and path CRC convention.
- `PakCompressRaw.cpp`, `PakCompressZLib.cpp`, `PakCompressPacketZLib.cpp`:
  codec values 1/2/4, zlib/raw-deflate choices and 64 KiB packet structure.
- `PakBuildHelpers.cpp::VerifyFileHash`: secure entry hashes cover stored bytes,
  excluding the v1 physical-length prefix.

No real archive is currently available here. The parser's structural and
decompression checks pass independently generated fixtures and malformed-data
tests; do not describe these as retail compatibility or complete preservation.
The adapter does not follow parent pointers for lookup, write extracted files,
modify game files, bypass game activation or execute game bytecode. FAT RSA
authentication is explicitly not implemented and is reported as unverified.

## Rendering and gameplay

Metal verification submits an actual indexed draw and checks both the center
triangle pixel and the clear-color corner after GPU completion. It writes a
256x256 verification PNG. This validates the native host/device path only.

The probe is not yet an implementation of Display/Display2. There is no ported
game shader system, material handling, scene rendering, audio or simulation
loop in the bootstrap. No real menu, track or vehicle has been loaded. No
frame-rate-dependent game behavior was changed by this milestone.

The Cocoa app can run the Metal check and inspect a selected game directory.
Manual folder-picker operation against real retail data remains unverified.

## Source-only continuation: original core runtime

Retail assets are not a prerequisite for continued compilation or isolated
engine checks. A source-tree scan found no `.pak`, `.level`, `.levelrender`,
`.levelstream`, `.model` or `.texture` files under code/. It did find authored
shaders and some tooling/test images, fonts and avatar material; these do not
establish a complete runtime data installation. The upstream README itself
requires an installed PC copy.

MAC006 compiles the actual upstream `CoreMemoryAligned.cpp`,
`CoreMemoryNonTracked.cpp` and `utility/endian.cpp`. These non-math files use an
explicit Core.h include on Darwin rather than the Windows/math PCH. Existing
non-Apple branches remain unchanged.

The aligned-allocation adapter accepts power-of-two alignments, strengthens
valid 1/2/4-byte requests to POSIX's pointer-size minimum and reports invalid
alignment as NULL/EINVAL. Freeing still uses the source's existing free path.
Usable-size queries use Darwin malloc_size, not Linux malloc_usable_size or
assumptions about MSVC's private heap headers. No fake physical-allocation
implementation was introduced.

The runtime test exercises all three aligned allocator entry points, plain
allocation/usable sizes, null pointers, invalid alignments, zero-size requests,
endian primitives, format-driven multi-record swaps and the engine's actual
debug assertion callback. It also performs 8,000 allocations on eight threads.
This verifies the selected core object files, not the whole engine, audio,
threading API or gameplay. CTest passes all four suites after these changes.

The same original objects and tests pass an independent UndefinedBehaviorSanitizer
build with recovery disabled. An AddressSanitizer attempt could not run: a
sample showed recursive sanitizer/malloc initialization under dyld, before
main, spinning in the sanitizer lock. The owned process was terminated; no ASan
pass is claimed and no host-wide allocator or toolchain setting was changed.
Evidence lives under _work/build/macos/mac006-*.


## MAC004: full core objects and native runtime interfaces (2026-10-26)

The user's active goal is now the complete running native game, including an
actual race, saves and LAN validation. This remains active and incomplete.
The current work does not require retail content and no transfer request is a
gate on continued source porting.

Source generation: tools/macos/generate_sources.py runs the existing Jam
extractor and resolves the checkout's actual path case. The original core target
has 50 sources: 45 compile as native objects, including all 34 math sources; five
Windows/console thread implementations are replaced by threads/macos/
NativeThreads.cpp. ThreadLocalStorage.cpp remains among the originals. The
mac_core_maths executable force-loads the archive so unreferenced original
objects cannot hide unresolved link dependencies. This verifies the whole core
object set, not every template instantiation or the rest of Mercury/Amax.

Native math changes preserve 64-bit addresses in BigMatrix alignment, fix Clang
alignment declarations, enable the existing scalar __fsel implementation on
Darwin and explicitly invoke the legacy const-value storage conversion. Three
ill-formed native stream paths now use their declared Vec8/Vec12 destinations
and DualQuaternion output pointer. Vec12 and Vec16 copies retain every declared
lane on macOS; the missing trailing lanes were reproduced by the stream test.
These are guarded macOS changes, not a claimed retail gameplay fix. FP setup
uses the Darwin SDK fenv_t/FPCR constants for round-to-nearest, exception masks
and optional denormal flush. ARM float already has 24 significand bits; x87
precision control has no native equivalent. Tests cover state setup/masking;
actual trapping behavior of FP_UnmaskExceptions is not validated.

The existing MT API now has native recursive locks/mutexes with ownership and
real timed waits, persistent auto-reset/coalescing events, bounded semaphores,
timed reader/writer locks, pthread lifecycle and owned TLS keys. Compiler
atomics use sequential consistency and preserve fetch-old results, especially
increment/decrement. Mutex/event/semaphore names resolve to shared state only
within one process; cross-process Windows object namespaces are not ported.
Priorities map coarsely to Darwin QoS, not scheduler equivalence. Worker errors
surface on Join. Workers must be joined before derived object state is destroyed.
Native thread ID/yield, debugger attachment and allocator consistency functions
call actual Darwin APIs rather than unsupported-platform fallbacks.

Three original system objects (Time, Address and Socket) compile and execute.
Time uses monotonic microseconds for durations and reentrant UTC/local calendar
conversion. Address uses owned getaddrinfo results, including a defined empty
address. Socket descriptors close for both plain Socket and TCPSocket, have
SO_NOSIGPIPE/FD_CLOEXEC, preserve the U32(-1) error sentinel, and use poll with
EINTR-aware deadlines. Local TCP x8 and connected UDP fixtures pass; this is
transport verification, not Blur LAN lobby reproduction or a fourth-player fix.

NativeFile implements the actual Sys::I_File interface over buffered disk and
read-only memory. Tests cover direct reads, odd/single-byte buffers, writes,
append, overwrite, zero fills, null-target skips, partial EOF, empty streams,
version/endian metadata, long engine strings and a sparse >4 GiB seek. Flush
reports write/durability errors; destructor failures can only be logged, so save
callers must flush explicitly. This is not yet an I_FileSystem mount/factory
replacement. Native I_BinaryStream reads keep pointer arithmetic within the
buffer; the legacy FasterMemCpy uses memcpy on Darwin to avoid aliasing pointer
storage and binary data through integer references.

NativePakFile bridges the existing decoder to Sys::I_File. The probe and all
successful archive fixture cases now read via the engine interface and compare
SHA-256 against independent payloads. The earlier FAT RSA, XMem and retail-data
limits remain unchanged. No actual archive/resource/menu has been validated.

Broader standalone syntax audit: 30/93 original system sources passed with their
Jam include paths. This is diagnostic, not a build result: some failures are
unity/header dependencies as well as platform gaps. Main clusters are TL
scheduler platform definitions/thread handles, Xt/Artery fixed-width platform
types, FilePath separators and Clang dependent-type declarations in
sortedbinarytree.h. Full output: _work/build/macos/system-audit.{json,log}.
Do not treat passing syntax as evidence that unported platform function bodies
are correct (VirtualMemory's existing Linux fallback is a concrete example).


## MAC004 continued: scheduler, mappings and game filesystem (2026-10-26)

The preceding goal turn made concrete progress: the core/stream/runtime code and
its verified tests changed authoritative state. This continuation also makes
source/build/runtime progress; the complete-game goal remains active, with no
retail startup/race/save/LAN evidence yet.

The original scheduler now compiles and executes through a distinct
TL_PLAT_MACOS path, not jobqueue_generic.h's ineffective fallback. Native TL
workers reuse the real MT pthread implementation, preserve pointer parameters
and exit codes, gate suspended starts, unwind explicit worker exits, reject
stale/pseudo/self-join handles, and join before registry release. Handles are
monotonic IDs, rather than recycled object addresses. Native priorities remain
coarse QoS classes, not Windows scheduler equivalence.

Original jobqueue.cpp/kernel/memory/queue logic is retained with native desktop
worker events and atomic stop/count reads. Task.ParamData has the upstream x64
96-byte/aligned layout. Pointer/change-count pairs use 128-bit Apple Silicon
atomics. Native queue mutation is additionally serialized to keep pre-CAS
payload copies and node recycling within the C++ memory model. Jobs still run
on actual worker threads. Node allocations are tracked per shared pool and
released after the last sharing queue shuts down. Temporary worker storage is
also released on scheduler stop.

The eight source alert slots reserve logical slot zero for main; native worker
count is therefore capped at seven. These are scheduling roles, not OS CPU
pinning. Policy order uses the selected logical slot, including non-contiguous
masks. AlertBits packs lanes most-significant first, so native reads select that
numeric lane order explicitly. The scalar build's ALIGN128 macro was empty:
the first real scheduler run faulted on an unaligned U64 atomic. The owned crash
report identified AtomicAdd in AddTaskToQueue; alignas(128) now protects the
alert array independently of SIMD. Ungrouped completion also avoids a native
GroupPool[-1] access. Repeated worker/dependency/ungrouped tests now pass.

Memory::AllocVirtual/AllocPhysical/FreePhysical/FreeVirtual now dispatch to a
native backend from the original VirtualMemory.cpp entry. Reservations are
64-KiB aligned for the original allocator page mask, initially PROT_NONE.
Commits use real read/write protections; decommit replaces only the owned span
with a fresh anonymous reservation, producing zero-filled recommit. Bookkeeping
uses libc allocation rather than global operator new, avoiding recursion through
the game allocator. Independent adjacent commits remain separate; overlapping
commit requests merge into one tracked span. AddressReadAccess queries actual
Mach protections and is only an advisory snapshot.

The original BlockAllocator runs against these mappings. Native pointer masks
are full-width; sparse ownership bitmaps cover full addresses and publish leaves
for concurrent ownership queries. The game's actual default areas (16..256 in
16-byte steps) remain unchanged. Native free-list slots must fit/alignment-match
a pointer. Bitmap initialization preserves the source's pre-static setup shape;
the entire game's MemoryTracker/global-new/pre-static bootstrap is NOT yet
integrated or verified. Release testing found m_nAllocs read before initialization
in the original statistics accumulator; its native path now initializes it.
Tests exercise active totals, full/partial page recycling, threaded allocation
and zero remaining native mappings after cleanup.

C_FileSystem.cpp now dispatches to NativeFileSystem.cpp on Darwin. This is the
actual singleton/default-filesystem interface that C_CoreSystems references.
It opens NativeFile streams and implements existence, metadata/access/time,
enumeration, exclusive rename, empty-directory/file removal, directory creation
and working-directory operations. Canonical game keys remain ASCII lowercase
with '/' separators. Resolution finds original disk spelling; physical-path
results preserve that spelling via C_FilePath::SetFormatted. Ambiguous names
that differ only by case are rejected. Symlink parent traversal follows the
physical directory. Runtime case-sensitive volume collision behavior is not
separately tested on this host; rebuilding the source on a case-sensitive
volume still encounters inherited include-case problems.

Call-site review found zero-buffer hints in Audio::Core::FileStreamFileWrapper,
renderingdata StreamData and rampak loading. The earlier native adapter rejected
zero; it now supports direct reads and direct skips, with one pointer-anchor
byte and no positive-read prefetch. Filesystem fixtures cover this route.
Packed BinarySerialise scalar operations use memcpy on Darwin to preserve
on-disk bytes without unaligned access/aliasing. Scalar EOF/write failures are
reported rather than leaving an undefined value. Independently authored mixed
PC-endian bytes and a big-endian round trip pass through C_FileSystem, including
odd/single-byte buffers. Base-to-derived adjustment uses the real pointer on
Darwin; map template/friend/dependent-base fixes have runtime insertion/lookup
coverage. Original non-native backends remain selected on other hosts.

Build scope: all 50 core source entries remain accounted for as before; 16
original system source entries now compile, including filesystem/VM native
backend dispatch, plus the new NativeFile and TL implementation objects.
Mercury's optional profiler is explicitly disabled via its existing disabled
build mode. Native scheduler/filesystem/block-allocator diagnostics go to stderr
while full logger/subscriber integration remains ahead. C_FileSystem uses
synchronous native IO; the separate async service is still unported.

Verification: all eleven CTest suites and the bundle Metal self-test pass in
the standard Debug build. Separate Release and UBSan builds pass all eleven,
with sanitizer recovery disabled. Scheduler fixtures include 8,000 concurrent
queue records, real logical-slot-3 execution, dependency order, wide parameters,
6,000 execution-count checks and three complete lifecycle cycles. Mapping/block
checks include real protections/zero recommit and 9,000 outstanding small blocks
plus six contending allocators. Filesystem tests install the same default
interface as the core, then verify binary bytes, native path/metadata/mutation
behavior and cleanup. These are component tests, not actual gameplay or save
compatibility verification.

A fresh standalone system syntax audit passes 70/93 sources using native build
flags. It does not constitute a linked full system build; some sources compile
out optional profiler bodies. Reports: system-audit-platform2-final.{json,log}.
The remaining failures center on ArterySync/native BSD transport, async IO,
legacy C_File backend selection, MemoryTracker/MemoryBuffer dependent members,
network services and CPU timing. C_FileSystem now uses NativeFile, so a full
native source target must exclude/replace the old C_File.cpp implementation.
The original VM dispatcher already selects the correct native backend.


## MAC004 continued: Artery transport and native tree object lifetime

The original Artery service is a development/debug transport, distinct from the
game lobby. Its public service, peer, handshake, queue and message serializers
now compile on Darwin. ArterySync dispatches to the native backend beside the
original source: recursive timed mutexes, condition variables, retained
manual-reset flags and coalescing auto-reset events, plus joined TL workers.
Synchronization handles cannot be copied. As upstream, condition waits require
one mutex acquisition, and clients must join/wake all waiters before destroy.

BkSocket.cpp is selected from the actual PS3SRC branch of the system Jamfile;
the macOS generator extracts that manifest and selects the BSD implementation
instead of the Windows WsSocket.cpp object. Darwin-specific behavior supplies
sockaddr length, nonblocking I/O, asynchronous connect completion, SO_ERROR and
remote-address queries, FD_CLOEXEC, SO_NOSIGPIPE, interrupted-call retry and
stream EOF/would-block distinction. The wake channel remains the original
loopback TCP cross-connect. Accepted socket ownership, Attach transfer and
restart after TIME_WAIT are exercised. Calls use genuine sockets, not success
stubs. Artery discovery enumeration still follows the unsupported non-Windows
branch; only the existing listener/broadcast responder is ported here.

Native ArteryService uses atomic shutdown/publication flags and atomic shared
message references. It initializes services for local as well as static
instances, frees peers on failed startup and final cleanup, and treats the game
name as a string rather than a printf format. Discard ArteryConnection handles
before final ArteryCleanup; do not use freed peer handles. The test uses sixteen
peer slots because eight loopback connections consume eight client plus eight
server slots in the same service. This is unrelated to the remembered game
lobby limit. The test completes the original HELO/service-ID handshake,
exchanges independently generated 64 KiB messages, and restarts three times.

Independent golden bytes verify the original asymmetric scalar protocol:
16/32/F32 network order, 64-bit fields little-endian. Native scalar float copies
avoid aliasing, message builders/readers bound length arithmetic, and string
reads require the terminating NUL emitted by the original writer. Concurrent
message copy/assignment/release is exercised by eight threads. Debug compiles
these original objects but full service execution is currently checked in
Speed, where upstream disables memory tracking. Debug memory-tracker startup
and linkage are still pending; no empty tracker implementation was supplied.

The optimized UBSan build exposed a pre-existing container representation bug:
Ut's sentinel was only a base header downcast to a larger Node. Both Ut and Btl
native trees now store an actual Node with an inactive union payload at the
sentinel; ordinary nodes construct/destruct the payload. This changes internal
native node storage only, not serialized bytes. Non-default-constructible keys
remain supported. Erase tests then exposed GetNode(Iterator) returning a
reference into its copied parameter; native overloads now return pointers by
value. Clang dependent type/friend/base lookup was corrected at instantiated
Ut/Btl callers. Both maps are tested through insert/find/erase/ordered traversal
and clear; a tracked non-default key checks payload lifetime and no sentinel
payload construction. The inherited Windows paths remain selected there.

Verification: standard native Debug build/bundle self-test: 12 suites; Release
Speed: 13 suites; independent Debug UBSan: 12; optimized Speed UBSan: 13. All
pass with sanitizer recovery disabled. The remaining RGBA/header-case warnings
are inherited. Logs: mac004-artery-{debug-build,release-build,release-tests,
ubsan-build,ubsan-tests,speed-ubsan-build,speed-ubsan-tests}.log under
_work/build/macos. Earlier failed sanitizer/test diagnostics were resolved;
ASan was not retried. Standalone Debug syntax audit is 84/93 in
system-audit-artery-final.{json,log}; this does not establish a full system link
or runtime success. Two of the failing entries are the replaced legacy C_File
and Windows WsSocket backends. IoDevice/IoService syntax success still excludes
missing platform bodies and is not an operational async implementation.

Next source work: real native async I/O and IoRequest/IoFile ownership and wait/
cancel behavior; remaining network service/debug integration; finish tracker
and global allocation bootstrap, and compile the wider engine/framework. The
full-game goal remains active. Display/Display2 still needs an actual Metal
backend; the existing hardware triangle remains only a probe. No original game
startup, menu, race, save/load, audio/input or game LAN result is claimed.


## MAC004 continued: real async I/O and wider system integration

Native IoDevice/IoService/IoFile/IoRequest dispatches now execute real Darwin
open/pread/close operations. The service retains the original two-device API and
priority/deadline selection; deadlines use monotonic performance-counter
milliseconds as the original GetTickCount did. A close acts as a barrier behind
earlier reads on its file. Offsets remain 64-bit, descriptors are close-on-exec,
and physical-case resolution shares the C_FileSystem resolver. Regular-file
validation rejects non-file devices. Partial reads report fulfilled bytes;
zero-byte EOF is reported as EndOfFile. This native error mapping is explicit;
it is not a claim of reproduced retail behavior. The original PC ReadFile path
and [Microsoft's ReadFile documentation](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile)
were inspected while checking EOF/offset behavior.

Requests retain their queue ownership until completion and publish progress
atomically. Wait includes callback Event and Release, and rethrows callback
exceptions after releasing callback ownership. Pending cancellation completes
with Failed/Unknown because the historical enum has no cancellation code;
already executing I/O finishes normally. Wait is required before reusing its
buffer. Waiting on one's own callback is rejected rather than deadlocking.
Shutdown closes admission, joins active work, fails/drains queued operations,
releases callbacks, closes remaining handles and supports restart. Concurrent
shutdown callers wait for the join. Callback reentry receives an operation
failure during shutdown. Shutting the whole service down from its own worker is
rejected. CoreSystems retains responsibility for explicit shutdown before
allocator/static teardown.

The original AsyncFile buffering/read-ahead layer now runs on that backend.
Native fixes cover null-target skipping, fulfilled-byte positioning at partial
EOF, safe empty-buffer arithmetic, signed-offset bounds and waiting for close
before stream destruction. C_FileSystem routes reads to AsyncFile once the
service is initialized, using the original default 128 KiB and buffer-count
settings. Inspection before I/O startup and filesystem calls inside worker
callbacks use the real synchronous backend; queuing a blocking read onto its
own worker would deadlock. Writes still use the existing native file backend.

The original system Jam target now contributes 28 runtime and 64 wider engine
source entries (92 total including the BSD socket selection and excluding the
replaced C_File/WsSocket backends). The wider target also compiles the original
misc_shared MessageDebug.cpp and real native logger registration. Its default
integration executable force-loads every system object so unresolved objects
cannot hide in static archives. Source-derived include directories are generated
from Jam. BuildStamp's native dispatcher uses real generated UTC date/time,
user/host, Git revision/dirty state and an explicit native-development version;
SOURCE_DATE_EPOCH supports repeatable metadata. No fake blob symbols or success
functions were added.

Whole-library startup/instrumentation found and resolved additional faults:
VarBase used a virtual root object before its global constructor; a native
function-local, placement-constructed root now exists before registration and
outlives static variable teardown. Local CPU timing objects had uninitialized
frame counters; native constructors now zero them. XML DOM code downcast the
artificial base document root to an element although it only needed base APIs;
those operations now use their actual base type. Native nested Genesis list
iterators retain their owner. MessageDebug comparison uses the associated Debug
namespace for Clang lookup. Artery service callbacks now also execute in Debug
with the original tracking contexts; full game global-new/tracked-allocation
startup is still not validated. The optional Mercury profiler and filesystem
performance-event instrumentation remain disabled for bring-up.

Verification: the standard Debug build/bundle self-test and all fifteen CTest
suites pass. Independent Release, Debug UBSan and optimized Speed UBSan also
pass all fifteen; sanitizer recovery is disabled. New checks include 480 reads
from eight callers, callbacks/release/exception handling, pending cancel with
untouched buffers, close ordering, a >4 GiB sparse-file read, buffered/read-
through AsyncFile seeks/partial EOF/skips, actual C_FileSystem async routing,
callback filesystem reentry, shutdown/drain/concurrent shutdown/restart, and
zero live native I/O resources after shutdown. Wider integration executes real
logging, XML through the default filesystem, CPU timing/registry/list iteration,
messaging metadata/debug options and build-stamp queries. This is an engine
system executable, not Amax. ASan was not retried.

Evidence is under _work/build/macos: mac004-async-{debug-build,release-build,
release-tests,ubsan-build,ubsan-tests,speed-ubsan-build,speed-ubsan-tests}.log and
mac004-system-integration-*.log. Earlier sanitizer failures were resolved; the
final standard suites include the wider integration run.

A fresh standalone source audit honors Jam defines (including XT_GAME) and
records unresolved Windows SDK include inputs instead of mistaking them for
native dependencies. It now passes resource 6/6, logicalFs 29/29 and Xt 8/8;
framework remains 0/17 at its Win32 PCH. Preliminary fixes: Btl::Set dependent
type import; resource typed handles delegate legal base copy/comparison access;
native resource waiting preserves the original 100 ms interval; MemoryFs failed
open returns a null stream and deletes its wrapper; Security's pure-virtual
specifier uses zero; Xt's old fallback no longer misidentifies Mac as PS3.
Those three libraries are syntax-checked only, not yet linked or runtime-proven.
Evidence: engine-audit-system-integration.{json,log}.

Exact next work is native resource/logicalFs/Xt target generation, linking and
real manager/mount/schema tests, then port FrameworkPch and its actual
window/display dependencies to Mac/Metal. Concrete C_File users, remaining
network/debug APIs, game allocation bootstrap, renderer/shaders, audio/input and
actual Amax/game integration remain ahead. No real menu, race, save/load or game
LAN run exists. Retail assets are not a gate on this next source work. No commit,
push, public release, or original-game download occurred.


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

## 2026-10-26 — MAC004: actual Metal context, surfaces and textures

Found in / belongs to: macOS. MAC004 remains owned by Codex /root. The backend
now records actual engine draws with authored shader resources and immutable
per-draw constant snapshots. It retains buffers/functions/declarations after
wrapper destruction; dynamic DISCARD renames storage and NOOVERWRITE validates
GPU read ranges. Float/half/integer/normalized/BGRA vertex declarations map to
Metal; PC UDEC3 and DEC3N expand to float4 with documented 10-bit conversion.
This CPU expansion and index validation have not been performance profiled.

Native C_Context implements vertex/index offsets, signed base vertices, 16/32-bit
indices, multistream, point/line/triangle/strip/fan submission, viewport, MRT,
colour masks, culling, alpha/depth testing and blending. Full and viewport-rect
colour/depth clears execute on hardware. Surfaces use private textures and
synchronized staging. Texture-backed render surfaces share ownership with shader
sampling; simultaneous attachment/sample feedback is rejected. Original shared
context/Context.cpp now compiles alongside the six Jam-selected native files;
Context.h selects Metal. Shared setter widths now match their header declarations
(U16 stream, U32 type), rather than defining unreachable U8 overloads. Native
C_VertexFormat retains explicit padding rather than summing field sizes.

2D, volume and cube textures have separate mip/face views and staging locks.
Supported storage includes BGRA/RGBA8, alpha/luminance/A8L8, signed RG8, D16,
R/RG/RGBA16F and 32F, and BC1/2/3 (DXT1/2/3/4/5). PC undefined texture channels
supply one, except A8 RGB supplies zero; native swizzles implement this instead
of Metal's differing missing-channel defaults. Size helpers use tightly packed
PC rows; 256-byte staging padding is never inserted into disk-format sizes.
Native volume sizes shrink depth per mip, matching actual Metal storage and the
existing PC level-offset helper; the inherited PC size helper holds depth fixed
and therefore overcounts volume mip allocations. That PC helper was not changed.
Sampler state retains the PC 12-byte layout and implements point/linear min/mag,
none/point/linear mip, wrap/mirror/clamp and Metal's three border colours. Linear
and sRGB texture views are separate. Nonzero LOD bias, arbitrary border colours,
anisotropic quality integration, MSAA, stencil/D24, remaining packed formats,
partial-block BC root images and the complete state/device API remain explicit
unsupported paths. No successful fallback is fabricated.

Actual textured stage linkage exposed an inherited BLUR_DX11-path mismatch:
vsBasicRenderTextured3D writes COLOR0/TEXCOORD0, while its modified pixel stage
reads TEXCOORD0/TEXCOORD1. Native preprocessing now selects the original D3D9
shader branches (BLUR_METAL, no BLUR_DX11). SPIR-V locations follow original
semantics. Metal additionally requires equal varying widths where D3D9 allows
float2 reads from a float4 register. MSL interfaces therefore use float4 varying
registers with explicit original component reads/writes. Vertex attributes,
render outputs and constant layouts remain unchanged. All 693 built-in variants
translate, SPIR-V validate and compile/load as actual Metal shader resources.
This does not establish correct pipelines or pixels for all 693 variants.

mac_engine_render uses actual BasicRender coloured/textured, Perlin and
TextureCubeColour shaders with independent CPU geometry, bilinear, gamma and
volume-octave references. Checks cover >3,000 interior/background pixels,
non-symmetric row-major transforms, draw constant snapshots, padded/base-offset
geometry, dynamic renaming, multistream/fans/packed vertices, D16 depth,
alpha/blending, partial clears, texture orientation/addressing/mips, BC decoded
pixels, channel swizzles, GPU feedback rejection, all cube faces and volume
slices. Scene-thread ownership and retained-resource lifetimes are exercised.
These are authored fixtures and offscreen draws, not a menu/race/game frame.

Authorities: original D3D11Context, PCD3D11Device, shared Context and shader
sources; Microsoft D3DDECLTYPE, D3DFORMAT and Clear/viewport documentation;
Apple Metal vertex formats, texture-view swizzles and feature tables:
https://learn.microsoft.com/nl-be/windows/win32/direct3d9/d3ddecltype
https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dformat
https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-clear
https://developer.apple.com/documentation/metal/mtlvertexformat
https://developer.apple.com/documentation/metal/mtltexture/swizzle
https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf

Next: finish full engine device/context state and texture/render-target helpers,
compile remaining shared Display2/upper Display sources, and connect the actual
native window/presentation/application. Runtime material shaders need their own
path. Core framework, audio/input and game integration remain; real scenes,
races, saves and LAN still need later runtime data. The active goal is incomplete.

### MAC004 shared Display2 resource infrastructure continuation

Display2Logger.cpp and BaseDevice.cpp now compile in addition to Context.cpp
and the six native implementation files. Native render-texture metadata wrappers
provide the original single-resource accessors for this shared code; their full
creation, read/write slot switching and device integration are still pending.
The Metal library links the actual engine system services for resource logging.
Shared resource insertion, exact-base-address lookup, name metadata, sorted dumps
and removal run against a real native buffer. The original lookup is equality
based, not an interval lookup. Core Btl::Vector's debug iterator advertised random
access but omitted >, <= and >= and had a non-const <: current libc++ sort exposed
that omission. The complete const relational operators now retain the same-vector
assertion and pointer ordering. A 513-element permutation sort exercises partition
paths in the native integration check. This is a native compile/standard-library
contract correction, not a retail gameplay change.

Also corrected the optional-index DrawIndexedPrimitive overload to preserve an
already-bound index buffer when its optional pointer is null, matching the actual
existing Display2D3D11Bridge source. The rendered regression calls that overload.

### MAC004 native drawable presentation

MetalPresent.mm now configures a host-supplied CAMetalLayer on the main thread,
blits a completed BGRA8 engine surface to an actual CAMetalDrawable and schedules
presentDrawable on the command buffer. It does not create a game window yet.
framebufferOnly is disabled because Metal prohibits blits into framebuffer-only
drawables. Layer resize/detach, queued-present counters and optional vsync policy
are explicit. Invalid dimensions/formats, missing drawables, locked surfaces and
unsubmitted scene resources fail instead of reporting successful presentation.
A seven-file native Jam backend now compiles with three shared Display2 sources.

mac_engine_present presents six successive engine clears through real drawable
resources, waits for GPU completion and reads the drawable texture back through
a separate Metal queue. Every pixel matches the authored ARGB bytes. Tests also
reject cross-thread host configuration, incomplete/CPU-locked source frames,
resize mismatch and detached layers. This is real layer presentation with no
NSWindow or visible game; no screenshot or actual on-screen game claim exists.

Presentation authorities:
https://developer.apple.com/documentation/quartzcore/cametallayer
https://developer.apple.com/documentation/quartzcore/cametallayer/framebufferonly
https://developer.apple.com/library/archive/documentation/Miscellaneous/Conceptual/MetalProgrammingGuide/Render-Ctx/Render-Ctx.html

### MAC004 Cocoa platform host

Framework's MACOS Jam branch now contains application/macos/MacWindow.mm. Its
plain C++ host API owns NSWindow, responder view, CAMetalLayer and delegate on the
main thread, pumps AppKit events without taking over the fixed-step engine loop,
and publishes physical backing dimensions on size/Retina changes. Close defers
to engine shutdown. Raw Cocoa key/modifier and mouse/scroll fields reach platform
callbacks; full game input bindings, controller support, IME and the original
C_Application integration are not yet implemented. Callback exceptions return
through the C++ event pump; shared in-flight state permits a callback to release
its own host. UI delegate pointers are detached during destruction.

The mac_cocoa_window check creates an actual minimized window without making it
key or calling finishLaunching (Apple documents that method activates the app).
Miniaturization is asynchronous; the check pumps the main run loop with a bounded
two-second deadline, instead of falsely requiring immediate completion. The host
presents six real engine frames while minimized, checks Retina dimensions, routes
local responder keyboard/mouse test events, propagates a callback exception,
resizes, defers close and safely destroys itself from an input callback. No global
keyboard/mouse events are injected. It is a platform host, not game startup.
A dedicated RUN_SERIAL hardware/window CTest brings totals to 20 base / 22 with
shader tools. Source manifests for this host are separately derived from MACOS
framework Jam; the six existing original framework services remain unchanged.

Host lifecycle authorities:
https://developer.apple.com/documentation/appkit/nsapplication/finishlaunching()
https://developer.apple.com/documentation/appkit/nswindow/isminiaturized
https://developer.apple.com/documentation/appkit/nswindow

### MAC004 engine-facing native device lifecycle

Device.h now selects MetalEngineDevice.h. The real shared device/Device.cpp
factory/lifecycle code compiles: four original shared Display2 sources plus eight
native backend files. Native C_Device inherits the existing C_BaseDevice resource
infrastructure and the real Metal backend, owns PC-style created/volatile stages,
and attaches the Cocoa presentation target during volatile creation. Its manager
actually acquires a Metal GPU; selected adapter ownership is shared through native
device parameters and retained by the GPU state. A failed volatile presentation
setup is cleaned up by the original factory. Explicit shutdown reports GPU
completion failure; destructor cleanup cannot throw during unwinding.

The Cocoa host test now uses CreateDeviceManager, SetupDefaultDeviceParams,
CreateDevice and DestroyDevice/Manager, rather than constructing the lower-level
backend directly. It verifies failed setup cleanup, real manager/device names,
created stage 2, explicit global registration, six minimized-window frames and
global/volatile teardown. Registration stays explicit, as in the existing PC
upper device; the inherited generic factory's self-assignment does not register
it. This does not yet provide the complete original C_Device state/draw wrapper
API or connect upper Display/C_Application. Native core backend tests still use
C_MetalDevice directly. Detaching a layer only releases backend ownership and can
run during offscreen shutdown; attaching/configuring a host layer requires main.

### MAC004 render-state interfaces and device draw forwarding

Native context now stores original-layout alpha/blend/colour/cull/depth/fill,
scissor and stencil state records with persistent const-reference getters and
validated whole-state setters. Defaults follow the original D3D11Context.inl
records (including CCW culling, enabled depth and the original alpha blend
factors). Render checks set intentional diagnostic overrides explicitly. The
engine-facing C_Device forwards these state, shader, buffer, texture, viewport,
scene/clear/draw and surface creation calls to its actual command context; the
minimized host uses those device entry points directly.

Scissor rectangles affect both draws and clears; clear coverage is the viewport/
scissor intersection. Rectangles outside the render target fail. Binding target
zero resets viewport/scissor bounds to the full new target as D3D9 documents.
Wireframe uses Metal's actual triangle-fill line mode; solid restores correctly.
Colour masks and whole alpha/blend states are checked through pixel output.
Anisotropic min/mag filtering uses an explicit 1..16 quality setting, default 1,
and is exercised on magnified texture data against the CPU filtering reference.
This verifies that path, not all anisotropic minification quality cases.

Native state records can retain disabled stencil configuration and zero depth
bias, but enabling stencil drawing, point fill or nonzero depth bias still fails
explicitly. Stencil/D24 resource formats and faithful bias conversion remain
unimplemented. Nonzero sampler LOD bias and arbitrary border colours remain
unsupported; the new macOS26 sampler lodBias API additionally requires Apple10
hardware per Apple's feature table, so it is not used as a purported fallback on
this Apple9/M4 device. Complete unchecked context/API compatibility and upper
Display/application integration are still pending.

State GPU regressions include rectangular and empty scissor, viewport/scissor
clear intersection, target reset semantics, wireframe edge-only coverage, colour
mask preservation, destination-preserving blend, whole alpha tests, state restore,
anisotropic magnification and explicit unported-state rejection. Source/state
layout assertions and real GPU effects are both checked.

Additional authorities:
https://learn.microsoft.com/en-us/windows/win32/direct3d9/scissor-test
https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-setrendertarget
https://learn.microsoft.com/en-us/windows/win32/direct3d9/depth-bias
https://developer.apple.com/documentation/metal/mtlsamplerdescriptor/lodbias
https://developer.apple.com/metal/limits/

### MAC004 faithful render-texture storage and gamma corrections

Native render-texture wrappers now follow the existing PC read/write mechanism:
two physical textures/mip-surface arrays when E_Flags_ReadWrite is set, initial
write slot 0/read slot 1, and Resolve swaps both indices without copying for
single-sample targets. Single-slot targets keep index zero. Creation is built
in temporary owned resources and commits only after all views succeed. Cube
render targets own actual face/mip aliases. MSAA resolves, cube read/write mode,
external memory and differing texture/surface formats remain unsupported.
There are now nine Jam-selected native Display2 implementation files plus four
original shared files; remaining shared sources are Buffer.cpp, Memory.cpp and
Utils.cpp. Upper Display/application linkage remains separate.

Source authority: PCD3D11GameDevice.cpp CreateRenderTexture2D/Resolve and original
DeviceTypesEnums.h. The current ReadWrite flag is 0x10000000; BaseDevice's older
hard-coded 0x40000000 means Streamed under the current enum. Native resource naming
uses the actual enum and visits both textures/mip arrays. The inherited PC branch
was not changed. Shared tracking tests name and remove all native read/write
children. IsCreated now checks both texture zero and its first actual surface.

Comparing the existing PC view/creation code also corrected native gamma policy:
formats without SRGBREAD ignore SRGBTEXTURE rather than failing creation/draw;
SRGB metadata selects sampling views and does not change the PC backend's linear
render-target writes. Native surface creation now preserves that distinction.
Actual alpha/luminance/signed/float texture tests set unsupported gamma state and
retain the original channel values. An SRGB-flagged surface clear reads back
0xff808080, rather than an unintended hardware sRGB encoding.

GPU checks verify initial read storage, alternating red/green history frames,
rendering a nonzero mip, slot switching without CPU copies, retained sampling
after wrapper destroy, unchanged single-slot resolves, MSAA failure and all six
render-cube faces/mips, including subsequent authored cube-shader sampling. The
source/class changes do not establish real game frame composition or gameplay.

### MAC004 — original file helpers, resource tracking and host memory arenas

All seven shared Display2 Jam translation units now compile on macOS: logger,
Context, BaseDevice, Device, Utils, Memory and the empty Buffer unit. Twelve
native implementation translation units accompany them. Source extraction
continues to use Jam and records the inventory under the build's generated/.

C_Device resource factories now publish/remove the original resource metadata,
including render-texture child resources; the lower C_MetalDevice remains the
untracked backend. Explicit engine Destroy calls remain required before tracked
wrapper lifetimes end. Debug test cleanup follows this original contract.
The native device exposes a real matching-texture GPU copy used by the original
CloneTexture2D helper; current clone readback evidence covers 2D RGBA and BC1/2/3,
not volume/cube copying. Native shader extensions are bmsl for both stages.
Original Debug RefreshShaders and explicit Speed reload produce changed GPU
pixels. Pixel shader file reload now destroys the existing native wrapper before
recreation, matching the original vertex helper. Missing files preserve the old
program. No GPU timer implementation is claimed for the original non-PS3 path.

The original Utils file entry points call bounded native DDS/BMP/TGA decoders.
DDS supports 32-bit ARGB and DXT1/3/5 with validated dimensions, mip bounds and
payload; volume/cube/DX10 files remain unsupported by this helper. BMP supports
24-bit BI_RGB, bfOffBits, padded rows and either vertical orientation. TGA
supports 32-bit uncompressed truecolor, image ID, vertical/horizontal origins,
and rejects interleaving. Decoder params commit only after validation. Uploads
copy tight file rows into actual Metal staging pitch, including compressed tail
mips. Native streaming-map failures clean up file and texture. Authored fixtures
verify bytes after real GPU upload and clone. These are development file helpers,
not evidence that retail scene/resources load.

Format authorities: original PC serializers and Microsoft DDS documentation
(https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dx-graphics-dds-pguide),
BITMAPINFOHEADER (https://learn.microsoft.com/windows/win32/api/wingdi/ns-wingdi-bitmapinfoheader)
and the original Truevision TGA 2.0 spec mirrored at
https://raw.githubusercontent.com/DEAKSoftware/Truevision-TGA/master/truevision-tga.pdf.

C_MemoryBuffer has a native host-storage implementation. Main arenas use the
actual aligned allocator, indirect arenas borrow aligned caller storage, and
PS3 Local/Unknown are rejected. This is CPU storage for the original
C_MemoryManager/Sys::MemoryBuffer suballocator, not raw GPU-address emulation.
Metal resource allocation stays in the actual Metal resource classes.

Native regression evidence exposed three original allocator lifecycle faults:
Sys::MemoryBuffer had uninitialized requested-byte accounting, mismatched array
delete and stale state after Shutdown; SkipList::PopFront only unlinked level 0,
leaving freed nodes in higher head links; C_MemoryManager unlocked before
publishing/removing system tracker entries, allowing another thread to reuse an
address with an old tracker node. Native fixes initialize/reset the arena,
remove all skip-list head links before releasing the node, and keep publication
and retirement inside the arena critical section. The test initializes the real
system tracker, allocates/writes/verifies 257 varying-size aligned blocks,
releases them in a permutation, verifies full-pool merging/exhaustion, recreates
the pool, runs eight workers through 8,000 allocation/free cycles, and checks
borrowed-storage bounds/overflow and unsupported local-memory rejection.
Found in / Belongs to: local macOS source-only integration; all behavior changes
are guarded by PLATFORM_MACOS. No inherited Windows delivery is modified.

The initial allocator checks reproduced stale shutdown state, then a skip-list
SIGSEGV on recreation and tracker duplicate-insertion assertion under concurrency.
Fix verification passes mac_display_utils. LLDB launch stalled on this host;
its owned debug session was terminated, and the OS crash report supplied the
skip-list stack. No successful debugger session is claimed.

### MAC004 — native keyboard/mouse through the original game-input layer

The six shared input Jam sources compile and force-link with NativeInput.mm,
also selected from Jam. This does not compile the Windows DirectInput/XInput
implementations; the virtual-pad file only compiles its original common portion,
not native controller support. The native manager exposes four logical user
slots and creates actual original C_Keyboard/C_Mouse interfaces for window-local
Cocoa events. No gamepad implementation, IME/text entry or captured/raw mouse
mode is claimed yet.

The native event sink owns weak callbacks, serializes incoming event state and
publishes one snapshot per original device-manager update. Held keys/buttons
persist; mouse movement and scroll are consumed once. Focus loss clears held
state. Cocoa virtual key codes map to original keyboard enums using SDK
HIToolbox constants, with left/right modifier tracking and Command mapped to
Windows-key enums. No global event tap or input injection is installed. The
minimized host regression sends local responder events directly and verifies
original C_GameInput press/release and click actions.

Native mouse relative axes reuse the original MousePC sensitivity (0.25), dt
floor (0.001 seconds), and delta/time*0.01 scaling. Downward Cocoa Y is negated,
matching ApplicationPC::OnMouseMoveRelative. Absolute X/Y expose host pixel
positions; wheel values retain local Cocoa scroll units. Captured/raw relative
mouse behavior and game scene/camera validation remain pending.

Native game-input hold/repeat follows the existing elapsed-time PC mechanism,
using the engine's monotonic Time counters in place of QPC. The same 60-unit
held-time scale, crossed-tick repeat rule and 0.25-second clamp apply. Native
checks compare 300 rapid input updates against elapsed time, then wait 85 ms
and verify held time advances. This preserves elapsed-time behavior without
advancing controls per rendered frame.

A disabled-device regression exposed inverted Enable/Disable assignments and
getters that ignored the documented disabled state. Native-only fixes restore
the contract and return zero while disabled without losing pending held state.
Found in / Belongs to: local macOS input integration. Windows behavior unchanged.

Controller work consulted Apple's GCController/GCExtendedGamepad documentation
and SDK headers: https://developer.apple.com/documentation/GameController/GCController,
https://developer.apple.com/documentation/gamecontroller/gcextendedgamepad and
https://developer.apple.com/documentation/gamecontroller/gccontroller/withextendedgamepad().
No controller code or hardware verification is included in this milestone.

### MAC004 — first upper Display bridge and immediate-context compatibility

The original DisplayToDisplay2.cpp now compiles in a Jam-derived upper Display
library and its conversions drive real native shader draw/readback checks.
DeviceTypesEnums.h selects the existing PC vertex semantic/format branches on
macOS; quad-only PS3 conversion is excluded. Upper DeviceDefines identifies the
Metal platform, but the actual upper C_Device implementation remains pending.

Missing metadata enum IDs follow original D3D11Context.h and Microsoft's
D3DFORMAT definition (https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dformat).
Present interval IDs follow the PC constants; numeric reference:
https://learn.microsoft.com/en-us/previous-versions/ms858180(v=msdn.10).
Declaring D24S8/D24FS8, G16R16, packed integer formats, surface L8 or interval
metadata does not implement their GPU operations. D24S8 texture and surface
creation remain explicitly rejected in the regression. Stencil/MSAA and special
present intervals remain pending.

C_ContextNoCheckNoTrack and C_ContextNoCheck now provide copyable non-owning
views over the real immediate native context. Engine device getters retain the
original reference-return convention. Views forward actual operations and state
references; native validation and owner-thread checks remain active. Exported
copies hold weak references: device destruction expires them, stale operations
throw, recreation does not silently rebind them, and explicit Bind reconnects a
view. As with the existing native device interface, device destruction must be
serialized against active context calls. No independent deferred context or
state-cloning implementation is claimed.

The helper's red-to-green shader reload now executes its GPU state/clear/draw
through these views and the original upper enum conversions. Buffer lifecycle
checks verify unbound use rejection, shared state identity, stale-view rejection
and explicit rebind after device recreation. Full upper Display context/frame
composition and original application binding remain pending.

### MAC004 — GameController pads, stable slots and CoreHaptics adapter

The native Input manager polls Apple's GameController framework and creates the
original C_Pad360 class with native implementations of its actual public API.
Extended gamepad snapshots map digital face/dpad/shoulder/menu/options/thumb
buttons, analog triggers and four stick axes into original enums. Original
circular deadzone handling is reused (default 0.25). Physical controller identity
stays in its existing local slot when enumeration changes; disconnects clear
input and release that slot, without shifting surviving players. Up to four
slots are retained; extra controllers wait for a free slot. Micro-gamepad-only
profiles are not treated as extended pads. Focus loss and disabling devices
clear action input. Native raw/captured mouse, IME and physical hardware
verification still remain pending.

The private Controllers::PollList and manager PollControllerList entry points
allow deterministic authored Apple GCController snapshot arrays in checks;
production Poll queries [GCController controllers]. Tests never register virtual
HID devices or replace the system controller list. Actual framework snapshot
objects validate controls/analog values, enumeration reorder, a fifth device,
disconnect/reassignment, four original per-user action mappings and focus loss.
This is source-only adapter evidence, not a four-physical-pad or LAN session.

Controller rumble has a native CoreHaptics implementation for devices reporting
both left/right handle localities. The low channel uses the left handle with
low sharpness; the high channel uses the right handle with high sharpness.
Intensity follows the requested [0,1] values. Continuous controller haptic events
use GCHapticDurationInfinite; intensity updates use dynamic parameters rather
than frame-counted pulse durations. Players start muted until their requested
intensity has been applied. Zero rumble, disabling/focus loss, disconnect and
destruction stop players/engines. Engine reset/stopped callbacks signal a shared
atomic generation flag; callbacks cannot access a destroyed slot, and the next
update rebuilds invalidated engine/player state. Errors stop both channels;
unsupported localities return failure and the original pad interface logs one
warning. No fallback claims unsupported actuators work.

Snapshot objects without haptics reject nonzero motor requests in the check.
Actual physical rumble, device disconnect during motor playback, server-reset
recovery, latency and mechanical equivalence have NOT been verified here.
The native platform mapping is not evidence of bit-identical XInput motor
behavior. Controller haptic support depends on reported Apple localities.

Authorities consulted: Apple's GCController/GCExtendedGamepad documentation and
the installed SDK declarations, plus GCDeviceHaptics createEngine(withLocality:)
(https://developer.apple.com/documentation/gamecontroller/gcdevicehaptics/createengine(withlocality:)),
GCDeviceHaptics supportedLocalities
(https://developer.apple.com/documentation/gamecontroller/gcdevicehaptics),
and CHHapticPatternPlayer sendParameters
(https://developer.apple.com/documentation/corehaptics/chhapticpatternplayer/sendparameters(_:attime:)).
Apple explicitly describes separate left/right handle engines as woofer/tweeter
localities. No user background-monitoring setting is changed.

### MAC004 — original upper CPU device, views and renderer lifecycle

Ten upper Display Jam source files now compile and force-link together:
DisplayToDisplay2, DeviceBase, View/ViewFrustum/Viewport/ViewSettings/ViewSetup,
Renderer, DebugLog and Globals. NativeDeviceAccess adds an explicit scoped
binding for the actual C_DeviceBase CPU services, avoiding a dependency on the
unported Win32-derived C_Device declaration. Native views read original depth
flip state and renderer constructors/destructors register/remove with that
bound actual CPU device. The future native upper GPU device can own this
binding. Unbound access throws, duplicate binding is rejected, and scope exit
clears it. Binding lifetime/device destruction must be serialized against
clients, as with the original global device. It does not create a GPU device
or supply successful rendering methods. The check verifies IsCreated remains
false for its CPU fixture.

Shared sources use explicit native dependencies in place of the Display PCH.
PC dynamic-aspect behavior is selected on macOS rather than reverting to a
fixed aspect enum table. Projection/frustum algorithms are unchanged. Checks
verify a split view scales to physical dimensions, perspective and orthographic
near/far mapping in conventional/reversed depth, center/outside sphere culling,
dynamic aspect, view teardown, original renderer create/volatile/initialise/
reset/shutdown/destruction, and original partial-failure cleanup. The renderer
fixture performs no GPU work; it validates the actual lifecycle machinery.
Original Globals uses the real native C_FileSystem singleton. Existing empty
original functions (e.g. SetupTilingPlanes/Output) are not claimed implemented.

This removes header/source blockers for the actual native upper device; it is
not game startup or frame composition. The native upper GPU C_Device/resources,
full context, windows and application/game binding remain pending. Sixty-one
upper Display Jam entries remain pending in the generated inventory.

### MAC004 — upper native resource selection

Native upper buffer/declaration/shader/texture/render-texture/surface headers now
select actual Display2 Metal wrapper types through MetalResources.h. The native
vertex terminator retains PC stream 0xff and type 17 without a Direct3D header.
Original Texture/TextureParams/VertexFormat translation units compile and
force-link (VertexFormat is empty). Upper Display is now thirteen original
source entries plus native CPU-device binding; fifty-eight original entries
remain pending. The GPU helper now builds the upper declaration terminator and
uses upper shader/surface names for actual shader/clear/draw/readback work.
PC shader-profile/constant-lock metadata shapes are declared for compatibility;
these declarations do not implement runtime legacy-bytecode translation or
constant locking. Invalid native profile indexes throw rather than indexing
outside metadata. The actual upper C_Device adapter is still pending.

### MAC004 — actual upper GPU device and opaque depth/stencil

Fifteen original upper Display sources plus NativeDeviceAccess/MetalGameDevice
compile and force-link. Shared C_Device registration/Create/CreateVolatile/Destroy
now own the real Display2 Metal manager/device, tracked GPU resources, framebuffer
surfaces, declaration cache and CAMetalLayer presentation. Default states match
original CPU state and native bindings. mac_game_device verifies invalid creation,
original memory-category TLS, resource allocation, default D24S8 startup, six
minimized presentations, GPU colour/D16 depth readback and volatile recreation.
This is source-only engine integration, not actual game startup or a race.

D24S8 surfaces select Depth24UnormStencil8 where supported, otherwise
Depth32Float_Stencil8 (M4). This is an explicit native adaptation: M4 depth uses
float32 precision, not identical PC 24-bit unorm quantization. Nonlockable D24S8
never exposes an invented packed CPU layout. Nominal PC size helpers remain
four bytes per pixel; actual native storage accounts for the Metal format. D24
textures/streamed depth data remain unsupported. Native render passes and pipelines
attach combined stencil storage. Full and viewport/scissor partial stencil clears,
all eight operations/comparisons, read/write masks, stencil-fail/depth-fail and
clamp/wrap boundaries pass actual GPU colour readback in mac_stencil.
Authorities: Apple Metal MTLPixelFormat depth32Float_stencil8 and SDK stencil enums;
Microsoft D3DFORMAT defines D24S8 as nonlockable and opaque.

### MAC004 — asynchronous layer destroy queue publication

Found in / Belongs to: native MAC004 framework integration. The full suite
reproduced a real worker wakeup race: DestroyLayerAsync signals a pending request
before main-thread DestroyPendingLayers publishes it to the asynchronous destroy
queue. A worker can consume that wake, find an empty queue and sleep forever.
Native code now signals status dirty after PushBack under the queue lock. The
fixture permits the early request wake to be consumed before main Update; its
original three-second deadline remains unchanged. A privately rebuilt control
archive removing only the new publication wake fails with the original timeout;
the fixed fixture passes sixteen repeats. Control source/archive/log live under
_work/build/macos/framework-publication-control/. Windows branches are unchanged.

All 28 checks pass sequentially in Debug, Speed, Debug UBSan and Speed UBSan
(mac004-stencil-matrix-*.log). The standard build, all 28 checks and bundle
Metal/readback self-test pass in mac004-stencil-standard.log. ASan remains
unavailable on this host; no ASan success is claimed. Actual game/application,
remaining upper contexts/frame composition, audio and runtime systems remain
pending. No game screenshot, retail scene/save/LAN or physical controller evidence.

### MAC004 — original upper contexts drive native GPU draws

Two more original entries, DeviceContext.cpp and Context_Base.cpp, compile and
force-link with the native MetalGameContext implementation. Seventeen original
upper entries/three native entries are selected; 54 original entries remain
pending. Original registry allocation/initialisation/clone/flags/destruction and
shared state conversion/draw forwarding run against actual Metal objects. Native
headers select explicit portable dependencies rather than the broad Win32 PCH.
Base colour/depth/stencil surface binding forwards real attachments on macOS;
PC stencil-mask metadata is retained. Prototype metadata and native texture
pointers start deterministically.

Native C_Context CloneState(nullptr) explicitly binds a created upper device.
Cloning retains weak native context identity plus declaration/index/texture/view
metadata. A destroyed device expires all old views; recreation requires explicit
rebinding. Immediate contexts share GPU state and do not promise independent
deferred command buffers. Non-null deferred command buffers, point sprites and
enabled alpha-to-coverage reject until those features are integrated. Disabled
point sprites/coverage and the original immediate no-cache invalidation work.
Blend precision follows native render-target format rather than a separate D3D9
hint. Original upper sampler maxima now feed independent native per-slot
anisotropy overrides; lower callers can still use their global maximum.

mac_game_context creates the actual upper device offscreen, then original
registered contexts. Shared setters, reflected pixel parameters/arrays, indexed
and nonindexed draws and stencil state produce four independently expected
full-frame GPU colour outputs. Original context flags and cloned metadata pass;
stale views reject after shutdown/recreation and explicit rebind succeeds.
No actual game content or scene runs. Targeted checks and full 29/29 sequential
Debug/Speed/Debug UBSan/Speed UBSan suites pass in
mac004-game-context-test.log and mac004-game-context-matrix-*.log.

### MAC004 — original dynamic vertex allocator and transient draw path

Eighteen original upper Display entries/three native entries compile and
force-link; DynamicVertexBuffer.cpp is selected from Jam. Native branches retain
original PC dynamic allocation intent, double-buffer rotation and actual numeric
discard/no-overwrite locks (0x2000/0x1000). Native lock implementation already
renames discarded resources and validates in-flight ranges. Begin validates
count/stride/product/capacity before state mutation, resets failed maps, rolls
back partial creation and retires mapped data before destruction. These guards
are native-only; inherited Windows implementation remains unchanged.

Upper native Begin/EndVertices reuses this actual allocator and original PC
draw/budget mechanism. Default total/reserve are the original 2.25 MiB/256 KiB.
Presentation attempts rotate buffers and reset both usage budgets as PC does,
including headless attempts (which still return false for presentation). CPU/GPU
frame storage changes do not advance simulation ticks. Native context forwards
Begin/EndVertices and retains its new declaration metadata. Outstanding maps
prevent normal EndScene; explicit volatile destruction retires the map and
releases both tracked buffers. Budget setters reject invalid native capacities
and nonfinite/out-of-range scales.

The context GPU fixture now consumes real vertex attributes. Two distinct
left/right transient geometries and immutable shader constants retain their
expected red/blue pixels across six frames/both buffer rotations; fake-end
allocations do not draw. It verifies nested mapping rejection, wide multiplication
overflow/zero stride, user budget, unchecked-budget physical capacity/exhaustion,
mapped scene-end rejection and volatile shutdown/recreation. Targeted 3/3 passes;
full sequential 29/29 suites pass Debug, Speed and both UBSan configs in
mac004-dynamic-vertices-matrix-*.log. Standard 29-suite build plus native bundle
Metal/readback self-test pass in mac004-dynamic-vertices-standard.log. The prior
context standard build also passed (mac004-game-context-standard.log).
No actual game/race/save/LAN or physical controller evidence.

### MAC004 — original BasicRender service and upper PC blend table

Four more original units compile/force-link: BasicRender.cpp, VertexShaderUtils,
PixelShaderUtils and File.cpp. Native MetalGameStates adds native setters and the
original upper PC blend-mode table copied from D3D11GameDevice.cpp; upper Display
is now 22 original/four native entries, with 49 original entries pending. Native
helpers use explicit dependencies and bmsl extensions. BasicRender uses its
existing file-loading path for translated shaders; no retail shaders/assets or
DXBC fallback are downloaded or invented. The optional full embedded archive
remains separate.

Native GPU_RGBA packs ARGB integers/BGRA memory bytes to match the existing Metal
PC Colour vertex declaration (MTLVertexFormatUChar4Normalized_BGRA). Both component
and Maths::RGBA overloads agree. The inherited generic source packing remains
unchanged for other platforms. GPU red/blue colour checks establish actual
channel order, rather than relying only on integer assertions. Native SetView
binds the original viewport and uploads the original 2D/3D shader registers;
null-view 2D fallback retains the exact PC 1024x768 clip constants from
D3D11GameDevice.cpp. Configured-view and fallback coordinate systems both draw
correctly into the physical 64x32 fixture framebuffer.

Upper device blend modes use the actual original PC table, including independent
alpha factors (which differ from Context_Base's separate shared table). Native
setters update CPU fields and GPU state. The original table reapplies after
other immediate contexts change state, avoiding stale higher-level cache skips.
GPU alpha readback verifies SrcAlpha/InvSrcAlpha factors, not just RGB output.
Unsupported native alpha-mask/invalid blend modes reject before mutating state;
shared native Context_Base also rejects its absent alpha-coverage modes.

mac_basic_render compiles six Jam-authored HLSL programs through the real shader
toolchain, then executes the original renderer. Deliberately missing the final
pixel program causes actual partial-create cleanup; restoring it permits retry.
Original renderer lifecycle, two-colour geometry, cached-state interference,
alpha colour/alpha output, textured 2D and textured 3D triangles, view/default
constants and transient draw storage pass GPU readback. The 3D triangle uses an
explicit identity view-projection fixture; this is not a retail camera/scene.
The raw authored image is a 2x2 solid colour. No game content is represented.

Targeted 4/4 and full sequential 30/30 Debug/Speed/Debug UBSan/Speed UBSan suites
pass in mac004-basic-render-test.log and mac004-basic-render-matrix-*.log. After
retaining/testing exact PC fallback coordinates, the affected BasicRender GPU
check passes all four rebuilt configs (mac004-basic-render-fallback-*.log).
Final standard build, all 30 checks and native bundle Metal/readback self-test
pass in mac004-basic-render-standard.log. No actual application/game/race/save/LAN
or physical haptics validation, no completion screenshot or public release.

### MAC004 — opaque native depth textures and sampling

D24S8 2D textures/render textures now select real depth24/stencil8 storage where
supported, otherwise depth32float/stencil8 (M4), matching surface adaptation.
Nominal PC texture-size helpers remain four bytes/pixel; native allocation
accounting records actual storage. D24 CPU locks stay rejected through both
texture and surface aliases. Native D16/D24 shader views retain identity depth
storage; colour-only swizzles are not applied. Depth/stencil colour flags and
unintegrated D24 cube/volume creation reject. MSAA remains unsupported.

mac_depth_texture clears GPU depth mips and rasterizes actual geometry at depth
0.5, then samples the depth component through ordinary texture2d<float> sampling
into independently checked colour pixels. D16 depth sampling also passes. Mip
selection, two-slot read/write Resolve, copying both opaque mips, preserving
completed read data while the other slot is attached, and same-attachment
sampling feedback rejection pass. No packed depth/stencil CPU representation or
stencil texture sampling is invented. Native float depth is not bit-identical
to PC 24-bit quantization.

Apple Metal Shading Language specification (2026-06-04), section 2.9, states
that depth texture types are needed for sample_compare; ordinary sampled
depth-component reads are verified on the actual GPU here. Combined format
support is checked via depth24Stencil8PixelFormatSupported and the native
feature table. Primary sources:
https://developer.apple.com/metal/Metal-Shading-Language-Specification.pdf
https://developer.apple.com/metal/capabilities/
The downloaded manual/extracted diagnostic text remain ignored under _work/docs.

Targeted 4/4 pass; final full 31/31 sequential suites pass Debug/Speed/Debug UBSan/
Speed UBSan in mac004-depth-textures-matrix-*.log. Final standard build, all 31
checks and native bundle Metal/readback self-test pass in
mac004-depth-textures-standard.log. No actual game scene/startup/race/save/LAN
or physical controller validation.

### MAC004 — original Quad, declarations and default shader registries

Render.cpp, VertexDeclarations.cpp, VertexShaders.cpp and PixelShaders.cpp now
compile/force-link: 26 original upper entries/four native entries, with 45
original entries pending. Native default shader factories load the same authored
program symbols from bmsl files. A missing program triggers cleanup and reports
failure; it is not treated as an initialized renderer. Default vertex-colour
file naming follows its actual psDefaultVertexColour symbol (the PC embedded
call's metadata name was psDefaultTexture). Native packed-colour macro matches
the original PC declaration. New upper setters retain original default depth/
cull/flip selection and actual CPU viewport state; flipped depth ranges remain
unintegrated and reject.

mac_quad compiles eleven actual default programs, creates the original global
vertex declarations/registries, then verifies actual GPU output: constant and
vertex colours, a four-colour image, both texture flips, two physical viewports
and partial-screen geometry. A deliberately missing final pixel program rolls
back other newly created globals, then retries successfully. The test initializes
cull before the first scene as well as exercising defaults during scenes.

Found in / Belongs to: native MAC004 device reset ordering. Initial Speed UBSan
full-suite run exposed a genuine uninitialized m_CullModeFlip bool in the new
setter (invalid value 176). The original base constructor leaves this cache
field to InvalidateState, which PC Reset invokes. Native constructor/default
application now invokes that original initialization machinery before public
setters/defaults. Original inherited base constructor/Windows behavior is
unchanged. The pre-fix failure remains in
mac004-quad-matrix-speed-ubsan-before-state-init.log.

After the fix, full sequential 32/32 suites pass Debug/Speed/Debug UBSan/Speed
UBSan (mac004-quad-matrix-*.log); standard build, all 32 checks and native bundle
Metal/readback self-test pass in mac004-quad-standard.log. No actual game startup,
race, save/LAN, physical haptics or completion screenshot.

### MAC004 — actual upper framebuffer/window ownership and composition

Upper Display now compiles/force-links 26 original entries and five native
entries (MetalGameWindows added); 45 original entries remain pending. Native
C_BackBuffer owns the tracked colour/depth surfaces behind the original upper
getters. C_RenderTarget records actual bound upper attachment identities and
formats/dimensions; missing attachments reject getters that require them. Native
attachment changes select a full-target viewport coherently in CPU/GPU state;
callers select split viewports after binding the destination.

C_Window retains original PC fields and exact normalized-coordinate sizing
arithmetic from D3D11Window.cpp. Native coordinates/dimensions are validated
before unsafe casts. Each window owns actual colour/depth-stencil/mask resources;
external scene depth texture remains non-owning metadata as on PC. Window-buffer
resolution and screen dimensions stay separate. Native zero window dimensions
use the explicit framebuffer dimensions; native default MSAA is None. Per-view
mask support currently requires D24S8. Unsupported MSAA/format creation rolls
back all partial window/frame resources; metadata survives volatile retries.

SetWindow/EndWindow, stencil-equal-255 mask generation using actual original
Quad::DrawVertexColour, and RestoreWindow follow the original PC resource
mechanism. Restore uses a textured quad onto the back buffer or size/format
conversion, and a direct GPU copy onto matching offscreen storage. Native
CopySurface appends a real blit within the current Metal command stream and
retains surface hazards until submission; no CPU texture read/copy or fabricated
scene is used. Native RestoreWindow additionally honours its colourWrite
argument; partial masks select the shader route rather than whole-resource copy.
The inherited PC implementation ignores that argument; no Windows code changed.

Native upper scene boundaries now match the original Reset/Unbind contract:
clear the current view, apply original state initialization/defaults, unbind
shaders/declarations/buffers/textures and set default view constants. The direct
Display2 context retains its original separate persistence semantics. A GPU check
requires explicit bindings after upper BeginScene. Dynamic-geometry fixtures
now bind their shaders each scene accordingly. Native C_Device destructor
retires owned resources before clearing the original singleton, because the
shared dynamic-buffer helper uses that singleton during destruction. Implicit
owner cleanup is verified using a real minimized Cocoa host and GPU resources.

mac_game_windows creates two actual rendered views and verifies a red/blue
composed framebuffer plus independent stencil-mask pixels. It also verifies
screen/render sizing, original attachment identity, same-size offscreen GPU
copy, red-only restore preserving destination G/B/A, scene-unbind rejection,
unsupported-MSAA partial cleanup, scaled-resolution recovery and metadata
clear/recreation. Programs are compiled from eleven original authored HLSL
symbols through test_quad.py. This remains an engine fixture, not the game.
Targeted 5/5 passes; full sequential 33/33 Debug/Speed/Debug UBSan/Speed UBSan
suites pass (mac004-game-windows-matrix-*.log). Final standard build/all 33/native
bundle Metal/readback self-test pass in mac004-game-windows-standard.log.
No actual startup/menu/race/save/LAN or physical controller verification.

### MAC004 — original display manager on native Metal/display modes

Manager.cpp, ManagerBase.cpp and WindowBase.cpp now compile/force-link with
MetalGameManager.mm: 29 original upper entries/six native entries, 42 original
entries pending. Original C_Manager singleton, device list, declarations, default
shader factories and owned/indirect lifecycle run on real native C_Device.
Native failure handling rolls back partially loaded shaders/declarations/device
ownership and returns failure with a concrete error. Empty/group/destructor
cleanup is safe; a second upper singleton is rejected before allocation.
Indirect caller-owned devices must be unregistered with DestroyDeviceIndirect
before their own destruction, as required by the shared manager contract.

C_PlatformManager owns a real lower Metal adapter probe. It reads the primary
display through CoreGraphics, reports physical framebuffer pixel sizes and
actual refresh values (zero remains unknown), deduplicates/sorts supported
resolution metadata and provides live desktop queries. No display settings are
changed. The legacy PC wide-screen/HiDef flags remain false; native views use
the actual dynamic aspect. Only implemented windowed BGRA8/D24S8/no-MSAA
capabilities are advertised, regardless of additional hardware capabilities.
Fullscreen/mode switching remains unsupported. Native errors describe Metal
rather than Direct3D. SDK CGDirectDisplay.h is the local API authority.

Found in / Belongs to: native MAC004 mode enumeration. The inherited
C_VideoMode comparison returns true for equal resolutions, violating sorting's
strict comparison contract. Native mode sorting requires that comparison; the
native-only equal case now returns false. Windows behavior remains unchanged.
Native fixture checks strict/deduplicated order and mode lookup using actual
reported modes.

mac_game_manager verifies real adapter identity/physical modes, capability
limits, missing-shader factory rollback and retry, core-before-volatile state,
actual expected GPU colour output through manager-created shaders, owned and
indirect destruction, group cleanup, adapter restart and implicit manager
cleanup. It uses source-built default shaders staged by test_quad.py.
Targeted 4/4 and full sequential 34/34 Debug/Speed/Debug UBSan/Speed UBSan pass
in mac004-game-manager-test.log and mac004-game-manager-matrix-*.log. Final
standard build/all 34/native bundle Metal/readback self-test pass in
mac004-game-manager-standard.log. No actual application/game startup, race,
save/LAN or physical controller validation.

### MAC004 — original framework core startup services

CoreSystems.cpp now compiles/force-links as the seventh original framework
entry; ten original entries remain pending. The actual C_CoreSystems singleton
starts native async I/O, existing debug-log configuration, actual Artery/debug/
automation/memory/data-log connections in configured builds and the actual
Resource::C_Manager. The existing filesystem/default-global bridge is retained.
Framework links the real resource/logicalFs/Display services it now references.
The optional disabled FS performance logger is not claimed initialized.

Native startup validates its physical working directory before services, checks
IoInitialise's real result and sets initialized only after all required services
exist. Missing-root/partial startup rolls back actual ownership; duplicate
initialization preserves an already running core. Resource manager starts null
so unused/failed shutdown is safe. Static type registration remains available
after failed startup/retry; successful shutdown preserves the original registry
retirement order. Native core only stops I/O that it started; externally started
I/O remains live until its owner stops it. Network/service flags reset on
shutdown. Native destruction retires live services before clearing the original
singleton. Windows code paths remain unchanged.

mac_core_systems runs separate owned/borrowed-I/O processes to avoid promising
whole-game type-registry restart semantics. It verifies invalid-root failure,
real resource singleton, actual worker read/close of independent authored bytes,
configured network startup, sixteen real update calls, duplicate initialization,
idempotent shutdown, resource/worker counts and preserved external I/O ownership.
No retail data or application/game layer runs. Targeted 3/3 and full sequential
35/35 Debug/Speed/Debug UBSan/Speed UBSan suites pass in
mac004-core-systems-test.log and mac004-core-systems-matrix-*.log. Standard build/
all 35/native bundle Metal/readback self-test pass in
mac004-core-systems-standard.log.

A private native syntax probe including FrameworkPch.h, Display.h and the
original ApplicationBase header now passes. This confirms header integration,
not compilation/linking of ApplicationBase.cpp or a running application. Probe
source/log remain ignored under _work/build/macos/. Original application,
render/camera/UI/audio/remaining runtime and game integration remain pending.

## MAC004 — original application bodies, camera archive and native capture

All 34 Jam camera2 entries now compile and force-link into mac_camera. The
camera PCH selects Darwin before OS headers; standard dependent-template
keywords in UT operator/vector headers make existing camera serialization
compile on Clang. Arithmetic/serializers are retained. Original movie encoding
remains disabled by its existing COMPILE_OGG selection; no native encoder is
claimed. No actual game camera/scene/data is loaded.

mac_camera exercises original animation key interpolation, previous/current
camera pose and FOV, and nine render fractions within one fixed simulation
tick. Rendering leaves the tick pose/FOV/period unchanged. The original debug
feed exposes the application camera. Native LockScreen reads completed actual
BGRA8 GPU pixels with the original read-only lock flag, takes an owned CPU
snapshot, and immediately releases GPU mapping. It rejects unsubmitted frames,
nested captures, absent framebuffers and other formats; captured bytes remain
valid after device/framebuffer destruction until UnlockScreen. The capture
format is the existing ScreenToTgaFile little-endian BGRA branch (0), not a
new disk format. The fixture verifies actual bytes, not an image substituted
for GPU output.

Native upper renderer ownership now uses a separate original recursive MT
mutex with a checked owning thread/depth; cross-thread timeout, invalid release,
recursive release and ownership transfer run. BlockUntilIdle waits for actual
submitted Metal work and rejects recording scenes. SetPresentInterval updates
CAMetalLayer displaySyncEnabled, including inside CommonRender's recording
scene; unsupported intervals fail without changing state. Native SetupView
uses the original C_View::Setup algorithm and actual bound or explicit target
sizes. Optional zero screen/window dimensions derive from host framebuffer
sizes while explicit dimensions are preserved. These paths and six minimized
actual drawable presentations run in mac_game_device.

Sixteen of seventeen original framework entries compile: seven existing
service entries still force-link; nine application entries now form a separate
blur_mac_framework_application archive. ApplicationPC.cpp remains Windows-only
and needs a native host. Rendering headers now own explicit native dependencies.
Native application guards retain PC display-window sizing, three viewports,
per-view colour/depth/mask publication, render/restore order and pad/mouse/key
creation. They do not define PLATFORM_PC on Darwin. The D3D screenshot declaration
and existing Windows automation trace remain backend-specific. Native manager
bool results are explicitly converted to S_OK/E_FAIL; false previously passed
the inherited HRESULT failure test as success. Native SetupDeviceParams keeps
actual host/settings dimensions instead of console 1280x720 arguments (the PC
Setup function is empty). The signed performance timestamp stays 64 bits.

Nine original render support sources compile into blur_mac_render. Native fill
and shader-parameter forwarding use actual existing lower GPU state/reflection;
SetPixelShaderParameters retains the PC deferred-until-draw constant behavior.
The original globals now publish native per-view stencil masks. Compilation of
these archives is not a complete application link. A private actual link probe
fails on remaining original display debug/font/timing/resolution helpers,
Render view manager/primitive support, texture stream loading and eight authored
font/primitive shader symbols. Logs and probe source are ignored under _work/.
No missing function is filled with a success stub.

Sequential 36/36 pass in Debug, Speed, Debug UBSan and Speed UBSan in
mac004-camera-matrix-*.log. All nine application bodies also compile in all
four configurations (mac004-application-archive*.log). Standard build/all 36/
native bundle Metal-readback self-test pass in mac004-camera-standard.log.
The shader compiler and all-693 inputs were not changed in this milestone.
No original game startup/menu/race/save/LAN, physical gamepad proof, native
movie encoding, signed release or publication is established.

## MAC004 — native PC texture streams and original application link

Twenty-nine original upper Display entries plus seven native adapters remain
in the force-linked core archive. Eighteen more original Display application
helpers and eleven original Render support entries compile in every checked
configuration. ApplicationPCH selects Darwin before OS includes. Native font/
texture diagnostic renderers select the existing authored embedded shader path;
ADL calls are explicitly qualified because native resources are Display2 aliases.
Original view-manager/primitive support now resolves the application dependencies.
The original ApplicationBase body now links and its private instance-only probe
runs (exit 0) with real original helpers and the existing built-in shader archive.
That probe does not initialize an application/window/game. Remaining native host,
transactional application startup and actual game integration remain pending.

Native GPUTiming keeps original scope/TLS bookkeeping but explicitly disables
per-scope publication and sets depth -1: no native counter mechanism is present,
so zero CPU counters are not presented as GPU durations. Actual per-scope GPU
counter integration is still pending; this is not a GPU profiling implementation.

The new MetalTextureLoad adapter consumes the existing seven-field, 28-byte PC
TextureDesc and face-major/mip-major payloads. It checks dimensions, mip limits,
2D/cube kind, format and signed byte-size bounds before allocation. Layout comes
from the original PC tight-row conventions already implemented by native Display2.
Payload sizes must end on a whole mip; newly streamed top-mip prefixes are accepted.
All requested bytes are read before GPU mapping, so short reads do not change an
existing texture. Full-load failure destroys the newly created tracked resource.
Native staging uploads copy individual tight file rows to aligned Metal rows.
Already-created resource replacement, partial BC root dimensions, volume/depth
stream uploads and clone/common-mip-tail migration remain unsupported. No data
format, replacement alias or success stub is invented for these cases.

mac_texture_stream verifies independent PC bytes in actual private GPU storage:
2D and six cube faces, every mip, BC1/BC2/BC3 and RGBA32F, complete top-mip updates,
short reads and partial-mip rejection without mutation, invalid/unsupported
headers, full-load rollback and exact consumption. mac_texture_params exercises
the actual original UploadTexture2D and texture container paths; native upload
now respects Metal row pitch. Upper DDS/BMP/TGA APIs reuse the already verified
native Display2 decoder rather than introducing another decoder or porting the
legacy image-parser bugs. Native file fixtures include odd widths, origins,
BMP offsets/strides, mip bytes and truncated images. Retail texture resources
remain unverified.

Full sequential 37/37 and then 38/38 pass all four configurations in
mac004-texture-stream-matrix-*.log and mac004-texture-params-matrix-*.log.
Original application archives remain separate from the actual future Amax link.
The earlier application link failure is superseded by the successful private
probe in mac004-application-link-probe.log. Built-in compiler/693 shader inputs
are unchanged; no unnecessary complete shader regeneration was performed.

## MAC004 — reuse upstream SDL3 for native PCM output

Following explicit user steering to prefer open-source implementations, the
native output adapter uses SDL3 3.4.18. The official release archive was downloaded
and its SHA-256 matched 9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3.
CMake FetchContent now uses that pinned URL/hash, a shared archive cache under
_work/vendor and private per-build _deps source/build trees. Upstream source and
notices are preserved; licenses/SDL3.txt is copied beside local native output.
No global package or host audio setting was changed. See dependencies.md for
primary source/API links and current reuse decisions.

Audio::Mac::Playback is an actual PCM output adapter, not the game's audio mixer.
SDL owns CoreAudio, queueing, rate/channel conversion and device cleanup. The
adapter opens paused float32 streams, validates rate/channel and byte-size inputs,
queues real samples, pauses/resumes/clears and reports real pending frames. It
owns one SDL audio subsystem reference and preserves externally owned references.
Original PC currently uses DirectSound and already mixes completed PCM blocks;
this is the intended output boundary. Original mixer/DSP/decoder/voice integration
remains pending (181 original native-audio manifest entries); one native output
source is selected from Jam. No game sound or codec is claimed as running.

mac_audio_output verifies that the actual selected backend is coreaudio, not a
dummy device. Silent PCM at 48000 Hz/stereo, 24000 Hz/six channels and 36000 Hz/
eight channels is retained while paused and then consumed by the native output.
Invalid/duplicate initialization and invalid blocks preserve the active stream;
pause/clear/restart/owned versus borrowed SDL cleanup pass. No microphone or
audible tone is used. Full sequential 39/39 pass Debug, Speed, Debug UBSan and
Speed UBSan in mac004-sdl-audio-matrix-*.log. Original game startup/menu/race,
physical controller/haptics, saves and LAN remain unverified; the complete-game
goal and MAC004 remain active.

## MAC004 — native application runs original framework rendering

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

## MAC004 — original audio renderer and shared PC mixing through SDL

All 181 original common audio Jam entries initially compiled and force-linked
with the native worker/output adapter. Actual renderer rings feed SDL/CoreAudio;
the unchanged PC mono/stereo/surround mixing implementation is shared in
OutputMix.inl rather than copied. Native workers join before their buffers are
freed, producer/consumer counters and mute/shutdown controls are atomic, queue
backpressure is bounded, and quality changes restart actual output while
publishing the PC decoder pitch ratio. Original factory ownership is retired
on destruction, permitting a fresh native renderer. Thread/queue/profiling
helpers use actual native elapsed time and atomics. Unsupported asynchronous
suspend/termination and non-normal thread priority report unsupported.

The force-loaded archive exposed two startup faults under UBSan: base-offset
reflection fabricated address 1, and a static debug callback used an absent
System singleton to name a static function. Native base metadata now uses a
live aligned object; the callback is registered by its qualified static name.
The former requires default-constructible reflected native data types; any
nonconstructible/abstract base metadata encountered later needs an explicit
mechanism, not a guessed offset or disabled sanitizer. Original PC helpers are
retained. A fixture startup initially omitted the real scheduler group pool;
that fixture was corrected to use the actual scheduler initialization contract.
Failure logs remain under _work/build/macos/.

mac_audio_core force-loads the actual archive, then exercises worker
join/restart/duplicate rejection, concurrent original queue indexes, real
profiling time, original frame arithmetic/saturator, independent expected PC
speaker routing, actual renderer callbacks and silent SDL consumption at three
quality rates with stereo/5.1/7.1 input. Full 41/41 pass sequentially in Debug,
Speed and both UBSan configurations (mac004-audio-core-matrix-*.log), followed
by the standard build/all 41/native bundle Metal self-test
(mac004-audio-core-standard.log). This proves the renderer/output boundary;
whole Audio::System, game graph/bank/stream loading, spatial scenes and audible
game effects remain unverified.

## MAC004 — original world interpolation and public SoundTouch integration

Eleven original world_render sources compile, plus one native interpolation
wrapper from the authoritative MACOS Jam branch. CubemapPoints still needs the
real native FX texture-handle interface. The wider archive is not force-linked:
RenderingData/SceneGraph/FX bodies remain to integrate. The existing original
interpolation implementation is shared in ModelInterpolation.inl: the PC model
source includes it as before; the native wrapper permits a genuine isolated
interpolation link without faking the remaining world services. Native locks
replace Windows synchronization, with a map initialized before worker use.
The native application now installs actual WorldRender tick/render callbacks
by default, including application restart. No live world object/scene is claimed.

Native packed SHHD/SDDD constructors activate one union member and assign the
same signed bitfields; independent exact PC packing checks run. The world
fixture checks sixteen render fractions within one unchanged tick, restoration
of true matrices, skipped ticks, teleports, render-specific overrides, skin
palette layout and concurrent registration/removal of 128 real render hooks.
The native application fixture separately checks its actual default world tick
advances and its render scope retires on deferred shutdown.

The native pitch effect now reuses public upstream SoundTouch 2.4.0 from the
author's pinned source archive (dependencies.md). Its old wrapper copied fixed
256-frame blocks despite the PC path's actual 128-frame cadence, and had
uninitialized active/pitch fields. The native adapter initializes its state,
converts actual planar mono/stereo blocks, uses the active quality rate and
handles the library's returned frame count/latency. Seven old bundled
SoundTouchZ source entries are replaced in the native selection, leaving 174
original common audio entries plus the native engine and SDL adapter. Windows
retains its old implementation and source membership. This intentionally
upgrades native pitch processing; bit-identical retail audio is not claimed.
Offline guarded stereo tones verify +1 octave at independent channel frequencies,
finite/non-silent settled output, 128-frame boundaries and bypass behavior.
No audible test tone is sent to the hardware. Focused native audio/world/application
checks and the full updated 42/42 suites pass sequentially in Debug, Speed and
both UBSan configurations (mac004-world-soundtouch-matrix-*.log). Standard
build/all 42/native bundle Metal self-test also pass in
mac004-world-soundtouch-standard.log. git diff --check and Python syntax checks
pass. No game startup, race, save/load or game LAN is claimed. Next source work
is RenderingData/SceneGraph/FX integration; the private RenderingData probe
already compiles 19/24 entries and records missing native texture handles,
PC-path selection, archive return-type and streaming migration dependencies.


### Actual native game runtime, 26 October 2026

The actual original Amax application now links in Debug and Speed. Native
shaders live in bundle Resources/Shaders; only this configured prefix bypasses
the game overlay to the physical filesystem. Default shader fixture paths keep
their original contract. Current completed-frame diagnostics observe existing
rendering and do not synthesize UI or alter simulation cadence.

The actual game-data archive has117 extracted data members totalling
7,806,764,660 bytes. CRC32 and SHA256 were reread after extraction; private
provenance/audit are under ignored _work/runtime/research. Retail models.pak
is absent in the supplied archive and is treated as optional by original code.
All game assets remain outside tracked sources and the application bundle.

Actual secure mounting exposed native SHA1 returning success without updating
its hash. CommonCrypto now implements the real Init/Update/Final contract;
original RSA-PSS validation is enforced rather than accepting a nonGold
fallback. Independent OpenSSL verifies the original signed FAT, and original
C_PakFs reads the secure UI data byte-identically to independent extraction.
Retail texture bundles expose the original exporter's floor-block BC mip size:
omitted tiny mip tails are distinguished from partial/corrupt complete levels;
no bytes are borrowed from the next payload. MAC029 verifies66 UI texture
uploads. Actual startup now reaches C_UiManager creation after boot caching.

The first exposed callback error is `Native presentation interval is
unsupported`. Cleanup then found application OnShutdown destroying layers
still pending on the async creator. NativeQuiesceCreation joins that worker,
publishes completed layers, and permits cancellation of queued unstarted
layers before application dependencies are released. The focused framework
fixture covers an in-flight creator plus another queued request and exact
create/destroy/free counts. This is failure cleanup, not a gameplay bypass.

Use tools/macos/run_exclusive.py around actual game processes, CTest and
Artery service fixtures so the existing localhost6317 service cannot collide.
Automation clients run outside this lock while their game owns it. The wrapper
provides the native counterpart of the inherited Windows game-slot harness,
without terminating unrelated processes or changing the network protocol.


### Actual Showroom and UI render evidence

MAC054 reads the original158443-byte Showroom Level.levelphysics through
ValidatePlatformId, C_PhysicsLevel::Load and C_ImportScene::Import, including
original block wrappers. All four configurations verify1staticmesh,
3358triangles/555leaves/4925vertices/2142normals/1109nodes/862activeedges,
finite/ordered quantized bounds, reachable children, local vertex/edge indices,
nonoverlapping triangle ranges, file-version restoration and EOF rejection for
clipped real Main data. Disk v10 nodes are28bytes versus native runtime32;
triangle headers/triangles are decoded field-by-field from15/11 into16/12.
This establishes native real-data deserialization, not driving/simulation.

Actual UI rendering first requested1280x720 colour with2560x1440 depth at equal
sample count1. Original D3D9 permits larger depth; native now explicitly bounds
Metal render area to colour and preserves outside depth/stencil with scissored
GPU clear. Actual next run passes that check and GUIBank's screen-resolution
query, then Metal Validation catches an original UI fragment uniform block
with404 logical bytes but416 required compiled aggregate bytes. Original
psIntensityAsAlpha family ends its layout with a scalar at offset400; native
pipeline reflection now supplies actual compiled bufferDataSize instead of
assuming SPIRV-Cross occupied-byte count is the MSL sizeof. Logical bytes stay
unchanged and ABI-only tail bytes are zeroed. Actual frame validation pending.


MAC130 actual early-teardown evidence establishes that scene registration and
lazy typed renderer materialization are separate ownership states: scene11
versus renderer10, final LightTrails uninitialized. Native reserves one slot
per scene view, materializes at stable parent index and preserves the shared
nonordered permutation; rollback retains/cancels the explicit reserved owner.
The real lifecycle/first-error fixture exits0, but coherent integration and
actual-game clean exit remain pending. See view-slot-lifecycle.md.

MAC134 actual MirrorView sampler2 requests signed16 (+1mip); the original PC
backend divides by16 and applies sampler state to explicit LOD too. The
shared native loader now integrates both authored source and MojoShader MSL,
with exact explicit clamps and a build-copy TEXLDL correction. All693 authored
shaders pass the modified engine loader; all4 focused GPU binaries exit0.
Anisotropic biased explicit gradients and opaque sampler provenance remain
truthful unsupported boundaries, not silently ignored state. See
sampler-lod-contract.md; coherent actual-game validation remains pending.


## MAC324 — Quality-preserving optimization and reuse audit (2026-10-28)

Current delivered baseline is 0.1.4/build320, public 178ffbbf; historical bootstrap
limitations earlier in this append-only file are not current game status.
Read-only source audit confirms per-draw immutable depth/stencil creation,
DDS metadata path full-read/discard followed by a second mip read, temporary
draw preparation, synchronous shader compilation and unconditional target-setter
encoder endings. These are concrete repeated operations; new measured savings
are unknown. Existing Metal trace is CPU-span-only, so GPU/CPU bottleneck and
current memory high-water marks remain to be measured. See
[the ranked audit and validation contracts](optimization-and-reuse-audit.md).

Public host/window/playback headers still include original Core.h; Display2 Metal
links engine-system and embeds legacy state/shader contracts. Useful seams exist,
but an engine-independent installed SDK/second-client proof does not. Keep host
OS, runtime pointer ABI, GPU backend and PC serialized data schemas distinct.
Actual game asset loading uses original PakFs/PakEntryFile/GameSystems, not the
standalone NativePakArchive inspector. Reusable scope is source-port adapters,
maintained libraries, build/diagnostic tools and conformance fixtures; arbitrary
Windows executable conversion is not implemented. All 19 production/fixture
hashes from the delivered freeze remain unchanged; this audit adds no measured
performance gain or new game build.


## MAC330/334 — Bounded work removal and independent host qualification

Exact context-owned Metal state reuse preserves effective front/back stencil,
depth attachment behavior and dynamic reference. The final last-entry fast path
avoids hashing repeated adjacent state, while the owning map preserves other
entries across growth. An actual 20-car game diagnostic counted 7,597,565 draw
requests but only 28 factory calls, with zero failures. The GPU pixel/partial-clear
oracle verifies 680 draws + 63 clears with 64 creations; the no-reuse count
control passes pixels before its expected count assertion fails.

Native DDS2D now retains one validated original read through exact GPU mip/row
upload. The representative real-filesystem GPU fixture reads a 200-byte file
once instead of the pinned original's two opens/six backend reads/272 bytes.
Formats, L8 ghost-mask compatibility, filesystem authentication, resource flags
and other texture types stay unchanged. One temporary file owner overlaps
upload; neither peak-memory nor repeatable FPS savings are established.

The authored Cocoa host is an independently installed/relocated NativeHost::Window
CMake package with a separate find_package client. Blur compatibility names
alias the same implementation. A finite Metal triangle demo and window/event/
resize conformance run without engine headers/assets. The scoped MIT license
covers only this authored package. A general executable converter, renderer/
asset adapter for another engine and another clean Mac remain unverified.

Both complete final Debug and Speed suites pass 151/151; lower new fixtures
also pass both UBSan builds and seven focused Metal-validation tests. Muted
actual-game driving/normal exit passes. Sequential race timings are mixed,
including worse no-input rates and remaining hitches; do not advertise a
repeatable FPS increase. See [the full evidence and ablations](optimization-host-acceptance.md).
