# MAC222: World rollback and resource-worker timeout audit

This bounded audit made no production/test/timeout changes and launched no
additional runtime. It does not explain the observed timeout.

## Preserved observations

The coherent218 Debug selection passed10/10. The initial Speed selection
passed9/10 and timed out `mac_world_transaction` after90.04 seconds. Its output
is preserved in `_work/build/macos-release/mac218-tests.log`. The subsequent
focused Speed retry passed1/1 in0.28 seconds; that separate result is
`_work/build/macos-release/mac222-world-repeat.log`. The retry overwrote CTest's
`LastTest.log`, so it must not be presented as the first run's record.

The initial output reaches `World effect failure boundary 1` and reports
`AsyncFile::Closing .../worker-1.bin`. `AsyncFile::~AsyncFile` emits that message
**before** waiting for `_pendingOps` and before native `IoFileClose(...).Wait()`.
It establishes that closing began, not that close completed or that the worker
published post-load readiness. Manager log output and fixture/AsyncFile output
also use different logging paths, so their printed interleaving is not an exact
cross-thread execution trace. No stopped-thread stack exists for this timeout.

## Actual exercised boundary

`world_transaction_test.cpp` constructs the real Resource manager, original PAK
overlay and native GPU device. Its injected postprocess effect owns a real
texture, then returns false in mode0 or throws in mode1. Both failures occur
inside `C_GameWorldPostProcess::Create`, before `CreatePhysics`, scenegraph,
actor list, resource loaders and race logic creation. WorkerProof queues a real
resource/file read between failures; it requires readiness and exactly one
load, then releases that client and updates the manager.

The fixture does not construct, initialize or update a RaceLogicLayer. The
changed217 RaceLogic runtime method is therefore not invoked by this exercised
pre-physics failure path. Merely linking the full game types/static registration
does not constitute running race logic, and this source observation alone is
not a timing-based A/B exoneration of every linked translation unit.

Mode0 must have passed its rollback, WorkerProof and resource-baseline checks
for the loop to enter boundary1. The final close message does not distinguish
the later mode1 worker completion, main-thread update, world destruction or
final resource drain.

## Pause, wake and accepted I/O contracts

Native `C_GameWorld::Create` uses a local `LoadingPause` that records only its
owned pause. The explicit false-effect branch resumes before returning false;
the outer success/failure wrapper resumes again only if still owned. The
exception wrapper resumes before `Destroy` and rethrow. The scope destructor
also retires an outstanding owned pause. No unbalanced pause was found in these
two exercised source paths.

`C_ThreadPause` uses its named mutex as a checkpoint: `Pause` acquires it,
`Resume` releases it, and the loading loop waits/releases it before eviction
and loading work. This is a checkpoint mechanism, not by itself an accepted-I/O
completion fence. Manager pause/resume signal the separate status-dirty event.
The native event retains a signal before a wait and auto-resets one waiter,
matching the existing Win32 event contract.

`UpdateLoadQueue` drops the resource-list lock, acquires the separate blocking
mutex, runs the real `LoadResourceData`, then releases that mutex and publishes
PostLoading under the list lock. `PartialUpdate` releases the main thread's
blocking mutex, processes post-load/pre-delete work and reacquires it. Thus the
four-second WorkerProof polling deadline does not bound time spent inside a
blocking update call or inside the worker's file-close barrier.

`AsyncFile` waits its accepted buffer operations before synchronously waiting
for native close. `IoRequest::Wait` waits `nativeDone`; native completion runs
the callback and release before publishing that flag and notifying waiters.
This source contract prevents interpreting an API submission or close log as a
completed I/O operation. The timeout provided no actual `_pendingOps`, request
completion or I/O-worker stack values.

## Candidate to test only if the failure recurs

`NativeDrainUnreferencedResources` and the resource manager destructor call
`StopLoadingThread`, which signals quit/status and joins. The main thread
normally owns the separate blocking mutex between updates. A worker that has
already passed its quit checkpoint and is waiting to acquire that mutex for a
queued load could make a synchronous join wait for a mutex held by the joining
thread. This is a source-supported lock-order candidate, **not** evidence that
the218 timeout reached that sequence. The initial log's last line is also
compatible with an incomplete accepted file close. Neither explanation is
selected without a live stack.

If a bounded repeat stalls again, capture all owned-process threads before
termination: main pause/update/close/join site, resource worker checkpoint/list/
blocking mutex site, and native I/O worker request/callback state. Record queue
states, pause owner, quit/status signals and accepted operation counts from that
stopped process. Preserve the first failure and each retry separately. Any
repair or targeted stress extension belongs to a later evidence-backed task;
the passing retry does not erase the unexplained initial failure.
