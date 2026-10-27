# Normal career race configuration authority (MAC200)

## Result

The copied `game-menu-189/debug_frontend.xml` still describes the one-car,
one-lap Brighton debug slice, but the normal Master → SinglePlayer event path
does not consume those choices. The audited independent menu debug_values.xml
contains431 named variables and has the relevant global overrides disabled or
neutral. No native configuration correction or production change is justified
by this audit. The next original career event should retain its authored AI
population, route, laps, difficulty rules and normally selected owned/allowed car.

Actual194 reached a fresh-profile unlocked race event4597671, class3, very easy,
with1 fan/0 stars. That is existing runtime evidence supplied by root, not a
promise of event population or car identity from this source-only review. The
existing save and all legitimate ordinary-menu choices remain authoritative.

## Dispatch and stale-state barriers

All source paths below are relative to `code/amax/`.

- library/Ui/game/lua_scripts/UISetup.lua57–80 derives Lua DevMode=false from
  LaunchMode Master; ordinary StartScreen does not expose debug shortcut buttons.
- StartScreen_DefaultProfileVars (screens/Intro/StartScreen.lua605–613) clears
  UIGlobals.DebugEvent and LoadFromDebug. UIGlobals initializes DebugEvent=nil;
  only the two debug screens populate it. PrepLoading.lua65–78 selects normal
  Amax.SetupRace unless actually multiplayer/split-screen or DebugEvent exists.
- game/GameLogic/Lua/lua_Amax.cpp1202–1208 dispatches SetupRace to RaceBookLayer
  and marks DebugEvent=false. RaceBookLayer1015–1018 dispatches the current game
  mode; GameModeSinglePlayer.cpp49–53 uses the primary profile's event/SPevent IDs.
- GameModeDebug::Start57–65 alone calls LoadDebugValues413ff for
  debug_frontend.xml. Merely constructing/initializing the registered game modes
  does not call that Start. Its NumCars/Laps/City/Route/RandomCars/car-controller
  settings belong to that separate debug setup method71ff.
- Structures/LevelState.cpp459–464 begins actual SetupRaceFromEvent with Clear()
  and level-result Clear(). Clear108–270 resets city/route, cars/additional cars,
  random cars, environment override, viewports, PlayerIsAI, start-disabled,
  automated-human, controller/type and previous race data. Thus a prior showroom
  or debug level state is not an event population override.

## Grouped authority and concrete override checks

| Class | Original source contract | menu189 value / conclusion |
|---|---|---|
| Game-mode override | RaceBookLayer992–1012 can force SplitScreen before mode Start. | SplitScreenMode.ForceSplitScreenMode=false. Normal selected SinglePlayer remains SinglePlayer. |
| Route/city/race mode | LevelState489–502 reads ev->route/city and authored raceMode, default StreetRace. | debug_frontend Brighton and optional Debug RaceMode are not read. No manual route override belongs in career. |
| Laps | LevelState556–559 uses ev->laps unless Cheats.NumEventLaps>0. | Cheats.NumEventLaps=0, preserving real authored laps. It must stay0; debug frontend Laps1 has no influence. |
| Population | LevelState647–699 lets actual mode SetupOpponents own special modes; ordinary StreetRace inherits Base SetupOpponents=false and uses ev->opponents. Rules initialize from the event for that resulting population. | No debug NumCars1/additional-car setting is read. Do not force a one-car count for a real event. |
| AI omission | LevelState729 can omit normal characters when Game.RemoveMinorCharactersFromRaces is true. | false, preserving original opponents. Event/race-mode-specific disabled/spawned opponents remain legitimate data behavior. |
| AI throttle | AI/AI_Amax.cpp2653–2656 suppresses NPC throttle only if AmaxAI.DebugCar.DisableThrottle. | false. This does not disable original AI control. |
| Player control | Clear resets PlayerIsAI/AutomatedHuman; SetupOpponent810–859 sets player to real primary pad, human/player control, AI characters to invalid-pad/AI control. | No XML PlayerIsAI setting exists in this file; normal keyboard remains human. Automation EventId/NumVehicles/Route/Laps mutation commands are outside authorized operator actions. |
| AI difficulty/abilities | LevelState466–478,548–620 derives controller, balancing CSV, time limits/retry/perk/surface settings from actual profile/event difficulty. | Existing profile very-easy state is legitimate. Do not replace AI data with debug-slice settings. Perks.AI.EnableAIPerkUse/EnableAIShieldUse=true. |
| Car class/ownership | Normal SpEventSelect/SpPreRace chooses actual event; Garage_End1508–1548 requires owned and allowed car. LevelState sets AI cars from event overrides/character garages and player from actual selected profile car. | Cheats.EventIgnoreCarClass=false. Debug frontend BMW vehicle IDs and RandomCars0 do not choose a normal career car. |
| Shop overrides | LevelState416ff applies persisted Shop.* values only if non-persisted Shop.Overide=true (source defaultfalse). | Shop.Overide absent, no override enable. Numerous saved Shop.* defaults do not individually grant all those upgrades. |
| Debug perk slots | Cheats.Debug_PerkSlotOveride is declared in GameModeDebug.cpp; scoped search finds no read there. | Value3 is not a proven career perk-slot override. Original profile/event abilities remain authority. |
| Player assist tuning | AI_Amax93–105 defines steering/brake-assist enable flags defaultfalse; saved numeric Player.* torque/brake/boost values are not enable bits. | Enable flags absent, matching false source defaults. Saved numeric values match their source defaults; no manual AI driving is enabled by those numbers. |
| Launch vs C++ DevMode | Lua DevMode follows Master; C++ Game.DevMode GetSetDebugDevMode only toggles debug HUD/mini-monitor at lua_Amax1752–1773. | Game.DevMode=true does not turn Master into Debug or overwrite event state. No need to toggle gameplay-facing state to suppress a HUD. |
| Manual lap debug declarations | AI_Amax104–105 declares ManuallySetLapNo/DebugCar.Laps.Lap; bounded repository search finds no consumers beyond declarations. | Both absent. Do not mistake this unused declaration for a source-backed event lap mutation. |

Other one-car assignments found by scope search have explicit contexts:
Main.cpp312 is the developer teleport callback restricted to debugscreen;
RaceLogicCore.cpp1444 is HandleVehicleLoadedUI, the frontend showroom vehicle,
not race population after SetupRaceFromEvent Clear. They are not native career
configuration patches. No such callback should be invoked in the normal run.

## Input layout and timing

GameProfile.cpp48 initializes PC/native keyboard layout0 for a fresh profile;
real save loading can legitimately restore layout1. ProfileOptions and normal
Amax.Options(nil) reapply controls via native original-PC SetupPlayerControls
(UiXtConfig.cpp349–366, lua_Amax3159–3164), keeping keyboard plus profile pad
bindings. Layout is an actual profile option, not debug_frontend Car_Controller.
GameControls.cpp112–121/169ff gives layout0 Q throttle/A brake/Left–Right steering;
layout1 W/S/A–D. Read the actual current layout through original options UI/data
and use corresponding finite ordinary keys; do not overwrite the save to fit an
operator's assumed Q mapping. The constructor default alone is not proof of a
loaded profile's layout. No physics-cadence/debug-time override was identified
among the scoped relevant menu values; this audit changes no simulation timing.

## Before driving / evidence limits

After original PrepLoading/Amax.SetupRace and actual RaceLoaded, the existing
read-only Amax.GetLevelData (lua_Amax767–819) reports City,Route,RaceMode,
NumberOfCars,Laps,NetworkRace,Viewports and per-car Control/Character/Vehicle.
Read these without calling setup/mutation APIs. Compare with selected real
SP event/profile and expected ordinary AI population; do not assume a hardcoded
16 or20 cars without reading that event. Existing VehicleState observes the
actual human actor, motion, route/checkpoints. The operator may send finite
ordinary keys, never EventId/NumVehicles/Route/Laps/teleport/progress setters.

This audit did not load a new event, decode proprietary event contents, run the
game, change a config/profile/unlock, execute debug functions or build code.
A complete normal career race may exercise additional opponent assets, rule
sets, pickup combat, results, awards and save/restart paths that the183 debug
slice did not. Actual proof must retain normal event rules and genuine outcome;
configuration-source authority is not runtime success.
