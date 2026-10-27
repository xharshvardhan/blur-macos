# Native keyboard race verification

MAC110 adds `Blur2PC.VehicleState()` to the existing native, non-Gold UI Lua
library. This is a read-only diagnostic snapshot for slot0 of the current race,
not an AI driver or a race-progress API. Existing `Tap`, `Hold` and `RaceState`
remain the input/state mechanisms.

The helper returns nil outside the macOS main thread, while original scheduler
jobs remain active, during loading/UI/teardown, or when current World/RLC,
loaded live actor, actual model/physics or configured car identity do not match.
It does not flush or run jobs. Current race state must be Starting, Racing or
Finished. The current actor is resolved from the original RLC handle against
the current World's actor list, rather than trusting a remembered pointer.

## Fields and source authority

| Fields | Actual source |
| --- | --- |
| `x,y,z`, `fx,fy,fz` | VehicleLogicActor `GetPositionInfo`: world position and vehicle +AZ direction, populated from the actual graphical actor matrix in VehicleLogicActor.cpp304 |
| `speed` | Original signed forward velocity (`velocity dot zAxis`), copied from AIR by AI_Amax.cpp3156 |
| `mph` | Original magnitude-based mph in VehicleLogicActor.cpp481; reversing can have negative speed and positive mph |
| `lap,laps,lapTime`, `progress,combined,distance,wrongWay` | Actual VehicleLapsInfo, VehiclePositionInfo and current LevelState; AI_Amax.cpp3154–3164 copies the AIR progress/lap fields |
| `carId`, `vehicleIndex`, `raceState,raceTime,vehicleState,controlsEnabled` | Current matching actual actor/RLC/level getters; carId preserves the original signed PC32 Lua ID representation |
| `controlType,configuredAI,aiToggle,automatedHuman` | Raw current configured/logic flags; the diagnostic deliberately avoids IsVehicleAIControlled's automated-human disguise |
| `airReady` | Existing AmaxAIR::AllOK; absent road fields remain absent when AIR is unavailable |
| `inStart,inSplit1,inSplit2,inFinish` | Existing nullable GetLogicStart/Inter1/Inter2/Finish RouteInfo occupancy; these are not accumulated checkpoint counts |
| `roadRoute,roadSegment,roadSegments,roadIn,roadLooped,roadStep` | Actual current RouteInfo and authored route getters |
| `roadWidth,roadOffset` | Actual RouteInfo width and original `offsetCentre * width`, matching AI_Amax.h GetDistanceToCentreOfCurrentRoute |
| `roadFx,roadFz`, `roadX,roadY,roadZ` | Actual current route segment direction and authored TRACK_DATA_LINE_RACING point |
| `roadNextSegment,roadNextX,roadNextY,roadNextZ,roadNextFx,roadNextFz` | Adjacent authored racing-line sample and direction; wraps only routes explicitly marked looped, otherwise clamps at the route's last segment |

VehicleLapsInfo can begin at0 before crossing the start. Do not convert that
initial state into a completed lap. AIR StreetRace logic advances ordered
intermediates and lap/start/finish transitions. AI_Amax.cpp3653/3661 publishes
real split1/2 messages, and RaceLogicLayer.cpp3339 already exposes
`SplitCrossed:vehicle:lap:split` through Automation. Keep a watcher attached
before launching; these notifications provide ordered split evidence without
adding counters or mutating progress. Route-end samples are not finish proof.

## Short original Automation commands

The original Lua-command message accepts fewer than128 bytes. The existing CLI
reports dispatch acknowledgement; Lua `print` appears in the real game output.
Use short queries, and treat a nil snapshot as unavailable data to retry.

```lua
v=Blur2PC.VehicleState();if v then print('pose',v.x,v.z,v.fx,v.fz,v.speed)end
v=Blur2PC.VehicleState();if v then print('road',v.roadNextX,v.roadNextZ,v.roadOffset)end
v=Blur2PC.VehicleState();if v then print('lap',v.lap,v.laps,v.progress,v.wrongWay)end
v=Blur2PC.VehicleState();if v then print('input',v.controlType,v.aiToggle,v.automatedHuman)end
print(Blur2PC.RaceState())
print(Blur2PC.Hold('Q',0.4))
print(Blur2PC.Hold('Q',0.4),Blur2PC.Hold('Left',0.15))
```

The original layout0 source (GameControls.cpp121–124/203) maps Q to throttle,
A to brake, Down to handbrake, and Left/Right to negative/positive steering.
NativeInput.mm applies each explicit hold to the real physical-key sample,
then the original mappings/physics consume it. Independent keys can coexist;
a same-key active hold cannot be extended, shortened or cancelled by another
Hold. Acceptance is returned as a boolean. Durations are actual elapsed time,
finite and within (0,60] seconds.

For natural driving, first observe real Racing/RaceStarted and a valid snapshot.
Use a short throttle hold, observe actual pose/speed response, and calibrate
short Left/Right pulses at low speed against the authored road direction/next
point. Read actual route changes at junctions; do not extrapolate a clamped
route endpoint into a finish or invent another route. Keep throttle holds short
enough to expire before a braking decision; A is the real brake input. Query
new snapshots between decisions rather than drive from a stale table or a
long blind hold. Observe controls enabled and the raw AI/automation flags.
There is no call to AITakeControl, position setters, reset/teleport helpers,
SetLap, debug IncrementLapCount, or synthetic finish messages in this path.

A finish claim still requires ordered real splits/lap progress, original race
finish/state/results evidence, a completed actual track/car frame and clean
process exit. Compilation or telemetry availability alone is insufficient.

Verification at source freeze: the actual UiManager object compiles under
Debug, DebugUBSan, Speed and SpeedUBSan. Live-race helper validation is pending
root's coherent actual-game build/run; no game/service run was started by
MAC110.
