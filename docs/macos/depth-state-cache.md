# Immutable native depth/stencil-state reuse (MAC325)

The graphics audit identified a bounded source-backed opportunity: each actual
`MetalContext.mm::draw` constructed an `MTLDepthStencilDescriptor`, optional
stencil descriptors, and a new immutable `MTLDepthStencilState`. Partial clears
in `clearRectangle` did the same. This is repeated creation in the source, not a
current measured bottleneck or an attribution of earlier frame outliers.

## Exact contract

`MetalDepthStencilCache.h` is private to the context implementation. One context
owns one map and retains the native device and resulting immutable states.
Neither a process-global GPU object nor an engine/public ABI field is added.
The map dies with its context. Encoded commands retain their ordinary native
resources; the cache adds no completion callbacks, asynchronous C++ captures,
waits, or alternate queue policy.

The key uses explicit values, never padding bytes. It includes actual depth and
stencil attachment presence, effective depth comparison/write enable, and each
front/back face's enabled state, comparison, failure operation, depth-failure
operation, pass operation, read mask and write mask. Current original engine
state assigns the same face description to front and back; the cache keeps the
two descriptions independent. Missing attachments still select the existing PC
effective-state behavior. Requested state remains untouched for reattachment.
Stencil reference remains a dynamic `setStencilReferenceValue` command and is
not cached. Bias, viewport, scissor, sample count, shader outputs, blending and
alpha coverage retain their existing ownership and code paths.

Clear states use the same cache, preserving Always comparison, requested depth
write, Replace stencil operations and 0xff masks. Full load-action clears do
not create a depth state in either implementation; partial draw-based clears
exercise the cache. MSAA does not alter immutable depth/stencil operations and
therefore is not an artificial cache-key field. Existing PSO/attachment sample
validation remains intact.

Creation failure throws before binding or cache publication. No nil state or
success substitute is installed. The implementation changes no encoder ending,
PSO key, shader math, resource recording/wait, render-target load/store, quality,
resolution or simulation policy.

## Opt-in evidence

Set `BLUR_MAC_DEPTH_STATE_STATS=1` before creating the device/context. Each
retiring context prints one line beginning `blur2pc.depth_state_cache`, with
`draw_requests`, `clear_requests`, `hits`, `create_attempts`, `created`,
`failures`, and `entries`. MAC334 appends `last_hits` and `map_hits`; their sum
equals total hits, preserving the original field prefix for existing parsers. Disabled contexts do not update counters or emit
output; there is no per-draw diagnostic allocation, clock call, or logging.
The ordinary lookup/map allocation is part of the optimization, not diagnostics.
Counters describe native immutable-state calls, not GPU execution time or FPS.
The map remains context-owned for the existing device lifetime, so actual entry
count/high-water evidence must be inspected for long play before claiming a
memory improvement.

A separate real-game diagnostic run can establish actual draw/clear reuse. Keep
this opt-in **off** during quiet baseline/candidate performance windows, with
identical muted audio, pixel dimensions, graphics, track, population and assets.
A teardown summary does not identify which individual frame had an outlier.

## Fixture and controlled comparison

`tools/macos/depth_state_cache_test.mm` authors shaders and submits real lower
context operations. It compares GPU colour pixels against an independent CPU
comparison/stencil arithmetic oracle before using additional stencil probes.
Cases include absent depth, D16, D24S8, 1x/2x/4x samples, compare/write changes,
read/write masks, all eight stencil operations including saturation/wrap edges,
dynamic reference changes, partial clears, encoder/command boundaries, repeat
sequences, device recreation, and diagnostics disabled during actual draws.
The same sequence runs once and three times in new contexts: unique native
creation count must remain equal, while actual requests/hits increase. This is
not a test that simply looks up the cache helper directly.

The optional `--benchmark` path records five warmed 2000-draw CPU encoding
trials and reports their median. It reuses the already-exercised Greater /
write-disabled state (with passing z0.875 against cleared depth0.75), so the
optional timing path does not add a unique state to the repeated-count oracle.
It includes ordinary original setters and draw
work, excludes completion/presentation from its timed interval, and cannot by
itself prove actual-game speed or hitch reduction. Run old/candidate alternating
process pairs, preserve the complete lower renderer closure and compare pixels
before using timing. Both benchmark variants use the same counter setting.

Private `_work/build/macos/occlusion-agent-depth-state-cache/` preserves the
pre-change Context body. Its current private control builder disables both last-entry and map reuse
while retaining descriptor/counter/ownership code. Compile that helper with the
same current Context and all other identical lower objects for a controlled
per-request native-creation negative: all GPU pixel oracles should pass, then
real creation/hit/repeat counts must fail. The control still constructs keys and
attempts map insertion, so it is **not** the exact pre325 timing baseline. The
exact pre325 Context was separately relinked into the same full candidate game
for MAC333. No production test macro selects uncached behavior.

Final production Context and GPU-fixture object probes pass all four configs
(Debug, Speed, DebugUBSan, SpeedUBSan): **8/8 PASS**. The initial fixture-only
signed literal in the CPU oracle was corrected before the final compilation.
A private CPU-only authored Objective-C factory exercises the actual cache and
MTL descriptors without acquiring an MTLDevice: all 18 key values, including
independent front/back fields, exact descriptor values, device isolation,
retirement, nil creation failure and retry. In each of four configurations,
the initial MAC325 revision used 95 requests for 19 distinct keys, creating
19 states with 76 hits. The uncached
control creates 95 states with zero hits and exits1 at the intended count oracle.
Cached4/4 pass; negative4/4 reproduce. This is factory/lifetime/count evidence,
not GPU or driver-cost timing.

The initial MAC325 coherent Debug lower fixture passed actual GPU operations
in 0.84s.
Private `build_gpu_control.py <config>` prepares cached/uncached binaries by
replacing only Context in copied current archives and reusing the complete
existing fixture link command. It never modifies a shared output or runs a GPU
process. At that initial handoff, controlled GPU negative/timing and actual acceptance
were pending. The completed final checks are recorded in
[the combined report](optimization-host-acceptance.md); no reliable speedup is claimed.

Root review caught an initial benchmark-only Less/write-disabled key absent
from the correctness matrix. The fixture now uses the existing Greater key;
all four fixture object reprobes pass. Production/cache code is unchanged and
no benchmark result from the earlier fixture is accepted.

## MAC334: bounded hit-path revision and contrary actual observations

Initial 325 correct pixel/count tests did not establish a current game gain.
Actual stationary/moving quiet windows were 41.83/47.45 submissions per second
for the first cache candidate, versus 57.99/68.75 and then 62.44/49.33 for two
old320 runs. An exact pre325 Context relink retaining the new candidate's DDS and
host changes observed 45.06/56.36. These are non-deterministic race states, not
an isolated attribution of the entire difference to the cache. No universal
speed or hitch improvement is claimed. Diagnostic331 recorded 12,912,388 draw
requests, 28 creations, 12,912,360 hits and no failures, but missed its passive
measurement deadline: it establishes reuse, not a performance result.

MAC334 retains the exact key and strong owning map. A small `Bind` path first
compares the whole key with the last map entry. A match avoids hashing and node
lookup; otherwise normal map lookup runs, with native descriptor creation in a
separate cold method. Binding returns no Objective-C object through an extra
C++ return boundary. It **always** issues `setDepthStencilState`, including on
new encoders and after partial clears. Dynamic reference commands remain
unchanged. No setter-version dirty flags, key packing, encoder-binding skip,
shader, quality or synchronization changes are included.

The last entry is borrowed from the same non-erasing map; unordered-map rehash
preserves element references. No extra native owner, encoder or callback is
stored there. Both draw and clear use the same binder. The real-context fixture
now adds 40 distinct masks interleaved B→A, repeats after map growth and requires
both last-entry and map-hit evidence alongside exact pixel/count checks. The
pre334 production/helper/fixture/doc snapshot and hashes are under `pre334/` in
the private evidence directory.

Current Context and fixture compile probes pass 8/8 across four configurations.
Final coherent GPU, negative controls, both complete 151-test suites and actual
MAC334/335 game checks subsequently passed; see
[the combined report](optimization-host-acceptance.md). Compilation and high hit
rate are not a speedup claim.

MAC334 private CPU authored-factory checks pass four configurations, with each
case validating that the encoder is still bound on every request. 1,180 requests
for 59 keys create 59 native factory states, including 599 last-entry hits and
522 map hits. The no-reuse control creates 1,180 states and fails the intended
count oracle in all four configurations. Device isolation, independent front/
back fields, map growth, live-owner retirement and failed-creation retry remain
covered. These are CPU factory checks, not GPU or speed measurements.

The optimized object still contains an outlined `Bind` implementation; this
revision does not claim machine-code inlining. Root retained and hash-checked
the source freeze through coherent GPU/actual qualification. Further compiler hints or hot-path
changes require measured evidence and are not part of MAC334.
