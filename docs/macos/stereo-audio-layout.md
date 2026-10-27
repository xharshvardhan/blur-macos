# Native stereo channel-plane adapter (MAC301)

A source-backed signal fault exists independently of hardware starvation. The
original renderer publishes channel planes of 128 floats. The native output
adapter previously selected `right = input[j + 1]`: almost every right sample
came from the left plane, and the last sample came from the beginning of the
right plane. Independent channels can therefore acquire a discontinuity at each
128-frame boundary despite a full SDL queue.

This adjacent-read behavior was explicitly recovered for the Windows output
engine at `0x00544400` and is recorded in `docs/analysis.md` under Audio output
(PC). The correction is a **native adapter difference**, not a claim that the
retail Windows decompilation used a different read. `OutputMix.inl` retains that
Windows branch unchanged. On macOS only, its right sample comes from
`input[j + uFrameLength]`. Left routing, other speakers, original downmix weights,
PCM generation/decoding, rate conversion, queue bounds, priorities and startup
ordering remain unchanged.

## Original bus authority

- `audio/source/core/output.cpp:155–192`: Output MoveAudio adds each channel to
  its destination Input at `channel * uFrameLength`.
- `audio/source/core/input.cpp:74–109`: Input Process copies/clears the same
  channel planes; the renderer Input owns its alternate output buffer.
- `audio/source/core/Renderer.cpp:1535–1536,1581,2130–2135,2187–2221`: test signal,
  completed-frame copy and ring consumer preserve two separate 128-float planes.
- `audio/source/System.cpp:1011–1018`: stereo master outputs connect to the
  actual renderer StereoInput. `MusicPlaylist::Create` connects its original
  entity to SystemSubsystem (`GameAudio/MusicSubsystem.cpp:609–627`); the real
  System entity's OutputToRenderer connector determines its renderer bus.
  This last graph is data-driven: source alone does not prove that every song
  traverses the stereo bus rather than the surround/LFE bus.
  The exact boundary is `SystemSubsystem.cpp:236–249`: the playlist's entity
  executes its serialized `DoConnections` function parameterized by the real
  System definition ID. `OutputToRenderer` is selected at lines 255–272, then
  InterfaceController SetMaster (`1690–1718`) passes the loaded connector to
  System's component-flag dispatch. `GameAudioLayer.cpp:550–556` loads those
  functions/connectors from `gamedata/xt/AudioEntities.bin` and `Audio.bin`.
  This read-only follow-up did not deserialize those private graphs, so exact
  music-only bus attribution remains unverified.

## Differential fixture

`tools/macos/audio_stereo_planes_test.cpp` uses the actual Output MoveAudio,
renderer Input destination and running original Renderer ring. An authored
continuous left 317 Hz sine and right 733 Hz cosine have independent amplitude
and phase. The SDL consumer is joined before the test becomes the sole ring
consumer. It verifies the original ring's planes exactly, then checks the native
2/6/8-channel adapter, unrelated speakers and cross-quantum continuity.

All four baseline configurations, using the preserved old adapter archive, exit
cleanly with expected failure 1: left error 0, maximum right error 0.649989 and
maximum right quantum-boundary jump 0.621045. This exposes a signal error that
the previous constant-channel assertion could not distinguish. Candidate
compilation/link and muted real SDL checks pass across Debug/Speed and their
UBSan configurations: left/right error 0/0 and maximum right quantum-boundary
delta 0.0381786, consistent with the authored continuous 733 Hz channel.

The fixture also runs the real native Engine writer for more than one second,
then verifies its retired signal statistics: independent nonzero source planes,
zero numerical routing error and an inherited-adjacent hypothetical error. It
uses authored signals, not proprietary audio, and makes no listening claim.
The parent owns coherent actual-game integration and acceptance.

## Bounded actual-game statistics

`BLUR_MAC_AUDIO_PCM_STATS_FILE` opts into an absolute JSONL sidecar. It is off by
default. The owner allocates 512 entries before output resumes; the writer records
at most one entry per elapsed second and reports overflow. No callback reads,
writes, allocates, locks or logs for these statistics. Formatting/file writes
occur after the writer joins and the output stream retires. No PCM samples,
compressed audio or game assets are written.

Each record has common absolute `steady_ns`, submitted frame count, mean-square
energies for mono, stereo left/right, surround left/right and all surround/LFE
planes. It compares the actual stereo mix contribution against each source
plane and records the hypothetical adjacent-read right error, plus raw-right and
final-output right boundary deltas. Nonfinite observations are counted and do not
produce invalid JSON. Statistics are before per-stream gain, so automated
`BLUR_MAC_AUDIO_MUTE=1` keeps tests silent while retaining signal evidence.
System volume remains unchanged.

The statistics are sparse observations, not exhaustive waveform analysis. A
nonzero stereo bus during a music-playing phase proves that the corrected route
is exercised; it does not isolate music from simultaneously mixed sound effects.
A source-right/output boundary difference can also contain other legitimate bus
contributions. Use original playlist/UI timing and the other bus energies for
interpretation. A successful muted signal proof is not subjective confirmation
that every reported lag/noise symptom is resolved.

## Prior timed baseline

MAC296 has no positive SDL additional-demand observations over the recorded
stream lifetime and no extended producer-starvation events. All 33 writer gaps
were observed with 128 queued frames, spanning 21.3486–29.1401 ms versus the actual
21.3333 ms hardware period. Consuming 1,024 of 1,152 frames leaves 128, so these
observations fit ordinary burst refilling; they are not 33 measured underruns.
The first callback followed the first full-period/headroom marker and requested
zero additional bytes. Its 22 clipped samples out of roughly 26.9 million sample
values and zero nonfinite samples do not explain sustained choppy audio by
themselves. Priority and startup priming were not changed for this repair.
