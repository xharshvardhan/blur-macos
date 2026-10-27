# Scene light ownership during native shutdown

MAC152 audits the actual MAC149 assertion in `SceneGraph::C_LightManager::~C_LightManager`: one point light remains when the world deletes its scene. The manager requires owners to remove their lights before its destructor; it neither schedules deferred deletion nor frees active owners by policy. Native generational handles prevent stale-pointer reads but do not transfer ownership or remove allocated lights.

## Bounded owner inventory

| Original owner | Acquisition | Paired release | Lifetime boundary |
| --- | --- | --- | --- |
| VehicleActor hero/rear lights | `VehicleActor.cpp`6309/6315 | `Shutdown`4145/4147 | Actor `Destroy` calls `Shutdown`; native world deletes actor list before scene |
| Vehicle wheel rim light | `VehicleWheel.cpp`325 | `Destroy`199 | Vehicle `Shutdown` destroys all wheel effects before its model/scene |
| SpecialFX light component | `LightSpecialFX.cpp`70 | `Stop`104 | In-game FX store is cleared and actor-owned FX stop before scene; actual last pointer still needs attribution |
| UI attached-car perk HUD | `UIPerks2.cpp`173 | `Exit`887 | UI screen transition sends `E_UiMsg_Exit`; full native application destroys world before UI layer |

These are the direct scene point-light acquisitions in the bounded game/Mercury source search. Amax lighting prepass `C_Context::AddPointLight` creates draw-task data, not a scene-manager pool allocation. DynamicEffect::Light manages flare objects in its own pool, not this SceneGraph point-light pool. There is no missing PC-to-Mac release guard in the listed `RemovePointLight` paths. Vehicle's PS3-only rear-light split-screen condition does not remove the normal native release.

The actual MAC155 pure-SB capture confirms the perk HUD owns the retained light: manager active-list first pointer and `UI::UIPerks2::s_pBlur2PCCarGadget->m_PointLightHandle.m_pObject` both equal `0x0000000c40f1d678`, owned by scene `0x0000000c3f80b800`, managed=true, salt0, with manager count1. Capture errors are empty. The static gadget pointer is set for the attached local-player0 widget when it receives its player binding. Private original evidence: `_work/runtime/verification/mac155/light-proof.json`. The earlier MAC151 capture omitted the handle field, so attribution relies on MAC155, not on the one-light count alone.

## Existing UI borrow-retirement contract

Before MAC152, `C_GameWorld::NativeDestroy` flushed world jobs, then queued `_UI_MESSAGE(Game::E_GameflowMessage_ClearStoredActorPtrs)` before deleting actors and later the scene. The same game message is also broadcast synchronously to game/actor subscriber systems, which are distinct from the UI subscriber system. `_UI_MESSAGE` calls `gButtons.BroadcastMessage`, which deliberately enqueues UI work for its main consumer. On abrupt render-error shutdown there may be no subsequent UI update; later `UiManager` closes and cancels pending messages. Queuing this message is not proof that UI borrowers detached before actor/scene deletion.

`UILocatorNode` already subscribes to `ClearStoredActorPtrs` and clears its cached actor, demonstrating the original message's purpose. The remaining bounded cached world borrowers are:

- `UIPerks2`: VehicleActor, VehicleLogicActor, pickup-info pointer, plus its owned scene light and native render-interpolation static gadget pointer.
- `UICarNode`: cached VehicleLogicActor; its declared VehicleActor member is unused in the inspected implementation.
- `UIHudDummy`: cached `RacingPhysics::Car` acquired through the local player's vehicle.

Before MAC152, these three did not handle the clear message. Many other HUD widgets resolve actors locally each update/render and do not hold a world light; this inventory is not an assertion that every possible UI data dependency has been proven safe.

`C_ScreenControl::SetEnd` sends `E_UiMsg_Exit`, which dispatches virtual gadget `Exit`. By contrast `C_ScreenControl::Exit` merely clears its gadget list/allocator; the gadget base destructor is empty and does not call virtual `Exit`. Therefore later UI-layer destruction cannot be assumed to pair the perk light acquisition, even if the world were retained longer. Normal script screen transition and final application teardown are distinct ownership paths.

## Native retirement boundary

Retire world borrowers through the existing message while scene/actors and UI subscriber/Lua owners are still alive. Any new borrower handlers must be idempotent, run even for inactive widgets, clear cached actor/pickup pointers and interpolation anchors, and permit the existing local-player/setup flow to bind a later world. The perk light must be removed through its owning scene before that scene retires; no pool clear, forced free, assertion suppression or global layer-order change is justified.

UI callbacks must respect their main-consumer thread boundary. Normal layer destruction can be handed to the creator thread; final native `OnShutdown` instead joins that creator before draining callbacks and destroying children. A synchronous UI clear at that quiescent final-shutdown boundary can cover the observed exceptional exit without running arbitrary UI callbacks from every worker-world destruction or changing normal outgoing loading-screen flow. The implemented native world boundary performs the existing UI clear synchronously only when the callback is running on the recorded application main thread, after its owned job/animation flushes and before actor/scene deletion. Creator-thread unload keeps the original deferred UI message. This does not reorder world/UI layers or clear a scene pool. This change still requires coherent actual-game clean-exit validation.

Source checks for the dispatch boundary: `ApplicationBase.cpp`513 joins the layer creator before invoking application `OnShutdown`; `LayerManager.cpp`142 asserts creation is quiesced before synchronous `NativeDestroyLayerNow` callbacks. Normal `DestroyPendingAsyncLayers` instead runs through `UpdateAsync`, so an unconditional direct UI dispatch from every world destruction would change the original thread contract. The application already exposes its recorded `GetMainThreadId()`. Native world teardown flushes its owned scheduler/animation groups before the clear-message block. A final integration should retain that work-before-borrower-release order.


## Implemented borrowers and validation

Native `UIPerks2`, `UICarNode` and `UIHudDummy` subscribe to the original clear message. Their handlers run even while visually inactive. Perks retires its actual light, clears vehicle/logic/pickup pointers and interpolation static gadget reference, and resets local-player binding; car-node clears its cached actor pointers and player index; HUD-dummy clears its cached physics car. Perks `Exit` now removes a live light through `light->GetSceneGraph()` instead of re-querying a possibly replaced or absent global world, then resets its handle. Repeated clear/Exit after scene deletion is safe. No gadget header/layout or Windows body changes were needed; UI screen trees/resources and normal creator-thread loading flow remain intact.

`tools/macos/ui_world_borrowers_test.cpp` is a no-argument full-game/bootstrap-first fixture, using the same original Lua/full-game archive closure as existing UI lifecycle tests. It creates the real three gadgets and their subscriptions, deactivates them through the original API, and seeds opaque borrowed pointer markers which are never dereferenced; it creates no actors and claims no actor/physics initialization. The perk handle is acquired through the actual `SceneGraph::AddPointLight` API in two real scene lifetimes. A queued clear leaves ownership intact; the real UI `BroadcastMessageNow` clears pointers and removes the light. The fixture verifies repeat clear/Exit, independent queued-message cancellation, real pool limit/capacity recovery, stale-handle invalidation after reuse, original scene-manager destructor assertions, and final Exit without a global GameWorld. Test-only access to existing private fields follows the repository's current lifecycle-test pattern; there is no production test seam or pool-bookkeeping access.

The private Debug full-game executable passes under `MTL_DEBUG_LAYER=1`, including real process/static teardown, in `_work/build/macos/physics-agent-ui-borrower-macos/runtime.log`. The fixture does not boot game message-name registration, so it logs informational `MissingDebugInfo` for the known clear message ID; the actual original subscriber routing and every ownership assertion still pass. Direct full original gadget Setup through game Lua/assets, the world main-versus-worker dispatch branch at runtime, and a complete actual game exit remain separate validation boundaries. Root owns coherent integration and the next actual game attempt.
