# Playable slice and validation gates

The immediate user-approved scope is one Brighton route, one BMW, keyboard
control, a completed race and clean exit. Normal menu flow, career save/reload,
multiplayer enhancements and distribution follow only after this slice works.
Completed source-only work outside the slice remains local and is not evidence
that those later goals have been met.

## Configuration and source authority

The ignored `game-test` root uses the verified original PC archives and level
quartets, an independent debug configuration and no copied Windows executable.
`Ui.NewLaunchMode=Debug` is the engine's existing direct debug entry from
UISetup.lua. The original DebugScreen launches the configured level through
its normal MenuNext/Return handler. Its configuration is Brighton_1, one BMW,
controller0, one viewport, one lap, no random cars and no additional debug cars.
The original DebugCarScreen permits one car and RaceLogic has a single-car
spawn branch. Race-mode and finish behavior still require source/runtime
verification; a lap setting alone is not proof that the event can finish.

The implementation keeps simulation on its fixed original ticks with unlocked
rendering. Configuration and normal input may select/start the event; tests
must not synthesize a finish, teleport through checkpoints or replace the
vehicle/game with a fixture.

## Required evidence

1. The actual archive loader and complete production renderer/effect graph
   initialize every required resource. Failure reaches the main application
   before an incomplete layer becomes Ready or renders. Tests of an individual
   shader/effect do not establish this gate.
2. The real configured track and exactly one expected vehicle load. Its actor,
   rendered model and physics are valid. Logging once or emitting RaceStarted
   cannot bypass these conditions.
3. A completed actual GPU frame shows the real car/track. Native keyboard input
   changes the real vehicle's motion and steering through the normal mappings.
4. The original route/checkpoint/lap and race rules produce an actual finish;
   record both real state/events and progress rather than a test-assigned state.
5. Normal shutdown releases workers, layers, subscriptions, cached resources
   and GPU owners in dependency order. An owned external timeout is a failed
   exit check, even when it cleans up the test process safely.

The grouped audits are [platform/format/LP64/ownership](slice-assumptions.md),
[startup and vehicle readiness](slice-initialization.md), and the production
renderer/effect graph work recorded in [the ledger](LEDGER.md). Fixes should
cover the identified contract/class and then run through the real game path.

## Current actual evidence

**MAC183 passed all five slice gates** with the coherent Speed182 arm64
Cocoa/Metal application and original Brighton/BMW archives. The actual GPU
capture shows the car, track and HUD. Ordinary finite Q/A/Left/Right holds
travelled approximately3309.57m; all705 observed Racing telemetry records used
human control with configuredAI, aiToggle and automatedHuman false.

The original game emitted RaceStarted, RaceLoaded, LapInc:0:0, both ordered
split crossings, LapInc:0:1 and RaceFinished. The operator changed no pose,
checkpoint, lap, progress or result state. Only after that lap-qualified finish,
the harness requested the original application quit. The actual process exited0
after571.51seconds with no primary error or external termination. The existing
Debug MP results screen was not part of this slice; normal menu/results flow is
being validated separately. Actual179 separately verifies populated shutdown
with Debug assertions enabled.

Evidence is under ignored `_work/runtime/verification/`: the
`mac183-playable-slice-proof.json` records the binary SHA256, events, controls,
travel and successful exit; `mac183-game-frame.png` is the gameplay frame;
`mac183-notifications.log` and `mac183-keyboard-drive.jsonl` retain the observed
race and input records. The screenshot alone does not prove the finish.

Normal Master menu flow is now the active phase, using independent configuration,
profile identity and save roots. Actual190 created a genuine career save with
career-completion overrides disabled. Actual194 Debug reloaded version1378 with50903bytes/0unread and reached
normal SP hub/tier/first unlocked event, with genuine intro-movie autosave.
Its shutdown asserted in the audio PagedArray and failed;195/196 investigate.
Normal career results, end-to-end clean menu exit, multiplayer and distribution
are not yet verified.
