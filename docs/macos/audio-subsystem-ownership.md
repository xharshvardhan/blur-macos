# Explicit native game-audio subsystem ownership (MAC138)

Actual MAC137 shutdown reached all layers Destroyed but the game subscriber pool still held audio objects: ten UI, three System and two Dialogue subscriptions. The private owner dump is `verification/mac137-subscriber-owners.json`. These match the original GameAudio subsystem registrations; the actual process was diagnostically stopped, so that run is not clean-exit proof.

## Ownership and order

`GameAudioLayer.cpp` allocates eleven subsystems with `new`: Default, Music, Vehicle, UI, Dialogue, DynamicObj, System, Wheel, Listener, PickUp and Collision. Original `InterfaceController::Release` freed only its pointer array. It did not destroy those objects, so the MessageHandler base destructors that remove subscriptions never ran.

Registration cannot universally imply ownership: original Audition callers register a global `&testSystem` in `amax/applications/Audition/Audio/TestSectionAudio.cpp` and `mercurygame/applications/Audition/Audio/TestSectionAudio.cpp`. `AddSubsystem` therefore remains borrowed. Native `AddOwnedSubsystem` transfers only successful explicit acquisitions; the game layer keeps each pending object in a unique_ptr until registration succeeds. An interrupted allocation/registration sequence retires the acquired prefix.

The layer now retires both Music and UI loading threads before deleting any subsystem, without holding the layer's async lock. Each worker uses that lock and still needs the layer, controller, audio System, WaveManager and Xt audio database. Never-started and already-retired loaders are inert. A worker failure observed after termination is logged; cleanup continues and the layer returns failure. A loader cannot join/delete its own owner.

Music and UI are then destroyed before other clients, with System last. MusicPlaylist destruction calls `InterfaceController::ReleaseEntity` and `XtEntityProxy::ReleaseEntityType`; those paths require the live controller table and original System managers. Each owned slot stays valid through its virtual destructor and is nulled only afterward. Subsystem has an original virtual destructor, and the UI/System/Dialogue MessageHandler bases remove their own subscriptions. UI also retires its real scheduler group. No new shutdown message is fabricated: the current game subsystems do not implement the otherwise-declared eShutdown message.

Only then does the existing layer sequence release Audio::System, its native renderer/output workers, WaveManager and the controller table. System::DoWork delivers return callbacks on the main thread, so teardown does not pump them against retired metadata. The native renderer's existing shutdown stops and joins its consumer/producer work before freeing its buses. The change does not alter mixer timing, audio algorithms, bank formats or Windows code.

Native controller Release also balances any remaining explicitly owned standalone entries, preserves borrowed objects, and resets pointer/count/table state. It rejects an attempt to retire a resource-backed owned subsystem after its real audio System has already gone. Native ownership metadata is appended to the runtime class, preserving earlier field offsets; the native class size changes and needs a coherent rebuild.

## Verification and limits

`tools/macos/audio_subsystem_ownership_test.cpp` takes no arguments or assets. It uses the full-game link closure, generated Lua bundle and bootstrap-first library policy, with GameLogic includes first. It starts no game, service, graphics device or audio output.

The fixture covers borrowed destructor ownership, successful/rejected/null/partial acquisitions, live self-lookup during virtual destruction, repeated Release and no-resource Init, actual Music/UI subsystem objects, real MT loader completion/deletion, real scheduler-group retirement, and the exact fifteen source-derived UI/System/Dialogue subscriptions. Real subscriber-pool destructors enforce that all subscriptions are removed. Registration is source-derived in this isolated fixture; it does not fake eInit, a game singleton, entity creation or bank data.

All four production source configurations pass (Debug, Speed, DebugUBSan, SpeedUBSan; sixteen actual source objects). Private Debug and Speed fixture links and executions pass with true process/static exit0. Root MAC139 coherently rebuilt the actual Debug game and integrated fixture, and the integrated `audio_subsystem_ownership` check passed. The complete 45-check batch was still running when this result was recorded. Actual populated-world shutdown remains a separate gate.

This does not establish loaded-bank reload. EntityManager definitions, callback/data-tracker collections and bank/Xt caches retain their original lifetime policies. Resetting the controller's safe table pointers is not a claim that those resource graphs can be reloaded. Actual queued entity reversals, loaded playlists, output completion and game clean exit still need the real path; full menus/saves/reload remain deferred until the one-track/car slice is complete.
