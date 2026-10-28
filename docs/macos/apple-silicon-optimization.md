# Apple M4 Pro optimization: research and measured scope

MAC337, 2026-10-28. Baseline is native 0.1.5/build334, executable SHA-256
`5a311dc7d787f37a56560718771ad9410467ac8c1a99cded7723ae18e61bb7e5`.
This work preserves the original game, archive authentication, native rendering
resolution, shaders/effects, texture formats, simulation and audio behavior.
Automated tests mute playback before the device starts; system volume and saved
settings are not changed.

## Hardware actually observed

Local sysctl and System Information identify an Apple M4 Pro, 12 CPU cores
(8 performance + 4 efficiency), 16 GPU cores, 24 GiB unified memory and a
3024×1964 built-in display configured at 120 Hz. Metal runtime queries confirm
unified memory, Apple9 and Mac2 families, native BC texture compression,
stage-boundary timestamp counter sampling and hardware ray tracing. Apple10 is
not supported. The installed operating system supporting Metal4 does not make
this an Apple10 GPU. Query capabilities; do not select features from an OS or
chip-name string.

The idle device's recommended working-set size was 19,069,665,280 bytes, and
maxBufferLength was 14,302,248,960 bytes. These are queried limits/budgets, not
available bytes for unrestricted allocation, performance measurements or a
promise that all 24 GiB can be used for GPU resources. The counter set exposed
here is `timestamp`; the total permitted threadgroup size is not the product of
its per-axis maxima.

Apple lists 273 GB/s memory bandwidth for this M4 Pro configuration. This is a
published hardware specification, not measured game bandwidth.
[Apple configuration specifications](https://www.apple.com/shop/product/g1fb6ll/a/Refurbished-14-inch-MacBook-Pro-Apple-M4-Pro-Chip-with-12%E2%80%91Core-CPU-and-16%E2%80%91Core-GPU-Silver).

## Applying the MLX analogy

MLX benefits from an array/execution model built around Apple's shared CPU/GPU
memory. Its own documentation describes operating on shared arrays without
explicit device copies. The analogous work here is choosing appropriate Metal
resource ownership, avoiding unnecessary copies and pass boundaries, and
feeding the GPU efficiently. [MLX unified memory](https://ml-explore.github.io/mlx/build/html/usage/unified_memory.html).

| Hardware property | Implication for this renderer | Scope |
| --- | --- | --- |
| Tile-based deferred rendering | Avoid needless pass endings that store attachments to system memory and reload them for the next pass | Preserve a live encoder across identical attachment bindings; validate exact pixels and all required boundaries |
| Unified CPU/GPU memory | CPU-produced data can use Shared storage; GPU render/intermediate textures can still benefit from Private storage | Current VB/IB sharing and Private textures remain; no blanket conversion of render targets to Shared |
| Heterogeneous 8P/4E CPU | OS scheduling, dependency structure and synchronization matter; logical engine worker IDs are not hardware-core IDs | Measure actual worker/lock costs; no core pinning, priority guesses or blind worker-count increase |
| Persistent native state/compiled pipelines | Reuse immutable state and avoid compilation stalls | Existing depth-state/PSO caches retained; persistent shader archives need identity/invalidation and separate evidence |
| Metal feature families | Availability depends on hardware and API version | Gate specific features; no automatic Metal4 migration or Apple10-only sampler assumptions |
| Ray tracing, Neural Engine, media engines | Accelerators serve particular workloads | The existing raster game does not become faster merely by enabling an unrelated accelerator |

Apple's GPUs render tiles using on-chip memory. Keeping compatible work within
a pass can avoid external attachment traffic; actual cross-pass dependencies
still require boundaries. The experimental render-pass implementation targets repeated
bindings of the identical shared native Surface, while preserving colour-zero
viewport/scissor resets and all changed-target, clear, resolve, snapshot and
query boundaries. It does not guess transient lifetimes or change store actions.
[Apple GPU architecture](https://developer.apple.com/documentation/metal/tailor-your-apps-for-apple-gpus-and-tile-based-deferred-rendering),
[load/store guidance](https://developer.apple.com/library/archive/documentation/3DDrawing/Conceptual/MTLBestPracticesGuide/LoadandStoreActions.html).

Unified memory does not remove resource storage-mode or synchronization rules.
Shared is appropriate for CPU/GPU data exchange; Private is appropriate for
GPU-owned resources, while Memoryless requires proven temporary attachment
lifetimes. Copying an original resource into another staging buffer can still
cost bandwidth even without a discrete GPU bus.
[Apple storage guidance](https://developer.apple.com/documentation/metal/choosing-a-resource-storage-mode-for-apple-gpus).

For precise CPU/source constraints, see [the CPU audit](apple-silicon-cpu-audit.md).
The existing seven-worker cap participates in an eight-slot legacy contract;
changing it to match the twelve hardware cores is not a safe isolated setting.

## Measurement method and baseline

Only Command Line Tools are installed; `xctrace` is unavailable. The built-in
Metal Performance HUD works without installing full Xcode. MAC337 enabled
`MTL_HUD_ENABLED=1`, `MTL_HUD_LOG_ENABLED=1`, shader logging and encoder timing
for the immutable baseline. Log capture filters only the owned process and
Metal HUD subsystem. Full HUD samples appear in its redirected stderr; unified
logging redacts some payloads. No global private-logging setting is changed.
[Apple HUD setup](https://developer.apple.com/documentation/xcode/monitoring-your-metal-apps-graphics-performance).

The actual original frontend launches a 20-car race from an isolated copy of
the accepted profile/save. Matched 90-second no-input and 45-second W-input
windows use normal controls; source state remains unchanged. GPU profiling
runs have no screenshots, GPU image readbacks, per-draw timers or injected
measurement IPC inside those windows. Normal original race/menu/desktop exit
qualifies each run. MAC337 exited 0 after 308.026 seconds.

The installed HUD emits repeated adjacent equal pairs and counters that cannot
safely reconstruct unique frame IDs. Equal rounded values are not deduplicated.
The parser selects emissions at least five seconds inside each qualified
window. It summarizes the median of each emission's sample medians, then the
median across emissions; reported maxima may include repeated samples. These
are not unique-frame percentiles or derived FPS. Encoder timing adds CPU cost;
compare only equally configured profiling runs, not HUD-off historical rates.
HUD GPU time is not proof of shader occupancy or a precise decomposition of
all CPU, GPU and display waits.
[Apple HUD metric definitions](https://developer.apple.com/documentation/xcode/understanding-metal-performance-hud-metrics).

| Baseline MAC337 window | HUD emissions / driver pairs | Adjacent equal pairs | Interval median of report medians | GPU median of report medians | Max reported interval / GPU ms | HUD graphics / process MB range |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| No input | 159 / 10,672 | 5,258 | 14.320 ms | 6.150 ms | 39.680 / 31.110 | 572.73–588.05 / 1163.99–1228.97 |
| W driving | 70 / 5,001 | 2,466 | 13.900 ms | 6.490 ms | 34.120 / 13.680 | 573.86–581.39 / 1161.33–1208.38 |

Memory columns are the HUD's rounded estimates, not an allocation census or
process peak RSS. Do not add them as disjoint memory on a unified-memory system.
GPU timing being shorter than typical presentation intervals suggests CPU
submission/synchronization or presentation also deserves investigation; it does
not isolate one bottleneck. Single sequential races have varying scene/system
conditions and cannot establish a repeatable percentage improvement.

## Experimental coalescing result and revised optimization choice

The first candidate340 passed both complete152-test suites (Debug84.06s,
Speed100.81s), both lower attachment-fixture UBSan runs and11 focused tests with
Metal validation. A private old-setter body passes the exact pixels but creates
81/159 passes without/with repeated bindings, versus the new72/72. All693
non-notice resources are byte-identical. An initial Debug suite had three
unreproduced timeouts (depth texture and two unchanged audio fixtures); each
passed three isolated repeats without increasing its timeout. The passing full
rerun used bounded idle-sleep inhibition. The first failed log is retained.

Actual340 completed the qualified original20-car driving/normal exit route
(0,306.152s), sound muted and source save unchanged, with the same HUD options:

| Candidate340 window | HUD emissions / driver pairs | Adjacent equal pairs | Interval median of report medians | GPU median of report medians | Max reported interval / GPU ms | HUD graphics / process MB range |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| No input | 159 /10,577 | 5,195 | 14.550ms | 6.510ms | 48.950 /40.670 | 548.47–562.53 /1121.92–1231.50 |
| W driving | 70 /4,429 | 2,174 | 15.648ms | 7.532ms | 35.280 /26.960 | 549.59–555.52 /1145.28–1223.86 |

These timings are worse than the first baseline. They do not establish causal
regression: AI impacts moved the unpowered baseline car only about5m between
its bookends but moved the candidate car about304m, changing camera/scene work.
Fewer passes can save tile traffic while also changing vertex/fragment overlap;
Apple explains both considerations. No geometry-spill or tile-memory-exhaustion
cause was measured. [Apple GPU scheduling](https://developer.apple.com/videos/play/wwdc2020/10632/).

Therefore coalescing is held from default delivery. It remains an explicit
`BLUR_MAC_TILE_PASS_REUSE=1` experiment, sampled once at context creation. Default
behavior preserves the old pass boundaries. Its exact-pixel/count results are
retained as correctness evidence, not a shipping speedup. A fresh baseline344
completes the bounded profiling check. Run343 was cancelled at the user's stop
and never qualifies; queued345 was cancelled before game launch when superseded
by the CPU allocation investigation.

## Actual parked-race CPU diagnosis

MAC341 deliberately parked at the original20-car HUD for30.073s, sampled only
the owned process for3s, then exited normally (0,180.588s); muted, no key holds,
readback or further measurement IPC, source save unchanged. It is a counter/CPU
diagnostic, not a performance comparison. Lifetime pass counters recorded
767,493 passes,3,752,570 colour setters and976,091 depth setters. There were
38,584 colour and9,646 depth identical setters while an encoder was live. Those
are setter counts, not a one-to-one count of passes or memory transactions saved.

The main thread has1,592 stack observations: OnRender943, OnUpdate620,
WorldRender interpolation RecordImpl285, sampler143. malloc_size appears277
across main-thread paths, including sampler temporary destruction and native
allocation-zone queries. Six workers each have400–422 observations inside
RecordImpl, often including mutex/condition waits. These are inclusive stack
residency counts, not CPU milliseconds, disjoint percentages or lock-hold times.
The batch-pool CAS lock appears once; this sample does not justify rewriting it.

The lowest-risk measured-source follow-up is the sampler's fresh32-byte string
key on every binding, including cache hits. Reusing context-owned scratch and
copying only cold map keys preserves the exact eight-word key, map ownership,
validation and sampler settings while removing warm key capacity growth. The
final implementation/qualification is recorded below; it changes no allocator,
shader, input, interpolation lock or scheduling policy.

The first metadata candidate declared Apple's documented Game Mode support.
Final packaging makes this an explicit default-OFF opt-in and preserves prior
policy; minimized profiles do not verify activation or benefit. [Scope and opt-in](game-mode-opt-in.md).



## Final default optimization and coherent checks (MAC346/347)

The final default path reuses the sampler's lookup string capacity while
copying successful cold keys into the owning map. All eight original32-bit
words and their order remain; filters, addressing, border, anisotropy and
shader-based LOD behavior are unchanged. Context/scene ownership serializes
access. No allocator, interpolation lock, worker count, priority or memory
storage-mode change is made. Native sampler states already had a cache; this
removes repeated temporary key growth on its hits.

The real GPU oracle binds samplers in both shader stages to2D and3D textures,
checks addressing/filter/border/LOD/clamp pixels, alternates96 keys and returns
to prior states across map growth, then rejects invalid state and retries. One
sequence has716 requests and112 native states; three repeated sequences have
2,138 requests and the same112 states. Both have only one lookup-capacity-growth
call. A private old-local-key control passes those pixel/native-state checks
but reports716/2,138 growth calls, then fails the intended plateau assertion.
This counts lookups whose capacity grew, not all allocator calls, native driver
allocations or bytes; the control is not an old-game timing baseline.

The attachment fixture now verifies default old boundaries (81/159 passes
without/with repeats) and explicit experimental coalescing (72/72), with equal
known pixels/query results. Tests also cover diagnostics disabled and new
contexts. The experimental tile flag remains off in the final actual profile.

Both complete final suites pass153/153: Debug162.46s, Speed109.62s. Each of the
two lower fixtures passes in both UBSan builds (four checks);12 focused Metal
validation tests pass. The old-local-key negative control exits1 at its
expected growth assertion after passing GPU pixels/state reuse. The C++ qualification froze42 source/fixture paths; final packaging refinement
adds stage_bundle.cmake and updates only the two metadata helpers, producing43
frozen paths with36 unchanged from earlier accepted work.
All693 non-notice resources remain byte-identical to0.1.5. Game Mode metadata
passes source-app staging, signature, relocation and refusal/immutability
checks; NativeHost SDK/demo remains unchanged.

## Final actual game: counter result and adverse timing

The final default CPU-key build347 completed the original20-car race, both
qualified quiet windows, actual W driving and original race/menu/desktop exit
(0,343.398s). Source state remained unchanged, audio muted and the tile flag off.
The HUD configuration matches the earlier runs. These measurements are worse:

| Run/window | Emissions / driver pairs | Adjacent equal pairs | Interval median of report medians | GPU median of report medians | Max reported interval / GPU ms | HUD graphics / process MB range |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| Old344 no input |159 /11,072 |5,452 |14.160ms |6.210ms |38.490 /30.650 |572.73–574.48 /1145.63–1224.49 |
| Old344 driving |70 /4,728 |2,334 |14.420ms |6.940ms |30.430 /22.900 |574.91–581.39 /1161.85–1254.70 |
| Final347 no input |155 /4,243 |2,045 |35.995ms |10.690ms |546.460 /193.040 |558.67–614.92 /1146.49–1212.25 |
| Final347 driving |69 /1,597 |771 |42.330ms |18.195ms |232.790 /159.230 |563.41–638.44 /1167.42–1253.60 |

Delivery was held for a final old-build comparison349 rather than treating
fewer allocations as proof of a faster game. A point-in-time check after347
observed unrelated active compiler processes and a virtual machine; it does not
establish their activity inside347 or attribute its slower numbers. Power was
AC, a65W adapter was reported, battery was7%/charging and both power-mode values
were0. No thermal/performance warning was reported; those observations do not
prove stable clocks, no throttling, exclusive CPU/GPU use or a causal power issue.
Both runs logged the same2560×1440 world views. No system power/QoS setting or
unrelated process was changed.

A separate actual348 diagnostic deliberately parked30.054s at the original
20-car Racing HUD, with no keys, profiler, readback or further measurement IPC.
It exited normally (0,181.085s), source state unchanged and sound muted. Its real
context made24,134,430 sampler requests:24,134,409 cache hits,21 native states/
entries and one lookup-capacity-growth call, retaining47 bytes of string
capacity. The empty early context had zero requests/growth. Default render-pass
stats show zero avoided colour/depth endings, confirming the experimental path
was off. These are lifetime counters across startup/frontend/race/shutdown,
not counts limited to the parked30s, total heap allocations, saved bytes or a
frame-performance comparison. Counters remain off by default.

The implemented work removal is established by exact-source/GPU counter controls
and these real-game lifetime counts. A repeatable FPS, tail-latency, power or
peak-memory gain is not established. The final comparisons and delivery are recorded below.


## Final isolation and delivery decision

The old334 build repeated as349 and completed normally (0,305.865s), with both
quiet windows qualified and source state unchanged. The metadata-only350 control
cloned347, removed only LSSupportsGameMode and LSApplicationCategoryType, and
was re-signed. All other Info fields and bundle files remained unchanged except
signatures; normalized unsigned executables match SHA-256
`c4dece9e3ff09edf5613bb7ce62d7008bc9fcb5d5d10be081bca9da7d60a5304`.
It also completed original driving/menu/desktop exit normally with sound muted.

| Run/window | Interval median of report medians | GPU median of report medians | Maximum reported interval / GPU |
| --- | ---: | ---: | ---: |
| Old349 no input |14.365ms |6.410ms |27.670 /16.980ms |
| Old349 driving |15.110ms |6.555ms |28.470 /13.870ms |
| Metadata-only350 no input |13.360ms |6.650ms |45.060 /28.540ms |
| Metadata-only350 driving |14.692ms |7.070ms |25.580 /13.930ms |

The control returned to approximately the prior timing range. This single
isolation does not establish a Game Mode scheduling cause or an allocation
speedup: AI motion, scene history, HUD overhead and uncontrolled host activity
remain limits. The adverse347 samples stay in this report. No repeatable FPS,
tail-latency, power or memory reduction is claimed. Testing did not change
OS priorities, affinity, power settings or unrelated processes.

The delivered0.1.6/build352 uses the sampler scratch reuse, original pass
boundaries by default, and unchanged/absent Game Mode metadata by default.
`BLUR_BUNDLE_GAME_MODE=ON` explicitly requests Apple's documented metadata for
future manual fullscreen/OS-indicator tests. The optional tile experiment remains
`BLUR_MAC_TILE_PASS_REUSE=1`; neither experiment is required for normal play.
The final package has exactly the same normalized compiled executable as actual
350 and347. All693 non-notice resources match0.1.5; shaders/effects, resolution,
simulation and audio are unchanged.

Both complete153-test C++ suites, four lower-fixture UBSan checks and12 focused
Metal-validation checks apply to that unchanged compiled code. After metadata
refinement, explicit-ON source staging reran pure rejection/immutability and
metadata checks, notice/SPDX closure, strict signature and copied relocation.
Default-OFF staging checks absence of both new keys, resource identity and
normalized executable identity. ZIP extraction/relocation and standard-delivery
checks are recorded in ignored MAC352 proof files. Raw game data, profiles,
artifacts and diagnostic payloads are excluded from source publication.

The next hardware investigation should use Xcode GPU timelines/counters and
a controlled reproducible route if finer attribution is needed. That tooling is
not installed here. Persistent shader archives require a separate identity and
invalidation design; worker-count, allocator and synchronization changes remain
unjustified by the present stack sample. This phase delivers verified CPU work
removal and bounded experiments, preserving the visual/gameplay workload.
