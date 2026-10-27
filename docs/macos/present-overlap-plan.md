# Bounded native presentation overlap (MAC302)

MAC296's actual 20-car, no-input unpaused race measured a mean 7.429 ms in
`present_resource_wait` (p99 13.809 ms, maximum 43.75 ms). This CPU span waits for
the source render command; it is not a GPU execution timestamp. Removing that
wait alone can move it into the next Context/CPU-lock path and hide predecessor
errors, so the change preserves an explicit bounded ownership dependency.

`MetalPresentDependency.h` owns one strong producer/consumer pair, with an
independent mutex and a cached terminal result. It installs no completion
handler and contains no Surface or device pointer. Its wait retires both
commands even if the producer failed, then releases both command references.
A failed cached result remains failed. No record lock acquires a Surface or
device lock.

The shared Surface (including texture aliases and MSAA resolve storage) and
the device's single outstanding-present slot share this record. Before the
next valid Present submits, it retires the prior device record; switching to a
different source cannot grow a presentation queue. A Surface wait also retires
the same record before GPU acquisition or CPU LockRect. Device WaitIdle retires
the record before its ordinary queue barrier. Device failure is sticky until
recreation, including when an alias already observed the error.

The CPU wait is skipped only for a tracked texture whose actual pending
producer belongs to the current submission queue, has committed/scheduled/
completed status, and retains its resources. The allocation queue is not a
substitute for the pending command's queue identity. Cross-queue and unretained
producers retain the existing synchronous wait. Uncommitted producers are
rejected. The consumer command must retain references too. Existing recording,
CPU ownership, dimensions, format, resolve and device guards remain in place.
Drawable acquisition/encoding failure does not overwrite the source pending
identity or count an unsubmitted presentation.

`tools/macos/present_dependency_test.mm` contains deterministic authored command
objects for producer and consumer errors, repeated failure, command-reference
release and public alias/WaitIdle rejection. These are not deliberately induced
GPU faults. Real shared-event-gated GPU work verifies first-Present CPU return,
second-Present blocking with an independently initialized different source,
alias LockRect waiting and exact pixels, wrapper retirement/resource lifetime,
and cross-queue/unretained fallback. Every owned worker unblocks and joins
before checking a failing oracle. Existing presentation tests retain the
single/2x/4x and original refresh-interval coverage.

Source and fixture are frozen for the parent's coherent builds and hardware
checks. No performance improvement is claimed until the same actual-game,
no-readback observation windows compare total frame intervals and all wait
phases. The next Context acquisition deliberately retains synchronization;
any wait migration must be included in that comparison.
