# Attachment feedback contract audit (MAC174)

Actual optimized MAC172 presents a complete gameplay image and moves the car,
then rejects attachment sampling. Root MAC173 captured exact stage, slot,
resource identity and caller before the production repair. The source contract
below does not make every feedback path legal.

## Upper route and binding-time authority

The recovered PC Context_D3D11.cpp43 routes upper Context::SetTexture through
Device()->SetNativeTexture, just as Device::SetTexture does. The PC native
pixel setter checks all four active colour resources and copies actual prior
contents to a shader-readable resource before binding. Its vertex counterpart
is separate and has no such snapshot. Native upper Device already uses
SetTextureWithColourSnapshot; native upper Context currently forwards to raw
lower SetTexture, bypassing the existing colour adapter. FX local/shared
parameter, dynamic and gamesys Context bindings all use this latter route.

The existing native adapter is binding-time, not automatic per-draw copying.
It freezes contents through repeated draws and refreshes them on explicit
rebinding. It checks all four colour attachments and their MSAA resolve aliases,
copies actual prior bytes for all faces/mips, preserves format/swizzle/sRGB and
flags, owns the snapshot through the slot and command lifetime, and ends/restarts
encoders without counting blits as visibility samples. It creates a bounded own
command when no scene is active. Existing real-GPU tests cover non-square
R32F/RGBA16F/BGRA8,1x/2x/4x samples, repeated/refreshed writes, all cube faces and
mips, source destruction, sRGB and no-scene copies.

Native GetSurface exposes the texture's existing shared Surface metadata for
that face/mip, so the selected render-texture owner paths preserve exact alias
identity. No extra alias-view constructor API was found in this inventory.

## Limits that remain distinct

Raw lower bindings continue to reject sampled active attachments. Depth feedback
has no colour-copy authority and must not be substituted with a colour snapshot.
Vertex sampling has no corresponding snapshot in the original PC backend, so it
must not be generalized speculatively. Binding a texture before subsequently
selecting it as a render target is a different late-alias order: target setters
do not resnapshot old slots. Correct explicit rebinding can take a new snapshot;
blind automatic copying at draw would change binding-time semantics.

The FX same-pass/immediate-parameter cache can skip redundant SetTexture calls.
An actual late target alias therefore needs the source pass/owner/cache order
traced, not a guard removal or default texture. Root MAC173 before-unwind proof
will distinguish that case from the proven upper Context adapter omission.

## Actual173 confirmation and final change

The capture has no debugger errors. Real water pixel shader622 binds
ColourBufferSampler at slot10 (alongside DepthBufferSampler11 and
ScreenReflectionSampler15). Its actual BGRA8 2560×1440 texture has surface
0x105940718, identical to active colour0; depth is a different surface. The draw
is an actual RenderingData static primitive through SceneGraph and
Game::C_View::RenderWithoutAA. This matches the upper Context adapter omission,
not a vertex/depth feedback request.

Native Context::SetTexture now forwards through the existing actual upper
Device::SetTexture policy, mirroring Context_D3D11's shared pixel route. Only
MetalGameContext.h/.cpp change. There is no new copy implementation, lower
guard change, alternate texture, draw skip or shader rewrite. Existing cache
semantics continue to freeze a snapshot through same-binding repeated draws;
explicit rebinding refreshes it. No automatic target-change copy is added.

The existing colour_feedback fixture now exercises both Device and upper
Context routes for all original format/sample cases and all cube faces/mips,
sRGB views, source destruction and no-scene copying. Additional actual GPU
cases sample nonzero MRT slots1..3, reject bind-before-target late aliases, then
recover via explicit rebind using newest contents. Active depth feedback and
raw lower pixel feedback still reject before sampling. The original
60/144Hz elapsed-time recurrence check remains unchanged. All resources retire
through actual owners; process/static teardown exits0.

All8 private source probes across Debug/Speed/DebugUBSan/SpeedUBSan pass. All
four standard fixture closures linked with the scoped replacement objects and
ran under Metal API validation, exit0; both UBSan variants use halt_on_error.
Logs are _work/build/macos/occlusion-agent-attachment/{configuration}. No CMake
or CLI change is required; the existing authored-shader-root argument remains.
Coherent actual game continuation after this repair remains a separate gate.
