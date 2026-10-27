# MAC175 — Audio entity observations and final controller ownership

MAC173 reached complete layer/archive teardown, then the actual Debug process
asserted in PagedArray<EntityDataPollInfo*>::~PagedArray, called from the static
InterfaceController destructor. The pool requires every occupied record to be
retired; its assertion remains unchanged. Optimized exit without that assertion
was not evidence of balanced ownership.

## Concrete acquisition and teardown mismatch

InterfaceController::TrackValue allocates an actual EntityDataPollInfo, publishes
a generated handle and increments its entity's data-tracker reference count.
Normal InterfaceController::ReleaseEntity removes associated polling/callback
records before invoking the subsystem entity release. In contrast, the original
Subsystem destructor directly deletes remaining entity instances. MAC138 correctly
retired explicit subsystem objects but this raw final path bypassed their
controller-owned observation records. UntrackValue also returned without freeing
its own record when the referenced entity had already disappeared.

SystemSubsystem declares 48 oscilloscope bands and acquires their Low/High
TrackValue handles in its playback initialization. Its destructor is empty and
there is no matching Untrack for those handles. This is a concrete populated
producer from source, not a measured claim that exactly 48 records remained in
MAC173. Dialogue has a separate VO polling lifetime. UI Bin1..8 matches in this
bounded source pass were commented code and are not counted as active producers.

## Native owner repair

Before deleting an explicitly owned subsystem, the controller retires only poll
records whose entity handle belongs to that subsystem, frees each actual handle
entry and deletes each owned allocation. Live entity reference counts balance
while the real owner still exists. Normal ReleaseEntity uses the same poll
retirement helper, while its original callback/script behavior remains. Native
TrackValue publishes exception-safely; UntrackValue retires its owned record even
when its entity vanished, and invalidates the caller's handle.

Final controller Release also retires observations on still-registered borrowed
subsystems before clearing its pointer table. Borrowed objects survive; their
live reference counts balance. A dedicated bounded live-slot resolver handles
retirement of departed-owner records without weakening the original strict
GetEntityInstanceFromHandle getter contract. This is specific record ownership,
not arbitrary borrowed entity deletion or a pool/counter reset.

SetCallback passes the full generation/index handle to the ObjectManager, and
System::DoWork consumes its return queue on the main thread. Subsystem deletion
precedes complete underlying action-producer retirement, so it does not prematurely
recycle callback records. Final callback retirement requires the actual Audio
System object to have stopped, as in GameAudioLayer's existing ReleaseSystem then
controller Release order. Remaining callback objects/slots retire without dispatch,
with live borrowed entity refs balanced if any. A remaining callback with an actual
System still alive reports an invalid shutdown stage. No fake callback/engine
success or queued callback execution is used during cleanup. The native final
path is quiescent with its main-thread return consumer; no new arbitrary concurrent
entity/table mutation guarantee is made.

## Handle identity through reinitialization

Original PagedArray shrink/reallocation discards removed pages' per-slot generation
history. Regrowing past 32 entries can therefore recreate an old handle identity.
Native controller observation release now selects a scoped retained-capacity mode:
actual payloads delete, occupancy bits/counts become free, but per-slot container
metadata remains at the controller's high-water mark until its ordinary destructor.
All native controller poll and callback release paths use this mode consistently.
Generic default and original Windows shrinking policy remain unchanged, no fields
or handle format change, and the original per-slot 1..254 reuse/wrap bound remains.
At the existing 8192-entry bound this retains at most two 8192-container metadata
arrays (128 KiB each on this native pointer layout), not live observation objects.
The pool destructor still asserts zero occupied entries and frees its containers.

## Regression evidence and limits

All four final production object probes and four extended fixture probes pass.
The existing audio_subsystem_ownership_test now acquires 40 actual TrackValue
records against real EntityInstance allocations in the original Subsystem pools.
It verifies exact live refcounts, owned/borrowed isolation, orphan Untrack, normal
ReleaseEntity, retained downstream data, final borrowed reference balancing and
stale handles across three reinitializations beyond the first page. Its pending
callback uses the real controller record boundary to verify departed-owner final
retirement/no dispatch; it does not fabricate an action-out producer or claim
actual SetCallback/ObjectManager registration. Existing real Music/UI loader and
subscriber ownership checks remain. No fake definition scripts or gameplay state
is introduced. Parent owns coherent standard runtime verification and the next
actual populated game shutdown; those results must be recorded separately.

Coherent MAC177 Debug integration subsequently built successfully and passed all
14 affected standard checks in 19.79 seconds, including the extended actual
40-record observation fixture and departed-owner callback/reinitialization paths.
This integrated result is separate from later actual populated game shutdown.
