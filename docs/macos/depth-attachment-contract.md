# PC depth/stencil attachment contract (MAC128)

The actual MAC127 Brighton/BMW run emitted RaceStarted and RaceLoaded, then the native adapter rejected `DepthEnable=true` with no depth attachment. MAC129 captured the primary failure before unwind: `Deferred::C_Light::RenderBox` → `RenderSpotlightVolumeSimple` → `C_LightingPrepass::RenderSpotLights` / `RenderLights1` → `C_MainView::RenderPrepass1`. The requested state was enable1/write0/comparison2, with the actual depth pointer null. Evidence is private `verification/mac129-game.log` from line278; this diagnosis did not prove a completed frame or clean game exit.

## Recovered contract

[D3D9 SetDepthStencilSurface](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-setdepthstencilsurface) documents that NULL disables depth/stencil operations. The [D3D11 Output-Merger overview](https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-output-merger-stage) specifies that an absent depth buffer makes the depth test pass, and an absent stencil component or depth buffer makes the stencil test pass. A requested enable flag therefore does not require an attachment to exist for every draw.

The recovered Windows backend preserves these independently: `display/source/device/d3d11/D3D11GameDeviceNative.cpp` lines1723–1727 stores the nullable depth surface and rebinds render targets; line593 passes the nullable view to `OMSetRenderTargets`. Its lines1812–1815 update requested depth enable separately. Original `render/source/texture/TextureUtils.cpp` lines62/89/114 and `render/source/effects/perlin/Perlin.cpp` line139 intentionally bind no depth without rewriting depth enable. The actual light pass is also a PC path: `amax/game/Graphics/Lighting/Prepass.cpp` lines263–268 selects sampled depth/normal textures, then `RenderLights1` begins the lighting output and renders spotlights without the console-only stencil-mask setup.

The native change in `display2/source/device/metal/MetalContext.mm` derives effective depth enable from the bound depth surface and the requested flag, and effective stencil enable from the bound stencil aspect and its requested flag. Missing effective tests use Metal's always-pass/no-write state. Cached requests remain intact, so attaching D16 or D24S8 reactivates the corresponding tests and writes. [Apple's depth comparison documentation](https://developer.apple.com/documentation/metal/mtldepthstencildescriptor/depthcomparefunction) defines Always as passing; [the pipeline attachment documentation](https://developer.apple.com/documentation/metal/mtlrenderpipelinedescriptor/depthattachmentpixelformat) defines the default format as Invalid. The existing native pipeline already selects Invalid when no attachment is bound.

This changes draw-state translation only. Existing validation of an attached surface's real device, format, size and sample count, and explicit clears of missing depth/stencil aspects, remain in place. Windows code, GPU resources, depth projection/default policy, shaders and simulation cadence are unchanged. The separate MAC126 PC reversed-depth default is not part of this change.

## Focused verification

`tools/macos/depth_attachment_test.cpp` needs no arguments or original assets and links the same `blur_mac_display2_metal` closure as `stencil_test`. It creates real D16, D24S8 and colour surfaces and executes two detach/rebind cycles. GPU colour and depth readbacks verify:

- Requested Never depth/stencil tests pass with no attachment; D16 restores depth rejection and D24S8 restores stencil rejection without reissuing the enable flags.
- D16 ignores the requested stencil comparison because it has no stencil aspect; rebinding D24S8 restores that comparison.
- A detached draw leaves formerly bound depth bytes and stencil contents unchanged. Rebinding restores requested depth writes.
- Explicit missing depth/stencil clears, including a stencil clear against D16, still fail.

Detached depth storage is compared byte-for-byte against its actual pre-draw contents. Independent .25/.5 D16 value checks allow one UNORM unit for the device's clear/render quantization. There are no fallback resources or state-inspection substitutes for GPU output.

Private Debug, Speed, DebugUBSan and SpeedUBSan builds and GPU executions all pass with Metal API validation enabled; UBSan configurations halt on the first error. The exact prior MetalContext reconstructed privately fails this same fixture safely with exit1 at the missing-stencil draw guard. The shared actual game rerun remains a separate integration and gameplay gate.
