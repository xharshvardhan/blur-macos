# Normal career stencil-clear boundary (MAC209)

A bounded owned Debug197 LLDB run reproduced the normal-career render failure
before exception unwinding. This is diagnostic evidence for MAC208, not a game
completion/clean-exit test.

## Proven actual path

The fresh private app was copied from the frozen197 Debug app, with its binary
SHA256 checked against mac197-integration-proof.json. It used the independent
Master data/profile/save189 roots, CompleteCareer=0 and ObserveOnly=1. The existing
StartScreen-gated frontend operator used ordinary finite keys to navigate actual
profile reload, hub/tier/event/pre-race/garage and accept an owned/allowed car.
It observed career key4013244786 resolving to unlocked gameplay event4597671.
No setup/event/route/car/progress/unlock setter was dispatched by the operator.

The run stopped at the exact __cxa_throw whose MetalContext require message was
“Stencil clear has no stencil attachment”. The captured caller chain is:

```
Display2::C_Context::Clear
Display2::C_Device::Clear
Display::Metal::C_GameDevice::Clear
Deferred::C_Light::RenderStencilBox
Deferred::C_Light::RenderSpotlightVolume
C_LightingPrepass::RenderSpotLights
C_LightingPrepass::RenderLights1
Game::C_MainView::RenderPrepass1
Game::C_View::PreRenderSplitscreen
Game::C_GameWorld::PreRenderSplitscreen
```

The source guard is display2/source/device/metal/MetalContext.mm532; flags4 is
E_Clear_Stencil per MetalContext.h22. Before the guard throws, pure LLDB SBValue
reads record flags4,colour0,depth0,stencil0. Actual context depth shared_ptr has
both storage pointer/control block null. There is therefore no actual depth or
stencil attachment and no depth format to report; this is not evidence of D16
being substituted for D24S8. Colour0 is A8R8G8B8,2560x1440,sample1; other colour
slots are null. A real command buffer and render encoder are nonnull. Requested
stencilEnabled is false at this pre-clear instant; it does not remove the need
for a real attachment when explicitly clearing stencil.

This matches the source-owner policy candidate sent by MAC208: the native
prepass manager binds null for a lighting target with no owned depth, whereas
the original PC path retains the previously selected normal-pass depth surface.
MAC208 owns any repair and fixture;209 does not weaken the lower clear guard or
modify render targets/state/source.

## Capture and process limits

Ignored evidence is `_work/runtime/verification/mac209/stencil-proof.json`
(errors[]), source-proof.json, capture.commands/capture209.py, frontend-operator.log,
mac209-game.log and mac209-probe-status.json. The capture uses SB fields/memory
only; it does not invoke game methods to query or change state. It records all
available selected-thread frames up to32 and context/attachment fields.

At167.18s the diagnostic explicitly killed its owned inferior26484 using LLDB
process kill (status9), then quit LLDB. The wrapper returned0, slot_released=true;
exact private executable process listing showed no leftovers. The separate
operator exited when the owned game status became available. The harness marks
qualified_game_exit=false and its primary field is null because the debugger
stopped before the application caught/logged the exception. The exact primary
is established by the filtered throw and SB capture, not by that status field.
No PNG or successful normal race/exit claim is made. No shared build/archive or
production source was changed by this diagnostic.
