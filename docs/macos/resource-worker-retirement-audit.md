# MAC230 — Cross-owner worker quit event during World rollback cleanup

## Preserved failure and actual thread proof

The initial218 optimized World transaction timeout remains recorded in
`resource-worker-pause-audit.md`; its later passing retry does not erase it.
Root225 then reproduced the unchanged optimized fixture after three passing
fresh processes. That short diagnostic budget did not recover a usable thread
proof. Root229 reproduced again and captured all threads before its owned
watchdog termination:

```
_work/runtime/verification/mac229-stress-a/cycle-1/
    threads.sample.txt
    lldb.log
    thread-proof.json
    fixture.log
    status.json
```

229's diagnostic exit-9/29.565s is an explicit owned watchdog kill, not a clean
process exit or gameplay proof. The captured main stack is:

```
MT::Thread::Join
Framework::C_LayerManager::NativeQuiesceCreation
Framework::C_ApplicationBase::Shutdown
Framework::C_Application::Shutdown
Framework::C_Application::~C_Application
main
```

The live LayerManagerThread waits in `MT::Event::Wait` from its main loop. There
is **no ResourceLoadingThread** in the all-thread capture. Both native I/O
workers are idle in `NativeIoDevice::Run`'s queue-condition wait. The sample and
LLDB agree on that boundary; optimized source-local values are unavailable.
The final printed `AsyncFile::Closing worker-1.bin` therefore does not identify
the blocked owner. The process has reached application destruction. No source
I/O change or request-success fabrication follows from this evidence.

## Corrected active-source classification

`resource/Manager.h19` unconditionally defines
`REMOVE_BLOCKING_RESOURCE_MUTEX`; its265–268 implementations of
BlockingMutexWait/Release are empty, and292 excludes the old member. The
StopJoin-versus-main-owned blocking-mutex candidate in222 is dormant in this
actual source configuration. Do not repair or attribute this reproduction to
that inactive lock. The original222 observations remain useful; this audit
supersedes that candidate for the captured229 reproduction.

The other reviewed active boundaries remain:

- `Manager.cpp135–157` uses a real pause checkpoint mutex. Native World Create
  acquires/resumes only its owned pause through the existing scoped helper;
  the exercised false/throw pre-physics paths are balanced.
- Manager load/eviction/postload publication is protected by its actual
  recursive atomic resource-list lock. PartialUpdate is not a completion
  timeout around every underlying callback.
- `AsyncFile.cpp53–69` prints Closing before waiting `_pendingOps` and before
  synchronously waiting its real native close. Event and Release callbacks
  acquire the same Artery mutex; Release separately decrements pending count.
- `NativeIoRequest.cpp75–91` publishes nativeDone only after actual callback,
  callback Release and queue-self reference retirement. `IoRequest::Wait`
  waits that publication. Queue admission, wake or an early log is not close
  completion. None of these paths is present as the blocker in229's capture.

## Actual signal ownership fault

Both original constructors used exactly the name `EventSignalQuit`:

- `Resource::C_LoadingThread`, `resource/source/Manager.cpp163` before repair.
- `Framework::C_LayerManagerThread`, `framework/source/layer/LayerManager.cpp15`
  before repair.

The native `MT::Event` implementation deliberately preserves named desktop
sharing through `Named<EventState>`; it retains/coalesces a signal and resets
it after one successful waiter. That generic contract is correct and remains
unchanged. These two unrelated private quit owners inadvertently select the
same event. Their ShouldQuit methods each consume it with `Wait(0)`.

During native layer quiescence, the manager signals the shared quit event and
its own separate LayerStatusDirty event. An active resource worker can consume
that quit and exit. The layer worker consumes its status wake, finds the shared
quit already clear, and waits for status again. The main thread joins the still
live layer worker forever. This accounts for229's exact vanished-resource,
live-layer-waiting, main-layer-join combination. It is further demonstrated by
the deterministic old-code actual-worker regression below; no inference from
just the last file log is required.

The native fix gives each worker constructor an **unnamed private quit event**.
It covers both families and distinct instances of the same family. It does not
rename one family to another fixed global name, which would retain same-family
aliasing, and does not alter general named event sharing. Original Windows and
console constructor branches remain unchanged; no header/object layout changes.

## Original PC versus native owner lifetime

The original Windows Event.cpp212–216 calls
`CreateEvent(NULL,FALSE,FALSE,pszName)`, which shares a same-name auto-reset
kernel object. Thus the generic native implementation is not at fault for
honoring that contract. Original `C_GameApp::OnShutdown`'s PC branch explicitly
uses `TerminateProcess(GetCurrentProcess(),0)` rather than walking these final
owners. Original LayerManager's destructor also does not perform the added
native ordered quiescence. Native `ApplicationBase::Shutdown512+` joins layer
creators, destroys owned layers/resources and returns to its host; the full
World fixture exercises application destruction while its resource manager
still exists. The port must give each actual stop owner a separate signal to
support that real join/return lifecycle. It does not replace original work,
worker scheduling, callback completion or failure propagation with a shortcut.

Stop-before-entry remains safe: auto-reset events retain the private quit until
the intended worker's first ShouldQuit. Signal coalescing within one owner is
unchanged. Native restart continues consuming only that joined owner's residual
signal before starting a new worker; repeated quiescence remains idempotent.
The manager's status wake and normal work publication mechanisms are unchanged.
No sleeps, longer timeouts, no-op join, synthetic completion or removed check is
used as a repair.

## Meaningful baseline and regression

Added `tools/macos/worker_stop_ownership_test.cpp`. It first instantiates the
**real** two worker classes without starting them, signals Layer quit and tests
the real Resource ShouldQuit. This makes the old fault deterministic without
risking an old-code join hang. Linked against the unchanged218 Debug archives,
`--isolation-only` exits1:

```
Layer quit was consumed by Resource worker
```

Fixed private compilation replaces only the actual two implementation objects
before the same existing framework/resource archive closure. It proves:

1. Reciprocal cross-family and same-family stop isolation through real
   SendQuitMessage/ShouldQuit methods; each own signal still auto-resets.
2. General same-name MT::Event sharing remains working as before.
3. Real C_Manager/C_LayerManager coexistence with24 asynchronous resource
   Load/PostLoad/Free callback lifetimes and12 async layer creation/destruction
   and manager restart lifetimes. Resource loads complete after the layer
   worker has quiesced, and resource drain/restart does not retire its live
   layer-worker peer. Callback ownership is balanced exactly once.
4. Thirty-two additional immediate quiesce/repeated shutdown/restart lifetimes
   cover requests made before a new worker gets its first time slice. This is
   not a scheduler guarantee that every run selects that interleaving.

Debug, Speed, DebugUBSan and SpeedUBSan each passed the two production object
compiles and the fixture compile:12/12. All four private executable links
passed, and each configuration's isolation-only plus full real-worker run
exited0:8/8. No sanitizer failure was observed. Existing actual archive paths
are read-only; the private link explicitly overrides the two changed objects
and does not modify shared archives. Logs and exact link commands are under
`_work/build/mac230/<config>/`. The full test uses authored CPU callback
payloads without file/device/GPU/Artery initialization, not original data or a
running game. It does not claim actual World/normal career success.

The new standard leaf needs the same include/link closure as layer_failure:

```cmake
add_executable(blur_mac_worker_stop_ownership_test
    ${BLUR_TOP}/tools/macos/worker_stop_ownership_test.cpp)
target_include_directories(blur_mac_worker_stop_ownership_test PRIVATE
    ${BLUR_MAC_FRAMEWORK_ROOT})
target_link_libraries(blur_mac_worker_stop_ownership_test PRIVATE
    blur_mac_framework)
add_test(NAME mac_worker_stop_ownership COMMAND blur_mac_worker_stop_ownership_test)
set_tests_properties(mac_worker_stop_ownership PROPERTIES TIMEOUT 30)
```

No original data, GPU label or command-line argument is required. Root owns
standard integration and coherent rebuild, then unchanged original World
rollback/stress repeats and the other application/worker owner checks. The
original World fixture and its90-second CTest limit are unchanged. Those
coherent standard and actual original-PAK/GPU repeats are pending at handoff;
private four-configuration success does not substitute for them.

## Coherent integration follow-up (MAC231)

The standard Debug and Speed applications now build with both private quit
constructors and the standard worker fixture. Each configuration passes all19
affected integration checks, including worker_stop_ownership and the unchanged
World transaction test. Ten additional fresh optimized World processes pass
without a watchdog or diagnostic trigger (maximum0.2803s). The World fixture
and its90-second CTest timeout remain unchanged. This validates the reproduced
rollback class; full normal-career completion/results is a separate actual-game
acceptance boundary.
