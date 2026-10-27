# MAC217 — Populated race pickup teardown dependencies

Actual MAC215 used the ordinary Master/profile/event/garage flow and original
20-car career configuration, reached Racing/HUD with a completed real frame and
finite human keyboard driving. Its elapsed300s shutdown exited245. The last
messages were RaceLogicLayer state4 and state0, without `Destroyed layer`.
Neither an actual completed career race nor clean race shutdown follows from
that run. Prior one-car/frontend clean exits do not cover live AI-fired pickups.

## Exact failure and source owner map

MAC216's before-unwind capture confirms:

```
ActorList::GetActorPtr(this=NULL)
RaceLogicLayer::GetLogicVehicleActor(i=12)
C_PickupBase::~C_PickupBase
C_PickupMine / C_PickupBomb destructors
actual typed pool Free / PickupsManager::Delete
KillAllPickups / Clear
C_RaceLogicLayer::~C_RaceLogicLayer
NativeDestroyLayerNow / NativeShutdownChildren
```

The live pickup was BOMB, unique17, firing vehicle12. SB capture errors were
empty; the diagnostic owned inferior54667 was explicitly killed9 and LLDB quit.
No clean-exit claim follows from the diagnostic wrapper. Evidence is ignored
`_work/runtime/verification/mac216/shutdown-proof.json`; actual215 evidence is
`_work/runtime/verification/mac215-game.log`.

| Boundary | Source contract |
| --- | --- |
| Destroy then delete | Framework LayerManager.cpp142–185 sends the Destroy callback, logs state0, then deletes the actual layer. Thus the final state0 pinpoints the derived destructor boundary, rather than a failure inside the callback. |
| Native child order | RaceBookLayer.cpp477–492 retains RaceLogic publication until synchronous destruction returns, then destroys the actual world. World/RLC/physics/actors are still live during pickup retirement. |
| Premature borrow clear | RaceLogicLayer.cpp484–515 originally cleared m_pGameActorList in its Destroy callback, but retained vehicle handles on normal final destruction. |
| Late dependent retirement | RaceLogicLayer's derived destructor calls PickupsManager::Clear. Clear108–120 invokes actual live/buffered typed-pool destruction, shock deletion, scheduler group retirement and pool deletion. |
| Required lookup | PickupBase.cpp133–160 obtains the published RaceLogic owner and resolves its firing car before original deleted-powerup notifications. VehicleIsLocal includes both human and AI control (MultiplayerUtilities.cpp21–29). GetLogicVehicleActor1118–1125 uses the still-valid handle index and directly dereferences its actor-list borrower. |
| Other owned services | Mine::~57–88 removes its actual inner/outer collision bodies; Projectile::~89–98 removes its body from the still-live physics scene. These are real cleanup callbacks and cannot be skipped by nulling owner publication. |
| Pending tasks | ProjectileBase.cpp267/448 dispatches deferred rays using the pickup scheduler group and pickup-owned ray storage. Original PreUpdateTwo/Update flush before consuming it, but Clear used to destroy pickups before DestroyGroup's eventual flush. |

The ordinary original queued RaceBook exit paths clear their published RaceLogic
pointer immediately after queueing destruction (lines808–809,897–898,918–919),
so the late Base destructor can skip notifications when no owner is published.
Original PC application OnShutdown terminates the process. Native returning
shutdown intentionally retains live owners for actual resource cleanup; it must
keep their internal required borrowers valid through that cleanup too. Merely
unpublishing RaceLogic or adding a null getter return would hide the dependency
failure and could skip body/notification cleanup.

## Narrow native correction

Only `GameLogic/Layers/RaceLogicLayer.cpp` changed; no class layout or file format
changed. Native construction explicitly initializes the unentered actor-list
borrower to null. Native Destroy retains that borrower until true object
retirement. The native derived destructor now waits for the actual acquired
pickup scheduler group before invoking unchanged PickupsManager::Clear. All
actual typed-pool pickup/shock/body/deleted-message work runs while actors,
lookup, RLC, physics scene and publication are live. Only after it completes is
the actor-list borrower cleared, followed by the existing subscriber detach and
mode-cache retirement. Constructor failure uses the same task-before-object
cleanup for an entered partial pickup-manager initialization.

The original Windows actor-list clear remains in place. QuickRestart's original
handle reset/Create policy, collision/event callback unregister, cache clear,
damage-pointer clear and Sticker shutdown remain unchanged. The cleared damage
pointer belongs to collision processing, not pickup deferred-ray completion or
deleted notifications. Shock retirement owns plain ManagedShock records; the
actual NewScoring/Challenges recipients still have their live vehicle components
and RLC. Independent MAC220 source review found no blocking dependency issue.
Idempotent empty/group-none cleanup remains the original Clear mechanism.

## Verification and remaining acceptance

The actual RaceLogicLayer TU passes Debug, Speed, DebugUBSan and SpeedUBSan syntax
probes4/4, with no scoped diff whitespace errors. Logs are ignored
`_work/build/macos/mac217-agent/`. Production is frozen for root MAC218 coherent
integration and the existing10 affected owner checks.

An asset-free test with no active pickup would not reproduce this failure.
Creating a real pickup requires the actual RLC/vehicle actors/world/audio/rule
metadata; fake owners, poisoned pointers or a helper test mirroring line order
would not validate body callbacks and deleted messages. Root MAC219's controlled
20-car ordinary career run with active AI pickups and real normal shutdown is
the required meaningful regression. Four configuration probes and read-only
review do not establish that process exit or a completed career race.

No game, debugger, shared build, archive or GPU run was performed by MAC217;
MAC216 owned the diagnostic and root owns the coherent/actual verification.

## Actual integration acceptance

Root MAC218 built both standard Debug and optimized games. Actual MAC219 then
repeated the original normal 20-car event with the same elapsed240 shutdown as
MAC216: real HUD/Racing20, one completed GPU frame, full normal teardown and
actual exit0 after253.46s. No external kill/debugger/timeout. This accepts the
confirmed pickup final-borrower class; race completion/results remain unverified.
The unrelated-path optimized world rollback timeout and passing focused retry
remain separately recorded in the resource-worker audit.
