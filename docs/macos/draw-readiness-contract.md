# Selected native draw readiness audit (MAC162, read-only)

Actual coherent MAC160 passes upper-mask and dynamic-quad boundaries, then
reports Incomplete draw state after a Q pulse. Root MAC161 is capturing the
first missing attachment/shader/declaration and actual caller before unwind.
This source inventory does not identify that cause from the error string.
No production changes are made here.

The checked lower draw requires colour0, vertex shader, pixel shader and vertex
declaration. Encoder changes and BeginScene do not discard those CPU-held
objects. The immediate setters copy resource shared ownership, so deleting a
wrapper later does not itself turn a retained binding null. However, a nonnull
uncreated shader/declaration wrapper passes a null backing through the setter.
A null binding at draw therefore needs its actual setter/owned creation status
traced; it is not automatically evidence for a redundant-state cache bug.

Upper native Context/Device shader and declaration setters forward immediately;
no redundant same-pointer checks suppress them. Context SetVertexBuffer also
reasserts its stored declaration, whereas Device SetVertexBuffer changes just
the stream. Device BeginScene/EndScene Reset deliberately calls Unbind, clearing
all transient shader/declaration/stream bindings; original PC does the same.

The dynamic renderer starts each batch with no previous effect. The first
particle effect unconditionally binds material pixel shader, vertex shader and
its initialized declaration. Particle Manager shader setters directly bind its
actual array entries, without a shader skip cache. Subsequent same-effect nodes
reuse that established state. Dynamic renderer Unbind restores texture/cull/
blend state and does not clear shader bindings.

SplashCamera binds its declaration and each nonempty splash material branch
selects owned vertex and pixel shaders. It does not clear them at the tail.
Its required owned creation status must be checked if the actual stack points
there. Other selected explicit null shader clears occur in Device Reset/Unbind,
ImmediateDebugRender and Render::Font tails.

FX has a distinct same-effect/pass cache. A cache hit can omit shader selection
and update shared bindings; an unrelated direct draw requires the original
context-transition protocol. SceneGraph RenderTask clones child device contexts
and FX CloneState invalidates its effect/pass/shader cache. Direct dynamic
render sets the context flags and invalidates the scenegraph render-list-node
cache when switching context classes. These are real state contracts to follow,
not grounds for blanket shader restores or missing-resource substitution.
Actual161 field/caller evidence remains required before choosing a repair.

## Actual161 proof and repaired owner boundary

The before-unwind MAC161 capture confirms real colour/depth attachments and a
created declaration, but both lower vertex and pixel shared pointers are NULL.
The caller is DynamicEffect particle SpriteRotating through the original
T_DynamicVertexBuffer path. Its actual states value7 includes BindEffect, so the
manager's direct shader setters ran; this was not a skipped FX cache bind.

ParticleManager::Create returns CreateShaders. The complete original PC shader
initialization was guarded only by DEVICE_PCD3D11 or DEVICE_PS3GCM, while native
selects DEVICE_MACMETAL. Thus it skipped all actual shaders and returned true.
The native backend is now included in that same source-built initialization.
A native owner guard retires every partial shader on false or exception, and
true requires the complete selected original set:16 vertex variants and7 pixel
shaders. Two unused enum placeholders, TwoDimensionalRefract and
AlphaAndAdditive, have no initializer or reference beyond their declarations;
they remain absent rather than being filled with guessed shaders. Recreate
retains the original destroy/rebuild mechanism and now propagates native failure
instead of continuing after a false return. Windows and console behavior is
unchanged. Other dynamic manager shader creation paths are unconditional and
bool checked; no analogous native-exclusion guard was found there.

A separate source-supported same-family omission remains in SplashCamera's
geometry selector. Native now selects its already recovered retail T107 PC
six-vertex triangle writer instead of entering the console quad writer. Its
required shader creation already checks actual bool results and uses the common
renderer rollback contract; no replacement material or shader is added.

The existing no-argument particle_geometry fixture now also attempts actual
shader creation on an uncreated native device, then retries the same manager
with a real created device. Debug validates tracked16VS/7PS ownership and
recreation. It binds the manager's real source-built VS_UVW/PS_Default pair,
original declaration, mapped CPU particle geometry, actual texture and source
register constants, and checks independent rectangle/gap pixels. Destruction
returns tracked counts to baseline, and the process exits0 under Metal API
validation. All12 final source probes (two production TUs plus fixture across
four configurations) pass. Tracker count assertions are intentionally compiled
only where the engine provides its tracker; optimized builds still execute
real creation/readiness and GPU rendering paths. No late partial-allocation
fault injection or all-four runtime claim is made. Root's coherent actual game
continuation remains separate.

Logs: _work/build/macos/occlusion-agent-particle-shaders/{config} and
occlusion-agent-particle-geometry/macos/runtime-162.log. One intermediate fixture
attempt left its deliberately uncreated lower device selected before upper
creation; the fixture now explicitly retires that temporary selection. No
production behavior was changed for that fixture prerequisite.
