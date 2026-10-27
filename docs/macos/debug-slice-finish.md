# Original Debug one-car finish and exit

MAC095, source audit, 2026-10-27. No production changes, game runs, progress
changes or additional multiplayer work. This extends the mode finding in
[slice-assumptions.md](slice-assumptions.md).

The supported minimal configuration candidate is a **fresh process**, original
`Ui.NewLaunchMode=Debug`, the existing Brighton1/BMW/controller0/NumCars1/Laps1
private configuration, and the existing Lua call below before normal Return on
DebugScreen. It has not yet completed a driven lap on native macOS.

```lua
Amax.SetupRace({RaceMode=UIEnums.RaceMode.StreetRace})
```

`GameModeDebug.cpp:100–102` accepts this original optional RaceMode member.
`LuaVariableHelper.cpp:100–110/153–154` exports the named enum. Safe reads preserve
omitted members (`lua_shared/source/LuaHelper.cpp:140–149`), including when normal
DebugScreen LaunchGame later submits its gadget table without RaceMode. One-car
population is preserved; no numeric enum, new gadget or career event is invented.

## Why no SP pass rules are needed for a genuine lap finish

- RuleManager is the namespace-static instance at
  `GameLogic/RaceRules/RaceRulesManager.cpp:24`. Its vectors begin empty. Init26
  subscribes to LapIncremented/fatal damage and resets elimination count; it
  does not populate event rules. AmaxWorldLayer791 calls this original Init.
  Debug SetupRace does not call InitRulesFromEvent. UISetup's temporary
  SinglePlayer selection also does not set an event: SinglePlayer::Start44 is
  empty. This empty-rule conclusion requires fresh direct Debug startup, not
  a Debug race entered after a career event whose rule arrays may remain.
- LoadingGame155–158 uses its original Debug branch to select HUD and send
  RaceStarted. StreetRace's base StartRace125–137 enables normal racing;
  StreetRace Update73–92 calls RuleManager Update only while in progress.
- The active lap authority is AIR vehicle logic, also used by human vehicles.
  `AI/AI_Amax.cpp:2603` calls FillInAnyRaceLogic before its conditional AI-input
  branch; the equivalent human/shared PreUpdate path calls it at3445. It copies
  AIR LogicState::GetLap to vehicle GetLapInfo().mCurrentLap at3163. This does not
  give the human AI driving control.
- `ai/air/source/splinetracker/csplinetracker.cpp:525–580` posts actual entering/
  leaving start/intermediate/finish-zone messages. Vehicle logic handles them at
  `vehiclelogic/cvehiclelogic.cpp:105–180`; StartZone896–944 increments circuit
  laps on FINAL_THIRD to FIRST_THIRD. IntermediateZoneOne1051–1073 and
  IntermediateZoneTwo1100–1141 use real vehicle position and route planes;
  ValidateZone/ValidateInter1/ValidateInter2 handle leaving/reversal rules.
  Point-to-point routes use the original FinishZone989–1036 instead. These are
  the original progress rules; no substitute checkpoint algorithm is proposed.
- AIR Update717–857 sets the lap/split notification flags from its real state;
  `AI_Amax.cpp:3622–3665` broadcasts those actual LapIncremented/split messages.
  The legacy RLC TrackPositionController has separate ordered split/lap counters
  and timing logs. Those are not the direct source of the vehicle lap value used
  by StreetRace; their logs alone must not be mistaken for finish-rule proof.
- RuleManager388–406 checks currentLap greater than configured laps, or an
  actual configured time limit. Its explicit `!mRaceRulesPass.Size()` case permits
  lap completion without SP rules. Empty rules do not immediately pass: that
  branch is inside the lap/time gate. The last-survivor shortcut257 requires
  more than one vehicle. Keep the fresh Debug time limit0 and Laps1.
- SendPassMessage430–438 emits Passed_All_Rule; StreetRace335–345 emits the real
  RaceFinished message for the human. Its original RaceFinished handler forces
  player control to the game's postfinish AI; this is after genuine completion,
  not an AI substitute for driving the lap.

Nothing remains unsuitable for this finish proof: RaceBookLayer464 omits its
Update, and the base Update is empty. Lap timing alone is not a RaceFinished
signal. Switching to SinglePlayer would rebuild from an SP event
(GameModeSinglePlayer49–53), changing the selected event/population. Other event
modes require their event-specific setup. SplitScreen's original setup accepts a
player table and optional AI population, but enters the multiplayer mode/result
contract and is outside this slice; it is not a reason to enable single-racer
network settings.

## Postfinish boundary and legitimate application exit

The original Debug UI does not provide a proven complete offline results route.
HUD762–786 sends SinglePlayer to SP results, while Debug reaches MpFinished.
Its NetRace.FlagInRace call is guarded when no real race is running
(lua_NetRace2241–2260), so that call alone is not evidence of an immediate crash.
However after1second MpFinished56–63 calls GetRaceResults: Lua_Multiplayer461–495
uses C_MpResults sorted vehicle indices directly, while C_MpResults::Clear sets
local indices to-1 and player indices to-1. Plain StreetRace does not initialize
that multiplayer result set. Its inherited RaceFinished() flag also remains
false; StreetRace does not set the flag used by Multiplayer.RaceFinished(). A
normal, safe Debug results progression cannot therefore be inferred from the
lap evaluator. Do not invoke SP SetupResults either: StreetRace GetEvent284–299
asserts an SP event lookup, while plain Debug mSpEventId starts at-1.

For the bounded finish-and-exit acceptance, the existing application quit API is
`UISystem.SystemQuitGame(true)`, after observing the genuine finish. It sets the
original quit request (Mercury UI Lua_System665–675), latched by UiManager2648–2651
and handled by native GameApp903–907 through RequestShutdown. This does not set
race progress or manufacture results. An existing external verification client
can issue that call when the genuine RaceFinished notification arrives; actual
validation must confirm the request is serviced before entering the unsupported
later Debug result processing. This source audit does not promise that timing.
Do not intercept or suppress the real finish callback to make the test pass.

## Existing observation, without a new gameplay API

GameAutomationMessageHandler99–120 already publishes RaceStarted, RaceFinished
and `LapInc:0:(lap-1)`. Correlate actual AIR route progress and `LapInc:0:1` with RaceFinished for a
one-lap race. An initial `LapInc:0:0` may occur when crossing the start; initialization
can also begin at lap1 based on the real start zone, so do not require an invented
fixed initial notification sequence. Blur2PC.RaceState() reads
RaceLogicLayer's state, but Finished alone is insufficient: the same handler
family also handles quit/restart actions. Amax.GetNumVehicles() and the existing
level-data query can verify the one-car configuration; they do not prove motion.

Existing read-only diagnostic logs can be enabled through the already available
DebugValue configuration interface: `RLC.Stamp`,
`RLC.LapSplitTimeController.OutputLapTimeInformation` and
`RLC.LapSplitTimeController.OutputSplitTimeInformation`. Stamp output reports
legacy RLC ranking, distance from start, current split/lap and times
(RLLapSplitTimeController79–90); it does not report XYZ/speed or replace the active
AIR lap scalar.
HUD/GAME data finders read real MPH and lap values but format/clamp them for UI;
the inspected Lua registrations expose no general scalar XYZ/speed/checkpoint
getter. Do not fabricate one for this audit. Use the existing native frame
capture for visual driving, and bounded read-only debugger inspection of the
actual vehicle world matrix/position/speed and AIR LogicState
zone/lap/route position plus the vehicle-logic lap info when exact scalar evidence is needed. Existing Tap/
Hold drives the real keyboard map; no checkpoint writes, reset/teleport calls,
AutomatedHuman, StartEvent, QuitAndWin, `RLC.IncrementLapCount` (an actual
progress mutation at cvehiclelogic833–837) or synthetic RaceFinished messages belong
in the completion proof.
