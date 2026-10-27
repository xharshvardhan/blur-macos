# Native dynamic CPU texture mirror (MAC298, GPU validated; actual comparison pending)

MAC296 completed its qualified90-second unpaused/no-input window:31851 native
download waits consume5.39seconds (mean0.169ms), with no dropped trace records.
MAC298 implements this bounded repair under the user's existing instruction to
fix measured performance classes. Actual moving-game comparison remains pending.
No synchronization barrier is removed.

## Why these six locks exist

`GameWorld.cpp::BeginCarEffectTasks` maps six faces of the focused vehicle's
light cube before scheduling `CarUpdateLightCubeMapTask`. The resource is8×8,
one mip, double buffered, dynamic and shader readable; ordinary creation has no
render-target usage. A default-frequency1 update requires main-view visibility,
a camera distance below60 and the prior focus-view association. Normally one cube
is updated per local viewport, not all20 race vehicles. The world update runs
before the layer's paused early return, so the accepted paused sample includes
this work. A task may remain pending across repeated simulation updates until
BeginFrame consumes it; this is not a requirement to update once per rendered frame.

The worker computes every texel of all six faces from CPU rim-generator inputs.
It never reads prior cube texels. It packs RGBM into BGRA8 or the existing RGBA16F
extension. At8×8 this is1536 or3072 actual texel bytes; native256-byte row alignment
makes staging12288bytes per six-face cube. EndCarEffectTasks flushes both task
groups, then unlocks/uploads all six faces before normal scenegraph rendering.
The two-cube alternation alone does not prove GPU completion.

The current native surface lock always downloads private GPU storage to a fresh
shared buffer and waits, even for these CPU-produced textures. The existing PC
bridge already retains CPU staging mirrors for textures which cannot be rendered
into (`D3D11Context.cpp::CanMirror`, `LockResource`, `D3D11ResourceWritten`). Its
plain-lock contract preserves untouched bytes, so native flags0 cannot globally
be reinterpreted as discard or write-only.

## Bounded first repair

The implementation caches CPU staging bytes only for **dynamic texture subresources without
render-target/depth usage**. Static retail textures remain uncached, avoiding a
retained CPU duplicate of every uploaded asset. Standalone render surfaces,
MSAA/resolve targets and snapshots retain their present path. The eligibility is
set on the actual shared `Metal::Surface` at texture creation, not inferred from a
temporary wrapper's flags: cube/2D/3D lock wrappers currently use temporary surfaces.

Every existing `Surface::wait()` and ownership/thread/recording check is retained. On
Lock, wait as before. If an eligible mirror is valid, return its existing bytes;
otherwise perform the current download and completion wait to initialize/refresh
it. On successful writable Unlock, keep the staging buffer after its existing
upload/commit. The next Lock still waits the surface's pending command before
CPU reuse. This protects both in-flight staging uploads and GPU readers, including
another context/queue for the same physical GPU. It requires no renamed storage,
ring-depth assumption or removed CPU/GPU barrier. Failed maps/uploads retain their
existing failure/owned-lock behavior and must not publish valid stale data.

A valid mirror is authoritative only until a tracked GPU destination write.
Invalidate under the existing resource ownership lock at the actual write boundary;
invalidating before a subsequently abandoned command is conservative and safe.
On the next Lock, the existing recording guard/pending wait plus download restores
current GPU bytes. Do not mark source-only reads dirty.

## GPU mutation and alias inventory

| Native path | Effect on candidate mirrors |
| --- | --- |
| `MetalSurfaces.mm::UnlockRect` staging→texture | CPU-authored upload; cached bytes remain authoritative, existing pending lifetime remains |
| `MetalTextures.mm::CopyTexture` | Invalidate every copied destination face/mip (including common mip-tail mapping); sources remain valid |
| `MetalContext.mm::CopySurfaceRegion` | Invalidate destination only, including partial regions; full CopySurface delegates here |
| `MetalContext.mm::beginEncoder` clear/draw color/depth and store/resolve | Candidate textures lack render-target usage and are rejected as color targets; actual render/resolve resources remain uncached |
| `SetTextureSnapshot` | Newly allocated snapshot surfaces remain uncached; copies delegate to CopySurface |
| `MetalPresent.mm::PresentSurface` | Source read only, drawable destination is not an engine candidate; retain mirror validity |
| texture file/memory/stream loading | Uses normal lock/unlock paths; static resources remain uncached |
| C_Surface aliases, texture copies and linear/sRGB views | Share canonical `Metal::Surface` subresource ownership; invalidate/cache there, not in wrappers |
| unsupported shader image writes/raw external native handles | No supported shader-write path exists; no promise of tracking arbitrary out-of-band private Metal mutations |

The source inventory contains no other supported texture mutation encoder:
copy operations are in Context/Textures/Surfaces/Present; shader image writes
remain rejected. Buffer copies, visibility queries and constants are separate
storage and are outside this candidate.

## Required regression and controlled comparison

1. Dynamic2D/cube/volume repeat maps: known bytes, partial CPU edits preserve
   untouched texels, every face/mip, padded rows and multiple formats; const/read
   maps agree with actual sampled GPU output. Static resources retain downloads.
2. Warm a mirror, then actual full CopyTexture, common mip-tail copy and partial
   CopySurfaceRegion into it; next map must expose exact new GPU bytes, preserving
   untouched regions. Verify aliased wrappers and independent subresources.
3. Event-gate an upload or GPU reader. A subsequent lock must still block until
   that pending operation completes, then return exact bytes; this proves the
   prior synchronization/lifetime contract was preserved, not optimized away.
4. Repeated create/destroy, read-only unlock, invalid flags, duplicate locks,
   wrong-thread unlock and failed-map retry; preserve required-resource errors.
   Actual render-target/depth/MSAA paths remain unchanged and pass existing tests.
5. Controlled old/new fixture traces establish repeated download counts/costs;
   no CPU wall-time threshold is a correctness test. The actual muted20-car
   moving-race frame/audio/backend traces determine whether the class contributes
   to starvation or hitches. Do not infer this from a separate paused sample or
   call the result GPU execution time.

Potential source scope, only after approval: private MetalNative.h surface state,
MetalTextures.mm creation/copy invalidation, MetalSurfaces.mm lock/unlock and
MetalContext.mm CopySurfaceRegion invalidation, plus a focused GPU fixture. No
public API/layout, game cadence, cube resolution, material, shader, quality,
presentation or audio scheduling change is needed for this first candidate.

## Source freeze and verification status

The implementation and `tools/macos/cpu_texture_mirror_test.mm` are source-frozen.
All16 affected-native-TU/fixture probes pass across Debug, Speed, DebugUBSan and
SpeedUBSan. All four fully coherent private lower closures (21 source files each) link and
pass their muted GPU regression with Metal API Validation enabled; both UBSan
variants use halt-on-error. The fixture counts actual download trace spans and verifies independently
read GPU rows, every cube face/mip, partial edits, static-map noncaching, full and
common-tail copies, partial surface-copy invalidation, volume GPU-copy contents,
float formats and render-target/depth exclusion. Event-gated upload and foreign-queue
reader tests require the cached map to preserve the original blocking lifetime.
Each run records20 downloads:18 initial cube face/mips plus2 intentionally
uncached static maps, with repeated dynamic partial/read maps issuing no additional
downloads. Actual GPU row bytes, copy invalidation and both event-gated barriers
pass. This establishes correctness and removed redundant transfers in the fixture;
actual performance benefit or an audio repair is not claimed yet.

A controlled optimized negative uses the same pinned298 headers and coherent
private archive, replacing only the three changed implementation bodies with their
pre298 versions. Independent actual GPU bytes remain exact, but the fixture exits
cleanly with its expected regression:56downloads versus20. This separates plain-lock
pixel correctness from the removed redundant-transfer invariant without mixing
old/new object layouts. Source/header hashes and logs are retained privately.
