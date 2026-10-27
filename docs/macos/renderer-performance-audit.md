# Native renderer performance audit (MAC270)

The user reports jitter/roughly30fps in the manually launched signed Speed app, with a3024×1898 screenshot. Root sampled the actual process while it was stuck in Options; UI/input and audio investigations are separate owners. The initial audit reads source and `_work/runtime/user-play-stuck-sample.txt`. The bounded constant-upload repair below is privately tested separately; it does not alter quality/resolution or claim an improved actual-game frame rate.

## Measured main-thread sample

The main thread has2846 samples. Three `C_GameApp::OnRender` instruction ranges account for2370 (1921 plus447 plus2). Inclusive call counts below are summed across distinct call-tree branches; subset rows must not be added together.

| Main-thread operation | Inclusive samples | Fraction of all main samples |
|---|---:|---:|
| Native uniform `constants()` |720|25.3%|
| Top-level AGXBuffer initialization, a subset of the above |648|22.8%|
| Native `pipeline()` |281|9.9%|
| Explicit depth/stencil state creation |14|0.5%|
| All command-buffer completion waits |923|32.4%|
| Surface wrapper destruction, a subset of those waits |420|14.8%|

419 surface-destruction samples follow `EndRender→EndScene→submit→releaseRecording`: dropping the final temporary Surface wrapper after assigning the freshly committed command invokes a synchronous GPU completion wait. Another sampled group shows temporary resource destruction during the autorelease-pool drain. Sampling measures where the main thread spends wall time, including blocked time; it is not GPU execution duration or a draw/allocation count. It cannot establish the GPU's independent critical path or predict a particular FPS gain.

## Source-backed candidates

In the sampled pre-MAC270 baseline, `MetalContext.mm::constants` creates a new shared MTLBuffer for every nonempty reflected uniform on every draw. It zeroes actual Metal-reflected aggregate size, copies original logical bytes, and binds that allocation. Uniform data may be vertex registers, pixel registers, the private alpha-test/HDR block or sampler-LOD data. A replacement must preserve each independently and keep per-draw contents immutable until consumed.

Apple's [buffer binding guidance](https://developer.apple.com/library/archive/documentation/3DDrawing/Conceptual/MTLBestPracticesGuide/BufferBindings.html) explicitly recommends transient `setVertexBytes`/equivalent fragment APIs for single-use data smaller than4KB to avoid allocating an intermediate MTLBuffer. The proposed bounded first repair uses that API for the actual padded binding size below4096, including zero tail padding, and retains the existing explicit-buffer path for4096 and larger. No shader ABI or required uniform is skipped. A regression needs distinct values across multiple unsubmitted draws/stages/UBOs, encoder transitions, actual padding and the large fallback.

In that baseline, `MetalNative.h` Buffer/Surface wrapper destructors synchronously wait their pending command. Root's sample identifies Surface retirement as a measured serialization point. All native command creation currently uses ordinary `commandBuffer`; Apple's [retainedReferences contract](https://developer.apple.com/documentation/metal/mtlcommandbuffer/retainedreferences) says that method retains resources used by its commands. A retirement repair must explicitly prove native storage stays owned through GPU completion and preserve error/map/reuse/cross-queue behavior. This is not authority to remove CPU map/write synchronization, query completion, readback waits or device shutdown barriers indiscriminately.

`MetalPresent.mm` waits for the completed engine render before submitting its same-queue copy into a drawable. Surface reuse and draw-buffer validation likewise wait prior commands. Same-queue ordering may make some waits redundant, but resources can be shared across contexts on the same physical GPU; current validation checks GPU identity rather than queue identity. Any future queue-aware change needs ownership provenance and actual coverage tests, not a blanket no-op wait.

Pipeline cache hits still create descriptor/layout objects and assemble keys before finding the cached compiled pipeline. Depth/stencil state is recreated per draw. The sample makes pipeline work a plausible later CPU target, while explicit depth-state creation alone is a much smaller measured contribution. Preserve complete cache keys and effective depth/stencil attachment semantics before changing either.

## Pacing, resolution and startup boundaries

The native application loop contains no explicit30fps sleep/cap. It accumulates real time into original1/60second simulation ticks, consumes up to four ticks per update, and renders independently with interpolation. Reported aggregated simulation frame time is not a direct render-duration measurement.

Presentation uses the owning screen's actual minimumRefreshInterval for original interval2/3/4 requests; immediate and interval1 remain separate. `CAMetalLayer.nextDrawable` may block, but this source audit does not prove a wrong interval or drawable wait caused the observed jitter. No vsync/tick policy change is proposed from the screenshot alone.

Cocoa converts window points to backing pixels and rebuilds framebuffers/window targets at that physical extent. A large Retina window therefore means a genuine larger render workload, not an automatic logical-resolution cap. Record actual drawable/engine dimensions and GPU timing before any quality recommendation; this audit does not reduce them.

Startup is not covered by an Options-state sample. Runtime shader compilation, package/resource loading, initial pipeline compilation and window creation have different startup ownership paths. Attribute startup delay only after a startup sample/timeline, rather than assuming the measured steady-frame allocation issue explains it.

## Bounded constant-upload repair

`MetalContext.mm::constants` now uses `setVertexBytes`/`setFragmentBytes` when the larger of serialized logical size and actual pipeline-reflected size is strictly less than 4096 bytes. Apple's [API contract](https://developer.apple.com/documentation/metal/mtlrendercommandencoder/setvertexbytes(_:length:index:)) says it copies the provided bytes. The fixture overwrites the same scratch/register inputs on later unsubmitted draws. Reflected tail padding is zeroed before each binding, every original buffer index and uniform kind is preserved, and the 4096-byte boundary plus larger blocks retain the original shared-MTLBuffer allocation path. No command completion wait, map, shader source, frame timing or resolution policy changes in this first repair.

`uniform_upload_test.cpp` builds real VS/PS pairs with two ordinary UBOs in each stage at distinct reflected indices. It checks independent colours over eight scissored draws, three complete frames, mid-frame command submission and render-encoder transitions. Logical/reflected byte pairs are 20/32, 404/416, 4068/4080, 4084/4096 and 4100/4112. The final two cases retain the explicit-buffer path. Debug, Speed, Debug+UBSan and Speed+UBSan all pass with strict Metal validation; paired coherent old-upload controls also pass.

The controlled Speed benchmark runs the same fixture with old versus new upload code: 2000 draws per trial, five trials per process, three alternating baseline/fixed process pairs, Metal validation disabled equally. Median CPU encoding times per process were 15.6303–16.1074 ms baseline and 3.18233–3.32625 ms fixed (medians of those process results 15.7713 ms and 3.22829 ms). This includes ordinary draw/state/upload CPU work and excludes command completion/presentation. It demonstrates a reduction in that synthetic CPU workload; it is not a measured game FPS or startup improvement. The actual Options/game profile and manual frame pacing must be measured again after coherent integration.

Private evidence is under `_work/build/macos/occlusion-agent-uniform-upload/`: original archive SHA256 manifest, compile/link logs, GPU outputs and benchmark logs. An initial Context-only replacement into older UBSan archives produced a mixed-private-header shader-cache mismatch. That mixed closure is excluded from validation. The untouched old-archive control passed; rebuilding the full lower target coherently resolves it, with paired baseline/fixed GPU tests passing in both UBSan configurations.

## Bounded Surface retirement repair

The second repair changes only `Surface::~Surface`: it waits when a pending command has `retainedReferences == false`. For ordinary retained-reference commands, Metal owns the actual texture, view and staging buffer resources through GPU completion; releasing the C++ Surface wrapper does not retire those native resources early. The native backend has no Surface callback or host-memory no-copy deallocator requiring that CPU wrapper to remain alive. No C++ wrapper, shared_ptr or engine allocator is captured asynchronously.

An earlier proposed completion-hold callback was rejected during API review. Apple's [addCompletedHandler contract](https://developer.apple.com/documentation/metal/mtlcommandbuffer/addcompletedhandler(_:)) permits registration only before commit, whereas this destructor can run after commit. The implementation uses documented command ownership directly and installs no late callback.

`Surface::wait`, map/download/readback waits, same/cross-queue reuse validation, Buffer ownership, query reads, presentation waits and device shutdown are unchanged. A native command created with unretained references still gets the existing synchronous lifetime barrier. GPU failure reporting remains through the existing command-status, map and shutdown paths; no deliberate GPU device fault is injected by this fixture.

`surface_retirement_test.mm` first exercises 12 real engine clear→colour snapshot→sample→submit→drop-owner→exact-readback cycles. Native shared-event cases then hold the GPU behind an unsignaled event while another thread retires the Surface. The test proves the texture, texture view and staging MTLBuffer remain alive and exact pixels arrive after the gate opens. Retained commands return before the gate; unretained commands remain blocked. Separate cases cover CPU-exception unwinding and dropping the caller's command reference so the queue owns execution. Every gate is opened and the thread joined before asserting a negative result, so the old baseline exits cleanly rather than hanging.

The controlled baseline keeps the original unconditional destructor wait and fails with `Retained GPU resources still forced synchronous CPU-wrapper retirement`, after opening the gate and verifying exact pixels. Fixed Debug, Speed, Debug+UBSan and Speed+UBSan all pass under strict Metal validation (approximately 1.4–1.5 seconds per final fixture run). The controlled paused-game comparison is recorded below. Startup and smooth running-race frame pacing remain separate acceptance boundaries.

## Full-suite fixture correction

The later full Debug suite exposed a stale `game_device_test.mm` assertion that treated the original two-refresh presentation interval as unsupported. Multi-refresh support predates MAC270 (MAC036), and the published1965fac source already accepts it. The fixture now checks successful `Two`, rejects the invalid bit combination3 while preserving the current mode, and restores `Immediate`. The existing lower presentation fixture separately renders actual drawables for all six original policies. This is a fixture correction only; it does not change presentation, vsync or frame pacing. Root build/runtime validation is pending.

## MAC284: actual callback and submitted-present metrics

Set `BLUR_MAC_FRAME_DIAGNOSTICS=1` before startup for opt-in, five-second summaries from `RunDiagnostics.mm`, called after actual OnRender/endRender. It reports callback count/rate, unclamped callback interval mean/max, and deltas of the existing native submitted-present counter. Counter availability is explicit. Submitted presents are committed engine commands; these metrics do not establish GPU completion or compositor/display refresh rate. Default-disabled execution adds only the diagnostics branch, with no readback or synchronization.

The original `blur2pc.timer` output is not a per-render FPS or jitter trace: CommonUpdate invokes it when the decoupled loop consumes simulation ticks. Its FPS is a short update-rate average sampled at report time; its printed frame time is one interval clamped to 1–66.67 ms. The new observation instead uses actual elapsed wall time. Clock rewind, device/counter availability changes or a decreasing present counter restart its window. The existing fixture covers disabled/default, exact-window, clock/reconfiguration reset and invalid-time boundaries; actual offscreen GPU work must correctly report zero submitted presents with an available counter. Four-configuration source probes pass. MAC284 apps build. Both Debug/Speed runtime fixtures now pass after correcting the fixture’s nine-conversion assertion and reading its redirected stderr through the actual descriptor rather than a stale stdio offset.

Compare baseline/fixed apps with identical current framework/audio/UI, assets and dependencies, replacing only the preserved pre-MAC270 lower renderer archive. Qualify an original Paused screen and paused flag, 2560×1440 backbuffer and 20 vehicles after one ordinary F1. Exclude transition warmup, watch/checkpoint IPC and CPU sampling from the marked rate window. The private parser requires pause/dimension/population bookends, discards the first potentially straddling five-second report, and requires three complete counter windows. It aggregates submitted counts over measured elapsed time; CPU sample fractions remain inclusive main-thread wall time, including waits. Neither a key ACK nor a post-exit sample qualifies a performance result.

## Controlled actual paused-game comparison

MAC280 baseline and MAC281 fixed use the same MAC284 framework/UI/audio/dependencies, muted streams, original normal career entry and an observed Paused screen with 20 cars at 2560×1440. Baseline differs only by the preserved pre-MAC270 lower renderer archive. The link-input manifest records exact arguments/input hashes, and the staged dependency manifests match. Both actual processes finish their normal guard shutdown with exit 0 and no primary error.

| Qualified observation | Baseline | First two repairs |
|---|---:|---:|
| Complete measured window |25.17 s|25.06 s|
| Submitted presents/second |24.8338|32.1246|
| Mean callback interval |40.2677 ms|31.1288 ms|
| Maximum callback interval |61.256 ms|97.267 ms|

Measured submission throughput improves 29.36% in this paused scene. The larger maximum interval means this does not establish smooth frame pacing, eliminate hitches, reach 60 fps, or prove the same gain during a moving race. The 97.267 ms maximum belongs to the quiet 117.420883–122.451219 second window, with no logged Automation dispatch or framebuffer capture inside it. The separate CPU sample occurs after the rate window, so it cannot identify the cause of that individual interval. The summary lacks a per-frame timestamp/phase trace; attribution remains unknown.

## MAC288: pipeline cache-hit work

The post-window MAC281 main-thread sample contains 507/3157 samples in `pipeline()` (16.06%). Subsets include 172 descriptor initialization/destruction samples and 184 string-append samples; allocations, frees and allocator-zone searches overlap these counts. No native pipeline compilation is sampled. This supports reducing repeated cache-hit preparation; it does not prove compilation caused the 97 ms outlier. The larger relative pipeline fraction after other work was removed is not itself evidence that absolute pipeline cost increased.

The bounded repair keeps the exact existing key fields and append order: shader identities, declaration records, all stream strides, samples, coverage, blend factors/operations, each colour format/mask and depth format. It reuses context-owned scratch string capacity and returns an existing pipeline before creating native descriptors/layout. Successful misses copy their key into the owning map; later scratch mutation cannot change cached keys. Existing shader/device checks and miss-time validation/reflection stay intact. No resource binding, shader, target, timing, quality or synchronization policy changes.

`pipeline_cache_test.cpp` alternates A/B/A twice across 25 real GPU state variants: distinct VS/PS, recreated shader lifetimes, three declaration/stream/stride layouts, colour/alpha blend factors and operations, RGBA/BGRA, independent R32F MRT masks, D16/D24S8, 1×/2×/4× samples and coverage. Expected pixels are computed independently. Strict Metal validation passes all four configurations, including both UBSan variants, and the old-pipeline positive control passes.

Three alternating controlled Speed benchmark pairs use five-trial medians for 2000 warmed draws: old 3.33883–3.44979 ms versus fixed 1.30592–1.34662 ms CPU encoding. This excludes completion and is a synthetic cache-hit workload, not an actual-game FPS result or hitch fix. Private manifests/logs are under `_work/build/macos/occlusion-agent-pipeline-cache/`. Root's coherent whole-game regressions and any later actual performance comparison remain pending.

## Final coherent application integration (MAC289)

Both complete standard suites pass 141/141 after the cache-hit repair. The final
actual muted run uses the same 2560×1440/20-car Paused qualification and quiet
window protocol. Its five complete reports cover 25.063191 seconds: 1,287
submitted presents, 51.3502 submissions/s, 19.4741 ms mean callback interval and
40.188 ms maximum. The preserved old renderer measured 24.8338 submissions/s
and 40.2677 ms mean; the intermediate two-repair build measured 32.1246/s. The
final post-window sample places pipeline preparation at 1.385% of main-thread
wall samples, versus 16.060% in the intermediate sample; final uniform upload
is 2.181% and Surface destruction is 0%. Relative sample fractions are not
absolute GPU/CPU durations. This is bounded paused-scene evidence, not steady
60 FPS or moving-race coverage. See [player-feedback acceptance](player-feedback-acceptance.md)
for conditions, all three results and remaining audio/frame-pacing limits.

The diagnostics fixture passes in both standard configurations. Its initially
incorrect sscanf conversion count and Darwin stdio cached-offset capture were
fixture defects; descriptor reads now preserve actual captured reports across
repeated reads. The production frame observer was unchanged by those repairs.

### MAC293: correlate remaining CPU waits with actual frame/audio timestamps

The accepted289 quiet paused scene is a throughput result, not proof that all
moving-race hitches or audio starvation are resolved. Its separate CPU sample
contains2889 main-thread observations:1072 in `waitUntilCompleted` (37.1%).
The sampled wait owners are Present724, surface CPU LockRect320,
useSurface25 and other3. These are sampled CPU stacks, not GPU execution times.
The LockRect route is actual `BeginCarEffectTasks -> LockTextureRectCube`, not
PNG capture. `nextDrawable` has only5 observations in this sample. Removing
synchronization or reducing image quality based on these totals is not justified.

`BLUR_MAC_METAL_TRACE_FILE=/absolute/path.jsonl` enables a bounded CPU-only trace.
It uses exactly `duration_cast<nanoseconds>(steady_clock::now().time_since_epoch())`
and the common `steady_ns` key used by the frame/audio traces. On this Darwin host
Python's `clock_gettime_ns(CLOCK_MONOTONIC_RAW)` is the matching observational
clock; `monotonic_ns()` is suitable for deadlines but has a different epoch here.
No clock offset is silently estimated in the engine.

The262144 preallocated POD records are written without per-event allocation or
logging. Already completed command waits still execute unchanged but are omitted
from the trace. Pending/error command waits record begin/end, command status and
numeric resource/command/thread identities. Additional spans cover Present,
its resource wait, nextDrawable, submit, downloads and device idle. A successful
Present records draw/pipeline/constants/textures CPU totals and call counts from
that render owner since its prior Present. Draw contains the other encoding
categories; Present/resource-wait/surface-wait may also overlap. These are inclusive
measurements and must not be added as exclusive costs. A dropped-record count
qualifies any exhausted trace; it does not silently overwrite history.

The default is disabled. No synchronization, command ownership, presentation,
resolution, simulation step or resource policy changes. Output occurs only after
the last participating device's existing WaitIdle/destruction and diagnostic
writers finish. It contains CPU timestamps, not GPU command start/end or displayed
frame times. The exact accepted289 optimized lower archive is preserved privately,
SHA256 `2ac5fe3e47978e7f49adbea46da0e4efb7b48400207b252f0ed06dace2204b3e`.

The CPU-only `metal_trace_test` exercises default-off/relative-path rejection,
shared session lifetime, exception span completion, concurrent writer/flush
exclusion, status/identity fields, monotonic begin/end and restart/reset. Actual
moving-race causal attribution remains pending the coherent frame/audio/backend
trace run; these diagnostics alone do not establish a repair.
