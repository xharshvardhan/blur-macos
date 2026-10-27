# Ordinary keyboard operator and natural-finish evidence (MAC176)

Bounded source/read-only review of ignored
`_work/runtime/research/drive_keyboard.py`, its actual MAC172 logs, the original
AIR/control/rule code and [the acceptance gates](PLAYABLE_SLICE.md). No game
session or shared build was launched. The only production change is the narrow
read-only route-adjacency correction below; no input, pose, checkpoint, lap,
race result, AI flag or game progression is written.

## Source-supported telemetry correction

`code/amax/library/Ui/UiManager.cpp`, native `Blur2PCLua_VehicleState`, formerly
hand-computed the next sample as current+1, wrapping a loop to0. Original
`AITrackDataRoutes::GetIndexOffset` in
`code/mercury/library/ai/air/include/trackdata/trackdata.h:1013–1040` wraps the
forward navigation seam to1, explicitly skipping0. Real CSplineTracker uses that
helper at `source/splinetracker/csplinetracker.cpp:797/803/877/883`; initial nearest
search scans1 through NumSegments-1 at1029/1033. Segment bounds also use both
index and index-1 (trackdata.h1055–1063). Thus an authored boundary sample0 is
not the original forward navigation seam.

VehicleState now uses that existing helper with offset1, and requires more than
one sample before publishing navigable road data. Current position/direction
reads retain the existing 0<=index<NumSegments bounds, so a valid authored point0
can still be observed. Open routes retain the original clamped last endpoint;
the operator correctly refuses to invent an extension. The helper-selected next
index is valid for this bounded one-step call. No broad arbitrary-offset helper
or route-layout change is made.

The rest of this getter already copies actual RouteInfo route identity,
occupancy, offset/width, authored step/line position/direction, and original
vehicle logic lap/progress/distance fields. Distance/progress are not recomputed
from roadSegment, and volume fields inStart/inSplit1/inSplit2/inFinish are current
occupancy, not accumulated passed-checkpoint counts. The snapshot remains guarded
by main-thread execution, scheduler completion and actual actor/world lifetimes.

`tools/macos/route_adjacency_test.cpp` constructs real original AITrackDataRoutes
objects with2/3/8/742 authored samples. It checks the original loop seam1,
interior adjacency, valid navigation bounds, open-end clamping and unchanged
sample-count/step/length metadata. It uses the real GetIndexOffset, not a copy.
All4 UiManager translation-unit and fixture compilations pass. Private CPU
helper links/runtimes exit0 in all4 configurations using the original CHash.cpp
plus core_runtime closure. These thin links completed around the parent's177
build handoff and are **not** claimed as coherent177 archive validation. No
further shared archive reads were made after the parent hold. Standard leaf
integration needs AIR/include and the original air/core dependencies, no game
assets, arguments, GPU or service. Logs are private under
`_work/build/macos/physics-agent-route-adjacency-<configuration>/`.

## Keyboard and timing findings

Original `Game/GameControls/GameControls.cpp:111–125` maps layout0 Q to throttle,
A to brake, with original left/right steering mappings. Its native platform
selection already reuses that recovered PC setup. Actual MAC172 proves Q motion
and Right positive yaw (0.0395367 radians in the bounded calibration), rather
than inferring movement from an accepted IPC command. PLAYER controlType0 and
three separate false AI/automated-human flags are checked; AIR providing route
and lap logic does not make that vehicle AI controlled.

NativeInput holds are elapsed performance-counter overlays starting at the first
real input poll. Expiry leaves physical pending keys intact. The private operator
uses bounded holds, acknowledgement output, freshness checks and advancing game
time; no rendered-frame-count timing is introduced. A Hold acknowledgement alone
is not displacement or proof of exact input release. The operator deliberately
waits between actions, but its telemetry has no key-release timestamp. Unexpected
hold rejection or a missing acknowledgement must remain visible; neither is input
proof. A changed keyboard layout likewise requires fresh mapping/motion evidence.

Road occupancy and wrong-way gates are conservative pause conditions. Once the
car leaves the road or turns sufficiently wrong, the script stops sending all
keys; it cannot steer its own recovery. That can strand this private operator,
but is not evidence the game cannot recover through ordinary manual keys. There
was no such gate in the actual172 drive rows. `adjacent_sample_not_ahead` similarly
pauses rather than extrapolating a target. The source-backed seam repair prevents
one demonstrably different navigation convention; broader lookahead/steering
changes would require actual evidence and separate operator tuning.

MAC172 drive observations cover only about14 wall seconds, raceTime77.08–85.55,
and road samples168–177 of742 before the render failure. The42.91m displacement
is not79 seconds of continuous driving. The observed authored route metadata is
742*4m nominal length≈2968m. At the script's7–11m/s targets even a nominal full
loop takes roughly270–424 simulation seconds before startup, bends and staging;
this is a budget estimate, not distance-to-finish or a guaranteed completion time.
Default operator20seconds or probe240seconds including startup cannot be treated
as a full-lap budget. Use an explicitly longer bounded session and actual progress
observations. Operator exit0 when its observation budget expires is not a finish.

## Finish classification and exit boundary

The fresh direct Debug candidate remains original StreetRace configuration,
Laps1, one human car, no time limit and empty SP rules, as recovered in
[the finish audit](debug-slice-finish.md). RuleManager388–406 requires actual
vehicle logic currentLap>configuredLaps (or an actual configured time limit).
AIR StartZone increments a circuit lap only after FINAL_THIRD→FIRST_THIRD;
intermediate-zone validation remains original. The initial lap may be0 or1.
With configuredLaps1, a genuine completed lap normally produces actual lap2 and
Automation's `LapInc:0:1` because GameAutomationMessageHandler subtracts1. A
starting `LapInc:0:0` or a fractional progress scalar alone is not completion.
Actual172 lap0/progress0.874 does not establish87.4% of a validated finished lap:
AIR logic progress is start-relative and separate from the road sample index.

A concrete private watcher classification weakness was reported to the parent:
`run_native_probe.py` previously labelled **every** actual RaceFinished notification
`NATURAL_FINISH_QUIT`. Original RaceModeStreetRace.cpp335–345 emits that event for
passed rules, but360–375 also emits it on human fatal damage and385–389 on failed
rules. RaceFinished alone is therefore not natural-lap proof. The watcher should
still request normal quit promptly on any terminal event to avoid the unproven
Debug results route, while qualifying success only with real lap evidence and
without failure/wreck evidence. A non-lap terminal event must remain failed
acceptance, regardless of clean process exit. MAC176 does not edit that parent
owned launcher. Parent has now implemented `LAP_QUALIFIED_FINISH_QUIT` only when
actual `LapInc:0:1` precedes RaceFinished, otherwise `NON_LAP_FINISH_QUIT`, plus
a separate status field. Both still request ordinary quit. This is a stricter
evidence classification, not new runtime completion proof.

The original Debug results route still reaches MpFinished and its uninitialized
multiplayer-result assumptions after a delay. Existing UISystem.SystemQuitGame
is a legitimate application request after observing the terminal event, not a
progress mutation; its dispatch acknowledgement is not proof of normal exit.
Record actual notifications/progress, final process status and teardown outcome.
No natural finish or normal successful full-game exit is established by MAC176.
