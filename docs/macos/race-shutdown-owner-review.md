# MAC220 independent race-shutdown owner review

Read-only review of the MAC217 `RaceLogicLayer.cpp` candidate finds no blocking
issue in the confirmed shutdown class. The actual MAC216 fault and authoritative
null receiver are recorded in [the debugger proof](career-shutdown-pickup-proof.md).
No production, fixture or CMake edits, builds, archive reads or runs were made
for this review.

## Required lifetime

`RaceBookLayer.cpp:477-490` keeps RaceLogic published during its synchronous
NativeDestroyLayerNow and destroys the world afterward. LayerManager sends
Destroy, marks Constructed, then deletes the object. Actual MAC216 reached the
derived destructor after Destroy had cleared its actor-list pointer. A bomb's
base destructor still queried that published owner.

The candidate preserves the native actor-list borrow through derived cleanup,
then nulls it after the real pickup manager Clear. It does not hide the owner,
add a getter null fallback, skip a pickup destructor or replace normal cleanup.
Windows retains its original Destroy-time field reset and Clear call. The native
host genuinely unwinds; the Windows application also has a TerminateProcess
shutdown path and cannot establish the native destruction guarantee.

## Joined consumers and callbacks

- The native helper at `RaceLogicLayer.cpp:126-138` Flushes the acquired pickup
  group before Clear. `PickupProjectileBase.cpp:267,448` schedules work into that
  group with addresses of pickup-owned deferred-ray fields. Waiting first is
  necessary before typed-pool Free and ray storage destruction; waiting only in
  the later DestroyGroup would be too late.
- `PickupsManager.cpp:108-140` retains its original full cleanup: active and
  buffered pickups through typed Delete, shock records, group destruction and
  pool deletion. The helper preserves this policy and does not hold a lock
  across worker completion.
- `PickupMineBase.cpp:57-88` clears body user data, deactivates and removes actual
  collision bodies through the still-live physics scene. Projectile destruction
  at `PickupProjectileBase.cpp:89-99` similarly requires its live RLC/scene.
  Unpublishing RaceLogic to avoid the null lookup could skip Mine cleanup.
- `PickupBase.cpp:133-160` sends actual deleted messages to live vehicle scoring
  and challenge components. Their handlers retain their original behavior;
  the examined deleted-message branches do not require the damage pointer that
  Destroy already cleared. The physical collision/event callbacks were already
  unregistered by Destroy.
- `ShockManager.cpp:333-344` deletes its owned ManagedShock records and clears
  its vector/counters. It does not introduce a later actor lookup. The pickup
  position member has ordinary owned map cleanup, not a destructor callback
  into the cleared actor list.

## Owner retirement and partial paths

After joined Clear, native destructor clears the actor-list borrow, detaches
static pickup/startline handlers from the expected live game subscriber pool,
and clears the race-mode cached pointer. The derived member destructors and
MessageHandler base then retire ordinary containers/own subscriptions while the
game pool remains live. World/actors/physics and audio are destroyed later.

Native constructor now initializes the borrow to null. The existing entered
flags ensure partial pickup Init is cleaned only after it was entered; the same
helper accepts the original JQ_GROUP_NONE state, joins an acquired group if any,
and invokes original Clear before subscriber detach. A successfully constructed
but never-Created layer has no vehicle handles requiring an actor lookup.
Repeated manager Clear retains original empty-list/none-group/null-pool handling.
This does not promise arbitrary duplicate C++ object deletion or full race
reinitialization after every allocation failure.

QuickRestart's original handle clear, Reset/Init and subsequent Create assignment
are unchanged. That route is outside this final-deletion repair; this review does
not claim a new quick-restart runtime test or expand its gameplay policy.

## Acceptance limit

Source inspection supports the ownership order. An empty manager/layer fixture
would not exercise MAC216's real AI-fired pickup, component notification and
collision-body owner dependencies. Root's coherent build and populated 20-car
actual shutdown retry remain required before claiming this regression fixed in
the running game. No successful career result or natural finish follows from
this source review.

Paths without prefixes above are under `code/amax/game/GameLogic/`, except
LayerManager under Mercury gamesys/framework. Shader/render behavior is outside
this review.
