# Reachable render startup boundary audit (MAC139 companion / MAC140)

Read-only audit scope is the actual Brighton1/BMW preset2 graph. No graphics
policy changes follow solely from the findings below. Actual frame, input,
finish and teardown evidence remain owned by the running-game checks.

## Selected graph and sampler capabilities

`GameWorld/PostProcessLayer.cpp` constructs ReducedMotionBlur, DoFFgBg, the game
HDR implementation and BloomFeedbackGhost. Initial Create checks both the
manager's nonvolatile and volatile results. Earlier native transaction work
checks required weighted shaders, returns actual effect creation status and
retains cleanup of partial target owners. This is the selected graph, rather
than the older MotionBlurId/MotionBlurDoFId classes also present in the tree.
Those older classes still have PC/console policy branches, but their existence
is not evidence they are reached by this slice.

The preset2 view chain enables Main, Mirror, Reflection and CubeMap. Its
prepass runs independently of light count, publishes real PC depth and unbinds
MRT1 before later sampling. Mirror's required offscreen target is checked after
its original void creation API; missing targets throw before render. The
previous actual MirrorView failure was a legal nonzero sampler bias, covered
by MAC134's shared adapter; it was not a missing resource.

The authored693-shader inventory has1854 direct entry sample calls and no
explicit gradients. Material/vehicle HLSL source includes explicit cube LOD,
vertex damage-info2D LOD and damage-volume3D LOD. MAC134 retains explicit LOD
operands in both compiler paths. The HLSL inventory does not establish which
instructions the original proprietary compiler emitted in every cached PCFX
variant: automatic gradient lowering or subroutines cannot be ruled out from
source alone. MojoShader supports TEXLDD and emits separate CALL/LABEL
subroutines, while its own source notes Metal subroutine-state limitations.
Those remain shader-translation/capability boundaries; they must fail shader
creation or an unsupported nondefault draw state, rather than publish a
substitute texture or fake successful playback/rendering.

Anisotropic explicit-gradient bias is an acknowledged adapter limit. Isotropic
bias scaling has focused GPU coverage; no general anisotropic equivalence is
claimed. Opaque helper sampling with nondefault bias or MaxLOD state is also
rejected explicitly. Current authored source shaders do not hit these limits;
actual PCFX variant coverage requires actual loading/execution evidence.

## Required initialization boundary fixed by MAC140

`FX::System::Initialise` previously assigned the required3D default wrapper
before checking allocation, ignored a failed3D map and unconditionally called
Unlock. `GetDefaultVertexTextureFor` could subsequently expose its missing
backing. This is incomplete required initialization, distinct from an
unsupported shader operation. The native branch now stages required2D-white
and3D-zero resources locally, checks actual creation and maps, and publishes
both only after success. Local deleters retire actual resources on false or
exception paths. Native shutdown tolerates absent/repeated teardown; duplicate
live initialization fails without replacing owners. The public void API
propagates real failure by exception. `FX::Initialise` uses the existing tracker
RAII scope so that failure unwinds tracking too. Legacy platforms are preserved.

The real FX fixture exercises actual creation failure using an upper device
owner whose Metal device is not created; it verifies no published globals,
tracker restoration, repeated shutdown and a retry after actual device creation.
It checks both initialized backing maps, two tracked owners, actual white
sampling through a file-loaded FX shader, duplicate-live rejection and complete
retirement/reinitialization. This is not fault injection of a late3D allocation
or map failure; those paths have ownership structure and source probes, with
no claim of a runtime late-failure differential.

Debug and DebugUBSan full FX fixtures exit0 under strict Metal validation;
all four configurations compile the changed source and fixture. Private logs
are `_work/build/macos/occlusion-agent-fx-defaults/{config}/runtime.log`.

Default vertex-texture **dimension selection** is separate from these resource
creation invariants. The existing generic conversion of every missing vertex
texture to the3D volume is under MAC141's actual-binding audit. MAC140 neither
changes that policy nor treats an allocated3D volume as an appropriate2D
texture.

## Concrete failure-only gaps repaired by MAC144

- `GameWorld/PostProcessLayer.cpp::Resize` ignores `CreateVolatile`'s result,
  although initial Create checks it. A failed resize can therefore lose the
  selected required graph without reporting completion failure at that caller.
- `render/source/postprocess/Manager.cpp::OnCreateVolatile` still wraps actual
  potentially throwing target/effect acquisition in manual tracker Push/Pop.
  The lower renderer transaction can retire GPU owners, while this manual scope
  remains unbalanced after an exception.
- `vehicle/source/customisation/PaintEditor.cpp::CreatePaintTextures` returns
  allocation failure, but its failure path pops a tracker context whose matching
  push is commented out. The native World already owns partial paint teardown;
  that does not make the erroneous context pop valid.

MAC144 repairs these three native paths while preserving legacy platforms.
Resize throws the actual required failure after I_Renderer has performed its
existing rollback. Manager volatile acquisition uses tracker RAII, resets only
borrowed cached/active references during actual owner retirement, and rejects
Begin unless both creation stages are ready. The unmatched native paint pop
is removed; its actual allocation bool and World-owned teardown stay intact.

The actual original-PAK graph fixture adds an I_PostProcess callback which
acquires a real GPU texture and then returns false or throws an authored error.
It verifies resize propagation, actual owner rollback, absent volatile readiness
and targets, tracker restoration and rejected retired-graph rendering. It then
retries creation and draws the actual HDR/Bloom graph again after each failure.
An uncreated real device exercises paint allocation false without changing
tracker context, followed by actual-device allocation retry and retirement.
Debug strictMetal full fixture exits0; all three changed production objects and
fixture compile in four configurations. This remains a failure-boundary test,
not evidence that the real race encountered these allocation failures or that
its enabled motion/prepass graph has completed.

Remaining PC-only offsets in deferred DynamicLights/TiledVolumeSpotlights are
half-texel raster conventions. They should be compared against the native
shader/raster pixel contract with real pixel evidence, rather than enabled by
blindly broadening PLATFORM_PC. Ocean's unchecked vertex/index buffer maps
also exist, but this audit found no GameWorld instantiation proving that the
legacy Ocean renderer is selected for this Brighton slice; it is not promoted
to a current blocker.
