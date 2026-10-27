# Streamed ADPCM source diagnostics (MAC305)

This is an observation tool, not an audio scheduling or decoding repair. The
reported defect is noisy or choppy race audio through the Mac speakers. Earlier
muted race runs had no SDL demand shortfalls, which does not establish that the
upstream game decoder had continuous source data. MAC303 signal observations
were nonzero on the SurroundLFE bus and zero on the Stereo bus; consequently the
independently proven MAC301 stereo-plane fix cannot explain that race's audio.

Set `BLUR_MAC_AUDIO_DECODER_TRACE_FILE` to an absolute JSONL path before starting
the game. Automated runs also set `BLUR_MAC_AUDIO_MUTE=1`; this affects stream
gain only, preserves the decoding/mixing/device flow, and leaves system volume
unchanged. No raw PCM, wave bytes, filenames or game assets are recorded.

## What is observed

The existing mono and stereo `ADPCM4bitDecoder::FillBuffer` implementations first
request streaming data, then return a zero-filled quantum when the original
availability or required-byte checks fail. The diagnostic records that existing
decision immediately before the unchanged zero fill. It does not advance a
playhead, request additional data, add a retry or change any branch result.

Every enabled invocation contributes to mono/stereo attempt counters. Streamed
attempt counters distinguish valid original local stream indices from resident
waves. Successful-decode counters count completion of the original decode and
resample path; they do not mean that the result was nonzero or audible. Paused
attempts can contribute without a decode or source-hole event. These usage
counters prevent an unused ADPCM path from being called healthy merely because
it reported no holes. Other decoders, including PCM16, are not observed here.

An `audio_decoder_session` summary contains unsampled usage, zero-fill and reason
counts, bounded capacity, sampled reservations, retained events, overflow, and
the first/last retained `steady_ns`. Each `audio_decoder_zero_fill` contains:

| Field | Meaning |
| --- | --- |
| `steady_ns`, `session`, `ordinal` | Common process monotonic clock, session start, unsampled zero-fill ordinal |
| `channels` | Actual mono (1) or stereo (2) decoder |
| `stream_index` | Original index local to the audio Instance; not globally unique |
| `stream_handle` | Original WaveStreamData StreamHandle, including its existing unique-id/index bits; no new identity is fabricated |
| `sample_counter`, `sample_index`, `loop_end` | Actual decoder playhead/index and original loop/end boundary |
| `source_position`, `bytes_consumed`, `bytes_available` | Existing WaveStreamData accounting at the silence decision |
| `samples_required`, `bytes_required`, `pitch` | Existing pitch-scaled requirement, including the original end-boundary clamp |
| `primed`, `playing`, `finished`, `loop` | Actual source/decoder state, not inferred audibility |
| `reason` | Bit 1: not primed; bit 2: original availability check false; bit 4: available bytes below required bytes |

Reason bits can overlap. Priming silence, EOF and loop transitions must be
separated from post-priming midstream starvation using these actual boundaries.
An event alone is not proof of a playback glitch. Handle values may be reused
under the original generation scheme and should not be treated as permanent
identities across sessions. No instrumented event identifies a music playlist
or separates music from sound effects.

## Bounds and ownership

The process-lifetime BSS ring has 8,192 slots. It retains the first 64 decisions
and then every 64th decision; unsampled counters continue for every decision.
Reservations never overwrite a previous slot, and capacity exhaustion increments
explicit overflow. Concurrent retained timestamps need not be in reservation
order. Initialization, path validation and final file writes run outside the
decoder hot path. Enabled decoder observations use lock-free atomics and fixed
storage, with a clock read only for retained events. Disabled observations take
an atomic enable check, with no logging, allocation or game lock.

The diagnostic has one actual Renderer owner. It begins after successful native
Engine initialization and before the producer starts. Invalid configuration
returns Renderer initialization failure and joins its newly acquired SDL writer;
it does not acquire a diagnostic owner. Duplicate acquisition is rejected, and a
different owner cannot retire an active session. No shared heap pointer is loaded
and subsequently reference-counted, so there is no load/increment deletion gap.

Engine writer shutdown and quality restart are deliberately **not** retirement
boundaries: the renderer producer can still be inside its frame callback. The
Renderer destructor joins the writer, wakes and joins the original producer,
then drains this trace before retiring its buses. The game callback is
`System::RenderCallback` → `System::Update` → `ObjectManager::Update`; distributed
instance-generation work is flushed before that callback returns. This is the
actual producer/task ownership proof. Arbitrary external threads calling decoder
methods after their owner is destroyed are unsupported; authored caller threads
must also be joined before retirement. BSS storage avoids a freed-ring access,
but does not turn such external misuse into a valid lifecycle.
An empty file or missing final session summary means no completed drain was
observed; it must not be interpreted as zero source holes after a crash or kill.

## Verification and limits

`audio_decoder_trace_test.cpp` uses the original mono/stereo FillBuffer and
WaveStreamData readers with authored VAG packets. It checks unprimed, empty and
insufficient data, unchanged zero/consumption behavior, real nonzero recovery,
both usage-counter classes, concurrent bounded saturation/overflow, disabled and
failed initialization, duplicate owner rejection, quality restarts, separate
lifetimes, and an actual producer callback that decodes after the SDL writer has
joined. The file must remain undrained until the producer also joins.

The fixture keeps one Renderer at a time, as required by its original static
batch queue. An initial authored simultaneous-Renderer case overwrote that
singleton before Engine initialization could reject it; that invalid-host crash
is retained privately and is not classified as a new diagnostic regression.

Private four-configuration source/fixture checks and muted runtime results are
recorded under `_work/build/macos/audio-decoder-trace-agent`. All 16 translation
unit probes, four private links and four muted runtime checks passed. These
checks establish the diagnostic contract, not the cause of a perceptual defect.

## Completed menu and early-race observations

MAC306 completed its normal menu control with ordinary exit 0. Its read-only
`Amax.Options()` observation returned quality 0, effects/music/menu/voice volumes
7, music type 0, output 0, music mode 2 and LFE 0. The original GameProfile.cpp
comment defines music mode 2 as Surround (0 Stereo, 1 Quad), not music disabled.
The menu had active streamed stereo ADPCM attempts and no recorded source holes.
Those facts do not by themselves identify the currently audible music entity.

MAC308 used the current optimized renderer; MAC309 used the previous renderer
pressure baseline with the same diagnostic audio implementation. Both completed
their actual early-race windows and ordinary exit 0. Every run remained stream
muted; neither is a listening test.

| Run | Mono attempts / streamed / successful decode | Stereo attempts / streamed / successful decode | Zero fills / retained / overflow |
| --- | --- | --- | --- |
| MAC306 menu | 0 / 0 / 0 | 35,509 / 31,739 / 35,509 | 0 / 0 / 0 |
| MAC308 early race | 1,158,664 / 0 / 1,158,664 | 550,161 / 105,380 / 550,101 | 60 / 60 / 0 |
| MAC309 previous renderer | 1,151,806 / 0 / 1,151,806 | 540,453 / 105,770 / 540,384 | 69 / 65 / 0 |

All 60 MAC308 zero fills had sample counter/index, source position and consumed
bytes equal to zero. Fourteen were unprimed (reason 7); the remaining 46 were
already primed but empty (reason 6). Twenty-four occurred during its 60-second
finite-W window. MAC309 retained 65 decisions, all with those positions zero;
27 retained decisions fell in its finite-W window. **Four additional MAC309
decisions were intentionally unsampled** after ordinal 64. Their positions are
unknown, despite overflow being zero. It is therefore inaccurate to claim that
all 69 decisions were at source zero or that this run excludes every midstream
hole. Neither window had positive SDL input demand observations; those remain
approximate device-demand evidence, not an audibility verdict.

The source sequence explains why both primed states occur: ADPCM `GetData`
requests `WaveStreamData::Prime`, which sets playing/update flags and returns no
data. `WaveServer::UpdateStream` subsequently activates the AudioGraph; graph
priming and the first asynchronous FileStream read complete at different times.
The recovered ADPCM bank setup uses `m_bPrecache=false`; FileStream's original
setup forces one initial chunk. These first-read waits are not a newly discovered
native-only policy. They are also not automatically harmless: whether a start
delay affects a scheduled transition depends on the actual graph and voice.

### Sparse bank-header correlation

An independent private reader used the original PC `WaveBank.h` and
`WaveBankReader.cpp` layout (52-byte BANK, 60-byte base WAVE, 16-byte ADPCM extra
header), plus the existing raw PAK metadata layout. It read only 29,932 metadata
bytes across the 67 BAF entries in `root_streaming.pak`, covering 261 ADPCM wave
headers. It did not decode or extract sample payloads. The original
WaveBankManager assigns `uPCMLoopPosEnd` directly from the bank's `uNrSamples`,
providing an exact numeric correlation with recorded `loop_end`.

Within those headers, the periodic paired race starts match environmental
`env_battle` front/rear waves: sample counts 964,606, 922,275, 577,073, 1,009,525
and 889,165 in MAC308; MAC309 additionally matched 1,085,299, 1,118,077 and
1,150,228. Front versus rear remains ambiguous within each equal-length pair.
The 960,960-sample intro has 21 eligible streaming environment matches across
tracks, including Brighton front/rear; two further equal-length resident entries
are excluded by the source's streaming-bank condition. Selected Brighton makes
that candidate consistent, not uniquely identified from length alone.

The other retained lengths match vista music (686,208 samples) and the looping
menu variation (4,423,680 samples). None of the retained periodic race bursts
matched `ingamemusic.baf` within the inspected headers. In particular, the
roughly 20-second paired starts must not be labeled music-segment dropouts based
on timing alone. This correlation does not prove which music entity was playing
or that environmental first-read delays were inaudible.

Private correlation evidence and exact offsets are in
`_work/runtime/verification/mac308/decoder-bank-correlation.json`; the sparse
reader is `_work/runtime/research/inspect_stream_bank_headers.py`. The reported
race music noise/choppiness remains unresolved. These observations justify no
decoder, buffering or priority repair on their own.
