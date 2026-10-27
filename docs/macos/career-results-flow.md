# MAC223 — Genuine normal-career results, return and save

This is a bounded source audit and private ordinary-key operator. It does not
claim a completed normal career race or a post-race save/restart. MAC183 proved
the separate one-car debug slice; MAC219 proved populated normal twenty-car
shutdown. The current acceptance case is the actual unlocked normal event whose
RaceBook event ID is4597671, with original opponents, original three laps,
original keyboard input and existing profile. No event, AI, lap, result, score,
unlock or progress value is changed by this work.

## Completion qualification

| Observation | Source and meaning |
| --- | --- |
| Original human | Native `Blur2PC.VehicleState()` in `UiManager.cpp2540+` reports actual configuredAI, automatedHuman, aiToggle and controlsEnabled. Require the real human flags during Racing. A read can return nil while workers or owners are unavailable; nil is not progress. |
| Physical ordered lap completion | `GameAutomationMessageHandler.cpp111–120` publishes `LapInc:0:(rawLap-1)`. `AI_Amax.cpp3173,3647–3659` copies/reports the original AIR lap. `RaceRulesManager.cpp388–404` passes a lap-limited race when raw currentLap is strictly greater than level.mLaps. For three configured laps require ordered completed-lap1,2,3 notifications, with the original ordered AIR route/split evidence. Raw VehicleState.lap4 supports the final crossing; raw lap3 means third lap in progress. Initial notification0 is possible. |
| Successful original UI terminal path | `RaceModeStreetRace.cpp330–345` sends UI RaceFinished after Passed_All_Rule. Its354–385 branches also send the global RaceFinished on fatal damage/failed rule, while sending UI Wrecked/Failed. Therefore the global notification alone is insufficient. `HUD.lua762–769` stores the actual terminal UI message in `UIGlobals.Ingame.RaceFinishedMsg`. Read it and the separate `UIEnums.GameFlowMessage.RaceFinished` enum while the original SP result flow is alive, and compare them along with the lap/human proof. |
| Original finish camera control | `RaceModeStreetRace.cpp426–440` deliberately forces the finished local car into AI control. Post-finish aiToggle does not retroactively invalidate genuine human Racing evidence. Never enable it from the operator. |
| Actual medal and position | `lua_Amax.cpp5799–5840` exposes the real `Amax.SP_GetLevelResult()` copy: state, secondary, gfd and position. StreetRace SetupResults at556–625 reads actual actor statistics while the world is alive; position1/2/3 maps to gold/silver/bronze. A genuine completed race outside the top3 has state `none`; it is not a medal success. |

The historical ignored probe harness's non-observe-only finish handling accepts
completed-lap1 for the one-lap debug case and calls a direct quit. It is not an
appropriate normal-career acceptance/operator mode. The root's current normal
career run uses observe-only orchestration and separate finite ordinary keys.
No notification or result is synthesized to satisfy the conditions above.

## Original results and owner boundaries

1. On the actual UI terminal message, `screens/Ingame/HUD.lua762–770` calls
   `Amax.SetupResults()` and opens `SinglePlayer/Ingame/SpFinished.lua`.
   The actor/world ownership is still valid at this point. Do not call that
   function later from Automation after the world has gone away.
2. `SpFinished.lua` displays the actual result and normally advances after5.75s
   through `SpFinished_End`. No injected key is required. Ordinary MenuNext can
   advance it, but the private operator deliberately waits. The normal route
   opens `SpRaceAwards`; friend-demand routes are outside this normal event.
3. `SpRaceAwards.lua71–73` resolves its raw SP key from the actual selected
   TierInfo/events and obtains the result. This raw SP key is distinct from
   underlying gameplay/RaceBook ID4597671. Its PostInit173–190 caches the old
   event-info presentation, then clears `UIGlobals.DoResults` before calling
   original `SinglePlayer.CompleteEvent` and `Amax.PostRaceSinglePlayer`.
   Do not repeat those calls from a diagnostic/operator.
4. `SP_Lua.cpp914–995` always adds the actual earned fans for an unlocked event
   with a live primary profile. Medal/fan-par/GFD/record awards require at least
   bronze. Thus a completed20th-place race may save actual earned fans without
   a medal, timeBest, star or unlock. Such a result cannot be relabeled as a
   successful medal/unlock completion.
5. `SpRaceAwards.lua685–698` takes the actual photo and pauses through its
   authored timeline; `TryGoLeaderboard782–809` publishes next/share readiness.
   Wait for active unfinished awards, no subscreen, no loading flags,
   `has_taken_photo=true`, `DoResults=false`. For a medal also wait for
   `allow_next_and_share=true`; for `none`, PostInit239–251 directly displays
   INFO_A_NEXT and MenuNext348 goes straight to the options, so it does not
   require the medal animation readiness flag. Then an ordinary Enter reaches the original options popup. Optional online
   leaderboards/provider failures do not authorize fabricated service success;
   the offline original branches remain in control.
6. `CustomPopupData.lua4944–5003` builds SpPostRaceOptions159. The FIRST real
   option has value1, return to event select; optional challenge has value2;
   normal retry has value0. A `none` medal DEFAULTS TO RETRY0. Read the actual
   options and selection. Use finite ordinary Up to reach the observed first
   item/value1, then finite Enter. Do not assume default Enter returns home.
   `TheCustomPopup.lua99–119,271–280,297–334,415–430` proves these are item
   values, not list indices, and forwards selected value as PopupNext arg3.
   Like the desktop popup, it is Main0 state, not a separate popup Lua screen.
7. `SpRaceAwards.lua309–316,701–732` consumes original arg3=1, sets loading_ui,
   unpauses, requests QuitRace, calls original StartAsyncSave and opens LoadingUi.
   Those are game callbacks; the operator never invokes them directly.
8. `LoadingUi.lua122–171` waits for actual scene/audio load, normal unlocks,
   inactive LoadSave and its original return screen. `Shared/Unlocks.lua94–108`
   permits ordinary MenuNext only after `loading_done=true`. The operator may
   use a finite Enter then; it never calls ProcessUnlocks. Mode3's car-unlock
   page transitions to `Multiplayer/Ingame/MpUnlocks.lua` by original source;
   that additional screen is left untouched by this bounded helper.
9. `Profile/AsyncSave.lua208–228,294–303` gets real worker completion, requires
   local_error Okay, waits its normal minimum display, and publishes
   `UIGlobals.FileParams.FinishedSuccess`. There is no injected confirmation on
   success. Unknown save/storage/provider popups are left untouched. Actual
   worker/stream owner joins and malformed-input protection are the separate
   MAC187 repairs and fixtures, not career completion proof.

## Bounded private operator

Only ignored `_work/runtime/research/career_results_operator.py` is added. It
imports the existing passive Observer without changing it. Invocation after the
root's separately qualified original finish, against an already owned run:

```
python3 _work/runtime/research/career_results_operator.py mac226 --seconds 240
```

It uses fresh uniquely tagged Main/context-specific read-only getters and finite
native Enter/Up taps, with at most12 keys and at most300s. It waits for the root
harness's real StartScreen log before any IPC; it never launches a process or
activates an OS window. The entire original127-byte UTF8 command envelope is
retained. Options159 requires actual enum/data/sender identity, unfinished
nonerror/noncancel state, recognized actual ordered options `[1,0]` or
`[1,2,0]`, and observed selection. Unknown popups and unknown options do not
permit a key. The helper does not demand a next_CB on this popup: original
PopupNext forwarding is the correct route.

Multi-query snapshots are not atomic. A final context/popup/selection fence,
six-second total observation limit and one-second input expiry bound the gap;
execution-time commands recheck context plus screen or popup identity and
unfinished state. This assumes no concurrent human/other keyboard operator.
The popup action does not repeat if its acknowledged state is unchanged, and an
accepted return tap disables any further awards/popup action while the original
return callback runs. A failed/unknown tap acknowledgement stops without retry.
The longest commands and all passive expressions are tested at the bounded
maximum tag/context sizes. No production fault or success state is injected.

Exit0 from this private helper means only: its observed ordinary return tap was
accepted, a NEW post-tap FINISHED SAVING/local_error0 appeared, FileParams reports
FinishedSuccess, LoadSave and unlocks are inactive, and the original SP menu
returned. It emits available copied GameInfo/EventInfo fields. It explicitly
reports physicalRaceCompletionQualified=false and onDiskReloadQualified=false:
those are separate root acceptance checks. It sends no desktop quit; the
MAC211 ordinary desktop operator can be used later after the game is idle.

Pure decision tests cover awards animation/photo guards, `none` medal retry
selection, two/three real options, malformed/boolean IDs, unknown popups,
stale/expired/refused keys, loaded unlocks and127-byte builders. `--self-test`
and py_compile pass. No actual game, Automation client, shared build, archive
link, GPU operation or production-source mutation occurred in MAC223.

## Disk and restart acceptance still required

Retain `BLUR2PC_COMPLETE_CAREER=0`; keep the same isolated profile identity and
native `BLUR_MAC_PROFILE_STORE`/`BLUR2PC_SAVE_ROOT`. Save bytes are the real
`save/blursave` original PC envelope: LE magicABCDE337, raw/compressed sizes,
version1378, original FULL_FLUSH zlib. Do not replace an old good save or accept
only a file timestamp/hash or a worker's initially-zero local_error.

Before/after the real result flow record actual `SinglePlayer.GameInfo()` rank,
fans,totalStars,lastVehicle, and `EventInfo(rawSPKey)` resolved eventId, state,
difficulty,lastVehicle,totalStars,timeBest,pointsBest,fanParBest. The helper
samples a subset; root can read the other existing copied fields. Match raw
career key to underlying4597671 through EventInfo rather than passing4597671
as the raw key. Require meaningful actual award deltas according to earned
result; do not require a timeBest/medal increase after a below-bronze finish.

Finally close through the original desktop confirmation, require actual exit0,
restart with the same identity/save root through normal Master boot, observe
successful version/extent/decompression/deserialize with0 unread bytes and
recovered copied SP fields. Compare at the same completed post-load/unlocks
stage, since Load also legitimately processes derived unlocks. No Lua field
replacement, debug career-completion opt-in, fake event/AI/control change or
manual serializer invocation is allowed. This extends the owner/envelope
fixtures into actual career persistence; those fixtures alone cannot prove it.
