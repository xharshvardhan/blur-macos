# MAC154 — Bounded remaining native startup contracts

Read-only audit scope: current original Brighton1/BMW preset2 startup, selected
Main/Mirror/Reflection/CubeMap and ReducedMotionBlur/DoF/HDR/Bloom graph. Read
slice-assumptions.md, startup-publication-audit.md, render-startup-boundaries.md
and ui-required-resources.md first. No game run or shared build was part of the
audit. MAC151's actual colour policy, MAC152's world/UI light borrower and the
new particle QuadList diagnosis belong to their assigned owners and are not
repeated here. Parent approved the first two grouped repairs here; root subsequently repaired the final view handoff (MAC158) and required world texture failure guards (MAC159).

| Remaining class | Concrete source and caller contract | Bounded action / evidence needed |
| --- | --- | --- |
| Required configuration failure does not stop its consumer | RaceBookLayer.cpp:307 ignores GameXtLoader::LoadXt HRESULT; :311 ignores LoadMultiplayerPackFile. MultiplayerData.cpp:1323 and :1358 invoke InitSubsystems regardless of import result; :1547 reaches PreCacheData, which asserts and then reads m_pConfig (:1387). The original offline Debug startup executes this path too. | Native must return failed required startup before C_GameData/vehicle/effect users. Multiplayer.bin is required shared configuration here, independent of an online account/provider. Preserve caller-owned resource pause/tracker scope on false/throw. A failed/missing configuration must not call InitSubsystems. Parent approved this repair. |
| Allocation backing is not initialized default contents | Dynamic Renderer.cpp:131/149 creates its own white/grey textures but can return true after a failed map. Track/XtInterface.cpp:66 actually selects grey for absent diffuse; particle/Manager.cpp:1684 and minimal/Minimal.cpp:241 consume it. This is a different owner from the already repaired FX::System defaults. | Native failure must flow through existing I_Renderer Create rollback; publish readiness only after both exact texels are initialized. Preserve original white/grey fallback semantics, not a substitute. Parent approved this repair. |
| Final PC view state is still Windows-guarded | GameWorld.cpp:2431 omits view-dependent FX refresh on native immediately before selected postprocess; :2438 omits single-view preset>1 prepass depth/stencil restoration after it. ApplicationBase.cpp:1741–1747 dispatches the actual phase; AmaxWorldLayer.cpp:445 forwards it. Adjacent BeginFrame/current-view PC guards already include native. | Root MAC158 repaired both selectors after tracing the original PC ApplicationBase RestoreWindow/global-target chain and retail T232 body. Native now runs the existing view-dependent refresh and single-view detail>1 prepass depth/stencil restoration. All4 source probes pass; coherent MAC153 integrates them. Actual full-frame/HUD composition remains a runtime gate. |
| Native falls through console light-cubemap initialization | GameWorld.cpp:1168 `#ifndef PLATFORM_PC` maps/clears twelve dynamic cube faces on native, whereas PC skips it. Creation failures are asserted but execution still reaches map/clear before the bool returns. | Root MAC159 now rejects failed cube allocation or face map before assert/dereference/Unlock, with existing NativeDestroy retiring partial textures. Native explicit black contents remain unchanged. Only the separate PC skip-clear content policy remains deferred pending a first-use proof. |
| Allocated disabled-prepass depth can remain uninitialized | GameWorld.cpp:1499 silently accepts failed map, then :2185 can publish that1x1 R32F texture only when prepass is disabled. Main.cpp:292 already checks its separate black-texture map. | Root MAC159 now rejects a failed native map before Unlock/publication, preserving successful exact zero contents. Preset2 enables prepass, so no injected quality1 failure or selected-runtime exposure is claimed. |

The two historically known preload gaps remain classified, not newly expanded:
particle texture/cache Open and ResourceBag completion can represent optional
resources. Original Track fallback explicitly permits absent diffuse. Level
actor lists retain original Continue policy and optional FileExists behavior;
there is still no source-backed mandatory subset permitting blanket Abort.
Failure propagation for a selected required configuration does not authorize
changing those policies.

The current native Mirror/Reflection allocation gates, Main black-map gate,
postprocess initial/resize results, aggregate renderer results, failed-layer
handoff, actual vehicle/AIR readiness and sparse Xt final owner retirement were
verified in source and are not reported as outstanding merely because their
legacy branches remain visible. The old MotionBlurDoFReduced branch is not the
selected PostProcessLayer C_MotionBlurReduced object. The unused spotlight
threshold variable has no consumer found in this bounded scan; its PC/console
initializer difference is not promoted to a live blocker.

A bounded LP64 pass over World/view task payloads and dynamic/postprocess storage
found no additional pointer-stride defect. The light-cubemap task's `2*sizeof(U32)`
is its actual two-U32 range, not a pointer. Current particle trail storage uses
native typed stride, list/pool links retain native pointer alignment, and
visible/render/task vectors use typed or sizeof-based storage. No reason was
found to widen PC/GPU numeric vertex fields or change the fixed simulation tick.
This is bounded source evidence, not an exhaustive runtime sanitizer result.

Coding scope after parent approval: RaceBookLayer.cpp required-load/tracker/pause
boundary; MultiplayerData.cpp/h native required configuration consumer and
replacement ownership; Dynamic source/Renderer.cpp required white/grey map
publication. GameWorld, lower primitive policy, UI/world borrower cleanup and
shared build files remain outside this work. Focused fixture and source probes
will be reported separately; actual frame, keyboard lap/finish and clean exit
remain parent runtime gates.

## Root MAC159 required world texture failure guards

Root subsequently included the concrete false-result safety gaps in the same
required initialization audit: native light-cube creation now returns false
before its diagnostic assertion or face-map loop when either required cube
fails. Each failed face map returns false before memset or Unlock. The existing
world NativeDestroy checks IsCreated on all staged cube wrappers, so it owns
partial failure and resets the count. Native retains its current explicit
black initialization; the source-only PC skip-clear policy is not silently
selected without a first-use contents proof. Required window-depth map failure
also returns false before Unlock/publication, preserving the exact successful
zero float. No new backing, optional-resource policy or PC/console body changes.

Four source probes, coherent existing world transaction checks and actual
selected world success path validate integration. No injected late cube/map
failure is claimed.

## MAC154 implementation and bounded verification

Native RaceBook creation now stops at a failed configured Xt load or the required
Multiplayer configuration before downstream consumers. Its resource-worker pause
and memory-tracker scopes unwind on false and exception. Multiplayer's native
file/import paths reject absent root configuration before InitSubsystems; incoming
transferred bytes and rejected actual pack metadata retire through their original
owners. Replacement metadata is staged, but this does not promise transactional
rollback of arbitrary mutations inside the original Xt parser. This offline
configuration requirement is independent of online account/provider support.

Dynamic renderer creation now rejects a null White or Grey map before readiness.
The existing I_Renderer false/exception rollback retires both acquired defaults;
the successful bytes remain exactly 0xffffffff and 0xff808080.

All four changed production translation units pass object probes in Debug, Release,
Debug UBSan and Release UBSan (16/16). The extended existing
xt_owner_lifecycle_test.cpp also passes four object probes. It exercises missing
configured input, missing/wrong-root required Multiplayer input and transferred
buffer rejection, then verifies the actual resource worker resumes. The renderer
portion creates a real device before its graph, registers an owned manager which
rejects Create after actual default acquisition, checks original rollback, and
retries twice while checking exact texture contents and retirement. The standard
integrated mac_xt_owner_lifecycle CTest passed under strict Metal
validation and completed process/static exit0 in 1.31 seconds in coherent MAC153.
All other 50 selected checks passed (74.94 seconds), completing 51/51. It does
not inject a null Metal map, and the tiny authored creator pack does not
claim a successful full MultiplayerConfig/UI subsystem startup. Real game frame,
keyboard lap/finish and clean process exit remain separate runtime gates.
