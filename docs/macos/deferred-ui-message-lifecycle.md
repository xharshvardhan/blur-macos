# MAC142 — Deferred UI message shutdown ownership

Actual MAC139 completed every owned layer Destroy and closed the PAK archives,
then exited245 with SIGSEGV. The crash report
`BlurNativeGame-2026-10-27-043435.ips` shows the primary cleanup fault:

`atexit → UI::gButtons → C_ButtonController destructor → deferred queue destructor
→ DynamicPool<C_DeferredUiMessageStore> assertion(pool.h271)`.

The pool still owned message entries because the original queue destructor was
empty. Its assertion then entered logging after a static logger/host string had
already been destroyed, producing the observed `strlen(0xfefefefefefefefe)`
crash. This is distinct from MAC139's earlier vertex texture dimension error.
Changing logger lifetime or suppressing the pool assertion would leave the real
message/payload ownership leak.

Native UI now explicitly owns deferred delivery. NativeInitialise claims the
controller queue after its existing uniqueness gate and before fallible work.
An initialization failure closes/cancels that acquired stage immediately, before
returning false. The owner destructor also closes it before screens, gadgets or
Lua are destroyed. A rejected second owner never acquires the stage and cannot
cancel another owner's messages. A new valid initialization can reopen delivery.
Original queued pre-UI boot messages remain allowed before the first owner;
Begin does not discard those messages or reset the two-buffer permutation.

Controller Close holds the original producer lock, rejects later deferred posts
and cancels both buffers without dispatching callbacks. The main consumer must
already be quiesced: Close is not a new arbitrary consumer/close concurrency
contract. Producer concurrency remains protected by the existing lock. Native
broadcast uses an RAII lock so a failed payload copy cannot strand that lock.
Windows/console behavior is unchanged.

Queue cancellation unlinks each actual FIFO entry and calls its real pool Free.
Free invokes `C_DeferredUiMessageStore`'s destructor, which deletes the owned
copied byte buffer. It does not run a copied message's C++ destructor: the
original message contract requires memcpy-compatible POD data and does not
transfer ownership of embedded pointers. A failed payload allocation retires its
already allocated store. A throwing callback now retires the popped store via
RAII; any remaining/reentrant messages remain owned by the actual buffers until
normal processing or cancellation. Native controller and queue destructors also
cancel as fallback without dispatching into an already retired UI. The entire
queue owner must still be destroyed while its allocator is live: cancelling
payloads alone retains the pool blocks and vector capacity until queue
destruction. This does not promise arbitrary post-allocator static ownership.

Production files are the original Mercury UI ButtonMessaging h/inl/cpp,
ButtonController h/cpp and Amax UiManager h/cpp. The native controller and
UiManager ownership fields change native layout and require coherent consumer
rebuild. No Core allocator, pool-assertion, logger or gameplay/tick change is
made.

`tools/macos/deferred_ui_messages_test.cpp` has no arguments and uses the real
message/controller/subscriber/store/pool implementations. Focused Debug compile,
link and exclusive runtime pass with true process exit0. It verifies:

- owned byte copies remain independent of later caller payload changes;
- cancelling four64KiB messages releases their exact stores once and reclaims
  actual malloc-zone in-use bytes (the engine memory tracker is inactive in
  this private closure; no fake allocation tracker is used);
- a real subscriber callback throws after posting a reentrant message, the
  popped entry is retired, both remaining buffers cancel without callbacks;
- closed delivery rejects late posts, three legitimate lifetimes reopen;
- four real producer threads posting while the main owner closes cannot retain
  entries after Close or trigger callbacks;
- the real UiManager uniqueness gate rejects a second owner while preserving
  the prior queue's pending delivery;
- two real UiManager initialization failures against missing required UI bundles
  with a real Display device cancel pending and late messages **before** failed
  owner destruction; a new owner can enter and retire again;
- a zero-initialized static owner pointer acquires a real queue/payload in main;
  a main-registered atexit callback deletes the entire queue while the game
  allocator is live, exercising destructor fallback through actual process exit0.

The three affected production objects compile in all four native configurations:
12/12 probes pass, and scoped diff-check is clean. Evidence is under
`_work/build/macos/deferred-ui-agent/{matrix.log,runtime.log}`. The initial
private link put the bootstrap object first, which accidentally made an authored
namespace-static queue outlive its allocator safely in that one ordering. The
standard MAC145 CMake link exposed the invalid authored lifetime: Mach-O
`__init_offsets` put the fixture initializer at index1, PreStatic101 at2,
SBA102 at3, but actual production ButtonController/gButtons at895. The old
private link had PreStatic101/SBA102 at0/1 and fixture at4. Production gButtons
already has valid allocator-relative ordering; no allocator, production owner
stage or link-order workaround was needed.

The corrected fixture registers complete queue retirement from main, after
allocator initialization. A private link copied the exact standard target edge
and replaced only the fixture object, preserving the CMake object/archive order.
Strict Metal validation runtime and true process exit0 pass; evidence is
`deferred-ui-agent/standard.runtime.log`, `initializer-order.json` and
`standard_fixture.py`. Integration uses GameLogic includes before Amax and the
actual full-game closure with no arguments. Parent integrated leaf rerun, full
successful UI init/reinit and actual-game clean exit remain separate; this
focused fixture is not gameplay proof.
