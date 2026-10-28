# Apple tile-memory attachment rebinding (MAC338)

Apple's GPUs use tile-based deferred rendering. Ending a render encoder can
require storing live attachments, then loading them for a later pass. This is a
specific opportunity to remove redundant pass boundaries; it is not evidence
that all existing frame time comes from memory traffic or that the new path is
faster in the real game.

## Hardware and primary guidance

Apple lists the 12-core (8 performance + 4 efficiency) CPU,16-core GPU,24GB
unified-memory M4 Pro configuration in the **14-inch** MacBook Pro, with
3024×1964 native pixels and adaptive refresh up to120Hz. The M4 Pro's advertised
273GB/s is a theoretical memory-bandwidth specification, not measured game
bandwidth. [Apple model specifications](https://support.apple.com/en-us/121553).

M4 belongs to Apple GPU family9. Metal4 availability on macOS26 does not imply
Apple10-only features: notably hardware sampler LOD bias is Apple10, so this
backend's existing shader integration remains necessary on M4. Capabilities
should be queried, not inferred from a model name.
[Apple GPU family table](https://developer.apple.com/metal/capabilities/).

Apple recommends avoiding unnecessary attachment loads/stores on its tile
architecture. GPU-only textures still benefit from private storage despite
unified system memory; shared memory is not a universal zero-copy substitute.
[Apple TBDR guidance](https://developer.apple.com/documentation/metal/tailor-your-apps-for-apple-gpus-and-tile-based-deferred-rendering),
[load/store actions](https://developer.apple.com/library/archive/documentation/3DDrawing/Conceptual/MTLBestPracticesGuide/LoadandStoreActions.html),
[Apple GPU storage modes](https://developer.apple.com/documentation/metal/choosing-a-resource-storage-mode-for-apple-gpus).

## Narrow source change

`MetalContext.mm::SetColourSurface` and `SetDepthStencilSurface` previously ended
the live encoder on every call, including identical shared `Metal::Surface`
identity and null-to-null binding. The upper `SetColourDepthStencilSurface`
helper also binds all three absent MRT slots and the depth slot. `beginEncoder`
preserves attachments through Load/Store and StoreAndMultisampleResolve, so an
unnecessary restart may cause additional tile traffic and resolves.

Both setters validate context/thread, slot, creation and GPU ownership first.
As of MAC346, pass coalescing is **experimental and default-off**. Set
`BLUR_MAC_TILE_PASS_REUSE=1` before context creation to opt in. Only in that
mode do setters end the encoder and replace the owner only when the **shared
Surface identity** changes. With the variable unset, original unconditional
setter endings remain active. They do not merge independently created native texture
views, compare only dimensions, or assume two textures contain equivalent data.
A same-binding call avoids even an extra shared-owner assignment.

Colour0 always retains its original viewport-explicit reset and full-target
scissor-rectangle reset, including identical and null binding. Scissor enable is
unchanged. Actual target changes, null detach/rebind, full/partial clears,
explicit resolve, blits, colour-feedback snapshots, query begin/end/retirement
and command submission retain their existing encoder boundaries. All required
resource waits, load/store policies, shaders, quality and sample-count behavior
are unchanged. Fewer query segments must still sum to the same actual samples.

## Opt-in evidence and test

`BLUR_MAC_RENDER_PASS_STATS=1` before context creation enables integer counters:
`passes`, `colour_setters`, `depth_setters`, `identical_colour`,
`identical_depth`, `avoided_colour_end`, `avoided_depth_end`. The last two count
identical setters while an encoder is live **and reuse is enabled**; they stay
zero in default-off mode. One `blur2pc.render_pass_stats` line
is written at context retirement. Counters are disabled by default; no per-draw
clock, file output, allocation or GPU profiling operation is added.

`tools/macos/attachment_rebind_test.mm` uses authored shaders and real lower
context operations. It compares a sequence without extra binds against the same
sequence with repeated colour/depth/null bindings and interleaved draws.
Exact colour/readback checks cover viewport/scissor resets, partial clears,
actual target switches and preserved prior contents, depth detach/reattach,
explicit resolve then draw, GPU copies and MRT detach. It exercises absent,
D16 and D24S8 depth at1x/2x/4x. Actual1x visibility queries must still count the
same256 samples, and invalid/uncreated/wrong-thread setters must still reject.
In explicit experimental mode, the repeated sequence must create the same
number of render encoders while showing additional setters and avoided live
endings. With the environment variable unset, the identical pixel sequence must
instead retain original extra passes and report zero avoided endings. Complete
runs also check diagnostics disabled in both modes and context recreation.

The exact pre-change Context and SHA are preserved under the ignored private
`occlusion-agent-attachment-rebind/pre338/` evidence directory. A private control
retains current counters/headers but restores unconditional setter endings.
The same GPU pixels should pass, then the pass-reuse assertion should fail.
This is a pass-count negative control, not a faithful timing baseline because
it includes opt-in diagnostics. Root owns coherent builds and GPU/actual
acceptance; source compilation alone establishes no runtime or speed result. Private Context
and fixture object probes pass8/8 across Debug, Speed, DebugUBSan and
SpeedUBSan. The private `build_count_control.py <config>` prepares the old-body
control using copied lower storage and the existing complete fixture link
closure; it never executes a GPU process or writes shared outputs.

## Ranking follow-ups without changing quality

1. **Identical attachment rebinding:** smallest semantic surface and strongest
   direct source proof; qualify pass counts, exact pixels and actual GPU HUD
   intervals before any gain claim.
2. **Colour-feedback snapshot blits:** `SetTextureWithColourSnapshot` copies all
   mips/faces through one blit encoder per subresource. A single validated blit
   batch could reduce encoders while preserving snapshot-at-binding semantics.
   Reusing storage requires proving simultaneous old snapshots stay immutable;
   unified memory does not remove that ownership requirement.
3. **Shader/PSO cold misses:** `MetalShaders.mm` compiles integrated MSL at load;
   `pipeline()` synchronously creates a native PSO on a cache miss. An optional
   `MTLBinaryArchive` with exact shader/descriptor/OS provenance and compilation
   fallback may reduce first-use hitches. Current cold-miss frequency is unknown.
   [Apple binary archives](https://developer.apple.com/documentation/metal/creating-binary-archives-from-device-built-pipeline-state-objects).

Memoryless render targets and DontCare stores are not safe blanket changes:
existing depth, readbacks, feedback, queries and cross-pass samples need prior
contents. Single-pass deferred lighting/tile shaders would require an explicit
renderer/shader redesign, rather than this adapter-only optimization.

With only Command Line Tools installed, optional GPU timestamp sampling remains
possible through supported Metal counter sets/stage boundaries, resolved only
after completion. It must query capabilities; Apple GPUs commonly sample at
stage boundaries rather than arbitrary draw boundaries. Extra synchronization
for profiling changes the workload and belongs in separate diagnostic runs.
[Apple counter sampling](https://developer.apple.com/documentation/metal/sampling-gpu-data-into-counter-sample-buffers).


## MAC346: measured sampler CPU class; tile experiment held

MAC338's initial coherent Debug/Speed suites152/152,11 strict Metal checks,
two lower UBSan configurations and the old-body GPU control pass. The actual
pixel/count oracle observes81 reference/159 repeated passes for the old path,
and72/72 when coalescing is enabled. These are pass counts, not speed evidence.
The first sequential Apple HUD comparison reported interval medians14.320/
13.900ms(no-input/driving) for old337 versus14.550/15.648ms for candidate340;
GPU medians6.150/6.490 versus6.510/7.532ms, with worse candidate maxima.
Cars ended in different positions after AI collisions, so this neither proves
causality nor qualifies a gain. GPU HUD memory estimates are not process RSS.

Merging saves potential tile traffic but also changes scheduling granularity:
Apple explains that all vertices in a pass precede its fragments, whereas a
later pass's vertex work can overlap earlier fragments. Existing blending,
alpha-test/depth output and write masking also limit hidden-surface removal.
No geometry spill or tile-memory exhaustion was measured here. Those are not
accepted explanations for the timings.
[Apple scheduling/HSR discussion](https://developer.apple.com/videos/play/wwdc2020/10632/).
The narrow coalescing path therefore remains available only through the explicit
context-creation opt-in; count reduction alone does not justify default delivery.

The separate MAC341 sample observed sampler stacks143 times among1,592 main
thread observations;277 inclusive `malloc_size` observations also contain other
call classes. These are sampled stack residency, not disjoint CPU durations.
The source identifies a concrete avoidable action: `sampler()` constructs a new
32-byte string from eightU32 fields for every sampler lookup, including hits.

MAC346 reuses context-owned `samplerKeyScratch`, clearing and appending the same
eight exact words in the same order: U/V/W addresses, mag/min/mip filters,
border colour and anisotropy. A missed map insertion **copies** the key, so map
entries never borrow mutable scratch and scratch keeps its capacity. Bias/gamma
integration, validation, descriptor construction, native-state return ownership
and all waits are unchanged. First misses may allocate a separate copied map
key; warm hits reuse the lookup capacity. No native sampler cache policy or
shader quality change is introduced.

`BLUR_MAC_SAMPLER_KEY_STATS=1` before context creation prints one retirement
summary: `requests`, `capacity_growth_calls`, `max_capacity`, `hits`, `created`,
`entries`. A growth call means the string's final capacity exceeded its capacity
at entry to that lookup; it does not count every internal allocation, native
allocation, map-node allocation, process heap event or byte. Disabled counters
have no timer/log/allocation operation and no per-append diagnostic branch.

`sampler_key_reuse_test.mm` authors real2D and3D mip textures and both-stage
samplers. Independent known sample rows cover addressU/V/W, border alpha,
point/linear/mip filtering, fractional/signed sampler bias, MaxLOD0, vertex-bank
and two-pixel-slot isolation. Ninety-six key variants interleave earlier A keys
across map growth. Repeated new-device sequences must retain the same native
entry count and plateau of lookup-capacity growth; invalid draw state must reject
then permit a genuine valid retry. A diagnostics-disabled run remains real GPU
work. Private four-config Context and both fixture object probes pass12/12;
root acceptance now passes both complete153-test suites, four lower UBSan
checks and12 focused Metal-validation tests. Actual348 observes24,134,430
requests with one capacity-growth call and21 native states; actual350 drives
and exits normally. These lifetime counts do not establish FPS improvement.

The exact340 Context(c33c9356c1ef7bd13d9bdab33c288666d8bae0501bbb365a342f716af0386ac6)
is preserved in ignored `occlusion-agent-sampler-key/pre346/`. The private
`build_count_control.py <config>` restores only a fresh local lookup string in
current code, retaining diagnostics and map-key copies. Its pixel/state oracles
pass before the warm-capacity assertion fails in the executed Speed control. It is a capacity-work
negative control, not an exact old-game timing baseline; no shared output or
GPU execution is performed by the builder.
