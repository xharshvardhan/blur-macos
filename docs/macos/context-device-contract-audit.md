# Native context and device contract audit (MAC178)

Initial source audit, 27 October 2026, after the MAC173 water feedback proof and MAC174 pixel texture routing fix. It reads the selected native context, original PC `Context_D3D11` overrides, inherited context, D3D11 bridge/device, and their actual FX/scenegraph/game consumers. That initial audit performed no production changes, shared archive reads, builds or runtimes. A subsequently authorized mixed-geometry regression reproduced and repaired finding2; its bounded validation is recorded below. Neither part establishes a new actual-game failure or a completed race.

## Findings requiring a focused follow-up

| Priority | Source-supported difference | Reachability and next evidence |
| --- | --- | --- |
| 1 | Native context surface publication changes physical attachments during scheduling, although the PC base setter is empty. It also bypasses the device's borrowed target cache and does not clear MRT1–3. | Both `RenderTask.cpp:95` and `:424` call the setter after `CloneState(NULL)` while scheduling. Compare physical targets, device `GetRenderTarget()` and viewport across a real schedule/execute boundary before choosing a repair. Preserve actual window/prepass/reduced-render owners; do not blindly call the device setter during scheduling. |
| 2 (repaired) | Native inherited vertex/index methods replayed wrapper pointers; PC overrides preserve the device's current bindings. Native five-argument lower `SetVertexBuffer` unconditionally binds its supplied declaration, including null. | The authorized real GPU regression reproduced seven wrong-output/rejected-draw cases, with direct Device and explicit recovery controls passing. Native VB/indexed methods now use preserving overloads. Four private configurations pass; coherent game integration remains root-owned. No failing mixed sequence was captured in the actual game. |
| 3 | Native context `GetView()` returns its scheduled local copy; PC returns the device's current view. | Particle `BindPixelConstants` uses it for inverse viewport size. Exercise a reduced-alpha pass whose actual viewport differs from the scheduled full view and compare the original PC owner sequence. This is a proven accessor difference, not yet proof of an incorrect particle constant. |
| 4 | Native inherited context depth/fill setters forward raw `Default` sentinels; the PC bridge passes them through the device resolver. Context cull aliases Front/Back also differ from device handling. | The selected explicit Default depth/fill and Front/Back calls found in this audit use **Device**, which already resolves them. No selected context call with those sentinels was found. Record the API gap; do not weaken lower validation or change context-local cull policy speculatively. |

### Attachment publication belongs to the actual render owner

`display/include/display/context/Context_Base.h:28` binds native colour0 and depth directly. This setter is empty for the original PC path. `MetalGameDevice.cpp:188`, in contrast, updates `m_RenderTarget`, clears absent MRT1–3 and sets the colour-size viewport. These are different operations, so replacing one with the other is not automatically correct.

`scenegraph/source/RenderTask.cpp` publishes the group's surface settings during **Schedule**. `C_RenderGroup::Render` at `:478` flushes tasks and republishes multisample/texture metadata before immediate execution; it does not bind physical attachments. `scenegraph/source/Context.cpp:175` then clones the group's contexts for execution. This corrects an earlier preliminary hypothesis that RenderGroup itself binds the PC targets.

Actual owners bind targets independently: `gamesys/framework/source/application/ApplicationBase.cpp:1553` selects the display window/view before the two render messages; `amax/game/Game/GameWorld/View/Main.cpp:1148` enters the lighting prepass owner; `graphics/deferred/source/manager.cpp:491` selects every native MRT/depth slot; main reduced-alpha rendering calls its reduced-render manager before `RenderGroup` (`Main.cpp:1037` onward). Mirror/reflection bind their view owners (`View/Mirror.cpp:232`, `View/Reflection.cpp:375`). Scheduling must not substitute for those execution-time owners.

The borrowed device cache matters: selected consumers include deferred lighting, reduced rendering, HDR, sun rays and depth copies (`GetRenderTarget()` callsites). The current source difference proves that native context publication can disagree with this cache. It does not prove that this happens at the failing water draw: MAC173 identified the actual lower target and MAC174 repaired its texture-binding route independently.

### Geometry binding and ownership

The PC overrides in `Context_D3D11.cpp:10–23` delegate declaration, VB and index binding to the device. The base class cached pointers remain unused by those overrides. Before MAC178, native inherited `Context_Base.cpp:45–91`: it saved declaration/index wrapper pointers, reasserted the declaration with every VB bind and passed the cached optional index wrapper on every indexed draw. `MetalGameContext.cpp:22` also copies those pointers when cloning.

Lower `MetalContext.mm:546` always calls `SetVertexDeclaration(declaration)`. Lower optional-index draw at `:652` only changes the binding when the pointer is nonnull; this latter behavior already matched the PC bridge. An old **nonnull** cached index could override an intervening device bind. The lower backend retains actual bound backing storage through shared ownership, while these upper cached wrapper pointers remain borrowed. The repair stops replaying those pointers; it retains existing clone metadata and device-generation checks without changing public layout.

## Complete setter inventory

Paths below are under `code/mercury/library/graphics/` unless stated otherwise.

| Native context API group | Original PC / current native result |
| --- | --- |
| Pixel `SetTexture` | MAC174 now delegates to native Device, matching `Context_D3D11::SetTexture → SetNativeTexture`. Existing binding-time snapshot copies actual prior contents of active colour targets. MAC174 real GPU tests cover Device/Context routes, MRT1–3, cube faces/mips/gamma, MSAA resolve, source retirement and explicit rebind. |
| Vertex `SetVertexTexture` | Separate raw vertex-stage bind matches PC `SetNativeVertexTexture`'s separate path. There is no PC colour-feedback snapshot in that vertex binder. Do not reuse the pixel adapter without a source contract. Native FX's dimension-aware declared-3D fallback was separately fixed in MAC143; explicit wrong-dimensional textures still fail. |
| Depth / late-target texture alias | No invented depth snapshot. Binding a texture before it later becomes a target does not retroactively take a snapshot. Raw lower sampling still rejects active attachment aliases; MAC174 tests explicit context rebind recovery and depth rejection. This strict behavior remains intentional. |
| Pixel/vertex full sampler state | Native passes all base fields and validated per-slot anisotropy to the matching stage. PC bridge decomposes fields through device methods. Native `C_SamplerState` initializes anisotropy1 and base defaults. MAC134 applies actual per-stage LOD state through shared source/legacy shader ABI; unsupported opaque explicit-LOD/mipNone and biased anisotropic-gradient cases remain truthful errors. |
| Gamma after texture binding | Native changes sampler view selection against the retained bound texture/snapshot. PC `SetTextureGamma` retrieves the currently bound SRV's resource before rebinding, so it also retains an existing snapshot rather than recopying its original target. PC force-gamma override is absent natively, but no selected caller of that PC-only override was found. |
| Individual sampler Default setters | Outside context's full-state route, native Device single-axis address/filter setters use raw conversion; its triple setters resolve Default. Selected Default callers found use the corrected triple API. This is a bounded public API gap, not evidence of an actual reached failure. |
| Declaration / VB / index / indexed draw | Finding2 was reproduced and repaired below. Non-indexed draw immediately uses actual bound native storage. Strict draw completeness/bounds guards remain necessary. |
| VS / PS binding | Native lower setters immediately select shader backing, including explicit null unbind; they do not skip a same-pointer call. PC additionally flushes its separate NativeFX11 layer, updates SM5 register maps and rebinds input layouts. Native's source-BMSL path has no second FX11 layer; copying that Windows invalidation machinery is not justified. Missing required created shaders are owner initialization failures (MAC162), not a reason for fallback bindings. |
| VS / PS register constants | Immediate matching-stage register updates. Native full reflected parameter spans are intentional. The PC override's single-register parameter upload is not an instruction to truncate native matrices/arrays; the original lower bridge itself supports reflected full spans. A missing native handle has count0 and is a no-op; malformed nonzero spans still reject. |
| `SetPixelShaderParameters` | Same inherited non-PS3 no-op; not evidence for new native patching behavior. |
| Scalar / MRT colour masks | MAC150 routes both through native Device Default policy. Lower masks remain literal0–15 only. Actual MAC155 UINT_MAX Default proof and GPU cases already establish this route. |
| Blend enable / factors / operations | Native applies complete lower states. PC bridge also updates upper device caches. No native same-mode redundant-state skip was found: device blend-mode selection deliberately invalidates its cache then applies the table. The CPU-cache difference is real but no selected consumer requiring the stale value was found. |
| Context `SetBlendMode` | Keep the original shared context table, including its distinct Default/Unknown behavior. It is not interchangeable with the Device Unknown cache-invalidation sentinel. Existing blend fixture covers that distinction; SolidAlphaMask remains unsupported explicitly. |
| Alpha test state / function | Immediate native shader-integrated state. Original context state helper sets Greater plus ref; native preserves that contract. No Default caller was identified here. |
| Depth enable / write / compare / fill | Literal states forward correctly; Default forwarding gap is follow-up4. Attachment-conditioned effective depth/stencil is already MAC128: cached requests remain intact when a legal colour-only pass has no attachment. |
| Cull | Context Default/Reversed intentionally derives from context-local `m_CullMode`; this is used by scenegraph groups. Native Device additionally resolves Front/Back and global flip. Do not route every context cull request through global default policy. No selected context Front/Back use was found. |
| Stencil | Native sends enable and all function/op/ref/read/write fields. PC bridge updates the same requested operations through Device. Missing attachment effective behavior is MAC128; explicit depth/stencil clear validation remains strict. |
| Blend high precision | Valid boolean only; floating-point target format carries native precision. PC override also ignores this toggle. |
| Alpha-to-mask / point sprites | Native rejects enabled requests instead of silently claiming implementation. The inherited PC alpha-to-mask helper is empty; a future actual capability requirement needs dedicated coverage, not an adapter bypass. |
| Surface / colour-depth-stencil texture metadata / window index / multisample | Follow-up1 for native physical surface side effects. Texture/stencil metadata remains borrowed, multisample is metadata, window-index setter is a shared no-op. No ownership transfer or invented resource creation occurs. |
| View / CloneState / InvalidateState | Follow-ups2–3. Native copies local view and borrowed caches; PC GetView resolves actual Device. Both immediate PC and native have no separate display redundant-state cache in InvalidateState. Scenegraph's own render-list invalidation is separate. |
| Dynamic Begin/EndVertices | Native intentionally delegates to the actual original Device dynamic buffer owner; it rejects an unbound context. The PC Context_D3D11 stub returns null, whereas selected native geometry was ported using the recovered original PC Device map/triangle/unmap path (MAC156). Do not restore a null stub. |
| Begin/EndCommandBuffer | Native nonnull deferred command buffers explicitly reject; immediate null is supported. The selected PC/native scheduling policy uses immediate render execution. Supporting worker-recorded command buffers would require a separate ownership implementation. |
| Sampler usage flags / predication | Shared inherited no-ops, including original PC. This audit supplies no semantics to invent. |

## Validation boundary

The initial source inventory itself was not a GPU validation matrix. MAC174's four private standard GPU closures previously exited0 under strict Metal, including UBSan configurations. The coherent MAC177 app integration was still building during that initial source audit; no archive was read and no game or fixture was launched by this agent during that audit.

## Authorized mixed-geometry reproduction and repair

The existing `tools/macos/game_context_test.cpp` was extended without a new target or argument. Distinct left/right rectangles encode declaration offsets and index ranges, and every result compares all256 actual GPU pixels. The original PC contract requires preserving the right-hand Device binding after the context changes only a VB or submits an indexed draw.

The private Debug baseline under `MTL_DEBUG_LAYER=1` exited1 cleanly:

| Case | Before correction | After correction, all four configurations |
| --- | --- | --- |
| Direct Device non-indexed/indexed controls | Right128 / left0, PASS | PASS |
| Context declaration A → Device declaration B → Context VB | Left128 / right0, FAIL | Right128 / left0, PASS |
| Same declaration sequence through cloned context | Left128 / right0, FAIL | Right128 / left0, PASS |
| Null cached declaration → Device B → Context VB | Incomplete draw state, FAIL | Right128 / left0, PASS |
| Retired cached declaration A → Device B → Context VB | Incomplete draw state, FAIL | Right128 / left0, PASS |
| Context index A → Device index B → Context indexed draw | Left128 / right0, FAIL | Right128 / left0, PASS |
| Same index sequence through cloned context | Left128 / right0, FAIL | Right128 / left0, PASS |
| Retired cached index A → Device index B → Context indexed draw | Index buffer is unbound, FAIL | Right128 / left0, PASS |
| Recreated owner and explicit context rebind recovery | Right128 / left0, PASS | PASS |

Root approved exactly two native-only corrections in `Context_Base.cpp`: VB binding uses the lower four-argument overload; indexed drawing uses the six-argument overload. Both retain `m_Context` ownership/generation checks. Windows/console paths, lower strict validation, cloned metadata, targets, samplers, views and Default policy are unchanged.

Eight actual production/fixture object probes and four private standard `blur_mac_game_context_test` links pass. All four private GPU executions exit0 with strict Metal; UBSan configurations use `halt_on_error=1:print_stacktrace=1`. Existing context registry, parameter uploads, dynamic ranges/rotation/budgets, mapped teardown and stale device recreation checks also pass. Logs are under `_work/build/macos/occlusion-agent-context-bindings/{macos,macos-release,macos-ubsan,macos-speed-ubsan}` (`runtime-before.log` in Debug; `runtime-after.log` in each). Source and fixture are frozen for root's coherent integration. No actual game was launched by this agent and no completed race is claimed.
