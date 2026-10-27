# MAC216 actual populated-career shutdown proof

Actual Debug213 normal career startup reaches `ingame/hud.lua`, Racing and
20 vehicles. The elapsed240 shutdown then faults on the main thread before
unwind. This diagnostic establishes the owner dependency failure; it does not
establish a completed career race or successful game shutdown.

## Reproduction and capture

Fresh private app copied from the delivered Debug213 binary; SHA256 matches
`02f4d20fa4162ba6f63e5e0817844ebc5c884a20f46e449618da71e272bb8f1f`.
Original Master189 data, real profile/save189, CompleteCareer0 and ObserveOnly1.
The original ordinary-key menu helper selected unlocked event4597671 through
its real career key4013244786 and confirmed an owned car. No SetupRace, progress,
unlock, pose or result mutation; no keyboard driving operator was used.

At180s the harness observed HUD/Racing/20. The owned menu observer was then
terminated to reduce IPC traffic. LLDB caught EXC_BAD_ACCESS at address0x10 on
`Game Master` / main queue during normal elapsed shutdown. All29 threads were
captured through pure SB access; top-level capture errors were empty.

Selected fault chain:

1. `ActorList.inl:171`: C_ActorList::GetActorPtr, **this=NULL**.
2. `RaceLogicLayer.cpp:1123`: GetLogicVehicleActor(i=12), race layer nonnull.
3. `PickupBase.cpp:147`: C_PickupBase destructor, same published race layer.
4. `PickupMineBase.cpp:88`, C_PickupBomb destructor, real typed pool Free.
5. `PickupsManager.cpp:896,128,110`: Delete, KillAllPickups, Clear.
6. `RaceLogicLayer.cpp:220`: derived destructor.
7. `LayerManager.cpp:185`: NativeDestroyLayerNow delete.
8. `RaceBookLayer.cpp:481`: NativeShutdownChildren.
9. `GameApp.cpp:781`: OnShutdown.

The actual pickup is BOMB, uniqueId17, firingIdx12. Pickup and getter use the
same still-published race layer. Direct GetActorPtr this=NULL is authoritative;
locals whose assignment had not executed (such as pActor/pVehicleLogicActor)
were not treated as owner evidence.

## Source correlation and handoff

`RaceLogicLayer.cpp:484-515` completes Destroy by clearing its actor-list pointer.
Its destructor at218-231 subsequently clears pickups. A pickup destructor can
still query the published race layer and dereference that cleared dependency.
This confirms MAC217's independent source candidate. The real world is retired
later by the synchronous shutdown sequence. Repair must retire consumers while
their required owners remain valid, rather than suppress the null dereference
or skip actual pickup/resource cleanup. MAC217 owns that repair proposal.

Ignored evidence is under `_work/runtime/verification/mac216/`:
`source-proof.json`, `shutdown-proof.json`, compact `shutdown-summary.json`,
operator/harness logs and its retirement record. The top-level game log and
probe status carry the MAC216 prefix. No proprietary bytes are in this document.

After capture LLDB explicitly killed only owned inferior54667 (status9), then
quit. Wrapper diagnostic exit0 at248.64s and slot_released=true; exact private-app
process scan found no leftover. This is diagnostic completion, not game exit0.
No production, fixture, CMake or shared-build/archive writes were made.
