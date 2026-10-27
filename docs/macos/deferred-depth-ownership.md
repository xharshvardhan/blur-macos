# MAC210: deferred lighting borrows normal-pass depth and stencil

MAC201 reached the original normal-career event, then failed during its intro
vista with `Stencil clear has no stencil attachment`. MAC209 captured the
exact before-unwind state in the frozen Debug197 game: clear flags4 (stencil
only), no bound depth surface, colour0 A8R8G8B8 2560×1440/sample1, other MRTs
empty. The caller was `RenderStencilBox` → `RenderSpotlightVolume` →
`C_LightingPrepass::RenderSpotLights/RenderLights1` → main prepass rendering.
The diagnostic killed its owned inferior after recording proof; it was not a
successful game exit. Private primary evidence is
`_work/runtime/verification/mac209/stencil-proof.json`.

## Original ownership contract

`C_LightingPrepass::SetupRenderTargets` defines exactly two setups. The normal
setup owns A8R8G8B8 normals, R32F reversed depth and D24S8 depth/stencil. The
lighting setup owns its colour target and has no owned depth/stencil. For each
enabled render view, `SetupViews` registers the positive normal and negative
lighting IDs with identical dimensions and sample count. A repository source
inventory found no other production client of `Deferred::C_RenderTargetSetup`.

Original PC `Deferred::C_Manager::Begin` only replaces the bound depth surface
when the selected view owns one. A missing owned surface therefore preserves
the actual previous binding; it does not request a detach. The method separately
disables requested depth testing/writing for the no-owned-depth pass. Large
spotlight methods subsequently set their own depth/stencil state and use that
borrowed normal-pass stencil. Simple lights do not exercise the stencil clear.

The native MRT implementation incorrectly also called `SetDepthStencilSurface`
with null for that pass. This made the simple-light route appear usable through
the legal missing-depth draw behavior, but discarded a resource required by the
large-light route. The fix preserves the existing native stale-MRT retirement
and binds a depth surface only when one is supplied, matching original PC.
It does not allocate an attachment, change clear flags, force simple lights or
weaken the lower Metal guard.

## Extent, sampling and lifetime boundaries

Normal/lighting pairs have identical source-configured extents and sample count.
The native lower layer still validates any retained attachment at encoder
creation: a larger depth extent can cover a smaller colour viewport; a smaller
depth extent or different sample count remains an error. Explicit caller
`SetDepthStencilSurface(nullptr)` still detaches; a later missing-owned-depth
pass preserves null and an explicit stencil clear still fails. A subsequent
owned-depth pass replaces the binding. The manager does not acquire ownership
of whichever borrowed surface happens to be bound.

PC light shaders sample the R32F depth colour texture (`GetColourTexture(view,1)`)
and normals, not the retained D24S8 attachment. Native MRT slot1 is retired before
those bindings; strict attachment/sampling feedback validation remains in force.

## Focused regression

`tools/macos/deferred_depth_test.cpp` creates the actual manager, source-built
large-light shaders and an authored volume texture. It exercises the same
`RenderSpotlightVolume` producer, observes stencil-constrained GPU colour pixels,
checks depth preservation through a real depth comparison, and checks explicit
detach/rebind, legal larger depth extents, rejected size/sample mismatches and
resource retirement/recreation. It does not launch a race or substitute for the
actual normal-career run. Source probes and runtime results are recorded in the
board/ledger. Eight private objects (manager and final fixture in Debug, Speed,
Debug UBSan and Speed UBSan) compiled successfully. The identical final Debug
fixture linked against the original197 archive fails with the exact strict
missing-stencil exception and exits1. Replacing only that archive's Manager
object with the correction passes under strict Metal in Debug and Speed,
exits0, and observes 520/1024 stencil-selected pixels in each repeated lifecycle.
Private logs and link arguments are retained under
`_work/build/macos/occlusion-agent-deferred-depth/`.

The CMake hardware test is `mac_deferred_depth`; it accepts the source-built
`authored-shaders` resource root and uses the existing full-game fixture link
closure. Full-game UBSan runtime remains outside this fixture's registered
configuration because of the existing PortalBlocker RTTI closure limitation.
The test restores the default target before manager destruction; it does not
establish destruction/resize while a deleted manager-owned wrapper stays bound.
That pre-existing lifetime boundary was not changed by this pass-binding fix.
