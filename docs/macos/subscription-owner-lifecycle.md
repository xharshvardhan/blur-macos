# MAC136 — Native game message subscriber owner lifetime

Production source and focused fixture are frozen; coherent integration and actual
full-service unwind remain pending. Actual MAC131 first fails
at LOD handling, then completes Boot, RaceLogic, World, UI, RaceBook, Audio,
Debug and System layer destruction. `C_Game` destruction subsequently reaches
`C_MessageSubscriberSystem::~C_MessageSubscriberSystem` with allocated
subscriptions, asserting at MessageSubscriberSystem.cpp57 and exiting251.
The messages listed in `verification/mac131-game.log` identify message types,
not necessarily one unique handler for shared update/game-flow messages.

The pool deliberately requires its owning handlers to remove subscriptions.
`C_MessageHandler::~C_MessageHandler` calls `RemoveAllSubscribers`; the multi
handler destructor deletes its forwarding handlers. These destructors are too
late for static services outliving the heap-owned `C_Game`. Clearing the pool
would leave the services' subscriber links and forwarders dangling and would
hide owner/resource lifetime faults.

| Real owner / acquisition | Existing retirement | Finding and correct lifetime boundary |
| --- | --- | --- |
| Static `AIAmax::AmaxAIR`; GameWorld.cpp606 calls Initialise for each world; AI_Amax.cpp214–260 attaches AI/CutScene/ActorKilled/SetAIRoute messages | World native acquisition flag calls ShutDown at1827; AI_Amax.cpp164–198 deinitializes object manager/cars/group | ShutDown never detaches subscriptions. Pair detach/reacquire with actual per-world Init/ShutDown, or explicitly release only at final game shutdown; do not assume its static destructor can run after the pool. |
| All static race-mode store entries; RaceBookLayer.cpp330–339 initializes every mode, including inactive multiplayer/destruction variants | RaceBook Destroy420 only destroys RaceBookManager, clears layer pointers and closes FX pack | Add final RaceBook-owned mode release after children stop. Preserve all subscriptions during ordinary race transitions; detach only the final game binding, reset cached RLC/layer/level-state references under owner policy. |
| Sticker multi handler, initialized by AchievementsManager.cpp262 during RaceBook boot | RaceLogicLayer::Destroy482 calls StickerManager::Shutdown; StickerManager.cpp130 only clears per-race cached pointers | Current Shutdown is deliberately per-race, while subscriptions are boot lifetime. Do not add detach to that method. Final RaceBook-owned service release must detach its actual game forwarder while retaining per-race behavior. |
| Static Friends, FriendChallenges, EventOwner; RaceBookLayer.cpp322–327; handlers subscribe to Update/PausedUpdate/Results/StartUILoad | Clear methods reset state and Friends::Clear stops active enumeration, but no final detach | Add final owner release after callbacks/work are quiesced. These are known boot-owned acquisitions, not arbitrary borrowers discovered by scanning the pool. |
| Static EnvironmentSettings and InGameSpecialFX; RaceBookLayer.cpp346–347 | World layer clears SpecialFX store before world renderer deletion at AmaxWorldLayer.cpp1275; no final binding release | Keep early resource cleanup. Add final boot-service detachment before C_Game destruction; multi handler must remove the concrete game forwarder. |
| World-level RaceIntroOutro; AmaxWorldLayer.cpp663 Init | Failed-renderer rollback already removes its subscribers at678; normal Destroy has no matching detach | Pair normal/failed world lifetime explicitly, preserving intro/outro behavior during the world. |
| RaceRules; AmaxWorldLayer.cpp849 Init | ClearAllRules removes rule/action objects but not handler subscriptions; called on race-mode setup | Separate rules reset from final world/game binding release. |
| Pickups; RaceLogicLayer constructor185 / Reset230 Init | RaceLogic destructor195 calls Clear, which kills pickups and retires its group/pools | Clear never detaches its Update subscription or clears borrowed race-layer/level/event pointers. Owner destruction should detach/reacquire at the corresponding race lifetime; do not alter quick-reset semantics blindly. |
| InGameStartLineInfo multi handler; RaceLogic constructor168 Init | Destroy465 calls Clear; Clear73–78 removes all four message types | Subscriber count can be zero but its forwarder still points at the game pool. Final owner release should retire the actual matching forwarder, without inventing another. |
| Heap UI/Dialogue/System audio subsystems; GameAudioLayer.cpp514–525 explicitly allocates and registers11 subsystem objects | GameAudioLayer::Destroy920 destroys audio System,934 calls InterfaceController::Release | Release.cpp121–129 deletes only the pointer table, not these objects. Their MessageHandler destructors never execute. Retirement must be explicit in the GameAudioLayer-owned allocation contract; AddSubsystem also accepts borrowed Audition objects, so generic deletion is invalid. Audio companion audit owns order/worker/entity consequences. |

Unknown debug-name IDs are recovered directly from the original FourCC rules:
`0x03060001/2` are AMDA PlayAudio/AddWavebank in AmaxDialogue.h14–15 and
DialogueSubSystem.cpp89–90. `0x1ce22001` is DTRC WaveCompleted, subscribed by
RaceModeDestruction.cpp152. This explains a destruction-mode subscriber during
StreetRace: the original boot initializes all race modes.

GameXtLoader::Init, ScoreboardManager::Init, InGameCarInfoManager::Init and
InGameDialogueManager subscription implementations also exist, but no active
call was recovered in the selected game path. Do not add arbitrary cleanup for
these candidates without recovering their owner or live binding. Normal layer,
actor and application automation handlers have actual destructor paths; the
shared message names alone cannot prove those owners leaked. DriverManager's
subscriptions bind the actor pool, and its world-owned destructor already
cleans that owner, so it is not an inferred game-pool leak.

The multi-handler boundary needs particular care:
`SetSubscriberSystem(NULL)` **creates** a new forwarder when no matching one
exists. `RemoveAllSubscribers` removes message records but retains forwarders.
A narrow native `RemoveSubscriberSystem(expectedPool)` operation can remove
only the matching strong forwarder without disturbing borrowed UI bindings or
creating one during partial-init cleanup. Single handlers similarly need a
binding-aware detach that only acts on the expected owned game pool. A final
owner stage can then retire each named acquired service even after partial
initialization, while leaving unrelated subscribers alone. These native nonvirtual APIs are now implemented without changing handler
layout or existing platform behavior.

Proposed verification must run repeated real game-service lifetimes and retain
ordinary world/race restart subscriptions until their actual owner ends. Test
partial acquisition, final detach, second initialization against a new pool,
no callback after retirement, zero live pool records, and true static exit.
Audio ownership additionally needs owned-versus-borrowed and queued worker/UI
loader teardown checks. These checks are separate from gameplay completion.

Optional diagnostic prepared under
`_work/build/macos/subscription-owner-agent/subscriber.commands`: pure LLDB
field/memory traversal at the C_Game pool destructor groups subscription
pointers by dynamic handler/vtable type and resolves forwarding handlers back
to their real owners. Python syntax passes; runtime operation is not yet
verified. No target expressions or function calls, pool mutation, game run or
shared build was performed for this audit.

## Implemented native owner contract and verification

The native single handler detach checks its stored pool against the expected
live pool, removes its own records through the existing mechanism, then clears
its binding. The multi handler retires only the matching strong forwarder and
clears current selection only when that forwarder was current. It does not
select, create or remove any unrelated binding.

RaceBook records entered service calls and the entered mode prefix **before**
calling Init, including an invocation that partially subscribes then throws.
Final Destroy and the retained failed-layer destructor both retire those known
owners, idempotently. The Achievements stage includes its already-constructed
static Sticker collaborator; an actual matching binding proves whether the
Sticker call progressed to acquisition. No unentered lazy service is created.
Only subscriptions and cached mode owner pointers are released: Xt packs, game
data and resource caches remain available for later Audio destruction. Sticker
Shutdown keeps its original per-race semantics.

AIR detaches at its per-world ShutDown and resets acquired group handles once;
the next world Initialise reacquires. AmaxWorld records entered intro/rules
services, retains them through quick restart and retires them at final world
Destroy/destructor, including failed Create. RaceLogic constructor failure
cleans only entered Pickups/start-line acquisitions; successful owner
destruction retires them after original pickup cleanup. Quick Reset retains
the original subscriptions.

Root's actual MAC137 pure-read snapshot confirms172 remaining subscriptions in
121 lists across27 handler types, matching the source owner inventory: all
boot modes, Sticker, Environment, AIR, RaceIntroOutro, FriendChallenges, Rules,
Friends, EventOwner, Pickups, InGameSpecialFX and the three audio types. There
are no debugger read errors; no additional speculative singleton cleanup is
needed. Structured evidence: verification/mac137-subscriber-owners.json. The
owned diagnostic inferior was killed, so that snapshot is not an exit result.

Six production objects compile in all4 native configurations:24/24 probes.
The focused tools/macos/subscription_owner_test.cpp compiles/links/runs in Debug
with true process exit0 through the exclusive wrapper. It uses real subscriber
systems and original single/multi handlers, not a fake pool. Three repeated
two-pool lifetimes cover never-entered/wrong-pool/idempotent detach, callback
absence after retirement, partial Init failure/retry, preserving current and
noncurrent borrowed bindings, and no forwarder construction during cleanup.
Two actual C_Game lifetimes initialize real StreetRace/Multiplayer modes and
Sticker, prove per-race Sticker Shutdown retains the binding, then retire them
before each game pool and eventual static destructor. A real never-Create
RaceBook layer proves its finalizer does not retire external acquisitions.

The focused fixture does not load the entire RaceBook boot database or create
actual AIR/physics/pickup resources. Their entered-stage unwind remains for the
coherent actual-game run, together with separately owned Audio retirement.
Native RaceBook/AmaxWorld field changes require a coherent dependent rebuild.
Integrate the fixture with GameLogic includes before Amax and the actual
full-game/bootstrap-first closure, no arguments. Private evidence is under
_work/build/macos/subscription-owner-agent/{matrix.log,runtime.log}.
