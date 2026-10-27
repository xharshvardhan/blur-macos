# Remaining audio and frame jitter investigation

MAC292–MAC310 continue the player's audio and frame pacing report after the
0.1.2 feedback build. Automated game/device runs mute each SDL playback stream
before it resumes (`BLUR_MAC_AUDIO_MUTE=1`); system volume and saved preferences
remain unchanged. Muted measurements cannot establish subjective sound quality.

## Actual pipeline and clocks

MAC294 adds optional `BLUR_MAC_FRAME_TRACE_FILE` JSONL records to the real native
application loop. Completed frames contain poll, update, render and diagnostics
boundaries plus the actual simulation-tick count. The loop's fixed simulation
cadence, interpolation, unlocked rendering and failure paths are unchanged.
The first 65,536 records are reserved before execution, with explicit overflow;
records are written after the loop ends. A relative output path is rejected.
There is no per-frame file I/O. The option is off by default.

Frame, audio and Metal records share C++ `std::chrono::steady_clock` nanoseconds.
On this host Python's matching measurement clock is
`clock_gettime_ns(CLOCK_MONOTONIC_RAW)`, verified by eight before/native/after
brackets. Python `monotonic_ns()` has a different epoch here and is used only for
helper deadlines. CPU encoding totals and nested wait spans overlap; they must
not be added as independent work or described as GPU execution timestamps.

The private MAC295 operator enters the original menus using an isolated copy of
accepted MAC285 profile/save state. It verifies the actual Racing HUD, 20 cars,
human Focus, saved WASD layout and enabled controls. A 90-second no-input window
and a finite 45-second W hold have source-state bookends. No polling IPC,
screenshots, readbacks or profiler sampling occurs inside those windows. No AI,
position, race-progress or finish state is assigned. Original race and desktop
confirmations close the game afterward. The no-input car can move when hit by
AI traffic, so this is not a stationary-scene benchmark.

## Diagnostic baseline

The coherent Debug and Speed game builds and all four focused diagnostics,
audio, trace and application checks pass in each configuration. MAC296's first
90-second window qualified, but its observer reparsed an already typed boolean
keypress acknowledgement and falsely rejected the first W hold. The observer
stopped without retrying input. Its original failed experiment is retained;
separate normal-menu retirement exited the game with code 0 in 282.654 seconds.
The typed-outcome validator was repaired and tested before the fresh MAC297 run.

MAC297 completes both windows and the original exit routes: code 0, 309.302
seconds, no primary failure, and the accepted source state unchanged. Driving
bookends show 618.77 metres displacement and about 90 mph at the end. These are
two temporal samples of a live race, not a deterministic replay or a completed
race claim. Private artifacts are under `_work/runtime/verification/mac297/`.

| Window | Mean frame interval | p99 | Maximum | Mean callback frequency |
| --- | ---: | ---: | ---: | ---: |
| Unpaused, no input, 90 s | 21.344 ms | 31.286 ms | 86.636 ms | 46.85 Hz |
| Finite W acceleration, 45 s | 20.682 ms | 30.381 ms | 95.575 ms | 48.35 Hz |

The Metal trace fills its 262,144 slots and drops 1,613 later records during
retirement. Retained presentation records extend beyond both measured windows;
late-run absence is not interpreted as proof. The audio and frame traces have
zero overflow. Preserve whole-run saturation metadata with all comparisons.

The two measured windows contain 33,721 and 16,096 texture-download waits.
Presentation's resource wait averages 7.226 and 7.050 ms respectively, nested
inside its total CPU span. CPU-authored light-cube downloads are a reproduced
avoidable class; presentation waits additionally protect lifetime, ordering and
error reporting and cannot simply be deleted.

Neither MAC296 nor MAC297 reports a positive SDL additional-demand callback.
MAC297 records 41 submission gaps just over the 21.333 ms device period, without
extended unavailable producer frames. The observed queue drops to 128 frames
when the 1,024-frame period is consumed, then refills in 128-frame quanta. Those
burst intervals are not hardware underruns. No thread-priority or larger-buffer
repair is justified by these observations. A planar stereo extraction mismatch
is under separate signal-level investigation; full queues cannot exclude a PCM
layout error.

## Repair and acceptance

MAC298 retains CPU staging mirrors only for dynamic, non-renderable texture
subresources. GPU destination writes invalidate the canonical mirror; all
existing resource waits, CPU ownership and pending-upload lifetimes remain.
Independent GPU byte checks cover partial writes, cube/mip/volume copies and
cross-queue readers. All four build variants pass: 20 downloads, versus 56 with
the old bodies in an otherwise identical private control. Both coherent Debug
and Speed focused texture/retirement/pipeline/trace suites pass.

MAC301 corrects native stereo extraction from the original renderer's planar
128-sample channel buffers. The old adapter took the right output from left
sample `j+1`; the native path now takes right-plane sample `j+128`. The recovered
Windows branch is preserved. Distinct continuous 317/733 Hz channels through
the actual Output/Input/Renderer producer reproduce the old right-channel error
in all four variants (maximum 0.649989; block-boundary jump 0.621045). Corrected
left/right sample error is zero, with the expected smooth right boundary step
at most 0.038179. Real writer and bounded statistics checks also pass all four
variants, and both coherent configurations pass the three focused audio checks.

Optional `BLUR_MAC_AUDIO_PCM_STATS_FILE` writes energies and routing-error
statistics, never PCM samples. Its 512 fixed slots sample at most once per
second on the writer; formatting and file I/O occur only after stream retirement.
The exact music-to-renderer bus selection is serialized game data. Source proves
the connection mechanism, but nonzero stereo energy alone cannot identify a
music-only signal. Listening acceptance remains separate.

MAC302 bounds presentation overlap with one shared producer/consumer dependency
record. Only already committed, retained, tracked resources on the actual same
command queue can overlap CPU work. Aliased surface waits, the next presentation
and device retirement check both commands and preserve terminal failures. Foreign
queues and unretained producers keep the wait path; no asynchronous C++ callback
or unbounded command chain is introduced. Focused acceptance includes independently
initialized surfaces for the device-wide cap, actual GPU gates and deterministic
predecessor/consumer error paths.

## Integrated driving results

Both complete suites pass 145/145 (Debug 161.67 s, Speed 99.19 s). The two
sanitized presentation/mirror configurations also pass all four gated GPU runs.
The 23 production/fixture file hashes remain unchanged through those checks.

The signed 0.1.3 build 303 completes original menus, the two live 20-car human
driving windows and ordinary race/desktop exits: MAC303 exits 0 in 304.119 s,
MAC304 exits 0 in 303.433 s. Both preserve the immutable source profile/save
state. MAC303 has complete frame, audio and Metal traces with zero overflow.
The finite-W bookends show 651.73 m displacement. MAC304 runs the same executable
with per-frame/Metal/audio traces and PCM statistics disabled; conservative
complete five-second reports are retained between unique vehicle telemetry
bookends, excluding two reports at each edge. No capture or profiler is used.

| Actual window | Baseline MAC297 | Repaired MAC303 | Repaired MAC304, minimal diagnostics |
| --- | ---: | ---: | ---: |
| No-input mean callback/submission rate | 46.85 Hz | 67.29 Hz | 65.40 Hz |
| Driving mean callback/submission rate | 48.35 Hz | 65.09 Hz | 66.03 Hz |
| Driving mean frame interval | 20.682 ms | 15.364 ms | 15.145 ms |
| Driving p99 frame interval | 30.381 ms | 21.763 ms | unavailable in five-second summaries |
| Driving maximum observed interval | 95.575 ms | 52.519 ms | 26.881 ms in retained 25.03 s of reports |

The same 2560×1440 render workload and ordinary keyboard-controlled race are
used, but these live races are not deterministic replays. Rates measure the
render loop/submitted presents, not compositor/displayed FPS. The observed gain
supports the combined repairs; it does not establish the independent percentage
due to each change. In MAC303 the moving-window synchronous download count falls
from 16,096 to 3,216, despite more rendered frames. Required downloads and surface
waits remain. Presentation's resource-wait mean falls from 7.050 ms to 0.000812 ms;
some synchronization moves to the next resource acquisition as intended, so that
row alone must not be presented as total frame-time savings. Occasional frame
hitches remain; this is not a locked-60 or complete-jitter-elimination claim.

Both final whole-stream summaries report zero approximate SDL additional-demand
shortfalls and zero nonfinite PCM. MAC303's 302 one-second PCM snapshots show
265 nonzero SurroundLFE-bus samples and **no nonzero mono, stereo or separate
surround-bus samples**, including menus. Therefore the proven MAC301 stereo
repair is latent in this run; it does not establish that the player's reported
race music artifact is repaired. Clipping affects 114 of about 29 million
sample values and does not establish sustained distortion. Muted checks also
cannot establish perceptual listening quality.

## Final source-flow investigation and early-race limits

The user specifies race music through the Mac speakers. Final build 308 includes
bounded MAC305 source-flow diagnostics. Both complete suites pass **146/146**
(Debug 159.28 s, Speed 100.40 s); all 27 frozen production/fixture file hashes
remain unchanged. The later diagnostics add no playback-policy repair.

MAC306 verifies live music/menu/effects volumes of 7, music mode 2 (Surround),
and a quiet menu window. The actual streamed stereo ADPCM decoder completes
35,509 attempts, including 31,739 streamed, with no source-hole decisions.
Profile settings alone do not prove which music signal is playing. Native
output is 48 kHz even though the profile's quality getter reports 0; that
Windows-only quality-application gap is unrelated to a demonstrated glitch.

MAC308 exercises an earlier race interval: one second without input, followed by
60 seconds of finite W, then original race and desktop exits (code 0 in
233.539 s). All traces complete without overflow and the source save is
unchanged. The renderer averages 52.86 submissions/s during driving, but its
p99 interval is **63.415 ms** and maximum **288.956 ms**. Update averages
7.615 ms and reaches 162.340 ms; the longest render reaches 268.364 ms without a
corresponding long instrumented Metal wait. These spikes are not explained by
the repaired texture-download or presentation waits. The longer MAC303/304
windows therefore establish throughput improvement in those windows, not a
universal tail-latency improvement or a jitter fix for every race phase.

MAC309 is a private control using the entire older Metal backend archive with
the same current audio diagnostics, dependencies and original data. It exits
normally in 234.563 s, preserves source state and has zero trace overflow. Its
60-second early driving window averages 46.75 submissions/s, p99 31.530 ms,
maximum 65.170 ms. Live AI/contact states differ; this is not a deterministic
same-trajectory comparison. Do not suppress this worse tail-latency result in
claims about the combined repairs. The old backend control is not delivered.

Neither race reports positive approximate SDL additional demand. MAC308's mono
ADPCM completes 1,158,664 resident decodes without holes; stereo reports 550,161
attempts, 105,380 streamed, 550,101 successes and 60 initial zero-fill decisions.
All 60 retained decisions have source/playhead/consumed position zero, with no
midstream hole in that observation. MAC309 reports 69 stereo zero-fill decisions
but retains only 65; four are intentionally unsampled, so their positions remain
unknown. This is source availability evidence, not device output or listening
acceptance. PCM16 and DSP-generated silence are not covered by this trace.

Sparse original BANK/WAVE header correlation identifies the periodic retained
paired starts as environment ambience. Other starts match vista/menu music or
ambiguous environment intros; none matches the inspected in-game music bank.
No sample payload is extracted. The generic original asynchronous first-read
sequence explains unprimed then primed-empty states, but this does not prove
the starts are inaudible or harmless. See [decoder evidence](audio-decoder-trace.md).

A bounded reference audit compares the recovered 33-byte/64-sample ADPCM layout
with vgmstream's [BAF reader](https://github.com/vgmstream/vgmstream/blob/d9875b1fff307e4759dfb85e49e3d5e6fb196f7c/src/meta/baf.c)
and [configurable PSX decoder](https://github.com/vgmstream/vgmstream/blob/d9875b1fff307e4759dfb85e49e3d5e6fb196f7c/src/coding/psx_decoder.c).
Frame geometry, nibble order and predictor coefficients agree. The reference
uses different quantization/history semantics from the recovered float decoder,
so it does not establish bit-exact correctness. A separate source audit finds
no Mac-only SurroundLFE plane/stride/downmix defect; both platforms share that
active mixing path.

The reported perceptual race-music artifact remains **unresolved**. Neither
healthy queues nor these limited source observations prove clean sound. No
thread-priority, buffer-size, decoder-history, playhead or simulation policy is
changed on speculation. The final development build contains proven stereo and
renderer repairs, plus default-off diagnostics for the remaining investigation.


MAC310 repeats the early-race route with the final build and all detailed frame,
Metal, PCM and decoder traces disabled. Original race/desktop exits succeed
(code 0 in 234.581 s), accepted state is unchanged and finite W is acknowledged.
Conservative eight complete five-second reports cover 40.053 s strictly inside
the 60-second IPC-free driving window: **69.36 submissions/s**, mean interval
14.418 ms, maximum 44.671 ms. Two reports at each edge are excluded; p99 is
unavailable from this summary. The one-second no-input window is too short for
these reports and has no rate claim. Startup observation briefly timed out during
loading; no key was retried, and later fresh HUD/vehicle observations establish
the actual race and human controls. This does not explain the earlier 289 ms
outlier or isolate diagnostic overhead from different live contact states.
