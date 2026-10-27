# MAC163 — RaceBook self-pool and final data ownership

The actual MAC160 process completed world/UI/all layer teardown and closed four
archives, then crashed during static finalization. The DiagnosticReports
BlurNativeGame-2026-10-27-060238.ips and MAC161 exit-proof.json agree:
GetEffectiveLevel receives the retired logger value 0xfefefefefefefefe from
C_MessageSubscriberSystem::~C_MessageSubscriberSystem, called by the implicit
C_RaceBookManager destructor. This is a late secondary failure after the
separately owned incomplete-draw-state primary, not evidence that XtDbDestroy
caused the crash.

C_RaceBookManager::Init binds its inherited C_MessageHandler to its own member
mMessageSubscriberSystem and subscribes SaveGameLoaded. Original Destroy only
unregisters the Demonware lobby handler. C++ destroys members before bases, so
an implicit destructor reaches the pool while the base handler still retains
its subscription. The pool's diagnostic tries to log that real live binding
after logger shutdown. No pool, allocator, assertion or logger policy was changed.

Native Destroy now detaches this exact owner's binding to this exact pool while
retaining profile/dialogue/vehicle data for downstream Audio. Explicit final
retirement in C_GameApp's destructor, after dependent layers/audio and before
global Xt pack closure, unregisters the existing lobby owner, clears only game
profile borrower pointers matching owned RaceBook profiles, deletes owned
profiles and nested dialogue objects, deletes vehicles and clears owned indices
and time-attack state. Native constructor scalars are explicit, and repeated or
partial Init retires previous state before acquisition. Scope tracking and
unique_ptr cover exceptions and not-yet-published new profile/vehicle objects.

The explicit native RaceBook destructor detaches self before member destruction
and deletes residual owned arrays without accessing the unrelated static game
profile manager. Actual lobby registration is retired at the explicit live
application/layer stage, preserving its network-service dependency. The
fallback does not invent permission to run online provider operations after
provider destruction. Borrowed external pool semantics remain unchanged:
NativeDetachSubscriberSystem acts only on the expected pool. A bounded inventory
found NetPlatform::Core's similar owned pool already paired by its explicit
destructor's shutdown/RemoveAllSubscribers; C_Game owns a pool but is not a
message handler. ActorList and UI use separately owned explicit pool teardown.
Only RaceBook's manager adds a subscription in its library; profiles bind that
pool but currently add no subscription. External UI/Lua callers broadcast only.

The existing Xt owner fixture did not enter RaceBook Init or load all real
startup metadata, explaining why its clean static exit did not cover this class.
No global Xt teardown stage moved. Dynamic Particle/Light world Shutdown already
invalidates manager-dependent objects before renderer destruction; this bounded
source check is not a proof of every full-game pack callback dependency.

## Verification and limits

RaceBookManager.cpp and GameApp.cpp pass all four production object probes;
racebook_owner_test.cpp passes four more (12/12). The new no-argument test uses
the real full-game/bootstrap-first closure and bounded authored actual metadata
lists, exercising original permitted missing-base/garage fallback behavior.
It executes three Init/Destroy/final-retire cycles, nested real dialogue payloads,
repeated acquired Init, destructor fallback, and two actual C_GameApp owner
lifetimes with matching game-profile borrower retirement under one explicit
outer schema lease. It does not reset the schema after zero, fabricate RLSystem
or gameplay, mutate race progression, or require online authentication.

Private real runtime and full static exit pass with standard library positions
by replacing only the two changed archive members in private cloned archives.
An initial direct-object override linked RaceBook's namespace-static instance
before the allocator bootstrap and therefore had an invalid private static
ordering; the exact late stack was retained and no production/fixture workaround
was added. Final log: _work/build/macos/racebook-owner-agent/runtime-standard-order.log.
No injected allocation exception is claimed, and full-game normal exit remains
a separate root runtime gate. Root owns the standard coherent CMake integration.
