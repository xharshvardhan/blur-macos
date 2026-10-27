# Existing-file movie Open failure (MAC191)

## Evidence and boundary

Actual190 used Speed182 and created a genuine local career save. Its log
`_work/runtime/verification/mac190-game.log:421–427` records VideoStarted,
Ui/Movies/BlurAttract.bik, then FileOpenError for the absolute lower-case loose
movie path and a null engine stream. The exact existing file was independently
readable,112311932 bytes with BIKi magic. It then attempted a draw without the
required movie texture; MAC188 addresses that separate required-resource state.

Actual194 used coherent Debug192, the same isolated data/profile/save roots and
same movie path. `mac194-game.log:266–269` records the same VideoStarted, followed
by successful AsyncFile::Open bufferSize131072. Playback completed, real career
version1378/50903-byte reload had zero unread data, and ordinary menu/profile
progress reached the hub/event path. Its separate audio shutdown failure is
MAC195/196. These runs are different builds/configurations: they do not establish
a reproduced Speed-only I/O fault or a repaired Open defect.

No FFmpeg parsing/decoder problem is implicated in190: MacMovie.cpp209–215 calls
engine filesystem Open and checks it before Length, preload, avio or format-open.
The failed stream was already null at that check. Existence/readability observed
afterward does not prove successful OS access or allocation at the failed instant.

## Actual source path

All paths here are relative to `code/mercury/library/` unless noted.

| Boundary | Source authority | Consequence |
|---|---|---|
| Default owner | gamesys/Framework/source/application/CoreSystems.cpp171–172,406–417 | Starts native C_FileSystem; installs supplied game filesystem for resource/default/display clients. |
| Game overlay | code/amax/game/GameSystems.cpp128–143; util/logicalFs/source/gamefs/GameHelpers.cpp39–70 | Original game overlay maps virtual root to actual native working directory; shutdown restores native singleton before deleting overlay. |
| Loose path dispatch | util/logicalFs/source/overlayfs/OverlayFs.cpp21–47,457–463 | Best mounted layer receives translated path and unchanged access/open parameters; no-layer failure returns null with FileOpenError. A loose native-layer failure reports translated absolute path, consistent with190. |
| Native route choice | common/system/source/serialise/macos/NativeFileSystem.cpp86–114 | Read + I/O initialized + not native I/O worker selects AsyncFile; otherwise NativeFile synchronous fallback. No movie/game streaming flag participates. |
| I/O admission | common/system/source/io/macos/NativeIoService.cpp136–148 | Initialized query uses submission mutex; worker identity is thread-local native device. No request-generation token selects this route. |
| Async entry | common/system/source/io/AsyncFile.cpp81–89 | Unconditional native stderr entry diagnostic precedes validation, buffer allocation and IoFileOpen submission. |
| Queued open | common/system/source/io/macos/NativeIoService.cpp75–96; NativeIoDevice.cpp99–119 | Admission rejects stopped/invalid/OOM; worker resolves path and uses open/fstat/regular-file validation, mapping concrete errno categories. These occur after Async entry. |
| Sync open | common/system/source/serialise/macos/NativeFile.cpp34–57 | Buffer allocation/bounds, fopen, fcntl, fstat, regular-file and setvbuf checks can throw. NativeFile RAII closes partial acquisitions. |
| Path resolution | common/system/source/serialise/macos/NativeFileSystem.cpp28–67 | Resolves original case/symlink spelling; directory iteration, ambiguity, path length and filesystem errors can throw; RAII owns directory iterators. |
| VideoStarted | code/amax/game/Game/GameWorld/GameWorld.cpp3240–3250 | Only updates cutsceneVideoPlaying; its log says streaming disabled. It does not shut down I/O or select synchronous file access. |

C_OpenParams defaults are explicit in common/system/include/system/serialise/
I_FileSystem.h104–132: default buffer hint, critical=true in game, numBuffers0.
No uninitialized default parameter is established here. The original generic
non-Mac C_FileSystem uses AsyncFile for reads too; native synchronous fallback is
necessary for pre-service tools and callbacks that cannot await their own queue.

## What190 does and does not imply

The190 log has no AsyncFile::Open entry for this movie, whereas194 does. Normal
queued-open rejection, OS open failure inside NativeIoDevice, or later read/decode
failure would occur after that unconditional entry. They are therefore unsupported
as the explanation of the observed190 sequence, assuming the complete captured
stderr and native182 entry diagnostic are authoritative. The absolute failure
path is consistent with native backend rather than virtual no-layer rejection,
but no before-unwind stack captured190's exact Open call.

If NativeFileSystem was reached, remaining source classes include:

- A pre-Async entry exception during route selection, buffer validation or stream
  allocation/construction. AsyncFile's default native mutex/condition/file and
  buffer constructors are inert/null/value initialization; no file handle is
  opened by that constructor.
- A synchronous route selected because I/O was inactive or caller was an I/O
  worker, followed by resolution/allocation/OS metadata/open/setup exception.
- A different logical-filesystem failure boundary or corrupted lifetime/state,
  not established by this source-only inventory. Existing critical open reporting
  discarded the exception text, so the log cannot distinguish these classes.

The sources do not justify toggling async, retrying, renaming/copying the file,
changing media-layer policy, bypassing a failure, or attributing this to an I/O
request-generation race. No such behavior change was made.

## Diagnostic-only change and validation

NativeFileSystem.cpp Open now records a stable operation label and selected
route (selecting/async/sync), and prints those plus path, access and exception
what() when its existing std::exception catch runs. Only std::system_error adds
its captured error-code/category; ambient errno is never attached to unrelated
allocation/validation exceptions. The diagnostic does not re-query mutable I/O
state on failure. Existing FileOpenError, critical argument and nullptr return
remain unchanged; no new fallback, retry or I/O decision was added.

All four current configurations compile the production C_FileSystem TU plus the
unchanged existing filesystem_test (eight objects). Private Debug link uses the
standard filesystem target's original system/core closure. Its complete paths,
case, packed scalar/metadata/rename/directory tests pass, process exit0. Its real
missing-file case emits operation=native-open,route=sync,system_error_code2 /
generic, then unchanged FileOpenError/null. An initial invocation supplied a
relative scratch directory and failed the fixture's absolute-working-directory
precondition; the documented absolute scratch argument passes. This is no I/O
fault reproduction and no actual movie retry proof.

Next actual Speed run should preserve the ordinary movie path and use this
bounded exception diagnostic. If it recurs, operation/route/error selects the
next owned source investigation; a caught non-system error must not be relabeled
ENOENT. A null return with no new exception line requires a bounded stack at
FileOpenError/AsyncFile failure rather than an invented exception cause. Root
owns actual runs and integration; no game or shared build was launched for191.
