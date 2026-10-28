# Apple silicon CPU, unified-memory and profiling audit

MAC339, 2026-10-28. This is a read-only source and primary-documentation audit for
the M4 Pro development machine (12 CPU cores: 8 performance and 4 efficiency;
16 GPU cores; 24 GiB unified memory). Root owns the separate hardware/TBDR
acceptance report. No code, thread policy, worker count or resource-storage mode
was changed; this audit ran no build, profiler, GPU workload or game.

## Scheduling contract and bounded candidate

Apple recommends a worker pool, sufficiently coarse jobs, fewer synchronization
transitions, and explicit waits instead of busy/yield loops. QoS expresses work
importance rather than core affinity. GCD is an existing system-managed job pool;
it is an option for suitable independent work, not evidence that replacing this
engine scheduler improves it. Apple also warns that available performance cores
vary with system conditions. See [Tune CPU job scheduling for Apple silicon
games](https://developer.apple.com/videos/play/tech-talks/110147/).

The current native implementation has these source constraints:

| Source | Observed contract | Consequence for an experiment |
| --- | --- | --- |
| [jobqueue_xenon.cpp](../../code/mercury/library/common/system/source/scheduler/jobqueue_xenon.cpp), `_Init`, `_Start` | At most seven workers, eight alert slots including main. Worker `Processor` selects a logical policy slot; it does not pin an OS thread to a CPU. | Increasing the cap to eleven is not a safe isolated hardware tuning change. Policies, alerts and worker-local scratch consumers require a separate ownership audit. |
| Same file, `jqLockBatchPoolInternal` / `jqUnlockBatchPoolInternal` | One CAS lock, with `MT::YieldThread` on acquisition contention and fences around the protected interval. | This is a concrete place to measure contention, not a demonstrated frame bottleneck. |
| [jobqueue_kernel.cpp](../../code/mercury/library/common/system/source/scheduler/jobqueue_kernel.cpp), `CompleteNativeBatch` | Grouped completion holds that lock across executing-count decrement, possible event signal and global completion publication. | Preserve the entire existing completion-retirement span. Removing the signal from it reintroduces the retired-event race. |
| [jobqueue.cpp](../../code/mercury/library/common/system/source/scheduler/jobqueue.cpp), `DestroyGroup`, `SetGroupPoolSize` | Flush occurs before locking; event destruction/slot recycling and pool replacement use the same lock. | No event wait or job execution may be introduced while holding this lock. Creators, completion, destruction and replacement must continue to agree on its identity. |
| [jobqueue_xenon.cpp](../../code/mercury/library/common/system/source/scheduler/jobqueue_xenon.cpp), `Flush` | Main can execute pending work; event-enabled groups wait on an event, other waits yield. | A global switch to blocking waits is not equivalent without a lost-wakeup, timeout and main-assistance design. |
| [SchedulerConfig.cpp](../../code/amax/game/SchedulerConfig.cpp) | Native selects eight-entry source policies; audio is restricted to logical slots 4/5. | These source policy values are not physical P/E-core assignments. |

The lowest-scope candidate is a **native-only comparison of the batch-pool lock
implementation**, keeping all callers and their existing lock spans unchanged.
A real pthread mutex is one possible comparison after measurement. It is not a
change recommended for shipment from source inspection alone.

Before implementing that comparison, collect bounded per-thread counts and wait
durations for contended acquisition, job duration by module/group, and main
`Flush` duration. Avoid allocating/logging inside locks or adding new shared
hot-counter contention. Compare matched scenes and distributions, including long
tails, rather than only average FPS. Then exercise group completion/destruction,
pool replacement, main assistance, repeated scheduler lifetimes and shutdown.
Preserve fixed simulation cadence, audio quality and callback ordering.

[NativeThreads.cpp](../../code/mercury/library/common/core/source/threads/macos/NativeThreads.cpp)
adds two relevant limitations. Nonzero MT thread priorities map coarsely to
`USER_INITIATED` or `UTILITY`; zero does not explicitly set QoS. Its `RecursiveState`
tracks a logical owner/depth using a mutex plus condition variable, releasing the
internal mutex while the caller owns the logical lock. Therefore substituting
the current `MT::Lock` for the CAS loop would not itself provide a kernel-owned
mutex spanning the whole protected engine interval. Separately, native
[audio Thread.cpp](../../code/mercury/library/audio/source/common/Thread.cpp)
uses `std::thread` and only supports normal `SetPriority`. These are mapping
facts, not evidence for raising priorities or using real-time scheduling.

The packed `g_iCPUWorkerAlert` array is intentionally updated as two 64-bit
atomic lanes in `AddTask`; padding individual elements to separate cache lines
would change that algorithm. Do not apply generic false-sharing padding there.

## Allocation and unified-memory observations

[AllocatorFunctions.inl](../../code/mercury/library/common/system/include/system/memory/AllocatorFunctions.inl)
contains repeated domain/size classification along some free paths:
`BizNonAlignedFree` queries ownership, optional tracking queries size, and
`BizInternalFree` queries size and ownership again. Some work may disappear in
optimized code. Count actual surviving classifications or inspect optimized
assembly before proposing a single-classification change. Preserve tracking,
guards, alignment and genuine libc versus SBA ownership, including late
framework allocations. The native process-lifetime SBA domain is a correctness
requirement, not an allocator leak to remove for this optimization task.

Apple distinguishes CPU-accessible shared storage, GPU-only private storage,
and temporary tile-local memoryless storage even on unified-memory hardware.
See [Choosing a resource storage mode for Apple
GPUs](https://developer.apple.com/documentation/metal/choosing-a-resource-storage-mode-for-apple-gpus).

Current [MetalDevice.mm](../../code/mercury/library/graphics/display2/source/device/metal/MetalDevice.mm)
creates vertex/index buffers in shared storage. [MetalSurfaces.mm](../../code/mercury/library/graphics/display2/source/device/metal/MetalSurfaces.mm)
and [MetalTextures.mm](../../code/mercury/library/graphics/display2/source/device/metal/MetalTextures.mm)
use private textures and shared CPU staging. `LockRect` waits for prior ownership
and downloads unless an eligible CPU-authored mirror is valid; `UnlockRect`
uploads modified staging. A retained mirror does not remove the prior GPU-read
barrier. Readbacks, uploaded bytes and transfer waits are useful measurements;
switching all textures to shared or deleting waits is not justified by the
physical memory being unified. Memoryless storage is not a replacement for
resources subsequently sampled, copied or CPU-locked.

Use the device's `recommendedMaxWorkingSetSize` as a reported performance budget,
not all 24 GiB as a GPU allocation target. `currentAllocatedSize` reports Metal
allocations; process footprint and Metal allocation are not established as
disjoint sets that can simply be added on this machine. The installed SDK
documents `hasUnifiedMemory`, both size properties and their availability in
`Metal.framework/Headers/MTLDevice.h`.

## Profiling available without installing Xcode

Read-only tool discovery found `/usr/bin/sample`, `powermetrics`, `vmmap`,
`leaks` and `memory_pressure`. The selected developer directory is Command Line
Tools; `xcrun --find xctrace` returned unavailable. None of these tools was run
against the game by this audit. Sampling a deliberately parked owned process
can identify stacks; it is not a quiet-window performance comparison.

The installed Metal headers already expose command-buffer `GPUStartTime` and
`GPUEndTime`, counter sets, capability queries, sample buffers and CPU/GPU
timestamp sampling. GPU timestamps are valid after completion; collect without
adding synchronous waits. Counter modes must be queried, not assumed from the
M4 name. Apple describes stage-boundary sampling and completion-time resolution
in [Explore Live GPU Profiling with Metal
Counters](https://developer.apple.com/videos/play/tech-talks/10001/). Existing
`steady_ns` traces need an explicit clock correlation before combining absolute
GPU/host timestamps; do not assume matching C++ and Metal epochs.

Built-in HUD logging is the first available measurement path and avoids new GPU
instrumentation. Apple documents its once-per-second record format and notes
that encoder timing adds CPU processing cost in [Monitoring your Metal app's
graphics performance](https://developer.apple.com/documentation/xcode/monitoring-your-metal-apps-graphics-performance/).
Command-buffer GPU duration can include idle gaps; encoder timing measures
stage activity. The definitions of on-glass interval, GPU time and memory are in
[Understanding the Metal Performance HUD
metrics](https://developer.apple.com/documentation/xcode/understanding-metal-performance-hud-metrics).

## MAC337 log interpretation limit

A bounded read of the first five complete actual HUD records found 97, 87, 96,
97 and 96 interval/GPU pairs, including 49, 43, 48, 48 and 47 adjacent equal
pairs. First counters began 50, 93, 142, 190 and 238. Assigning `first + pairIndex`
therefore produces overlapping/conflicting apparent frame IDs. Apple’s public
format description does not explain the duplication or establish a safe
deduplication rule. Equal rounded samples alone cannot identify duplicate frames.

Retain raw records. Match stdout and unified-log records in order using their
first counter and two memory fields to obtain report timestamps, trim report
windows away from phase edges, and report sample counts/duplicate fractions
alongside explicitly sample-weighted GPU statistics. Do not infer unique-frame
FPS or precise per-frame IDs from this representation. HUD-on encoder diagnostics
also cannot be treated as an uninstrumented FPS comparison. The parent owns the
actual run, final window selection and hardware report.

## Initial audit acceptance boundary

This audit establishes source contracts and measurement options. It establishes
no CPU bottleneck, speedup, peak-memory reduction or scheduling repair. The next
step is attributable CPU/job and GPU timing, followed by one isolated comparison
only if its measured cost is material. No pinning, priority, DSP, quality or
worker-count change is proposed for shipment.

## MAC341 owned parked-HUD sample

Root subsequently captured a three-second `/usr/bin/sample` diagnostic during
a 30.073-second original 20-car parked-HUD interval. This used staged build 340,
executable SHA-256
`bcec243fcb889ed7aa7f56145a66545a0c9b0c21ef71d0af31dd796d1bcca4ba`.
The diagnostic proof explicitly sets `measurementQualified=false`: no keyboard
holds, readback or further measurement IPC occurred inside the parked interval.
The game then left through ordinary flow with exit 0 after 180.588 seconds.
This audit only read the owned sample and its corresponding proofs; it did not
capture another sample or run the game. An older generic harness field claiming
no profiler was requested is stale; the explicit diagnostic/sample proof is the
authority for this run, with the parent preserving and correcting that mismatch.

The call graph contains 1,592 main-thread observations:

| Observed path | Inclusive observations | Interpretation |
| --- | ---: | --- |
| `C_GameApp::OnRender` | 943 | Draw encoding, texture/sampler binding and HDR/bloom paths are represented. |
| `C_GameApp::OnUpdate` | 620 | Includes world updates, main-thread job assistance and waits. |
| `WorldRender::RecordImpl` on main | 285 | Many descendants are mutex/condition-variable waits. |
| `WorldRender::RecordImpl` on six JQ workers | 400–422 per worker | The same synchronization path is repeatedly observed across workers; these counts include blocked residency. |
| Main native sampler binding | 143 | Temporary destruction and allocator-domain queries are represented below this path. |
| Main `malloc_size`, all paths | 277 | Includes Caulk allocation-zone queries; this is not an audio-thread attribution. |
| Main `Scheduler::Flush` | 282 | Includes 154 observations at `swtch_pri` in the ordinary Flush yield path. |
| `CompleteNativeBatch`, all threads | 2 | Only one observation includes `jqLockBatchPoolInternal`. |

Loading/resource/layer threads mainly wait. The audio writer and renderer mainly
sleep or wait; the sample does not establish producer starvation or resolve a
perceptual audio symptom. Main/render paths and worker wait stacks identify
interpolation recording and sampler/allocation work as stronger follow-up
subjects than the batch-pool CAS lock in this particular interval.

These are inclusive stack-residency counts, not CPU milliseconds, operation
counts, lock-hold durations or disjoint percentages. Blocked threads are sampled;
inlined optimized frames can also limit attribution. Do not sum related parent
and child rows, infer CPU saturation or GPU utilization, or compare this parked
sample as an FPS benchmark. It is one short scene-specific diagnostic, not a
complete frame or hitch census.

**Decision after this evidence:** no CAS-lock rewrite is justified or performed.
The interpolation lock and its ownership semantics remain intact. This motivated
a separately owned, source-confirmed sampler-key temporary-allocation experiment
with key-copy correctness coverage. Its later bounded acceptance is recorded
below; implementation and complete measurement evidence belong to their own
task. No speedup is established by MAC341.

## MAC348 counters and MAC350 control: final bounded context

The sampler-key implementation now reuses context-owned lookup storage while
the cache retains independent copied keys. Root reports both final 153-test
suites, four UBSan checks and twelve Metal checks passed. These are correctness
gates, not a performance comparison; this documentation audit did not execute
them. The full acceptance record is maintained separately in
[Apple silicon optimization](apple-silicon-optimization.md).

Actual MAC348 lifetime counters recorded 24,134,430 sampler requests,
24,134,409 cache hits, 21 states created and one lookup-capacity growth, retaining
47 bytes of lookup-string capacity. The diagnostic exited normally, with audio
muted. These cover the context lifetime across startup, frontend, race and
shutdown, not only the parked interval. They establish this particular lookup
reuse behavior. They are not total allocator counts, saved-byte measurements,
FPS evidence or proof that other temporary allocation was eliminated.

Root also completed a metadata-only MAC350 control: the compiled unsigned
executable remained identical to MAC347, SHA-256
`c4dece9e3ff09edf5613bb7ce62d7008bc9fcb5d5d10be081bca9da7d60a5304`.
Only `LSSupportsGameMode` and `LSApplicationCategoryType` were removed from the
information property list, followed by signing. All other property-list fields
and bundle files were unchanged. The re-signed executable has a different hash;
the unsigned comparison is the compiled-code identity proof. MAC350 completed
qualified muted windows and ordinary exit 0.

| Actual run | No-input interval / GPU ms | Finite-W interval / GPU ms |
| --- | ---: | ---: |
| Old-build MAC349 | 14.365 / 6.410 | 15.110 / 6.555 |
| Candidate MAC347 | 35.995 / 10.690 | 42.330 / 18.195 |
| Same-code metadata-only MAC350 | 13.360 / 6.650 | 14.692 / 7.070 |

Each value is a median of per-emission HUD report medians, retaining the driver
sampling/duplication limitations above. These are not unique-frame quantiles or
derived FPS. The adverse MAC347 result remains unexplained. The later control
does not establish that Game Mode metadata caused the adverse result, nor does
it isolate a causal CPU benefit from sampler reuse. No repeatable FPS,
tail-latency, power or peak-memory gain is established.

Root-selected delivery defaults preserve absent Game Mode metadata, retain the
CPU sampler-key reuse and keep experimental identical-attachment tile reuse
off. Neither the CAS retirement lock nor interpolation lock was changed by
these tasks. Any future scheduling or interpolation repair still requires its
own attributable measurement and ownership proof.
