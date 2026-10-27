# Borrowed application audio Xt pack lifetime (MAC147 companion)

The Amax path owns the `C_XtAudioDb` heap singleton and its two pack-open
results; it borrows the application database and schema. `Create()` only
allocates the singleton. It does not call XtInitialise. The standalone
`XtInit()` path is different and is not used by Amax.

`GameAudioLayer::Create` opens AudioEntities.bin and Audio.bin against
`C_GameApp::GetGlobalXtDatabase()`, builds runtime entity definitions, then
flushes EntitiesPack. AudioPack remains open. `C_XtAudioDb::Release(db)` flushes
the actual AudioPack/EntitiesPack handles, deletes the singleton and clears the
static pointer. The second flush is a no-op after the earlier EntitiesPack
retirement. This path must not decrement schema initialization ownership.

## Callback dependency proof

The registered Audio pack callbacks are in audio/AppInterfaceLayer/XtAudio.cpp:

- EntityDefCallback: construct initializes indices/registration; destruct only
  emits the existing diagnostic log.
- AudioObjectCallback, EntityGroupCallback and AudioEntityProxyCallback: only
  handle construction, with no engine calls on destruction.

The C++ proxy fields contain owned strings/vectors, nonowning XtHandles and
scalar audio handles. No destructor calls ReleaseEntityType. XtHandle itself
is a record/salt/offset value without ownership increments or a destructor;
clearing the singleton's handle lists is not pack-reference retirement.
XtPackClose is the operation which actually decrements acquired pack refs and
runs object destruction when their strong counts reach zero.

These pack-close callbacks do not depend on Audio System, InterfaceController,
WaveManager or game audio subsystems. By contrast, live proxy consumers do:
Music playlist release, UI entity release and Default subsystem release call
XtEntityProxy::ReleaseEntityType, which reaches InterfaceController's subsystem
lookup and reverses scripts using the live audio system and wave resources.
Keep AudioPack alive until all such subsystem consumers and asynchronous
loaders have retired. Do not infer that merely closing the pack reverses
runtime audio entity types or balances independent resource-cache pins.

## Chosen native order

1. Join music and UI loaders, preserving actual failure status.
2. Retire the owned music/UI and remaining subsystem objects, with System
   subsystem last, using MAC138's existing live lookup lifetime.
3. If the audio Xt singleton was created, close its exact borrowed-db packs and
   delete it through NativeReleaseBorrowed(appDb).
4. Release the audio System, WaveManager and InterfaceController table using
   their existing owner operations.
5. The app/global owner later closes remaining packs, destroys its database and
   balances its own schema initialization references.

The native nullable query avoids accessing an app database on an unentered
AudioLayer Destroy. NativeReleaseBorrowed is a no-op when no singleton exists;
it rejects a standalone internally-owned database or live packs without their
borrowed database instead of pretending they were closed. Windows and the
standalone API retain their existing paths.

All eight focused source probes pass (XtAudio.cpp and GameAudioLayer.cpp in
Debug, Speed, DebugUBSan and SpeedUBSan). No shared build or runtime test was
performed by this companion; the combined MAC147 owner fixture and actual-game
teardown remain required integration evidence. In particular this pack fix is
not asserted to resolve the independently captured late component-data pool
assertion.
