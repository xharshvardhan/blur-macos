# MAC193: movie audio disposal at process exit

This is a dependency experiment, not a proven fix for the actual game. The
normal menu movie readiness work remains separate from this optional audio
stress failure.

## Owned failure

The private Debug `ui_movie_test` audio variant completed its real movie work
and crashed during process exit. The owned report is
`_work/build/macos/occlusion-agent-ui-movie/macos/crash.ips` (2026-10-27
08:05:42 local time). The faulting dispatch queue is `AudioQueueQueue`:

`AudioQueueObject` destruction → `AQMEIO_Base::RemoveRunningRequest` → global
delete → `BlockAllocator::Free` → `PageUsage::GetBit`.

The main thread is simultaneously in `__cxa_finalize_ranges`/`exit`, destroying
a C++ mutex. The failure therefore establishes concurrent system audio disposal
and process static teardown. It does not establish a foreign-malloc routing
error, nor does it establish that the actual game encountered this failure.
Allocator ownership/routing is unchanged by this experiment.

The report has no SDL dylib in `usedImages`. An installed Homebrew SDL 3.4.14
was not evidence that this binary used that version. The current project pins
and statically links SDL 3.4.18; private comparisons below use an explicit clean
copy of that pinned source.

## Source contract and candidate barrier

SDL 3.4.18 `COREAUDIO_CloseDevice` calls `AudioQueueFlush`, asynchronous
`AudioQueueStop(...,0)` and asynchronous `AudioQueueDispose(...,0)`, then joins
its own CFRunLoop thread. Joining that thread does not itself establish that
AudioToolbox's internal dispatch disposal has completed. The same disposal
sequence remains in upstream `main` inspected on 2026-10-27. A bounded review
of the current CoreAudio file history found no equivalent maintained fix;
this is not a claim about every SDL issue or pending branch.
[Pinned source](https://github.com/libsdl-org/SDL/blob/release-3.4.18/src/audio/coreaudio/SDL_coreaudio.m),
[upstream source](https://github.com/libsdl-org/SDL/blob/main/src/audio/coreaudio/SDL_coreaudio.m).

Apple's installed SDK declares immediate disposal synchronous, including queue
resources. The SDK also warns about disposal initiated inside a completion
callback/listener. These engine movie owners close from their owning thread,
outside such callbacks. SDL's `ClosePhysicalAudioDevice` sets shutdown and
unlocks `device->lock` before entering the backend close function; playback
callbacks acquire that lock and immediately return when shutdown is set. This
avoids holding that callback mutex while waiting for synchronous disposal.
This reasoning covers this engine close route, not arbitrary calls from user
audio callbacks. [Apple AudioQueueDispose](https://developer.apple.com/documentation/audiotoolbox/audioqueuedispose(_:_:)),
[SDL device close](https://github.com/libsdl-org/SDL/blob/release-3.4.18/src/audio/SDL_audio.c).

The private proposed helper, retained at
`_work/build/macos/occlusion-agent-coreaudio/patch_sdl_coreaudio.py`, changes
only macOS `AudioQueueDispose` to immediate mode.
It preserves iOS behavior and SDL's existing flush/stop/thread join order. It
checks the exact pinned file SHA-256 and can safely run again during configure.
It is **not hooked into the production dependency build** pending relevant
negative/positive evidence and root review.

Immediate disposal can discard audio still pending in the queue. The candidate
therefore does not establish preservation of the original asynchronous drain
behavior at normal movie completion. Successful PCM submission and correct
video frames do not measure that audible tail.

## Private comparison

Private source, archives, link arguments and results are retained under
`_work/build/macos/occlusion-agent-coreaudio/`. Both variants use a clean copy
of pinned 3.4.18 with the same Debug configuration; only the disposal call
differs. The candidate rebuild changes one Objective-C object and the archive.
The current real UI fixture is relinked against each archive with bootstrap
first and the same remaining engine closure. Runs hold the native exclusive
service slot and use strict Metal validation.

The ordinary audio variant passed **12/12 processes per variant**: 36 real UI
movie open/decode/close cycles for the baseline, and 36 for the candidate.
Neither repeated the original failure. These green-only samples establish
basic playback/close compatibility, not a before/after crash fix. The private
SDL builds target the current host by default; they do not validate an older
macOS deployment target.

A further bounded immediate-close variant exercises actual `Movie::C_Movie`
owners, decoded nonzero PCM submitted to the real CoreAudio driver and a real
uploaded Metal frame, then closes before media completion and exits. Its
baseline and candidate each passed **12/12 processes**, with 12 owner cycles
per process (144 per variant). Both printed `SDL_GetVersion() == 3004018` and
required the actual `coreaudio` driver. Logs are
`immediate-{baseline,patched}-results.json` and corresponding per-process logs.
There was no timeout, callback-mutex deadlock or reproduced exit crash in these
bounded samples. This remains a green-only experiment; the original rare
failure is unresolved and the candidate is not recommended for production
integration on the strength of these results alone.

The checked helper also passed first application, repeated application and
rejection of an unexpectedly modified source copy. No shared dependency source
or archive was modified. Root's current stable engine closure was read only to
relink private binaries.

## Proposed integration only

If the candidate is accepted after validation, root can add a FetchContent
`PATCH_COMMAND` invoking the checked Python helper (after explicitly promoting
it into `tools/macos/`) on `<SOURCE_DIR>` in
`cmake/macos_dependencies.cmake`, before `FetchContent_MakeAvailable(SDL3)`.
Keep the official archive URL/hash and license copy. Reconfigure/build all
configurations and rerun audio playback, immediate owner close, normal movie
completion and actual normal-menu game shutdown. Do not replace a failed
dependency patch with ignored errors or change allocator ownership to hide a
late system callback.
