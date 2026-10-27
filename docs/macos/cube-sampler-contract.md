# Original PC cube/2D sampler contract (MAC181)

Actual optimized MAC180 completes a Brighton/BMW frame, passes the first split
and drives roughly1km with ordinary keyboard input, then fails strict shader
validation: pixel slot8, shader387, expected type5 (cube), actual type3 (2D).
Error cleanup exits1; this is neither natural completion nor successful exit0.
The failure establishes the actual dimensions. The effect/sampler name and
whether this particular source is the generic FX default were not captured;
this repair does not guess those identities or classify a missing asset optional.

## Reuse the recovered PC mechanism

The existing Windows port already recovers exactly this class:
`code/mercury/library/graphics/fx/source/NativeFX11.cpp:453–460` classifies an
FX cube resource by reflected effect-variable type and maps a bound2D source to
an unbound native SRV. Its source comment and
`docs/blur1pcdx11/analysis.md:382–390` record direct D3D9 hardware measurements:
cube sampling a2D texture returns RGBA(0,0,0,0), including white/nonzero2D texels,
in all six directions; valid cubes return their actual texels. A null D3D9 cube
returns(0,0,0,1), a different case. The permanent reference regression is
`tools/dx11_probe/cube_binding_checks.h`; the recorded authority is67 original-PC
and54 native hardware/WARP checks. No Windows executable/reference GPU test was
rerun on this Mac. This is reuse of an existing measured PC adapter contract,
not an invented white-cube default.

## Native adaptation and ownership

Both native shader stages now expose the same public `GetTextureType(slot)`
reflection query. FX `Context::SetTexture` and `SetVertexTexture` test that
actual reflected dimension and the real source wrapper's public GetType.
Only expectedCube + actual2D selects the required zero-RGBA cube supplied by
MAC185's `FX::GetPC2DToCubeTexture`. The borrowed native GPU handle already is a
public Display2::C_Texture wrapper; no Metal implementation detail or pointer
registry is exposed to FX.

This applies to generic white defaults and explicit nonzero2D inputs alike,
matching the recovered policy. Explicit cubes retain their original storage and
six distinct face texels. Other mismatches still reach strict lower validation;
explicit null remains rejected rather than silently conflated with2D. The
existing vertex3D default-zero policy and normal2D white default remain intact.
Metal's raw dimension/missing-texture guards are unchanged.

MAC185 separately owns required zeroCube creation, six initialized faces,
transactional publication/rollback and retirement alongside the original FX
white2D/zero3D owners. MAC181 adds no independent cache or hidden lifetime owner.
The zeroCube is a representation of the recovered sample result, not a fallback
for an arbitrary missing resource. No serialized sampler, shader, material or
original Windows data layout changes.

## Pass selection, cache and binding-time snapshots

RenderingData can bind shared groups before the selected pass/shader, as actual
MAC146 demonstrated. A register-current bit alone does not certify a
shader-dimension-dependent native materialization. Normal/new-pass BeginPass
now rematerializes both stages' used shared texture records after Prefetch and
local bindings. The existing same-pass conflict handling remains intact.

The cached conflict-free path retains MAC146 vertex rematerialization and the
original BindSharedParameters dirty/mask handling, but **does not** rebind
unchanged shared pixel textures. Original PC early return does not refresh that
binding. Rebinding it on every cached pass would incorrectly refresh MAC174's
binding-time active-colour snapshot. No per-draw refresh or lower colour-feedback
relaxation is introduced. Cached local immediates remain untouched until a real
binding conflict requires the existing full pass path.

## Focused actual FX/GPU evidence

The existing no-argument `tools/macos/vertex_fx_texture_test.cpp` retains all
MAC143/146 tests and now exercises actual authored FX loading, reflection,
local/shared/immediate bindings and GPU readback for both sampler stages:

- Generic white2D and explicit nonzero2D to cube return exact zero RGBA in all
  six directions. The fixture explicitly enables all four colour writes so
  alpha0 is checked rather than hidden by the PC default RGB-only mask.
- Valid cube faces preserve six independent RGBA texels; cube→2D returns the
  real2D texels instead of retaining an adapted resource.
- Shared node bindings before any selected shader and after a preceding2D
  shader are rematerialized for the actual cube pass.
- Shared cached values, foreign-node conflicts and local immediate preservation
  retain their original state contract.
- Cube expected with explicit3D,2D expected with explicitCube, and explicit null
  still fail strict dimension/missing-texture checks; a subsequent valid binding
  renders correctly.
- An actual shared2D FX sampler snapshots the active transparent-black target.
  After the attachment is cleared white, an unchanged cached pass still draws
  the original black snapshot. Explicit rebinding captures fresh white.

Changed FxContext/MetalShaders and the final fixture compile in all4 native
configurations (Debug, Speed, DebugUBSan, SpeedUBSan). Private Debug linking uses
a stable copied8-archive FX closure plus the changed original objects, including
MAC185's required owner. Final strict Metal GPU execution and complete static
teardown exit0. No window/service/game assets are needed. Logs are private under
`_work/build/macos/physics-agent-cube-fx-<configuration>/`.

Two isolated failing controls establish the cache checks: omitting normal-pass
pixel rematerialization reproduces pixel slot8 expected5/actual3; restoring
blanket cached pixel refresh fails the active-colour snapshot assertion. Both
negative executables print their primary, then exception unwinding hits the
original tracked-resource destructor assertion (signal5). They are expected
failing differentials, not clean baseline shutdowns. Their private directories
end `-pre-normal-pixel-fix` and `-pre-cached-pixel-fix`.

Parent MAC182 owns coherent Debug/Speed affected suites and the next actual game
run. MAC181's authored GPU checks do not establish the actual missing sampler's
name, full track traversal, a natural one-lap finish or normal full-game exit0.
