# Normal career save and restart contract (MAC187)

Initial read-only audit followed by scoped native ownership/framing repair,
2026-10-27. Actual MAC183 completed the original Brighton/BMW
keyboard lap and exited normally. That Debug StreetRace is separate from a
normal career event and does not establish a career save or unlock result.
The later native repair changes owner retirement and invalid-input gates,
without changing valid save bytes or creating progress. No game state, saved
bytes, or runtime configuration was changed by this agent. MAC186 owns normal
menu/event navigation.

## Normal creation and load

| Boundary | Original source and observable contract |
| --- | --- |
| Press Start | `screens/Intro/StartScreen.lua:325–348,574–594`: ordinary MenuNext calls `Sp_NewGame`, flushes UI profiles, selects pad0 for keyboard, then `StartProfileLoad`. The latter sets primary pad, initializes the game profile, selects SinglePlayer, rereads the system profile and pushes `Profile/Boot.lua`. |
| New career defaults | `screens/SpGUIBank.lua:1–10`: `Sp_NewGame` calls real `SinglePlayer.NewSaveGame`, then original initial-unlock processing. It deliberately does not complete events. `SP_Lua.cpp:766` creates the real save maps from loaded rank/tier/index metadata. |
| Local identity | `network/source/SystemProfile/pc/SystemProfilemanager_PC.cpp:28–66,115–128,387–404`: real OS account, durable random nonzero identity, primary slot is active **Offline**, non-guest. `BLUR_MAC_PROFILE_STORE` names an absolute **file**, not a directory. Default is `~/Library/Application Support/BlurRebuilt/local-profile-id`. File is 8-byte little-endian, uid-owned regular file, mode0600, protected by flock; corrupt ownership/type/length/id fails initialization rather than silently replacing identity. Other local slots remain inactive until explicit guest acquisition. |
| Boot | `screens/Profile/Boot.lua:61–108`: active profile skips sign-in UI; `GuideActive == false` permits `StartAsyncLoad`. PC DLC returns false at149. Optional service reconnect at179 is gated by `net_CanReconnectToDemonware`, not required for offline boot. |
| Profile setup | `screens/Profile/AsyncLoad.lua:75–150`: active primary becomes `UIEnums.Profile.GamerProfile`; `Profile.Setup` refreshes actual SysProfile, then `GameProfile.ClearGameProfile` resets game defaults before load. A Blagged fallback is explicitly unable to save. Native primary normally never needs that fallback. |
| Storage selection | AsyncLoad262–281: CurrentPlatform PC sends ordinary `SaveLocationFound`, instead of Xbox device selection. On that message it starts the actual `Profile.StartAsyncLoad` worker. |
| Missing file | AsyncLoad289–343: `FileDoesNotExist` resets defaults, clears DoNotSave and displays `PROFILE_PAD0_CREATED` / `INFO_A_NEXT`. **Wait for observed MenuNext/Return**. AsyncLoad218–230 then routes to original AsyncSave. There is no automatic overwrite or injected profile. |
| Existing file | AsyncLoad298–305 calls `Finished` on Okay. `Finished` sends original SaveGameLoaded, applies options and processes/clears original unlock notifications. Boot only finishes after FileParams.FinishedSuccess becomes non-nil. |
| Error/corrupt file | AsyncLoad344–371 has genuine overwrite/delete/continue choices. Do not auto-confirm overwrite on a restart test: that would hide load failure and create another profile. |

Lua `Profile` reads the real `UI::C_ProfileInterface`, whose active/offline/online
checks are at `ProfileInterface.cpp:164–191`. Native `C_Manager::Poll` publishes
the acquired local account snapshot; it does not manufacture hosted sign-in.
`Profile.Setup` at `Lua_Profile.cpp:223` uses actual GetProfile for normal profiles.

## Save path, format and provider policy

`SaveInfoInterfaces.inl:38–62,124–135` selects native PC `SAVE/BLURSAVE`.
`SaveSystem_pc.cpp:36–58` places it under `BLUR2PC_SAVE_ROOT`, otherwise native
Application Support + `Bizarre Creations/Blur`. Root override is a **directory**.
Native filesystem normalizes the existing backslash separators. The save is
not keyed into per-UID subdirectories and has no platform-signature envelope;
the OS identity file and compressed game save are separate ownership layers.

`GUIBank.lua:448–473,2176–2204` allows saves only for GamerProfile,
DoNotSave false, no current LoadSave context, and the original split-screen
primary rule. Do not call `CanSave` as a purely observational predicate: it
also writes ProfileSaveIndex. AsyncSave starts its actual worker at365–397,
polls real completion at215 onward, and publishes FinishedSuccess after its
three-second minimum display interval at294–303. `FINISHED SAVING` alone is
insufficient; also require `local_error == UIEnums.ProfileError.Okay` and
FinishedSuccess true / completed LoadSave context.

The PC envelope from `SaveInfoInterfaces.inl:89–121,145–190` is three LE U32s:
magic `0xABCDE337`, uncompressed length, compressed length, then CCompression
payload. The decompressed stream begins writing-byte0 and version1378.
`lua_GameProfile.cpp:186–225` serializes actual primary GameProfile, SinglePlayer
maps, social state, friend challenges/history and lifetime statistics. The
loader at234–324 accepts1378 and older project versions1373–1375; it logs
`profile save load succeeded (version 1378, N bytes, 0 unread)` for a matching
normal save. A same-build round-trip is not proof of every old retail save's
raw-record ABI compatibility.

Offline saves do not require Demonware/Facebook/Twitter credentials.
`lua_NetServices.cpp:103–130` leaves OnlineConnectionPossible false on native;
`NetworkBank.lua:276` requires it for reconnect. AsyncSave only requests profile
upload when `Amax.IsGameModeOnline()` is true. Original stored social-auth bools
are saved data, not evidence of a real authenticated provider connection.

## Genuine save triggers and restart evidence

Normal autosaves are screen-driven, not a universal per-frame flush:

- Initial missing-save confirmation routes to AsyncSave as above.
- `screens/Shared/Options.lua:7–19` applies a genuine user option change and
  requests StartAsyncSave when leaving the options menu. Use a profile-backed
  harmless value such as music volume to distinguish reload from defaults;
  graphics resolution/AA/window mode are app settings, not this save.
- `screens/SinglePlayer/SpMovieScreen.lua:51–60` requests a save for the original
  played-video flag. This is not event completion.
- `screens/SinglePlayer/Ingame/SpRaceAwards.lua:178–192` invokes normal
  CompleteEvent **only** when DoResults is true, using real level statistics;
  returning to UI calls StartAsyncSave at731. Do not call CompleteEvent,
  SetupResults, AddToTotalFans, ProcessUnlocks or debug completion functions
  from an operator to manufacture results.
- Other existing requests include SpFriendDemand, garage, pause/results and
  PreviouslyOnBlur. Observe the actual selected route rather than treating
  one screen's End/GoMenu as a universal autosave guarantee.

For the parent-owned fresh normal-menu run, explicitly set
`BLUR2PC_COMPLETE_CAREER=0`: any other nonempty value whose first character is
not0 enables a preexisting completion cheat on both NewSaveGame and import
(`SP_Lua.cpp:703–772,1195–1198`). Keep
`Multiplayer.PrimaryProfile.ForceRebuild=false` and
`UI.Profiles.ForceErrorCode=None`; neither debug override is acceptance evidence.
Use fresh isolated absolute identity/save roots, then reuse **both unchanged**
for restart. Preserve copies/hashes after normal quit; do not place test saves
in original game assets or the user's default save directory.

Acceptance stages:

1. Fresh normal boot: active Offline/non-Blagged primary0; observed missing-file
   confirmation; genuine menu save finishes Okay. Record screen/context,
   ProfileState/DoNotSave, identity bytes/stat, BLURSAVE length/hash/header and
   ordinary exit0. Existence alone is not proof of complete career serialization.
2. Genuine menu change or genuine career event: capture profile option and/or
   actual event result from the UI getters before and after, wait for the real
   autosave to finish, then record changed file/hash and exit0. A mere rewritten
   timestamp, original debug-lap result, or memory-only unlock is insufficient.
3. Fresh process with exactly the same roots and metadata: ordinary Start loads
   existing file without the CREATED/corrupt prompt. Require FINISHED LOAD Okay,
   loader version/zero-unread log, unchanged local identity, equal saved option
   and saved career/result fields before any new race, and exit0. The file may
   legitimately change later if the normal UI autosaves another state.

Existing read-only Lua UI queries: `SinglePlayer.GameInfo()` exposes rank,
fans, totalStars and lastVehicle (`SP_Lua.cpp:14–34`); TierInfo provides tier IDs
and SP event IDs; `SinglePlayer.EventInfo(spEventId)` exposes state, difficulty,
lastVehicle, totalStars, fanParBest, pointsBest and timeBest (118–178).
EventInfo.eventId is the underlying RaceBook event ID, **not** the SP ID passed
into EventInfo; preserve both in logs. `Amax.Options()` exposes profile difficulty,
audio volumes, gamma, controls etc (`GameProfile.cpp:461–507`). UI getters can
lazily acquire default map entries; query actual original tier/event IDs.
`GameProfile.DoesSaveGameExist/IsSaveGameValid` inspect `debug_save.amax`
(`lua_GameProfile.cpp:50,588–620`) and must not validate normal BLURSAVE.

## Proven tests and remaining source risks

`network_profile_test.cpp` validates actual local account identity, lifecycle,
permissions, guest slots and same-process manager reinitialization, not a normal
menu career restart. `ui_save_test.cpp` validates native async storage using an
authored64-byte record. `amax_save_envelope_test.cpp` uses the original envelope
and native workers but **test-local authored** SaveGameImpl/LoadGameImpl; it does
not exercise production GameProfile/SP/social serialization. MAC183 proves the
actual debug lap and clean exit, without entering the normal save interface.
Parent-owned MAC190 ordinary Master boot subsequently created a real save at
`_work/runtime/verification/mac189-state/saves/save/blursave`: 2027 bytes,
magicABCDE337, raw50903/compressed2015. It logged FINISHED SAVING,
local_error0/upload_result1. Read-only inspection confirms the full-flush footer
0000ffff and writing0/version1378. This is genuine initial career serialization,
not yet restart/load proof; the following existing-file attract-movie Open
failure belongs to MAC188. The actual file was preserved unchanged.

## Scoped native repair and verification

MAC187 now owns and fixes the two identified native boundaries:

- `SaveInfoInterfaces.inl`: native profile stream starts null, destructor owns
  abandoned preparation, and Setup retires the prior stream and publishes only
  after actual serialization/compression/metadata preparation succeeds. Local
  RAII protects both streams and the exact new[] compressed buffer on false or
  throw. Compression output must itself validate before readiness. Empty failed
  preparation cannot become a successful zero-file save. Save checks the exact
  prepared output extent before memcpy. Original nonnative paths are unchanged.
- Its native envelope reader checks actual minimum framing, compressed extent
  equality, signed INT_MAX MemoryStream authority, full successful zlib input/
  output counts and completed/version prefix before calling the mutable game
  loader. Existing full-flush streams remain valid: the original encoder uses
  Z_FULL_FLUSH, not Z_FINISH. A bounded32KiB scratch pass validates before any
  advertised-size allocation; pending output is drained after input exhaustion.
  The existing empty-stored-block full-flush footer is required for non-EOS
  streams; genuine EOS streams remain accepted. A second decode fills the exact
  allocated buffer. No new capacity, magic, version or payload format is added.
- `SaveSystem_pc.cpp` rejects file lengths beyond actual signed-int MemoryStream
  representation before narrowing/allocation; original actual Read bool is
  still required before invoking the interface.
- `lua_GameProfile.cpp` owns the two exact lazy concrete interface allocations.
  `UiManager` closes them before screen/Lua/profile dependencies are retired.
  `SaveSystem::NativeQuiesceAndDetach` closes new requests, joins/closes all three
  actual workers/callbacks, then clears only the matching borrowed target.
  Unrelated borrowed targets/objects remain intact. Original worker exceptions
  are collected after all joins/detachment and propagated, not reported as success.
  Profile/photo owners are deleted even when a worker exception must propagate.
- Screen End callbacks cannot reacquire closed lazy interfaces or start new
  workers. A new real `luaopen_game_profile` registration reopens the next UI
  generation. One native bool is added to SaveSystem, requiring a coherent ABI
  rebuild. No global static destructor is used to guess UI dependency lifetime.
  JPEG encoder/decoder own their existing buffers; concrete photo destruction
  invokes those existing destructors rather than adding duplicate buffer frees.

Final private source probes: four production TUs × four configurations =16/16,
and three fixture sources × four configurations =12/12. Configurations are
Debug, DebugUBSan, Speed and SpeedUBSan. Initial small original-compressor/native
file round-trip passed true process exit0 with version1378/1029bytes/0unread.
Parent-owned coherent MAC192 Debug and Speed game builds succeeded. All12
affected checks passed in **each** configuration (`mac192-tests.log` in the two
standard build directories), including all three final fixtures below. Actual
MAC194 is testing restart of the preserved190 career save; its outcome remains
parent-owned and is not inferred from component tests:

| Existing/new fixture | Actual exercised boundary and limitation |
| --- | --- |
| `ui_save_test.cpp` | Actual blocked file worker and completion callback join before close; exact unrelated borrowed target survives; closed requests fail; real reopen loads payload. Existing real IO/enumeration/delete/overwrite tests retained. Authored record, not career. |
| `amax_save_envelope_test.cpp` | Original65536-byte authored serializer payload crosses32KiB scratch; malformed short/header/extent/decode/footer/length/prefix cases never invoke deserializer; abandoned stream reset/destructor, failed Setup, truthful start failure and successful retry. No production career serializer. |
| `profile_save_owner_test.cpp` | No args; full actual game/bootstrap-first closure. Actual GameProfile Lua loading preparations acquire production profile/photo lazy pointers; exact retirement/late callback rejection/re-registration for3 generations and unrelated borrowed isolation. No career bytes, assets, file writes or progress mutation. |

Remaining limits: valid framing is not a transactional parser for every nested
GameProfile/SP/social serializer. Those legacy readers still include void or
unchecked field reads; this repair does not claim arbitrary malformed raw
career input can never partly mutate state. Windows retail compatibility for
every raw-record layout also remains separate from same-build round-trip.
A genuine changed-option/event save followed by fresh-process real load and
matching state is still required before declaring save/reload complete.


## Mandatory normal race result/award ownership follow-up

Read-only after MAC192; no further production changes or runtime actions.

| Boundary | Source contract and observation |
| --- | --- |
| Selected event/population | `GameModeSinglePlayer.cpp:49–65` uses actual profile event/SP event and all event cars. `SetupResults` requires every loaded LogicVehicleActor to remain alive. Keep original event AI/car restrictions/laps; earlier one-car Debug proof cannot substitute. |
| Finish snapshot | `HUD.lua:762–770` invokes SetupResults while the world/RaceLogic still exist, then selects SpFinished. `RaceModeStreetRace.cpp:556–620` reads actual actors' time/position/scoring and broadcasts ResultsCalculated. `RaceBookLayer.cpp:848–862` invokes original InterpretRaceResults; no extra operator invocation is permitted. |
| Persistent result owner | `C_Game` owns C_LevelResult; `LevelResult.h:19–51` stores scalar statistics/owned containers, not borrowed vehicle pointers. Amax.SP_GetLevelResult (`lua_Amax.cpp:5799`) copies values into Lua but returns no table for an invalid SP event. Missing result/event is a readiness failure to diagnose, not an invitation to create a result. |
| Award application once | `SpFinished.lua:118` sets DoResults true after valid result setup; timeout is elapsed-time5.75s. `SpRaceAwards.lua:178–192` clears DoResults before real CompleteEvent and PostRaceSinglePlayer. Returning via garage/share must retain false. GUI.event_info is deliberately obtained before CompleteEvent for animation; capture a fresh getter afterward for saved-state assertions. |
| Stats versus service | `SP_Lua.cpp:905–996` adds real earned fans and records medal/records only from the existing C_LevelResult; no provider is required for this. PostRaceSinglePlayer may request stats/social writes, but those are separate from local save completion. Offline award leaderboard gates at SpRaceAwards419ff only wait when real reconnect/online conditions apply. |
| Return selection | The original SpPostRaceOptions popup (`CustomPopupData.lua:4944–5003`) uses value1=return to event select, value0=retry. Failed result defaults to retry. Observe the selected ordinary option; a blind Return can start a second race rather than save-and-return. |
| Save before world retirement | SpRaceAwards701–732 sends QuitRace, requests StartAsyncSave and enters LoadingUi. AsyncSave calls SetSaveInfoInterfaceProfile(false) synchronously, which serializes GameProfile/SP/social into its owned byte stream before starting the file worker. The normal profile worker copies prepared bytes, not live vehicle/world objects. This avoids a claimed asynchronous raw actor dependency; photos have a separate callback contract. |
| Real frontend completion | `LoadingUi.lua:122–171` requires actual UI scene/audio readiness, finished unlock sub-screen and no active LoadSave context before returning. `GUIBank.lua:1692–1718` starts real resource reload/world dump. Do not force GoScreen or treat return screen request as save completion. |

A subtle ordering is intentional source behavior: the first post-race snapshot
is taken before `Shared/Unlocks.lua:58` processes derived rank/tier unlocks.
`AsyncLoad.lua:435` also calls original ProcessUnlocks after loading. Compare
fans/event medal/record time/last vehicle and derived rank at matched completed
post-load stages; do not demand every transient new/animation flag or raw file
hash stay identical after legitimate menu processing. Do not call ProcessUnlocks
externally merely to improve a test result.

No additional native-only PC/console branch or newly proven ownership failure
was found in these mandatory result functions. The all-car-ready invariant,
valid selected SP event, one-shot award gate and original frontend/save waits
are concrete prerequisites for the next actual normal race. They remain runtime
acceptance boundaries, not conclusions from compilation or the prior Debug lap.
