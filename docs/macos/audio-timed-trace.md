# Timed native audio observations (MAC292)

MAC289's actual game totals include 19 approximate SDL additional-demand
reports in 11,261 callbacks and a maximum PCM submission gap of 223.456 ms.
The frame log also reports a 223.727 ms interval in an early window. These
aggregate values cannot locate individual audio reports in time, establish
steady-state failure, or prove which thread caused the shared delay. The
previous steady device-period mismatch has a separate reproduced fix in
[audio-device-period.md](audio-device-period.md). No new scheduling, buffering,
priority, mixer or decoder repair is inferred from these totals.

Set `BLUR_MAC_AUDIO_TRACE_FILE` to an absolute JSONL sidecar path. The option
is off by default. It appends one session per native SDL Playback owner, including
movie streams. Every event uses `steady_ns`, the integer nanoseconds from
`std::chrono::steady_clock::now().time_since_epoch()`, matching the application
and renderer phase traces. Session metadata is written at close; its timestamp
is the flush time. Event timestamps locate the actual observations. Sort
cross-thread events by timestamp; reservation sequence is a unique storage slot,
not a guarantee of timestamp order.

The owner allocates 8,192 immutable event slots before Resume. Writer and SDL
callback reserve unique slots using lock-free atomic increments and publish a
release commit. Slots never wrap or overwrite. Metadata reports capacity,
reserved slots and overflow; saturation makes later observations unavailable
and must not be presented as a complete trace. SDL's callback records only its
first invocation and positive additional demand. It allocates, logs and locks
nothing. It does not supply PCM or change playback decisions.

At shutdown, the application writer joins first. The original SDL stream callback
is detached through `SDL_SetAudioStreamGetCallback(nullptr)`; the public SDL
contract waits for an in-flight callback under the stream lock. The stream is
then destroyed before trace formatting/writes. Multiple stream sessions can
append to the same file. The application never reads partially published event
slots while playback is active. Trace I/O failures are explicitly reported.

| Event kind | Observation |
| --- | --- |
| 1 | Stream opened paused; trace storage ready |
| 2 | Resume requested |
| 3 | First actual SDL demand callback, including any additional demand |
| 4 | Later callback with positive approximate additional demand |
| 17 | Original renderer writer starting |
| 18 | First PCM quantum submitted |
| 19 | Submission gap exceeds the current measured device period; `detail_ns` is the gap |
| 20 | Continuous unavailable original renderer frames exceed one current period |
| 21 | That producer wait recovered; `detail_ns` is the observed wait |
| 22 | Writer joined/stopped before callback/stream retirement |
| 23 | First observation of the existing full-period/headroom backpressure condition |

For original-engine sessions, events before 23 belong to initial queue filling;
reports after 23 and before 22 are post-prime. Report this distinction explicitly
instead of discarding an arbitrary number of seconds. It does not establish
that loading or gameplay is complete; use the application phase trace for that.
A raw movie Playback owner does not execute the engine writer and has no marker
23. Do not call its entire unclassified lifetime startup or steady state.

The callback copies atomic writer observations: phase and its timestamp, latest
queue sample timestamp, queued input frames, usable output frames, target input
frames, output-period frames, submitted frames and wait counters. This snapshot
is deliberately approximate; related fields are not an atomic transaction.
Timestamp age reveals a stale queue observation. A callback neither queries SDL's
queue nor acquires the application mutex. Phase values are 0 idle, 1 queue query,
2 acquiring an original renderer frame, 3 mixing, 4 submitting, 5 backpressure
sleep, 6 producer-wait sleep and 7 stopped. SDL additional-demand byte counts
remain approximate, especially during conversion; they are not measured hardware
underruns. The original one-millisecond waits and existing queue target are
unchanged in this diagnostic version.

All automated checks and actual-game probes use `BLUR_MAC_AUDIO_MUTE=1` before
stream Resume. This changes only per-stream gain; it leaves PCM generation,
conversion, queue/device timing, diagnostics and system volume intact. An actual
music listening claim cannot be made from muted instrumentation.

Validation: all four source/fixture compilation probes, private links and muted
actual SDL fixture runs pass. The existing silent real-renderer/device fixture now checks startup,
authored 120 ms producer starvation, demand, recovery and joined trace flush. A
paused real SDL owner exercises concurrent unique slot publication and explicit
overflow. Stable 12-callback windows at 24/36/48 kHz and 44.1-to-48 kHz conversion
have zero new approximate-demand shortfalls in all four configurations. The
120 ms authored stall is observed and recovers. Parent coherent integration
and timed actual moving-game correlation
remain required; instrumentation alone does not fix the reported lag/jitter.

## Thread and producer contracts (read-only follow-up)

The original producer is `Audio::Core::Renderer`, created with eight buffers by
`audio/source/core/core.cpp:113–119`. System Init installs the System update
callbacks and starts that producer in `audio/source/System.cpp:722–731`.
Renderer `Execute` (`core/Renderer.cpp:1492–1755`) continuously clears scratch,
runs its start callback, copies finished buses, advances the atomic ring writer
and waits when that ring is full. Native output has a separate raw `std::thread`
writer in `core/macos/Engine.cpp`; its SDL demand callback is a third scheduling
context. Neither producer nor writer normally waits for a main render/update
tick. Device consumption and bounded queues pace them.

The start callback is not just a mixer copy: `System.cpp:525–539,786–917` runs
System Update, acquiring the System lock, draining messages, updating WaveServer
and ObjectManager, then releasing the lock. `ObjectManager.cpp:798–820` submits
instance generation to the shared scheduler and waits for its actual group.
Connections and batch processing complete afterward. Non-PS3 batch Process runs
inline, although its original group Wait remains (`BatchFrameAdd.cpp:263–285`).
A prolonged unavailable-frame episode can therefore originate in producer
scheduling, a System lock, shared jobs or stream/decoder work. Phase 6 alone does
not distinguish those causes. `Renderer::GetNextFrame` increments its original
`m_uDropped` on each unsuccessful poll; that count is not hardware underruns.

| Thread path | Actual native policy / wait contract |
| --- | --- |
| Audio producer | `common/Thread.cpp:23–47` uses raw `std::thread`. Constructor stores priority, but Start does not apply it. SetPriority supports only normal and explicitly rejects others. |
| Original PC producer authority | `Renderer.cpp:154–158` selects highest only on PC; native selects normal. PC Execute raises its own thread to time-critical at 1502–1505; native does not run that code. No equivalent native real-time claim is made. |
| Engine writer | Separate raw `std::thread`; no explicit QoS request. Queue-full and unavailable-frame polling use the existing one-millisecond elapsed sleep. |
| Loader | `WaveServer.cpp:54` requests below-normal; native SetPriority returns false and this caller ignores the result. Loader polls with one-millisecond SleepThread at 254. |
| Shared job workers | `NativeTLThread.cpp` delegates to real MT Thread. `core/source/threads/macos/NativeThreads.cpp:269–271` maps positive/negative requested priority to Darwin user-initiated/utility QoS. Audio producer/writer bypass this wrapper. |
| Audio job policy | Native currently selects original non-PC/non-PS3 `SchedulerConfig.cpp:54` policy `{0,0,0,0,10,6,0,0}`. Logical worker 4 accepts audio priority 10 and worker 5 audio priority 6; most other configured policies exclude worker 4. This is real dedicated-job policy, not proof of thread scheduling latency. |

The source has genuine native priority-policy gaps, including the rejected
loader request. It does not establish that raising priority repairs the observed
jitter. No mapping, sleeps, policies, renderer gates or production source changed
in this read-only follow-up. Normal original PC ring-full yielding uses
`SwitchToThread`, while native uses elapsed Sleep(1); a replacement likewise
requires measured evidence. PS3 SyncAudioWithGame event waits and explicit movie
capture overrides are separate paths, not evidence of a main-frame gate in
normal native playback.

## Candidate repairs pending the MAC296 trace

These are proposals, not changes or proven causes. The diagnostic production
source remains frozen during the muted actual-game baseline.

Startup queue priming can use the existing paused SDL owner and measured queue
condition. The writer would submit only original renderer PCM while paused and
resume after observing the existing full-period/headroom condition. It need not
inject silence, enlarge the target, change quality or force a device period.
SDL explicitly supports opening paused and queuing before Resume.
[SDL stream lifecycle](https://wiki.libsdl.org/SDL3/SDL_OpenAudioDeviceStream).

The ownership trap is important: Renderer Init currently starts Engine output,
then System calls Renderer Start only after Init returns. Blocking Engine Init
until the producer has filled the ring would deadlock. A candidate must defer
Resume in the existing asynchronous writer, preserve explicit Resume failure
reporting, allow Stop before the first frame, and restart correctly after quality
changes. Meaningful tests would gate the real producer, prove no callback before
priming, then verify first demand sees actual queued PCM; retain the existing
extended-producer-stall, queue-bound, conversion and restart checks. The actual
trace must first establish that the residual reports belong to this initial
queue-filling window. Priming cannot repair a later producer scheduling gap.

For native priority requests, maintained SDL already implements
`SDL_SetCurrentThreadPriority`, including calls from a current thread created
outside SDL; the API returns a real failure rather than promising a platform
policy. The actual linked, source-built SDL 3.4.18 implementation under
`_work/build/macos/_deps/sdl3-src/src/thread/pthread/SDL_systhread.c:188–285`
uses Apple SCHED_RR for high/time-critical
requests, with Apple's supported priority range. This is a reusable alternative
to inventing a second Darwin priority implementation.
[SDL priority contract](https://wiki.libsdl.org/SDL3/SDL_SetCurrentThreadPriority).

The existing generic MT adapter's coarser QoS mapping is another possible policy,
but neither option is equivalent to Windows time-critical scheduling. Apple's
public pthread QoS header states that incompatible pthread scheduling changes
permanently opt the thread out of QoS; do not apply both strategies to the same
thread. Any later implementation must apply supported creation-time requests
inside the worker, verify the actual API result, preserve live-thread failure
semantics and test owned joins/restarts. The caller's enum currently has seven
levels while SDL has four, so a mapping needs an explicit reviewed contract.
[Apple pthread QoS authority](https://github.com/apple-oss-distributions/libpthread/blob/main/include/pthread/qos.h).

Audio Workgroups are not a drop-in QoS setter: Apple describes participation for
real-time threads sharing rendering deadlines. This producer currently performs
allocations, locks, streaming and shared-job waits. Marking it real-time without
a different verified ownership/deadline contract would overstate what the source
can safely do. SDL's own CoreAudio device callback remains owned by SDL/CoreAudio.
[Apple audio-workgroup contract](https://developer.apple.com/documentation/audiotoolbox/understanding-audio-workgroups).

The installed/public `SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES` is only a request made
before opening a device and may be adjusted or ignored. The measured device
period already drives the native queue target. Forcing a smaller period or
adding extra buffering would not explain a 223 ms gap and has no approval from
these source observations.
[SDL device-period hint](https://wiki.libsdl.org/SDL3/SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES).
