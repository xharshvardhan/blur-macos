# MAC275: native allocation domain during late framework work

The full Debug suite's `mac_deferred_ui_messages` crash was a native allocator
lifetime race at process exit. The two missing `ui_common.bpr` messages are
intentional negative initialization inputs in that fixture. They did not locate
the crash. No failed-resource gate, UI ownership check or assertion was removed.

## Actual fault and source contract

The macOS report `blur_mac_deferred_ui_messages_test-2026-10-27-143835.ips`
records the main thread in `exit` / `__cxa_finalize_ranges`, destroying a mutex.
Fault thread 1 is an Apple utility queue executing Metal context telemetry:

```text
Metal context telemetry block
  → CoreAnalytics::Client::sendEventAlways
  → JSON serialization / libc++ string growth
  → global operator delete
  → Memory::BizInternalFree
  → BlockAllocator::Free
  → PageUsage::GetBit
```

Native global `operator new` also serves C++ allocations in loaded frameworks.
Those workers are not game-owned threads that CoreSystems can join. The old
global allocator destructor cleared its initialized flag and deleted its mutex
and area metadata; its PageUsage member then freed the ownership bitmap leaves.
A worker could pass a live check before destruction and subsequently access
that retired metadata. The existing genuine-malloc classification is useful for
malloc-backed buffers, but cannot protect a framework buffer allocated from the
game's small-block domain while it was live.

The fresh debugger run and the preserved older whole fixture both exited
successfully. That does not disprove the captured race or identify when it was
introduced. The older run also used the repository working directory rather
than CTest's build directory, so it is not an identical-environment differential.
Its negative resource directory is absolute. Earlier late-deferred-pool failures
remain separate evidence; this repair follows the actual CoreAnalytics stack.

## Narrow lifetime correction

Only the native global `g_SmallBlockAllocator` declaration in
`AllocatorFunctions.inl` uses Clang's `no_destroy` attribute. Its priority 102,
type, symbol, pre-static initialization and bootstrap link order are unchanged.
Unsupported native compilers fail clearly at the feature gate. Clang documents
[per-variable exit-destructor omission](https://clang.llvm.org/docs/AttributeReference.html#no-destroy);
this is not a blanket switch disabling static destructors.

The native domain, its mutex and classification bitmap remain available until
the OS ends the process. Before bootstrap initialization, allocations still use
the existing malloc path. Windows/PC destruction and explicitly owned local
BlockAllocator objects are unchanged. CoreSystems shutdown does not destroy
this global domain, so no per-CoreSystems owner is made immortal by the change.

Payloads still follow the original allocation/free paths. Empty small-block
pages are returned by `Area::FreeBlock` / `FreePage`; keeping domain metadata
alive does not authorize retaining game resources, caches or client payloads.
No late-free leak fallback, telemetry disabling, delay or forced exit was added
to production code.

## Deterministic validation

`process_allocator_lifetime_test.cpp` uses the actual global replacement
allocation functions and native allocator. A verifier registered at priority
101 runs after the old priority-102 allocator destructor would have run. It
releases an owned pthread during late finalization, exercises 4,096 small/large
new/delete pairs, retires an actual retained small-block payload, and checks
the late allocation count returns to its expected baseline. Eight earlier
payload generations also balance. An allocation made before bootstrap remains
genuine malloc storage and is classified and freed correctly during late exit.

The negative control changes only the private copy's `no_destroy` token. It
exits with code 73 at the dead-domain check before releasing the worker or
accessing dead metadata. Production code contains no negative-control switch.
All four configurations pass the fixed fixture and the updated named-registry
fixture, while all four negative controls exit with the expected code 73.
The named-registry fixture changes only its late lifetime expectation; its
handle-sharing and payload-count checks remain intact.

Eight source probes and positive links, four private negative variants and all
12 expected runtime outcomes passed with `MallocScribble=1` and automated audio
mute enabled. The fixtures are CPU-only: no game, assets, audio device, GPU or
IPC session is opened. Private logs/proof are under ignored
`_work/build/macos/process-allocator-agent/`. A coherent application build and
broader UI/CoreSystems checks remain the integrator's validation boundary; these
payload generations alone are not a claim of full game resource parity.

Final integration: both standard applications build and their complete 141-test
suites pass (MAC289), including deterministic allocator lifetime and required UI
failure fixtures. Actual menu/save/reload and guarded renderer runs also retire
with exit 0; no late-telemetry crash was observed in those bounded runs.
