# Reversible display-window MSAA policy

MAC359. This is an opt-in image-quality path using the original PC render-window
mechanism. The library default remains authored, which is currently no MSAA on
native. It does not add temporal accumulation, sharpening, grading, or a new
lighting algorithm. Implementation alone does not establish improved moving
image quality, acceptable cost, or complete-game compatibility.

## Source authority and selected graph

The original D3D11 `C_DeviceParams` constructor in
[D3D11GameDevice.cpp](../../code/mercury/library/graphics/display/source/device/d3d11/D3D11GameDevice.cpp)
selects a single-sample backbuffer and a 2x display window. Native
[MetalGameDevice.h](../../code/mercury/library/graphics/display/include/display/device/metal/MetalGameDevice.h)
previously and still defaults both fields to `E_Multisample_None`. No resolution
or sample-count default was raised by this task.

The selected Amax main path schedules `SetWindow(viewIndex, true)` in
[View/Main.cpp](../../code/amax/game/Game/GameWorld/View/Main.cpp). The framework's
PC/native `RenderSplitscreenViewWithAA` renders into that window, creates its
stencil mask, draws remaining scene passes and ends the window. The later
`RenderSplitscreenViewWithoutAA` restores its resolved colour into the postprocess
chain. The names of those callbacks alone do not prove MSAA was active.

Both normal/R32F-depth and lighting setups in
[Prepass.cpp](../../code/amax/game/Graphics/Lighting/Prepass.cpp) remain explicitly
single-sample. The game postprocess views in
[PostProcessLayer.cpp](../../code/amax/game/Game/GameWorld/PostProcessLayer.cpp)
also remain single-sample. These were already separate from the original PC
multisampled material/window path; they are not globally converted to MSAA.

## Configuration and publication

`BLUR_MAC_WINDOW_MSAA` accepts exactly:

| Setting | Selection |
| --- | --- |
| Unset or `authored` | Preserve the caller's authored window sample field; the native default is None. |
| `2` | Request 2x display-window sampling. |
| `4` | Request 4x display-window sampling. |

Empty strings, whitespace, `0`, `1`, `8`, suffixes and other values are rejected.
An explicit unsupported request fails with an actionable native device error;
there is no quality fallback. This is startup/creation configuration, not an
environment-variable hot-reload feature.

The pure parser lives in
[MetalWindowMSAAPolicy.h](../../code/mercury/library/graphics/display/include/display/device/metal/MetalWindowMSAAPolicy.h).
It is applied only at native device/volatile creation in
[MetalGameDevice.cpp](../../code/mercury/library/graphics/display/source/device/metal/MetalGameDevice.cpp),
not in static constructors. Shared `C_Device::Create` and `CreateVolatile` copy
selected parameters back to the caller only on success. Thus the application's
existing getters observe the actual selected window count even if its parameter
object was constructed before the process setting was established.

Only `m_WindowBufferMultisample` is selected. Backbuffer `m_Multisample`,
resolution, deferred/prepass samples, postprocess samples, assets and user-data
files are unchanged. Existing mirror/reflection sizing reads the application's
window sample count and can inherit it by the original PC policy; their actual
render/blur resolves must also be validated. “Window-only parameter” does not
mean every other view necessarily allocates identical storage to before.

## Capability and resource ownership

The narrow const `C_MetalDevice::SupportsMultisample` API queries the live Metal
device for the requested sample count and the same combined store/resolve family
support required by `CreateSurface`. It returns false before device creation or
for invalid counts. It allocates no texture or buffer and retains the actual
Metal device only for the synchronous query. Callers must serialize device
creation/destruction as for existing device methods; no new concurrent-lifecycle
guarantee is introduced. Per-format validation remains in resource creation.

Malformed and unsupported window requests are rejected before
`DestroyDeviceVolatile` and before new framebuffer/window texture allocation.
An existing live window is not retired by these early failures. This is not a
claim that all later GPU allocation failures preserve an old volatile graph.
The shared caller's requested params are not replaced with fallback values on
failure. Native shared `C_Device::CreateVolatile` restores its previous internal
params on false or exception, committing new metadata only after successful
creation/copyback. This keeps an early-rejected live graph's authoritative
settings intact. It does not reconstruct an old GPU graph after a later
destructive allocation failure; native creation retains responsibility for
retiring any partial new resources.

[MetalGameWindows.cpp](../../code/mercury/library/graphics/display/source/device/metal/MetalGameWindows.cpp)
already creates matched colour, depth/stencil and mask resources from one window
sample count. Native colour/MSAA render textures have single-sample resolves;
encoder boundaries preserve multisample samples with combined store/resolve.
The window mask pass uses the matching multisample stencil, then resolves the
mask for effects. RestoreWindow consumes resolved colour. Existing ownership,
resource waits, strict sample validation and resolve paths remain in place.

Full-graph risks to test are late-pass attachment compatibility, stencil/mask
coverage, alpha-to-coverage materials, mirror/reflection blur, postprocess depth
consumers, resize and partial-creation/teardown. Sampling more coverage increases
GPU storage and work. It does not by itself solve texture/specular aliasing or
provide temporal history, and no performance gain is expected from this policy.

## Why dormant EdgeAntiAliasing is not enabled here

`C_EdgeAntiAliasing` exists in the render library but is not instantiated by the
selected Amax graph. Its default-true debug flag therefore does not activate it.
The selected graph constructs motion blur, depth of field, HDR and bloom/ghost.

The dormant implementation binds depth to sampler 1 and then overwrites sampler
1 with normal/null, while its shader declares depth at 1 and normals at 2. The
Jam/generated variant metadata also gives `DepthNormal` no `NORMALS` define.
The depth/normal encodings and distinct sampled/output texture ownership would
need explicit validation before reuse. It is not a safe one-flag alternative.
This task makes no edits to that class or its shader.

## Verification boundary

The authored CPU policy test covers strict grammar, exact 2x/4x selection,
authored-value preservation and unchanged output on rejection. It needs only the
Display include root and the standard C++ library.

The existing `game_windows_test` keeps its shader-directory argument and actual
upper/lower GPU closure. Its extension checks real capability queries,
single-sample backbuffer with 4x colour/depth/mask windows, successful caller
copyback, malformed/count rejection before live owners are retired, subsequent
rendering and recovery. A separate late-failure case keeps a valid 4x policy but
requests D16 window depth: the real path allocates framebuffer resources before
`CreateWindows` rejects the missing stencil. The fixture requires false and a
non-renderable graph, retired partial framebuffer/window owners, restored
internal params, an unchanged caller request, then a real D24S8 retry and
colour/mask/composition readback. It does not inject a fake allocation failure
or claim to exercise GPU out-of-memory. Existing 2x/4x resolve/composition/mask
checks remain.
The geometric-coverage extension reuses the existing authored vertex-colour
shaders for a large asymmetric subpixel triangle in a 32x32 upper display window,
then restores it into a same-size region of the single-sample backbuffer. The
1x control must remain binary black/white; 2x must produce some fractional
equal-RGB edge coverage. Analytically interior/exterior unit-square pixels must
remain exact white/black in both modes. No driver sample positions or tuned
fractional-pixel count are assumed. The binary 1x control prevents treating
restore/upscale blur as MSAA. This is authored geometric coverage evidence, not
a claim about every edge, temporal stability, retail scenes or performance.
These tests cannot substitute for the actual selected Amax lighting/effects
graph or a controlled moving-image comparison. Root owns coherent builds,
serialized muted hardware checks, actual comparisons and any later game-only
default selection. No hardware/runtime acceptance is claimed at this freeze.

## MAC380 final qualification

Both complete final155-test suites pass. Six focused lower UBSan checks and14 strict Metal checks on the unchanged renderer/fixtures pass. Actual377/378 verify the complete original race graph, driving and ordinary menu/desktop exit with2x. Their measured GPU/frame/memory cost is higher than the authored-sampling rows; final380 therefore keeps authored window sampling by default, with2x/4x explicit optional modes. Final381 qualifies the compiled default through actual driving and normal exit0. Earlier pending-root paragraphs are historical. This does not establish4x performance, every track/effect, temporal stability or a universal FPS improvement. See [full acceptance](faithful-remaster-acceptance.md).
