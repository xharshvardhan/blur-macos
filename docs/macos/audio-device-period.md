# MAC269 native audio device-period contract

The reported noisy/choppy music exposed a reproducible native output fault.
The SDL writer used a 60 Hz queue target without accounting for how many sample
frames the actual output device consumes in one callback. No decoder, audio
quality, simulation cadence or thread-priority change is part of this repair.

## Source and measured baseline

The recovered PC engine has a separate device buffer and 128-sample renderer
frames. Its output clock is the device playback cursor; it does not limit the
device queue to one game tick. Original PC clock, quality-rate pitch correction
and output mixing are documented in `docs/analysis.md`, Audio output (PC).

Before MAC269, native `Engine::WritePCM` stopped adding frames when its SDL
input queue reached `sampleRate / 60`. At 48 kHz, writes of 128 frames therefore
filled an empty queue to 896 frames and stopped. The measured CoreAudio device
consumes 1,024 frames at 48 kHz per callback. Its SDL request was 8,192 input
bytes: 1,024 stereo float32 frames.

A steady silent original-renderer test, after startup and without an injected
stall, measured 12 shortfalls in 12 callbacks, totaling 12,288 input bytes.
That is exactly 128 missing stereo frames per callback. Producer-not-ready
retries did not increase during this window. All four build configurations
reproduced the shortfall. No nonfinite or clipped samples were measured in this
silent fixture. The user's sampled process showed mostly sleeping producer and
writer threads; sampling alone did not establish a dropped-frame count.

SDL generally describes callback additional-byte demand as approximate because
rate conversion can require padding. For this measured equal-rate case, the
installed SDL source computes the difference between requested and available
frames without resampling. The diagnostic field names still say approximate
demand; they are not presented as hardware-underrun counters.

## Native correction

`SDLPlayback::DevicePeriodInputFrames` queries the current logical device's
format and period with the existing SDL APIs. It validates the returned values
and rounds up `devicePeriodFrames * inputRate / deviceRate` using 64-bit integer
arithmetic. An invalid device format fails clearly instead of choosing a
fallback period. The writer queries each iteration so it does not retain an old
period across default-device format changes; quality changes already reopen
the output through its original joined lifecycle.

Converting the period alone was insufficient: with the physical device at
44.1 kHz and input at 24 kHz, the intermediate implementation still measured one
8-byte demand shortfall in 12 steady callbacks. That failed source, binary and
log are retained. No producer wait occurred in that window.

The final query obtains the actual stream destination format and batches
queued-input and usable-output reads under SDL's public recursive stream lock.
It queries the device before acquiring that lock to avoid reversing the device
mixer's lock order. `SDL_GetAudioStreamAvailable` accounts for current resampler
history and padding. The query converts those usable output frames back to
input units, rounding up, and measures retained input as the nonnegative
difference from queued input. The target is the larger of the original
`inputRate / 60` cadence and the converted device period plus that measured
retained input. No filter-padding constant is guessed.

The writer waits only when queued input exceeds that target and at least one
device period is usable. Its existing 128-frame writes can place up to 128
frames above the measured target, depending on rounding and queue phase; this
does not promise a full 128-frame margin on every format. With the measured
equal-rate 48 kHz/1,024-frame device, the filled queue is 1,152 input frames.
SDL continues to own resampling and channel conversion. No fixed latency
constant or larger renderer ring was introduced.

Additional retained resampler input is limited to one existing producer ring:
`CreateRenderer` in `core.cpp` constructs eight 128-frame buffers. Exceeding
that 1,024-frame additional-input budget stops output with an explicit error;
it does not keep moving the target and accumulating a backlog. The device
period itself is not capped at the ring size. This is a native bounded buffering
policy, not a retail format claim. Exotic conversions needing more retained
input are unverified and fail rather than silently selecting another quality.

## Opt-in diagnostics and validation boundary

All automated runs use `BLUR_MAC_AUDIO_MUTE=1`. `Playback::Open` applies SDL's
public per-stream gain of zero while the new stream is still paused, before
any resume. A rejected gain change fails the open with SDL's actual error.
This covers the native game and movie streams without changing system volume,
device-global volume or saved game options. The default remains audible for
manual use. Engine PCM measurements occur before the SDL stream gain, so the
mute does not replace decoding, mixing, queue flow or device-period checks with
a silent engine path.

Set `BLUR_MAC_AUDIO_DIAGNOSTICS=1` before launching to collect a device-lifetime
window. The writer measures producer waits, empty-queue observations,
backpressure polls, submission gaps, clipped/nonfinite mixed samples and peak
amplitude, maximum measured target and retained-input padding. SDL's demand
callback updates only lock-free atomic counters: no
allocation, application lock, logging, wait or sample generation. A single
`[BlurMacAudio]` summary is written after the application writer joins. Totals
include startup. Empty-input-queue observations are not proof of hardware
silence because SDL may already have converted data.

`audio_diagnostics_test.cpp` exercises actual silent renderer and CoreAudio
output, opt-in/off, steady 24/36/48 kHz quality windows, bounded queue size,
an authored 120 ms producer stall and recovery, and joined restarts. A separate
raw SDL 44.1 kHz input window checks rounded period conversion and rate
conversion; it does not add a 44.1 kHz game-quality option. The existing audio
core test now checks the measured period plus bounded mixer-quantum queue cap.
No proprietary bank or audible test signal is used.

Private before/fixed source probes, logs and binaries live under ignored
`_work/build/macos/audio-diagnostics-agent/`. Source probes pass all four
configurations. The usable-output version passed the silent actual-device
fixture in all four configurations: all 12 post-startup callbacks in each
24/36/48 kHz window had zero demand shortfalls. At 24/36 kHz into the 44.1 kHz
device, observed retained input reached seven frames; measured maximum targets
were 565 and 843 input frames. The authored 120 ms stall was detected and output
recovered. The final cap-enabled fixture also passed all four configurations,
including actual 44.1 kHz input resampled to a concurrently held 48 kHz device:
the converted period rounded up to 941 input frames, and all 12 steady callbacks
had zero demand shortfalls. Disabled diagnostics, device restarts and scoped
scheduler cleanup were exercised. The cap's exceptional path is a source-level
bound; no proprietary audio or production fault-injection hook was added to
force it.

The PC/native thread-priority difference remains unchanged: the sampled waiting
threads did not prove that a priority change was needed. Coherent game builds and muted actual music/device runs now pass their application
paths; [feedback acceptance](player-feedback-acceptance.md) records the remaining
sporadic demand shortfalls. Perceptual music listening remains unverified. These silent results do
not establish that every source of the user's reported audio noise has been
removed.

Primary SDL references: [device format and sample frames](https://wiki.libsdl.org/SDL3/SDL_GetAudioDeviceFormat),
[stream input-queue units](https://wiki.libsdl.org/SDL3/SDL_GetAudioStreamQueued),
[callback demand semantics](https://wiki.libsdl.org/SDL3/SDL_SetAudioStreamGetCallback).
