# Streamer pause cancellation and shutdown (MAC313)

The manual build 308 failure had two separate faults. A render clear rejected a
viewport larger than its target (MAC312). Afterwards all game layers logged
their destruction, but the process did not exit. The shutdown fault is an
unbalanced world-owned resource-loader pause, not an audio callback wait.

## Actual failure and source contract

The owned PID 88590 sample preserved at
`_work/runtime/verification/mac312/user-hang.sample.txt` shows the main thread in
`C_ApplicationBase::Shutdown` → `C_Manager::NativeDrainUnreferencedResources` →
`StopLoadingThread` → `MT::Thread::Join`. Its `ResourceLoadingThread` is in
`C_LoadingThread::ThreadMain` → `C_ThreadPause::WaitIfPaused` → native recursive
mutex entry. Audio and all game layers had already retired.

The matching log is `_work/runtime/user-play-0.1.3-audio.log`. Its final resource
pause is followed by `C_GameWorld::GlobalUpdate() Streamer started!`, without a
subsequent `Streamer finished!` or resume before the rendering exception.

`GameWorld.cpp::GlobalUpdate` deliberately pauses resource loading across updates
to give level streaming exclusive disc access. Its normal completion path
resumes the loader. `C_ThreadPause::Pause/Resume` in resource `Manager.cpp` owns
one recursive mutex acquisition per pause. The quit signal and status event in
`StopLoadingThread` do not release that pause mutex; the worker cannot inspect
the quit token while blocked behind it. Before this change, native world
destruction reset the streamer flags without releasing this outstanding depth.
The main thread then waited forever for a worker blocked by its own mutex.

## Owner-level repair

Only native `GameWorld.cpp/.h` changes production behavior. The world records the
exact resource-manager pointer and acquiring thread ID after its streamer pause
succeeds. The existing normal completion and native `Destroy` both release that
one acquisition. Cancellation releases it before resetting the streamer flags or
retiring world owners. Duplicate acquisition rejects; repeated release is a
no-op. Release verifies the acquiring thread and still-live resource manager
before touching the mutex, and clears its own record only after a successful
resume. It never loops over recursive depths, releases another caller's nesting,
force-unlocks a foreign thread's mutex, or bypasses the resource worker join.

Normal creator-thread cleanup with no streamer pause retains its existing
behavior. An outstanding pause must retire on its acquiring thread; a foreign
attempt is an explicit ownership error rather than a forced unlock. The observed
failure's final native shutdown runs on the original main owner after creator
retirement. The manager remains owned by CoreSystems beyond world destruction.
Windows/console paths and all simulation/render timing remain unchanged.

## Verification

The existing `world_transaction_test.cpp` gains two paths through the full real
World and resource worker: normal pause completion, and an authored render
failure followed by cancellation. Both use an additional real outer pause owned
by the test. After `World::Destroy`, a queued authored file must remain unread
until that outer pause resumes; it must then load successfully. The test also
checks duplicate acquisition, foreign-thread release rejection, repeated world
destruction and actual resource worker join/drain/restart. Cleanup can explicitly
release a retained world pause in an old-body negative control, allowing a
bounded loader-read failure instead of hanging the test runner.

The original negative evidence is the actual pre-fix manual process sample and
log above. A private differential can remove only the native Destroy release
call while retaining the same native acquisition/helper and coherent class
layout; the cancellation test must then fail its real queued-read deadline.
That is a controlled reproduction of the missing ownership edge, not a different
resource-manager implementation.

Private source probes are recorded in
`_work/build/macos/streamer-pause-agent/source-probes.json`. The native World
layout changed, so runtime validation must use a coherent rebuild of dependent
archives. Parent owns that build, the existing `mac_world_transaction` hardware
test (same original-data/shader/scratch arguments), and actual startup/error-exit
validation. Source compilation alone is not an exit or loading-screen pass.

Parent coherent Speed integration subsequently passed `mac_world_transaction`
and `mac_resource_shutdown` (**2/2 PASS**), covering actual worker pause nesting,
cancellation, join and resource drain in that rebuilt closure. Foreground manual
startup/error-exit validation remains separate from this focused acceptance.


Final source320 coherent Debug/optimized suites both pass148/148, including the
full-world/real-worker completion and cancellation cases. The source-qualified
final LAN run322 exits normally. Real-world cancellation failure is exercised
by the full owner fixture; ordinary actual-game exits are separate acceptance.
