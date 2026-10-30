# Audio startup refusal: diagnosis and successful recheck

MAC376/380,2026-10-30. User requested a fresh check and an explanation of the overnight failures. All tests remain muted before playback; system audio routing, volume, power settings and services were not changed or restarted. No dummy audio driver was used.

## What failed

The independent SDL/CoreAudio silence-only fixture reported `CoreAudio error (AudioQueueStart): -66681`. The installed Apple SDK defines this as `kAudioQueueErr_CannotStart`. It is a generic start failure, distinct from `CannotStartYet`; the error alone does not establish permissions, format incompatibility, device corruption or a specific workaround.

Installed/upstream SDL opens and configures the physical AudioQueue, allocates buffers, fills them with silence and enqueues them before calling AudioQueueStart. That happens inside SDL_OpenAudioDeviceStream. The game's stream gain, game PCM producer and logical Resume come later. Consequently the failing startup is not evidence of race-music decoding failure, queue underrun during racing, or a mute-gain problem. See [Apple result code](https://developer.apple.com/documentation/audiotoolbox/kaudioqueueerr_cannotstart), [AudioQueueStart](https://developer.apple.com/documentation/audiotoolbox/audioqueuestart%28_%3A_%3A%29), [official SDL implementation](https://github.com/libsdl-org/SDL/blob/main/src/audio/coreaudio/SDL_coreaudio.m).

## Timestamp-correlated evidence

For failed actual run369, the owned process was33683. CoreAudio logs at22:35:52UTC on29October (04:05:52IST30October) record UserActivityLevel0 and a transition from Sleeping to DarkWakeEntitledIO. Device configuration changed and the built-in speaker IO start reported success at the service side. At22:36:02UTC the game process's CoreAudio client reports `timed out after 15.000s`, then records its built-in-speaker client stopping after failed start. These are two sides of the same startup attempt: service-side start success does not establish that the AudioQueue client completed its start handshake.

This locates the observed failure at a physical audio-start timeout and associates it with background wake/device reconfiguration. A wake/startup timing problem is a supported hypothesis; these logs do not prove the exact internal macOS cause. We did not reproduce it deliberately by sleeping/locking the Mac or change wake settings. The first exploratory log query interpreted an unzoned start/end in local time; only the corrected explicit+0000 window above is used for this conclusion.

The same late-night failure affected candidate runs369/368, standalone output probe371, and the exact accepted0.1.6/build352 startup diagnostic370. This independent/stable-build reproduction argues against assigning it to the new graphics features. Failed runs remain failed and retained.

## Recheck after the user's return

The unchanged real-output fixture376 now passes with actual CoreAudio: silent PCM consumption,48kHz stereo input,24kHz six-channel and36kHz eight-channel input conversion, pause/clear/restart, duplicate-open rejection and borrowed-subsystem ownership. A backend assertion rejects substitutes; this is not a dummy-driver check.

Fresh actual-game runs377(AA2/authored filtering) and378(AA2/filtering16) also complete original20-car human driving, original race/menu/desktop exit0 and source-state preservation with real native output muted. No production audio change was needed to recover startup. Private telemetry handling was corrected separately to report nil safely and use a consistent bounded observation budget; it does not change game/audio code or inject input into an unqualified state.

Future private probes now recognize `ERROR: Native application initialization failed:` as an explicit fatal marker, rather than leaving primary=None when that prefix appears. Raw earlier proofs are unchanged. The game still stops initialization on a real audio failure rather than rendering an incomplete audio system.

## Limits

Successful silent output and actual-game completion establish current device startup/consumption, not a listening-quality verdict. The earlier reported race-music crackle/lag remains a separate perceptual check. If this start failure recurs, retain exact timestamps, SDL/CoreAudio error and device state; do not blindly reset system services, alter saved routing or retry an arbitrary generic error indefinitely.

## MAC380 final qualification

Final actual-game run381 also passes the compiled default settings, real native output muted before playback, quiet driving and ordinary race/menu/desktop exit0. Both complete final Debug/Speed suites pass155/155. No new audio-code workaround was introduced. The listening-quality limit above remains.
