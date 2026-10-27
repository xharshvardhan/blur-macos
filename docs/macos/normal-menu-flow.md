# Normal menu and single-player flow (MAC186)

This is a source map for the original normal frontend. Actual183 separately
completed the Brighton/BMW debug slice with ordered lap/finish notifications and
normal exit0. It does not validate career/profile/results. Actual189 reached
Master Start/showroom, then an attract-movie rendering failure and a camera
snapshot cleanup crash; normal profile/event/result completion is still pending.
The verified `game-test` configuration must remain unchanged.

## Configuration and ordinary inputs

Use `Ui.NewLaunchMode=Master`, the original default in
`code/amax/library/Ui/UiManager.cpp:278`. `UISetup.lua:57–103` makes Master
non-development mode and selects `Intro/Legal.lua`; Debug/Automation take a
separate blagg-profile/direct-debug-screen route. Final and DebugRb also start
Legal but enable development mode. They are not equivalent normal-flow tests.

All Lua paths below are relative to
`code/amax/library/Ui/game/lua_scripts/`; screens are under `screens/`.
`Intro/Legal.lua` waits for actual boot precaching and UI scene readiness, then
selects `Intro/StartScreen.lua`. The operative main menu is that screen's
`STAGE_MODE_SELECT`, not the apparently obsolete `Intro/MainMenu.lua` (which
references an old RaceWorld screen). Do not force screen transitions externally.

The source PC keyboard menu table is
`code/amax/game/Game/GameControls/GameControls.cpp:196–224`: Return/NumPadEnter
and Space confirm, Escape cancels, arrow inputs navigate. The normal operator
should send finite ordinary keys only after observing the active screen and its
selection/readiness, rather than replaying timed Return bursts. StartScreen's
PressStart attract timeout is30s; Return before it starts real profile loading.
StartScreen's mode selector explicitly uses Left for single-player and Right for
multiplayer. After real profile completion, Left then Return selects SP without
relying on a previous-mode preference. No right-Control debug shortcut is needed.

## Required original path

| Boundary | Source authority and real action |
|---|---|
| Legal → StartScreen | Legal's boot/scene predicates must become ready. Master keeps original boot movies/cache stages. |
| PressStart → Profile/Boot | StartScreen.lua328ff/574–592: MenuNext calls Sp_NewGame, establishes actual primary pad0 for keyboard/mouse, then pushes profile Boot. |
| Profile/Boot → AsyncLoad | Boot checks actual active profile; active native Offline profile skips sign-in UI. AsyncLoad sets real profile/options state and reads actual save. |
| Fresh save confirmation | Profile/AsyncLoad.lua307–343: FileDoesNotExist shows PROFILE_PAD0_CREATED. Ordinary MenuNext218–234 goes through AsyncSave; it is not an auto-success shortcut. |
| Profile finished → mode selector | StartScreen.lua502ff waits ProfileBootFinished. Boot has a5.6s minimum display interval. First-time/rival introductory movies may legitimately intervene. |
| Mode selector → SP hub | StartScreen MenuNext407ff → GUIBank.lua3033 EnterSpRaceBook → SinglePlayer/SpMain.lua. |
| Hub Races → tier/event | SpMain selects the actual Races carousel app; SpTierSelect180–200 only accepts unlocked tiers, SpEventSelect459–472 only unlocked events. |
| Pre-race → Garage | SpPreRace146–155 sets the selected SP event and pushes Shared/Garage.lua. |
| Garage → loading | Garage_End1508–1548 requires the selected car to be owned and allowed, stores that real car, and calls SpGoLoadingScreen. |
| PrepLoading → race setup | SpGUIBank1201–1224 chooses Loading/LoadingSpGame.lua via GUIBank GoLoadingScreen and Loading/PrepLoading.lua. PrepLoading waits outstanding UI/load-save work, calls real Amax.SetupRace(), and retires frontend resources. |
| Original SP event settings | code/amax/game/GameLogic/GameModes/GameModeSinglePlayer.cpp49–53 reads actual profile event/SPevent into SetupRaceFromEvent. No one-car/Brighton/debug RaceMode override belongs here. |
| Loading → intro → HUD | LoadingSpGame and GUIBank FrameUpdateLoading wait real loading, then IntroVista/IntroRace; HUD670–674 sends RaceStarted from the legitimate intro route. |
| Natural finish → result | HUD762–770 calls Amax.SetupResults and SinglePlayer/Ingame/SpFinished.lua for actual SinglePlayer mode. |
| Result → award processing | SpFinished161ff → SpRaceAwards; its182–194 block calls original CompleteEvent/PostRaceSinglePlayer once through DoResults. These must never be called externally to substitute a finish. |
| Awards → hub/save | SpPostRaceOptions popup selection1 calls SpRaceAwards_BackToUiStart701–733: QuitRace, real StartAsyncSave, Loading/LoadingUi.lua, then stored frontend. |

The default normal event may have AI/population, car restrictions and a different
track/lap count. The selected event/save is the authority; debug slice settings
cannot be carried over merely to reduce the career workload. Actual ordered
checkpoints, lap/race completion, genuine awards/save and a restart/reload remain
required evidence, not conclusions from this source map.

## Offline and online boundaries

An active local Offline profile is distinct from a Blagged/no-save profile.
Profile Boot reconnect is conditional on actual network capability and finishes
regardless of callback success. Its PC DLC worker deliberately returns false
rather than blocking boot. Native CurrentPlatform=PC follows the real local
SaveLocationFound path instead of the Xbox storage picker. MAC187 owns the exact
native identity, persistence, save/restart audit and actual disk evidence.

EnterSpRaceBook attempts a stats-only network session, but
`lua_Amax.cpp:4831–4872` creates it only for real StateOnline/non-Guest profiles
with initialized network services. ContinueCreateStatsOnlyMatchingSession4874ff
reports inactive/error completion, and SpMain polls without gating event input
on success. SpPreRace has an offline best-time path. Optional social, leaderboards,
link-code, friends and reconnect dialogs are not proof that local SP requires a
server. Conversely, do not fabricate successful service responses, credentials,
account state or unlocks when those optional routes fail. Actual offline flow
must still be observed; source presence alone does not prove absence of a stall.

## Reachable UI gaps and current ownership diagnosis

The generic TheCustomPopup is reachable from StartScreen quit/errors and
SpPostRaceOptions. It contains an unregistered UITextBox named `textbox` in the
original private ui.bin schema (MAC045); it cannot be dismissed as exclusively
optional account UI. MAC188 audits whether each real reachable popup needs that
missing widget. No replacement class or fabricated schema is authorized.

Actual189 shutdown reaches C_Camera::~C_Camera from C_DebugTools teardown after
AmaxCam::C_CameraSystem::Exit. Source camera construction initializes two owned
pointers, m_EffectInfo and m_pShakeCamera; destruction deletes both.
ShowRoomCamera.cpp469 sets actual effect metadata. DebugTools::SetLocalCamera
uses compiler-generated assignment (DebugTools.cpp176); NetworkLink's inline
SetLocalCamera copies again, and DebugTools also assigns the remote snapshot.
The original camera has no explicit copy special members. These snapshots can
therefore share deleted owners; ordinary CopyFrom is deliberately pose-only and
is not a full-value assignment substitute. Native-only copy construction/assignment now preserve all scalar/base-feed,
projection, exposure and interpolation state, while cloning the two owners.
C_EffectInfo contains only five floats; C_ShakeCamera adds scalar phase/parameters,
and its inherited effect/shake owners recursively use the same deep-copy members.
Borrowed shake-world and debug-name pointers retain their original borrow policy.
Self-assignment is inert; both new owners are staged before the destination is
altered, so an allocation exception leaves the old camera value intact. No object
layout, PC serializer, CopyFrom behavior or Windows special member is changed.

Camera.cpp, DebugTools.cpp, NetworkLink.cpp and the authored
`tools/macos/camera_ownership_test.cpp` compile in all four current native
configurations (Debug, Release, Debug UBSan, Speed UBSan). The private Debug test
links the actual full-game archive closure with the original Main bootstrap.
It checks source retirement before debug/transport snapshot use, independent
nested effect/shake owners and copied phase, overwrites/empty/self-assignment,
feed/projection/interpolation values and repeated actual DebugTools/Artery
client-only teardown. Its exclusive Debug runtime and process/static teardown pass with exit0; actual
fresh Master shutdown proof remains root-owned and separate. No lifetime guarantee is added
for a deliberately borrowed feed/world/name after its external owner dies.
