# MAC195 — Populated frontend audio final-owner boundary

Final focused verification: root MAC197 rebuilt coherent Debug and Speed audio,
application and fixtures. The corrected standard Debug `mac_audio_stream_retirement`
passed with actual process exit0 in0.69s. The full19 affected checks in both
configurations and actual populated Master normal-exit run are separate root
verification stages; no actual frontend clean-exit claim follows from this leaf.

Actual MAC194 loaded the real career save (version1378, raw50903, zero unread),
completed both introductory movies, performed the original intro autosave, and
reached the SP hub/first unlocked event. Ordinary input backed out to Start.
CLI quit initiated layer cleanup, but Audio destruction asserted. Exit250 is
not clean menu shutdown. MAC196 captured the exact before-unwind owner:

```
PagedArray<Audio::Core::FileStream*>::~PagedArray
FileStreamManager::~FileStreamManager
WaveServer::~WaveServer
Audio::System::~System / System::Release
C_GameAudioLayer::Destroy
```

Evidence: ignored `verification/mac194-game.log` and `mac196-game.log` under
`_work/runtime/`. The diagnostic inferior was explicitly killed; its wrapper0
is not a normal-exit result. No live stream count was captured, so this report
makes no count claim about that process.

## Ownership and actual worker contract

The earlier MAC175 assertion was late static InterfaceController poll-info
retirement, a different owner. Its real observation/reference cleanup remains
intact. This failure is the low-level file-stream owner during actual System
release, before static teardown.

| Acquired object | Original owner and release contract |
| --- | --- |
| AudioGraph | WaveServer's PagedArray; DestroyStream invokes node destruction, frees the actual graph slot, deletes graph/output data. |
| StreamSource | WaveServer's source table; DeregisterStreamSource flags its actual file wrapper for release, deletes source and updates count. |
| FileStream | FileStreamManager's PagedArray; DoStreamDestroy decrements the referenced wrapper, deletes stream/chunks, frees the actual handle and updates original global stream count. |
| FileStreamFileWrapper | Manager's fixed table; stream reference count must reach zero; DoFileRelease closes/deletes its actual owned I_File and wrapper. Unopened wrappers are also owned. |
| Chunk | Stream's new[] allocation until accepted deallocation request transfers it to loader; loader actually delete[]s it. A status/result record never owns a chunk. |
| Accepted read | Loader request contains a borrowed wrapper and destination pointer. Both remain alive until the actual read completes or loader joins. |

Previously native WaveServer destruction only stopped the loader, while the
manager destructor was empty. Renderer shutdown also stops its normal update
consumer, so queued Destroy/Release work cannot be expected to run later.
Loader Stop drains accepted requests and joins, but deliberately discards result
notifications when its renderer consumer is gone. Therefore an unfinished
logical `m_bLoadRequestSubmitted` may survive a physically completed read.
Passing a fake successful acknowledgement to clear it could resubmit reads and
would invent readiness.

## Native final-owner correction

The actual repository has no same-System Shutdown→Init caller: production
Shutdown is called by System::Release and its destructor. Output quality/device
changes use the separate Engine path. This change does not claim post-close
reinitialization of the same System instance.

`System::Shutdown` now deletes/joins the real renderer first, then calls
`WaveServer::NativeRetireOwnedStreams` while System's queues, file/chunk owners,
allocation scopes and services remain alive. The WaveServer destructor repeats
this stage for partial construction and idempotence. The final stage:

1. Stops/joins the actual loader while accepted read/free payloads remain valid.
   Native accepted-drain termination follows explicit Stop, not the unrelated
   global `g_bQuitAllTasks` early exit. Accepted chunk-free requests also bypass
   the read-suppression debug flag: toggling reads off cannot abandon memory
   whose ownership already transferred to the loader. Reads retain that flag.
2. Closes the acquisition generation and discards remaining status-only results.
3. Retires actual graph/source entries using original owner methods.
4. Invalidates each residual stream request ID and clears its logical pending
   flag only after physical join, marking destruction without success callbacks.
   Original DoStreamDestroy balances its real file reference and pool handle.
   Its legacy bool return is always false even on successful deletion; the
   native caller verifies actual handle invalidation rather than that value.
5. Deletes post-join chunks synchronously; closes only actual zero-reference
   owned wrappers. PagedArray and active-list assertions remain enabled.

Late file/source/graph creation is rejected after retirement. Final checks use
actual graph occupancy; the original `m_u32StreamsInUse` counter has no increment
anywhere in the source and is not treated as evidence or repaired here.
`DoFileRelease` uses a native scope guard so the unopened-wrapper path balances
its allocation-ID/tracker scopes too. Original Windows paths are preserved.

Native `SubmitLoadRequest` publishes request ID, byte count, next chunk and
pending state only when `LoadData` accepts the physical request. On rejection
its original chunk remains owned and available. A rejected chunk-free enqueue
retains its actual pointer in the manager's native deferred-free vector. Ordinary
Update retries ownership transfer without blocking the renderer; residual
pointers are delete[]d after loader join. No early deletion, arbitrary pool
clear, fake acknowledgement or borrowed-object deletion is used.

The manager's native flag/vector change object layout; all audio/System users
require a coherent rebuild. No PC asset/wire fields, audio algorithms, fixed
simulation ticks, unlocked render timing or normal result/callback contracts
were changed.

## Focused verification and limits

`tools/macos/audio_stream_retirement_test.cpp` takes one ignored scratch-parent
argument and uses the same leaf engine/audio closure as `audio_system_bank`.
It creates two actual System lifetimes with a live original file loader and no
renderer acquisition, making direct graph control deterministic. A forwarding
filesystem gate blocks a real native file read, then delegates actual bytes and
read result. It fills the real bounded request queue, checks rejected reads do
not publish pending state, and checks its rejected initial-chunk free remains owned.
One lifetime verifies residual post-join release; the other verifies ordinary
manager retry before final release. A delayed gate release proves Shutdown
waits for actual accepted reads. It also covers global-quit draining, opened and
never-opened wrappers, all actual derived owner counts zero before System
Release, repeated Shutdown, new-generation rejection, queued frees with debug reads suppressed, and a separately owned real
file remaining readable.

This partial-owner fixture does not claim audible output, original menu bank
playback or actual frontend clean exit. Existing `audio_system_bank` covers
real renderer initialization/PCM callbacks; root's next populated Master menu
run must verify the original MAC194 shutdown boundary with assertions enabled.

Production source probes: five actual TUs in Debug, Speed, DebugUBSan and
SpeedUBSan all pass (20/20). The new fixture also compiles in all four (4/4).
The final debug-flag condition and fixture toggle were re-probed all four
configurations (8/8). Logs: ignored `_work/build/macos/mac195-agent/`. Initial coherent MAC197 fixture run found an authored host prerequisite error:
GateFs inherited the singleton C_FileSystem despite the engine already owning
that singleton. The adapter now implements I_FileSystem, forwards every
operation to the existing native filesystem and wraps only its real returned
file; default filesystem is restored. Corrected fixture source probes pass4/4.
Production unchanged. That checkpoint awaited a corrected root MAC197 leaf rebuild; final
focused result is recorded above.

## Source audit boundaries not identified as MAC194's cause

UISubsystem's derived destructor does not itself call ReleaseUIEntity; generic
Subsystem raw object deletion differs from ordinary reverse-script teardown;
CreateEntityInstance can publish a null result. These were candidate classes
before MAC196 identified the exact FileStream owner. They are not silently
changed or reported fixed by this low-level streaming repair.

Further authored host corrections: main establishes the native default filesystem
before using its asserting getter. The real FileStream::Setup forces the initial
chunk count to one, so expected counts follow that original policy, rather than
the authored request's former four. A fixture owner fallback releases the read
gate and joins/releases the actual System before scratch/adapter teardown on
exceptions. Private runtime then exposed the new caller's invalid bool assumption
above; that was corrected without altering DoStreamDestroy's original contract.
Changed manager+fixture source probes pass8/8. That checkpoint awaited the coherent leaf rerun; final result is above.

The corrected exact-standard-argv private fixture now uses scoped actual cwd and
short `stream.pcm` bank names, preserving the original 64-byte filename bound.
Scheduler shutdown also has an explicit host scope on exceptions; the earlier
timeout after filename rejection is not evidence of a loader Stop hang. A direct
actual partial System→oversized-name rejection→Shutdown→Release stage passed.

The next private run completed all first-cycle stream/wrapper checks, then LLDB
captured an actual recreate ownership fault: ObjectManager::~ObjectManager89
called System::Free on the static m_pfMixerFrames owner. The first System had
freed it without nulling; Init allocates only when null, so the second lifetime
skipped reacquisition and later double-freed stale storage. Native destructor
now nulls that exact pointer after matching real Free. Other destructor statics
are scalar/diagnostic or unconditionally reacquired batch queue/group state; no
additional speculative change. Proof: ignored mac195-agent/lldb.log, explicitly
killed inferior. The fixture tests actual three System lifetimes (one rejected
partial owner plus two queued stream owners), not a synthetic tracker reset.
ObjectManager/manager/fixture source probes pass12/12 across4configs. Root
subsequently rebuilt both coherent applications; the standard Debug leaf passed
exit0 as recorded above.
