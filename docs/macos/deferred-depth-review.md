# MAC212 independent deferred-depth review

Read-only review of MAC210; no blocking issue identified in the current
`Deferred::C_Manager::Begin` change or `deferred_depth_test.cpp`. This is source
and fixture review, not an additional build, GPU run or normal-career acceptance.

## Actual owner and pass contract

- `graphics/deferred/source/Manager.cpp:491-537` retains the currently bound
  depth attachment only when the selected view owns none, matching the original
  PC branch in the same function. It still clears native colour slots 0-3 and
  keeps the original requested depth-test/write disabled state for that view.
  An explicitly detached depth stays detached; no normal/default view is guessed.
- The scoped source inventory finds one actual manager producer:
  `code/amax/game/Graphics/Lighting/Prepass.cpp:55`. Its setup at 92-132 pairs
  normal A8R8G8B8 + R32F with owned D24S8, and lighting colour with Unknown depth.
  Both use samples None. SetupViews at 134-151 creates each positive/negative
  view pair from the same viewport width/height. DropShadows receives a borrowed
  manager and reads normal textures; it does not introduce another Begin route.
- `Game/GameWorld/View/Main.cpp:1134-1220` runs normal Begin/render/End before
  RenderLights1. Its native handoff detaches R32F MRT1 and texture13 before
  sampling. `Prepass.cpp:255-305` binds that separate R32F as sampled depth,
  begins the negative lighting view, clears colour and renders the lights.
  RenderLights2's additional Begin is inside the PS3-only body. The native path
  does not insert reflection/cubemap target changes between the paired passes.
- The retained D24S8 is therefore hardware depth/stencil, not the R32F sampled
  depth. `deferred/source/prepass/Light.cpp:521-585` clears real stencil and
  selects depth-test with depth writes disabled for cone/box volumes. MAC209
  captured this exact actual career caller with no depth attachment; see
  [the before-unwind proof](stencil-clear-runtime-proof.md).

Paths abbreviated above as `graphics/` are under `code/mercury/library/`;
`Game/` is under `code/amax/game/`.

## Lifetime and strict lower boundary

The normal view continues owning the attachment throughout its immediate
lighting pass. Begin adds no new owner or transfer, and End resolves colour
without retiring the depth surface. Native lower SetDepthStencilSurface retains
its real shared storage; later explicit target selection replaces that binding.
This does not authorize keeping an upper raw surface pointer after its owner
has been destroyed.

`display2/source/device/metal/MetalContext.mm:120-150,321-325,531-548` still
requires valid depth format, sufficient attachment extent and matching samples;
missing-stencil clears still throw. Sampled attachment feedback remains rejected.
The change is confined to upper pass selection and does not relax these checks.

## Fixture coverage and remaining acceptance

The current fixture uses the original C_Manager and C_Light, actual large
spotlight shaders and real GPU stencil/depth pixel readback. It covers:

- nonempty, bounded stencil-mask coverage and preserved depth;
- explicit detach rejection and rebind to the actual current view;
- legal larger borrowed depth plus invalid extent/sample rejection;
- two acquisition/retirement cycles and tracked resource baseline.

It deliberately restores the default target before destroying the manager.
It does not prove manager Resize/Destroy while its upper raw surfaces remain
bound, nor arbitrary multi-view/resolution transitions or full career rendering.
The existing Resize destroys/recreates owned targets; no new live-bound lifetime
claim follows from this review. Root's coherent fixture runs, differential
baseline and actual normal-career retry remain the integration evidence.
