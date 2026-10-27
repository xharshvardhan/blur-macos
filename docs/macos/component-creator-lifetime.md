# MAC147 — Component-creator Sorcery storage lifetime audit

Source audit after actual MAC146. The log records `std::system_error:
mutex lock failed: Invalid argument`, then terminates through the noexcept
`DynamicPool<C_StorageSorcery<I_ComponentCreator>>` destructor. The unwound frame
at `btl/Allocators/pool.h:271` alone does **not** establish pool counts or the
original throw site. Root owns a filtered before-unwind diagnostic (MAC148).
The initial audit made no production changes. MAC148 subsequently confirmed
the exact primary condition, and the owner changes/regression below are now
source frozen. No pool/assert/allocator workaround is used.

## Storage contract

`misc_shared/DataRef/Sorcery.inl:34` OnConstruct registers a storage item for every
nonzero Sorcery UID, even when its reference count is zero. OnDestruct (:75)
clears the underlying data and frees the item if no external references remain.
NoMoreRefs (:168) frees only after the data has been destructed. This separates
live database records from live component references: a nonempty pool does not
by itself prove actors/components leaked.

`actor_system/GameComponent.cpp:54` forwards Xt events to that storage contract.
Actor creation data can also embed creator DataRefs through
`ActorCreation.cpp:146`. Actual game components retain their creator in
`GameComponent.cpp:179`; `Actor.cpp:70` destroys owned components, so normal
actor/component destruction releases those references through ordinary member
lifetimes. Component/actor factories hold type IDs and create/destroy functions,
not the loaded Sorcery record ownership.

The included native pool is `btl/Allocators/pool.h`, not the different
`core/allocators/pool.h`. Its line271 checks free-list count, then retires byte
blocks (:279) and storage (:280). Its free-list links are full native pointers;
this path does not use the 32-bit PointerSlot32 registry. It has no
DestructorOnce call. A direct pool/slot-registry mutex diagnosis is unsupported.

## Actual global owner gaps

| Owner | Acquisition | Existing retirement | Gap relevant to native normal return |
| --- | --- | --- | --- |
| Amax application | GameApp.cpp:250–262 calls XtInitialise, creates callback context and global XtDb | No Amax XtDbDestroy, callback deletion or XtShutdown | Required record-destruct callbacks never run for remaining global data |
| GameXtLoader | GameXtLoader.cpp:62 stores pack entries; :163 XtPackOpen into global DB | No close API/destructor | Loaded pack metadata and strong record ownership remain; callbacks belong before DB destruction |
| RacingPhysics Xt | racingphysics.cpp:16–45 increments schema ref and opens pack | DestroyXt:53 only decrements schema; Amax never calls it | Pack must close and native globals reset before its paired schema release |
| DynamicEffect Xt | XtInterface.cpp:85–122 increments schema ref and opens global-DB pack | PackClose:132 closes pack; RaceBookLayer.cpp:506 invokes it | Schema acquisition remains unmatched; borrowed DB pointer remains stored |
| Audio Xt | XtAudio.cpp:1386 creates heap C_XtAudioDb, GameAudioLayer.cpp:555–556 opens definitions | EntitiesPack flushed at565; Release(:1396) exists but Amax never invokes it | AudioPack/singleton remain; release must follow owned subsystem shutdown while required audio dependencies remain |
| UI definitions, global/world actor packs | UIXt and GameActorList/AmaxWorldLayer open global-DB packs | Existing owner paths close those handles | Preserve those explicit lifetimes; do not indiscriminately close borrower packs |

XtShutdown (Xt.cpp:34) decrements the shared schema acquisition count and destroys
the schema only when it reaches zero. The correct fix must pair each successful
acquisition once, not loop until zero or assume the app is the only owner.
XtDbDestroy (XtFlaskImport.cpp:1446) invokes XtRecordFreeObject for live records;
that calls destruct callbacks before type destruction (:1068). The callback
context and schema must remain alive throughout. GameData destruct callbacks
also update static indexes/handles, so retiring the DB before all its borrowers
or after arbitrary static destruction is unsafe.

On native, XtNetworkStart is the explicit no-op branch in XtNetwork.cpp:221;
the PC/PS3 Artery Sorcery worker is not compiled into that path. No native
network-stop action should be invented for it. Native actual application/layer
workers must nevertheless remain joined before record retirement.

## Late diagnostic hypothesis and needed evidence

Main.cpp installs the visual assert handler. ShutdownVisualAssertScreen
(BizAssertScreen.cpp:353) removes only the fatal-log subscriber and leaves that
handler installed. The handler enters namespace-static `g_AssertLock` at151
before any log text. Native MT::Lock destruction frees PlatformLock
(NativeThreads.cpp:130); a later Enter takes its freed std::mutex. Thus an actual
pool-count assertion after that lock retires can produce exactly the observed
system_error with no assertion text. This is a source-supported hypothesis,
not the missing before-unwind observation. Do not fix it by suppressing the
assert or extending an unrelated mutex lifetime while leaked record ownership
remains.

## Proposed coherent boundary, pending live confirmation

Make app database creation transactional: initialize owned pointers, keep a
local successful schema-acquisition stage, roll back callback/DB creation on
failure, publish only a complete owner. After all world/UI/race/audio borrowers
and global resource users stop, release each remaining owned pack/singleton,
then destroy the app DB, delete its callback context, and pair schema references.
Keep C_Game, creator factories, callback targets and schema alive during this
sequence. The companion Audio audit confirms its borrowed-global-DB Create acquires no
schema ref; XtHandle lists are nonowning. Audio close callbacks are Construct-only
or EntityDef Destruct logging, with no subsystem/controller/System/wave calls.
Thus Release(globalDb) after all Audio owners/loaders stop and before DB
destruction is safe for this route; it does not require moving audio service
teardown. Preserve ordinary
per-race reuse and shared DB ownership; do not free borrowed databases.

GameXtLoader hot reload currently overwrites a raw pack handle. The source
supports a replacement transaction without a new ownership registry:
XtFlaskImport (:1278) destructs/replaces the old object for an existing strong
record and increments that record's strong/weak counts. Only after successful
new open may the prior pack close; XtPackDestroy (:1156) decrements those counts
and destroys only the last strong owner. A failed new open must leave the old
handle published. Default native runtime has the watcher disabled, but a final
owner implementation should preserve this existing reload contract.

XtDbDestroy currently iterates `records[0..count)`, but pack close compacts
`index[]` and returns arbitrary record slots to the free list. Remaining records
can occupy slots beyond count after lower-slot packs close. Residual database
destruction needs the live index (or capacity with id checks), not a dense-slot
assumption. Closing all owner packs should leave count zero, but this boundary
requires a separate hole/reuse regression. DB destruction also does not delete
external XtPack metadata handles.

A real regression should open actual registered actor/component creator records,
release components while retaining the DB (storage remains valid), close owned
packs/DB (destruct callbacks retire data), then release any intentionally retained
DataRef (final storage retirement), repeat a lifecycle, and test partial app
acquisition failure. Parent actual full-game exit remains the proof for the
complete global owner set.

## MAC148 confirmation and implemented owner boundary

Actual before-unwind evidence in
`_work/runtime/verification/mac148/late-throw-proof.json` confirms the assertion:
`m_list.Size(97) PoolTypes<T>::N(128) m_pool.Size(1)`, i.e.31 live storage
entries. The subsequent system_error arises inside MT::RecursiveState::Enter
called by helper_screens::BizAssertHandler before log output. It is not a
PointerSlot32 failure. This supports retiring the live record owners; no late
assert/logging suppression is appropriate.

Native GameApp now null-initializes its existing owner pointers. Global Xt
creation acquires one schema reference, allocates an exact XtCallbackContext
(base type with GAME tag, matching its nonvirtual deletion type) and database
locally, rolls back on failure, and publishes only complete ownership. Its
destructor retires remaining DynamicEffect/Physics/optional MultiplayerData and
GameXtLoader pack owners, destroys remaining DB records while callbacks, schema,
C_Game and factories live, deletes the callback context, and releases exactly
one app schema acquisition. It does this after layers/global borrowers and
replay/automation owners have stopped, before deleting C_Game. No new class
layout is required.

GameXtLoader closes its actual acquired handles in reverse order, resets loaded
stages, clears the entries, and detaches only its expected live C_Game subscriber
pool if Init was entered. Native replacement opens first, retains the old handle
on failure, and closes the prior handle only after a successful new open.
Optional MultiplayerData likewise retires only its acquired handle and explicitly
owned downloaded byte buffer. DynamicEffect and RacingPhysics native pack owners
track successful schema acquisition and borrowed-vs-owned DB separately. Failed
loads roll back; repeated opens/close calls do not duplicate refs, and borrowed
databases are never destroyed. Their original Windows/console branches remain.
XtDbDestroy now iterates the actual live index on native, so sparse record slots
are retired. Audio's narrow nullable hook closes its existing pack/singleton
after owned subsystem retirement and before System release, as detailed in
[audio-xt-owner-order.md](audio-xt-owner-order.md); it acquires no schema ref.

## Focused verification and limits

`tools/macos/xt_owner_lifecycle_test.cpp` takes no arguments and links the actual
full-game/bootstrap-first closure with GameLogic include priority. It authors
PC32 Flask bytes for the real registered TranslatorMovementComponent inheritance
chain and ComponentCreator UID, not a fake DataRef or pool counter. A Debug
exclusive run under Metal validation passes with true process exit0. It covers:

- three lower-slot-hole DB destruction lifetimes and real construct/destruct
  callbacks; retained creator claims become null on record retirement and free
  actual Sorcery storage only after their last release;
- two app owner lifetimes with exact schema reference deltas, duplicate acquisition
  idempotence, missing-file DynamicEffect/Physics rollback and retry, real pack
  close/reacquire, and no destruction of the shared app database;
- actual AudioPack wrapper acquisition/nullable singleton retirement without a
  schema decrement; this is owner-mechanism evidence, not complete audio playback;
- actual loader reload with exactly one remaining strong/weak record ref, failed
  replacement preserving the previous record, and final app close of an acquired
  pack before C_Game dies; true static exit must satisfy the unchanged pools.

The fixture keeps one explicit outer schema lease until all repeated owners
finish, then releases it once. Existing XtGenerateApplicationSchema mutates
annotation fields (enum/flag labels and native type pointers), while schema
destruction does not restore them. A fully zero-ref schema cannot currently
regenerate in the same process; the fixture does not claim that unsupported
restart. Normal game/world reload retains the app schema until final process
exit, so this separate original limit is not changed by MAC147.

Production/fixture sources are frozen. Initial24/24 owner object probes and
companion Audio8/8 probes pass across all four configurations; the final28/28
owner-plus-fixture rerun passes. Private compile/link follows the
standard target closure and replaces only affected objects, with no link-order
workaround. Evidence is `_work/build/macos/xt-owner-agent/` (`matrix-final.log`,
`fixture.log`, `runtime.log`, helpers). Parent coherent integration and actual
full-game normal exit remain separate from these focused owner regressions.
