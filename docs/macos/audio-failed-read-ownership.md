# Failed native audio reads must not alter producer PCM (MAC320)

The final MAC317 full suite passed 148/148 Debug tests and 147/148 Speed tests.
The Speed stereo-plane fixture failed its exact independent-channel assertion.
Audio production code had not changed in that integration. This failure is not
classified as a harmless scheduling fluctuation: a source audit found an actual
producer/consumer ownership violation in the failed-read path.

## Recovered buffer convention

`Renderer::Execute` clears scratch frame 0, invokes the source callback, then
copies that mixed PCM into ring slot `(m_sNextWrite % m_uNumBuffers) + 1` before
publishing the write index. `GetNextFrame` advances the read index only when a
new frame is available, and exposes slot `(m_sCurrRead % m_uNumBuffers) + 1`.
Before the first publication the read index is -1. Because the buffer count is
unsigned, its modulo in the legacy clear expression selects ring slot 7 for an
eight-slot ring; it does not produce a negative pointer in this case.

The dangerous case follows consumption of frame 0. A failed read previously
called `ClearFrame(m_sCurrRead % m_uNumBuffers)`, which then means
`ClearFrame(0)`. The producer may already be mixing frame 1 in scratch slot 0.
The consumer can therefore erase the next mixed quantum before its publication.
At other read positions the clear also targets a different slot from the one
returned by the documented `+1` ring convention.

The native `Engine::WritePCM` is the only production native caller. It checks
false first, ignores those pointers and waits for a new frame. Other consumers
in renderer.cpp are PS3-only; the original Windows engine also waits on false.
`Renderer::Init` starts the native writer before `Renderer::Start`, but that
initial writer cannot advance the read index until a producer publishes PCM.
Thus simple consumption of an assumed frame 0 before source attachment is not
supported by this source path.

## Narrow native repair

On macOS a false `GetNextFrame` no longer writes any bus PCM. It retains the
original drop count and false return, and the caller retains its existing wait
behavior. Producer scratch and unpublished slots stay owned by the producer.
The original Windows/console `g_bClearDroppedFrames` body is preserved exactly.
There is no inserted silence, skipped source quantum, buffer enlargement,
priority change or signal-tolerance relaxation.

## Deterministic regression and evidence limits

The existing stereo-plane test now holds its actual second source callback after
`Output::MoveAudio` has populated renderer scratch but before the renderer can
copy/publish it. The test consumes frame 0, forces an unsuccessful read while
frame 1 is held, then releases the callback. The unchanged exact 64-frame
independent-channel checks must still pass, together with native 2/6/8-channel
adapter checks and actual writer statistics. All assertion paths release the
gate before renderer destruction, and the callback has a bounded timeout.

Private current-renderer, old-body renderer and fixture object probes passed in
Debug, Speed, Debug UBSan and Speed UBSan (**12/12**). Evidence is at
`_work/build/macos/audio-ring-read-agent/source-probes.json`. The old-body source
changes only the native failed-read clear back to its prior body; its objects
are retained for a controlled runtime comparison. No private SDL/hardware run
was performed while the parent-owned actual race held the serialized lease.
Coherent positive and old-body negative runtimes remain pending at source freeze.

This is a concrete corrupted-quantum mechanism and regression candidate. It does
not establish that the user's perceived race-music symptom has been eliminated;
actual-game validation and audible manual assessment remain separate.

Parent coherent runtime differential: the final Speed fixture passes, while a
copied audio archive with only renderer.cpp.o replaced by the ABI-compatible
old-body object fails cleanly with exit1 at the unchanged independent-plane
assertion. The same fixture object and dependency closure are used. The initial
private link used the wrong archive-member case and was corrected before any
runtime; shared archives were never modified. This private link error is not
negative runtime evidence. Proof is in
`_work/build/macos/audio-ring-read-agent/speed-negative/differential-proof.json`.
The combined strict Metal focused matrix passes8/8; both complete final suites
and actual source-qualified build320 runs are recorded by final integration.

Final coherent source320 complete suites both pass148/148 (Debug134.14s,
optimized81.01s). The actual source-qualified LAN run322 creates/leaves its real
lobby and exits0. Final race acceptance and package checks are recorded in the
integration ledger. Muted runtime correctness is not an audible-quality verdict.
