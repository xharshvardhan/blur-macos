# Native state reuse, DDS reads and an independent host (MAC330/334)

This implements the two bounded optimizations selected by the
[whole-port audit](optimization-and-reuse-audit.md), together with an independently
installable Cocoa host. Rendering resolution, effects, texture formats, shaders,
simulation cadence, audio quality and required synchronization are unchanged.

## Changes and boundaries

- A context owns immutable Metal depth/stencil states and reuses exact effective
  descriptor values for draws and partial clears. Stencil reference remains a
  dynamic encoder setting. See [state ownership and evidence](depth-state-cache.md).
- Native DDS 2D loading retains one validated original read through the exact
  mip/row upload. It preserves the existing filesystem/security policy and L8
  compatibility rule. It reduces repeated I/O; retaining the file during upload
  can increase temporary memory overlap. See [DDS details](dds-single-read.md).
- [NativeHost](../../code/mercury/library/gamesys/framework/native_host/README.md)
  exposes C++17 window/events/backing-pixel/CAMetalLayer contracts. The engine's
  `Framework::Mac` names alias that single implementation. The package has its
  own CMake install/export and a directory-scoped MIT license, which does not
  cover inherited engine code, assets or the compatibility wrappers. Staged
  game bundles carry the additional notice alongside the three existing ones.

The external client is a separate CMake project using `find_package(NativeHost
CONFIG REQUIRED)`. Root qualification copies the SDK and client outside their
source locations, installs and relocates the prefix, removes the copied SDK
source/build and initial prefix, disables package registries and ambient include
paths, then builds the client against only the relocated export and system SDK.
Its conformance program exercises actual minimized windows; its authored Metal
demo renders a finite scene, handles an asynchronously created stale-size target
across resizes, advances fixed ticks with interpolation and checks GPU pixels.
Neither program has an audio subsystem or uses game assets.

This establishes an independent window-host building block used by Blur and by
an authored client. Renderer adapters, asset readers, game logic, mappings,
audio and networking still need engine-specific integration. A second complete
engine adaptation and arbitrary Windows executable conversion are unverified.

## Verification method

All native device/window/game runs hold the exclusive native service lease.
Game tests set `BLUR_MAC_AUDIO_MUTE=1` before SDL playback and preserve the system
volume and saved settings. Debug and optimized builds must pass the complete
suite, including original archive authentication, resource failure/ownership,
shader/effect graph, luminance sampling, resize and input tests.

The actual-game comparison uses immutable build 320 as baseline MAC328 and the
combined candidate as MAC329. Each receives a fresh copy of the same accepted
profile/save, enters the original Master frontend and 20-car race, uses ordinary
W/S/A/D input, and leaves through original race/desktop confirmations. Both use
the original archive loader and complete renderer/effect graph. No profiler,
capture, GPU readback or per-frame trace runs in the quiet measurement windows.
The source save remains unchanged. Cache counters are disabled for timing and
enabled only in a separate diagnostic run.

Five-second reports measure callback/submission intervals, not displayed FPS or
GPU execution time. Post-exit analysis excludes two complete reports at either
edge of each vehicle-telemetry bookend. The matched 90-second no-input and
45-second W windows remain a single sequential pair with uncontrolled system
and race variation; they cannot establish a repeatable percentage speedup or
eliminate the previously observed occasional hitches. The independent state
creation/read-count controls establish the specific work removed.

## Coherent build and resource controls

Both complete builds succeeded. Final Debug passed 151/151 (170.24s) and Speed
passed 151/151 (97.04s). The two new lower-renderer fixtures also passed in
Debug UBSan and Speed UBSan, two tests each; this is not full-game sanitizer
coverage. Seven focused tests passed with `MTL_DEBUG_LAYER=1`, including the
new loaders/state cache, depth attachments, luminance sampling, Cocoa and both
application/layer resize paths. The final optional benchmark fixture was rebuilt
in all four configurations after correcting its selected warm state; production
code was unchanged by that fixture repair.

The first lookup-only cache revision's actual GPU state oracle observes 360 draws + 63 partial clears with 24 Metal
factory calls and 399 cache hits. Repeating the sequence three times observes
1080 draws + 189 clears, still 24 factory calls, now 1245 hits. A private control
removes only the cache-hit return, retains the same descriptors and lower
renderer closure, and passes the GPU pixel oracles before failing the expected
creation-count assertion. With the benchmark enabled, it makes 11270 factory
calls versus 24 in the cached variant. These are API calls, not proof of Metal's
internal allocation count or a reduction in resident memory.

For that first revision, three alternating process pairs report these five-trial median CPU encoding
times per 2000 draws. They exclude submission completion, presentation and GPU
execution. Both variants use identical opt-in counters. The uncached control still builds
the key and attempts map insertion; it is a creation-count control, not the
pre-change implementation or a causal full-game timing baseline:

| Pair | Cached ms | Uncached ms |
| --- | ---: | ---: |
| 1 | 1.35142 | 1.78638 |
| 2 | 2.06242 | 1.80958 |
| 3 | 3.10904 | 1.93562 |

Two pairs have slower cached timings. These very short trials do not establish
a reliable CPU speedup; the repeatable result is fewer factory calls while
preserving tested pixels. All values are retained rather than selecting the
fastest pair.

The DDS fixture's first BGRA case observes one open/read, 200 transferred bytes
for a 200-byte file, peak one live file owner, and exact GPU mip bytes. The
private pristine-source control observes two opens, six backend reads and 272
bytes, then exits 1 at the expected one-owner/read assertion. The fixed fixture
passes all format/failure/retry cases, including the original L8 compatibility
exception, without an asset-validation bypass.

The relocated installed host client passes both conformance and the finite
Metal scene. The packaged copies pass again without audio. The app staging test
passes strict signatures, four exact native notices, dependency hashes, derived
macOS 26 minimum, relocation and rejection of missing provenance/overwrites.
Another Mac and notarized distribution remain unverified.

## Regression investigation and final cache path

The first candidate completed the original driving/menu-exit route, but its
quiet rates were worse than the initial baseline. It was held from delivery.
The original build was repeated, then a private full-game ablation replaced
only the Context object with the exact pre-cache source. All other candidate
libraries, the DDS path, host implementation and shader resources stayed the
same. Public/private headers and the other 20 lower-archive objects were
unchanged. Both additional runs completed the normal exit route.

| Actual run | Cache/implementation | No-input submissions/s | Driving submissions/s | Maximum interval, no-input/driving ms |
| --- | --- | ---: | ---: | ---: |
| MAC328 | Original build 320 | 57.992 | 68.746 | 89.095 / 28.409 |
| MAC329 | First lookup-only cache, DDS and host | 41.827 | 47.447 | 119.244 / 82.620 |
| MAC332 | Original build 320 repeat | 62.439 | 49.331 | 80.050 / 115.387 |
| MAC333 | Candidate DDS/host with exact pre-cache Context | 45.062 | 56.365 | 125.800 / 66.744 |
| MAC334 | Final last-entry Bind cache, DDS and host | 43.807 | 57.622 | 207.522 / 78.535 |
| MAC336 | Final cache/host with exact pre-change DDS objects | 48.560 | 35.883 | 1018.776 / 152.932 |

The old build's driving rate itself varied substantially. Removing the cache
improved some candidate timings but did not explain the whole difference.
These nondeterministic sequential races cannot isolate all scene/system effects
or establish that one change caused the entire gap. All recorded world views
remain 2560×1440. Bundle metadata differs only in version/build strings.

The final cache addresses a concrete added hot-path cost: it compares the full
key against the last owning-map entry before hashing, and binds through a void
method instead of returning an Objective-C object. Every draw/clear still binds
to its current encoder; references remain dynamic. The map strongly owns native
states, never erases entries, and preserves node references across rehash. Cold
creation retains all checks and failure propagation. The compiler still outlines
Bind, so no machine-inlining claim is made.

The expanded actual-GPU oracle exercises 680 draws + 63 partial clears with 64
factory calls and 679 hits (388 last-entry,291 map). Repeated three times it
observes 2040 draws + 189 clears, still 64 calls and 2165 hits (1164 last-entry,
1001 map). Forty additional mask states exercise returns to earlier entries
across map growth. The no-reuse control passes the pixel oracle, then fails
its expected count assertion: 743 factory calls in the single sequence and 2229
in the repeated one. Both final full suites, both lower UBSan pairs and seven
Metal-validation tests pass after this repair.

MAC331 was a counter-enabled first-revision diagnostic, not a qualified timing
comparison. It observed 12,912,388 draw requests, 28 factory calls, 12,912,360 hits
and 28 entries with zero failures. A passive vehicle-bookend query timed out;
that rejection is preserved. Original race/menu helpers subsequently exited
normally (process 0,237.733s), without changing the source save. A three-second
OS sample after the rejected measurement overlapped cleanup, so it is not used
to attribute race performance. Final-path counters use a separate parked-HUD
diagnostic with no vehicle timing claims.

The final MAC334 actual game completed the original frontend, 20-car race,
90-second no-input and 45-second W driving windows, then original race/menu and
desktop exit (process 0, 306.023s). Human-car bookends confirmed 574.196m of
movement in the driving window. Both windows qualified; source profile/save
remained unchanged. The slower no-input rate and 207.522ms maximum interval are
retained in the table. This is not proof that hitches have been eliminated.

MAC335 intentionally measured counters, not frame performance: 30 seconds
parked at the original 20-car Racing HUD, no measurement IPC/keyboard/profiler or
readback, then normal race/menu/desktop exit (0, 181.900s). Its real context
observed 7,597,565 draw requests, 28 factory calls/entries, 7,597,537 hits
(7,184,265 last-entry and 413,272 map), zero failures. The early empty context
had zero requests. This run deliberately has no vehicle-bookend/timing
qualification; it is not a rejected timing run. Counters remain off by default.

A final private DDS-only full-game ablation retains the final cache and host,
replacing exactly the two DDS objects with their pinned pre-change versions.
The other lower archive members, shared link inputs and all 693 resources are
hash-matched. MAC336 completed both matched windows, confirmed 722.511m of
human-car driving displacement and exited through the original menus/desktop
(0, 312.278s), with source state unchanged and sound muted. Removing DDS improved
the no-input average but worsened the driving average, and retained a 1018.776ms
maximum no-input interval. These opposing results do not isolate a repeatable
DDS performance effect or prove absence of a regression. They also do not
establish a host-package effect; normalized host implementation bodies match.

The acceptance is bounded: exact rendering/resource behavior, dramatically fewer
state factory calls, one original DDS read, and an independently usable host
package are demonstrated. Overall FPS, tail latency, startup time and peak RSS
are not established improvements. Residual hitches require a separate controlled
investigation rather than changing quality or synchronization to hide them.

## Local artifacts and source delivery

The final development app is version 0.1.5/build 334. Its executable SHA-256 is
`5a311dc7d787f37a56560718771ad9410467ac8c1a99cded7723ae18e61bb7e5`.
The independent NativeHost 0.1.0 arm64 SDK/demo archive SHA-256 is
`33f30f5fee5633da4db150099c7e07163ef472b961e49d227b5cc7e734bde675`.
These local artifacts contain no original game data or profiles. The game uses
the existing original-data selection flow; SDK clients need neither.

Final ZIP extraction/relocation, strict signatures, four native notices and
delivery hashes passed. The standard app and manual launcher now point to
0.1.5/build334; no game is auto-launched. The immutable 0.1.4 app is preserved.
The game ZIP SHA-256 is
`73e600b778733c1a79f16aa6081b80352fe820d22a838e758b38d02a9517009b`.
Publication stages only the frozen source/fixtures and these documents through
the personal repository's curated index, preserving profile redaction and the
default worktree index. SDK scope/license remain limited to its authored folder.
