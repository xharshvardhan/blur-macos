# Native player-feedback acceptance

This follows the user's 0.1.1 play session: Escape → Options had an empty menu,
rendering felt uneven, music sounded choppy, and the desired controls were WASD
with arrow-key power-ups. Verification uses the original archives, game/effect
graph and Master frontend. All new test launches set `BLUR_MAC_AUDIO_MUTE=1`;
SDL stream gain is zero before playback, without changing system volume or
saved audio options. Decoding, mixing and device clocks still run.

## Options and saved controls

The real failure was a missing `Amax.UpdateCustomKeyboardControls` callback in
native builds, which aborted Options initialization. The portable PC callback
family now exists on macOS. The unsupported native Graphics backend is gated
as a complete capability family, so its row is absent instead of opening an
incomplete screen. Keyboard and Audio remain reachable. See
[the callback and layout audit](options-keyboard-initialization.md).

MAC285 used ordinary keys through the original frontend into a 20-car career
race, Escape, Options and Keyboard. Actual UI observations verified three
Options rows, no Graphics row, all 13 keyboard rows, the original Layout 2
selection, accepted profile layout 1, successful save and two fresh resumed HUD
observations. The nine-key Options sequence did not invoke setters or screen
callbacks directly. Original race-exit confirmation, event/tier Back navigation
and desktop confirmations then exited with code 0 in 206.98 seconds, before the
420-second guard. The genuine source profile/save remained unchanged: testing
used an isolated copy.

MAC283 loaded a copy of those accepted save bytes in a new process. The real
loader reported version 1378, 51,227 decoded bytes and zero unread bytes. Two
fresh SPmain observations read layout 1 from the real profile. Original desktop
confirmation exited with code 0 in 232.03 seconds, before the 240-second guard.
A supplemental observer completed this check after the first private observer
mistook an expected blocking profile-load screen for an unknown popup. Both
logs are retained; the initial wrapper failure was not relabeled as a pass.
MAC282 similarly retains an earlier private observer's overconstrained return
assumption. The actual game resumes directly after closing in-race Options:
Escape is both Cancel and Start in the original input route. The corrected
operator accepts two fresh saved, unpaused HUD observations without another
key. These were observer corrections, not gameplay bypasses.

New native profiles default to Layout 2; existing saved choices deserialize
unchanged. Keyboard input fixtures exercise both original layouts through
physical Cocoa keys and the original binding routine, including arrow power-up
actions and independent arrow menu navigation. This does not establish actual
power-up projectile behavior or non-US character-aware remapping.

## Renderer measurements

The user-session sample identified two classes: allocating uniform buffers on
every draw and synchronously waiting when retiring temporary Surface wrappers.
Small copied Metal byte bindings and documented retained-command ownership
repair those classes. A subsequent sample identified descriptor construction
and string allocation even on pipeline-cache hits. MAC288 defers descriptors
until a cache miss and reuses a context-owned key scratch buffer, preserving
all original key bytes and immutable owned map keys. See
[the renderer audit](renderer-performance-audit.md).

At 2560×1440 with 20 vehicles and the original Paused screen, MAC280 and MAC281
compared the preserved old lower renderer with the first two repairs. Current
framework/audio/UI production sources, original data and dependency closure
were shared. No quality or resolution reduction was used. Metal validation was
off equally for timing and on for separate GPU correctness fixtures. Only
complete five-second reports inside a marked, quiet 30-second window were used;
Automation watch/checkpoint traffic, transition warmup and CPU sampling were
excluded.

| Actual observation | Old renderer MAC280 | First two repairs MAC281 | All three repairs MAC289 |
|---|---:|---:|---:|
| Full measured window duration | 25.17 s | 25.06 s | 25.06 s |
| Submitted presents / second | 24.83 | 32.12 | 51.35 |
| Mean render-loop interval | 40.27 ms | 31.13 ms | 19.47 ms |
| Maximum observed interval | 61.26 ms | 97.27 ms | 40.19 ms |
| Main-thread samples in uniform upload | 30.57% | 1.24% | 2.18% |
| Main-thread samples in pipeline preparation | 9.63% | 16.06% | 1.39% |
| Main-thread samples in Surface destruction | 10.09% | 0% | 0% |

These are engine submissions and inclusive main-thread wall-time samples, not
compositor/displayed FPS or a guarantee of 60 FPS. The first two repairs improved mean throughput 29.4%, while their larger maximum
interval prevented a jitter verdict. The final coherent build, including the
pipeline-cache repair, measured 51.35 submissions/s: 106.8% above the preserved
old renderer and 59.8% above the intermediate build. Its measured maximum was
40.19 ms. The same current framework/audio/UI production code, dependencies,
original data and copied source profile were used. This remains one paused
20-car scene per build, with ordinary source-driven startup and pause timing;
it does not establish identical AI poses, moving-race performance, a displayed
frame-rate guarantee or the absence of hitches elsewhere. Later CPU samples
resolve the allocation classes, not the cause of an individual earlier hitch.

## Audio and remaining verification limits

The device-period fixture reproduced 12 shortfalls in 12 steady callbacks with
the old 60 Hz queue threshold: it supplied 896 frames to a device requesting
1,024. The repaired queue uses the actual device period, usable resampled output
and measured retained input, with a bounded additional-padding budget. Steady
silent tests pass at 24/36/48 kHz and actual device-rate conversions in all four
configurations, including injected-stall detection/recovery. See
[the audio contract](audio-device-period.md).

Real game diagnostic totals include startup, loading and diagnostic captures.
They still recorded occasional demand shortfalls: 5/11,262 callbacks in MAC280,
15/11,267 in MAC281, 8/9,587 in MAC285 and 19/11,261 in the final
MAC289 window. PCM was finite and unclipped in those
runs. These counters do not locate or prove audible artifacts. The steady
period mismatch is repaired; zero dropouts and perceptually clean music are
not established, especially because the requested tests are muted. Startup
optimization, a clean second Mac, remote multiplayer and notarization remain
outside this feedback acceptance.

The broad integration also exposed a late framework-worker allocator lifetime
race and a shadow fixture using an unowned scene context. The native allocator
domain now outlives process-exit framework work while payloads still free
normally. The fixture now borrows a real scene-owned context. Required-resource
failure gates and the original renderer graph remain active.

## Final component integration

MAC289 builds the standard Debug and optimized applications with all three
renderer repairs, Options/keyboard callbacks, native new-profile defaults, audio
period/mute support and process-lifetime allocator ownership. Both complete
CTest suites pass **141/141** (Debug 151.05 s; Speed 92.12 s), including the real
archive/renderer/effect graph, required-resource failure gates, pipeline cache
state/lifetime matrix, original input mappings and audio-device tests. The
new diagnostics fixture also checks the actual elapsed-frame window contract;
its private stderr-capture reader uses descriptor reads so Darwin stdio offset
caching cannot hide a report after repeated reads.

The final actual MAC289 process exits with code 0 in 241.73 seconds after its
240-second diagnostic guard, with no primary failure. This is a guarded benchmark
exit; the independent MAC285 and MAC283 checks establish ordinary desktop exit.
