# MAC223 — Genuine normal-career results, return and save

This began as a bounded source audit and private ordinary-key operator. Its
original MAC223 handoff did not claim actual career completion/save/restart;
the later MAC243 interpretation correction below distinguishes physical lap
completion from a failed-medal result and leaves actual restart/exit acceptance
to the owning runtime task. MAC183 proved
the separate one-car debug slice; MAC219 proved populated normal twenty-car
shutdown. The current acceptance case is the actual unlocked normal event whose
RaceBook event ID is4597671, with original opponents, original three laps,
original keyboard input and existing profile. No event, AI, lap, result, score,
unlock or progress value is changed by this work.

## Completion qualification

| Observation | Source and meaning |
| --- | --- |
| Original human | Native `Blur2PC.VehicleState()` in `UiManager.cpp2540+` reports actual configuredAI, automatedHuman, aiToggle and controlsEnabled. Require the real human flags during Racing. A read can return nil while workers or owners are unavailable; nil is not progress. |
| Physical ordered lap completion | `GameAutomationMessageHandler.cpp111–120` publishes `LapInc:0:(rawLap-1)`. `AI_Amax.cpp3173,3647–3659` copies/reports the original AIR lap. `RaceRulesManager.cpp388–406` resolves lap-limited pass/fail rules when raw currentLap is strictly greater than level.mLaps; crossing that boundary does not itself guarantee passing the career objective. For three configured laps require ordered completed-lap1,2,3 notifications, with the original ordered AIR route/split evidence. Raw VehicleState.lap4 supports the final crossing; raw lap3 means third lap in progress. Initial notification0 is possible. |
| Original UI terminal classification | `RaceModeStreetRace.cpp330–345` sends UI RaceFinished on passed rules;375–385 sends UI Failed on failed rules, including a completed lap-limited race whose pass objective was not met. `LuaVariableHelper.cpp32–33` exposes these as UIEnums.GameFlowMessage.RaceFinished and RaceFailed. Fatal damage also emits the global RaceFinished while UI receives Wrecked. Read actual terminal UI message from `UIGlobals.Ingame.RaceFinishedMsg` and compare to both original enums, together with ordered lap/human/nonwreck proof. UI RaceFailed is compatible with physical three-lap completion outside the objective/medal positions; requiring UI RaceFinished for every physical finish would be too strong. Neither UI outcome nor the global notification alone proves completed distance. |
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


## MAC243 correction: physical finish is distinct from career objective success

Root243 observed actual normal20-car three-lap human driving, ordered completed
laps1/2/3, rawlap4 and RaceState Finished. The actual stored UI terminal value
was25870352, while the separately read RaceFinished enum was25870351. The
original enum mapping (`LuaVariableHelper.cpp32–33`, plus adjacent declarations
in GameflowMessages.h62–65) identifies25870352 as **RaceFailed**, not Wrecked.
Original award UI then reported position20 and medal state `none`.

The earlier MAC223 recommendation to require UI RaceFinished as a positive
condition for every physical lap completion was too strict. The exact original
rule logic is:

- RaceRulesManager.cpp247–251 can send failure for a wreck before lap completion.
- The same manager388–406 also resolves pass/fail **after** rawlap exceeds the
  configured lap count. If pass rules have not passed (or fail rules apply),
  it calls the same SendFailMessage. Both SendPass/SendFail set StillInRace=false.
- A position pass predicate at RaceRule_FinishInXPosition.cpp39 compares actual
  race position to its loaded mPositionNeeded. The rule set is selected from
  the original event/difficulty data; this audit does not invent its threshold
  or claim a live individual-rule object capture after the world retired.
- StreetRace.cpp375–385 turns a non-AI human failed rule into UI Failed plus the
  global RaceFinished. HUD.lua762 explicitly consumes **either** UI RaceFinished
  or RaceFailed, computes original results and opens the same SpFinished flow.
- StreetRace.SetupResults600–613 independently assigns position1/2/3 the medals;
  position20 becomes kSP_Incomplete (`none`). SpFinished75–80 deliberately shows
  its original failed-result text for `none`. That is a failed career medal,
  not proof that the human failed to complete the physical lap distance.

Thus the defensible classifications are:

| Actual evidence | Allowed conclusion |
| --- | --- |
| Human/original ordered three completed laps, raw4, nonwreck evidence, original terminal/result flow and position20/none | Physical normal race completion with failed objective/medal; no win, medal or unlock claim. |
| UI RaceFailed/global RaceFinished before the required ordered lap proof | Failure/termination only; physical three-lap completion is unqualified. |
| Wreck/global RaceFinished without the ordered required lap proof | Wreck/termination only, not a completed physical race. |
| UI RaceFinished without the required ordered lap proof | Passed original rules, but no independent full-distance qualification; some rules may permit earlier results. |

`LevelState.h49–57` also matters: vehicleState1 means RACING,2 FINISHED and6
FATALLY_DAMAGED. Root243's observed1 is a nonfatal snapshot, **not** the finish
proof; RaceLayer Finished and logic StillInRace differ from the level car state.
The ordered actual crossings/laps and result flow supply the physical evidence.

The original postrace save is valid for this failed-medal case.
`SP_Lua.cpp929–939` always adds actual fans earned; the >=bronze branch gates
medal/fan-par/GFD/record progress. Root's existing223 operator permitted the
original `none` result (including popup159's retry0 default), chose return1
through ordinary keys and observed NEW FINISHED SAVING/local_error0,
FinishedSuccess and normal SP return. It emitted actual GameInfo rank1/fans60/
0stars and event state none/position20, with records timeBest0/pointsBest0/
fanParBest0. These are observations, not proof that all60 fans were earned in
this particular race; compare the owning runtime's actual pre-race backup for
an attributable delta. The operator explicitly kept physicalRaceCompletion-
Qualified=false and onDiskReloadQualified=false because it does not itself
adjudicate those separate root-owned proofs.

No operator, game or progress code is changed by this interpretation correction.
The failed shell assertion on UI RaceFinished must be preserved as an overly
strong acceptance check, not rewritten into a game pass or discarded from the
runtime record. The next command's unintended shell continuation must also be
recorded; the ordinary result flow itself still used source-backed keys and
actual callbacks. Root owns actual process exit and same-identity disk reload
verification. Winning/medal/unlock validation remains a separate unfinished goal.
