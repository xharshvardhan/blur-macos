# MAC224 private ordinary-key operator pacing

The new ignored `_work/runtime/research/drive_keyboard_paced.py` imports the
existing telemetry/finite-Hold transport without changing the old driver or any
production game code. Root owns the live normal-career run and results.

## Reason and selected policy

Actual MAC215's old operator requested only22.75s of Q holds across94.93s of
logged wall time, approximately24 percent requested throttle duty. Its observed
median speed was3.18m/s. Original MAC183 used the same adjacent-point steering
and successfully finished the qualified slice, with median sampled9.47m/s.
Those observations support addressing operator pacing before increasing speed
or inventing a different path.

The new operator retains the proven adjacent authored racing-line sample,
Right-positive steering,7m/s turning and11m/s straight targets. It requests at
most0.75s Q by default, keeps short existing A/steering pulses, removes the
old global .4s sleep and treats longitudinal and steering expiry separately.
The adjacent tangent difference is diagnostic only; it does not fabricate a
lookahead sample, extrapolate a track endpoint or alter physics.

Source authorities:

- `Game/GameControls/GameControls.cpp:112-121,169ff`: original layout0 Q/A and
  Left/Right mapping; layout1 differs.
- `UiManager.cpp:2527-2595`: actual primary vehicle/AIR read-only telemetry;
  route adjacency uses original GetIndexOffset, including last-to1 loop seam.
- `input/macos/NativeInput.mm:37-46,131-136`: Hold starts on a later input poll
  and expires by real performance-counter time, not Lua acknowledgement or
  rendered-frame/simulation-tick count.
- `Profile/GameProfile.cpp:462-509`: current options getter does not export
  mKeyboardLayout. The operator must not invent a runtime layout read.

## Guards and finite ownership

Every action needs Racing, primary vehicle0/PLAYER, enabled controls, AIR ready,
configuredAI/aiToggle/automatedHuman all false, fresh finite telemetry, advancing
race time, on-route/not wrong-way, valid heading and actual adjacent indices.
Snapshot maximum age defaults .8s and cannot exceed1s. Individual holds must
be finite in (0,1]; the command uses only Q/A/Left/Right.

ACK is not a hold-start or release proof. An acknowledged axis stays busy until
a fresh eligible advancing post-ACK state sample, then for the full requested
wall duration plus .08s. This conservative fence applies independently to each
axis, so ordinary steering can coexist with throttle while opposite keys cannot
be intentionally stacked. Rejected/unknown Hold acknowledgements stop further
input rather than guessing another owner's active duration. Existing physical
key ownership is untouched. Unavailable/stale/unsafe telemetry sends nothing;
previous finite overlays expire naturally. Finish is observed only; this
operator has no quit, checkpoint, lap, result or event setter.

Drive requires explicit `--layout0-confirmed`, based on the real profile/options
and observed mapping. Root confirmed its unchanged fresh-profile layout0 and
actual Q-motion evidence for this run; the flag is not a dynamic layout query.

## Interface and bounded verification

```sh
python3 _work/runtime/research/drive_keyboard_paced.py \
  --game-log /absolute/path/to/mac226-game.log \
  --seconds 3000 --drive --layout0-confirmed
```

The log must belong to the owned actual run. Runtime is finite and at most3300s;
existing matching owner-status appearance stops the operator. It neither
launches the game nor acquires a GPU/service slot itself.

Pure replay of852 actual MAC183/MAC215 records yields745 eligible candidate
decisions with the unchanged steering calculation. Guard checks cover
nonprimary/AI/disabled/off-road/wrong-way/nonfinite/stale telemetry, duration
rejection, loop seam, independent axes, no-ACK-start assumption, advancing-time
fence and rejection fail-closed policy. Syntax compilation and missing-layout
CLI rejection pass. Results are ignored `mac224/pure-checks.json`.

These checks use no IPC and predict no vehicle dynamics, improved speed,
collision recovery or natural three-lap career finish. Root's actual run must
establish those results from original notifications, checkpoint/lap progression
and normal results/shutdown; external timeout is not finish evidence.
