# MAC239 — Actual postrace UI and photo scene boundary

Actual232 used the coherent Speed231 game with original normal20-car career
rules. Root observed three ordered completed laps, rawlap4 and Finished through
ordinary human keyboard input. Its last actual UI observation is
`singleplayer/ingame/spfinished.lua`, Finished,20 vehicles, at game.log184690.
The run then failed with `Draw operation requires BeginScene` at184751 and
exited1/1762.3s. No results-operator keys ran. Completion is a real race milestone;
this error is not successful results/save-return or a clean normal process exit.

This audit is source-only. It has not captured the actual first-throw caller,
run the game, sent Automation input, built/link-read archives or changed
production. Root237 owns the bounded before-throw reproduction; graphics240
owns the accumulator correction and GPU regression. The observed SpFinished
screen is not proof that the later awards callback was actually entered.

## Original UI path and callback contract

| Boundary | Source contract |
| --- | --- |
| Terminal race UI | HUD.lua762–770 calls actual SetupResults while vehicle owners are alive and schedules SpFinished for SinglePlayer. The game, not Automation, computes results. |
| Finished presentation | SpFinished.Init initializes the existing winner camera and result UI. FrameUpdate164–177 subtracts elapsed time from its5.75s timeout and SpFinished_End179–185 schedules SpRaceAwards. No operator key is needed. |
| Awards photo request | SpRaceAwards.lua685–698 calls `Amax.TakePhoto(1,false,false)` when FriendDemandSent is false, then pauses and marks has_taken_photo. Its explicit ordinary Skip path at850–877 can call that helper; other functions in this screen are original authored-timeline callbacks. The actual packed timeline invocation at232's failure has not been captured by this audit. |
| Native Lua entry | lua_Amax.cpp2540–2556 copies samples/hide/UI arguments and calls the real AppPhotoCapture().TakePhoto. This queues a capture; it does not render in the Lua callback. |
| Rendering owner | GameApp.cpp1045–1061 calls OnAppRender before the normal RenderNow path. Only the noncapture branch calls RenderNow/BeginRender. |
| Queued capture | camera2/capture/PhotoCapture.cpp51–61 consumes m_TakePhoto and calls RenderPhoto. RenderSingle185–211 calls RenderStart before its sample loop. |
| Accumulator preparation | AppPhotoCapture.cpp176–184 calls the real accumulator Start. AccumulateStrip.cpp250–258 directly calls ClearTextures, which changes targets and clears its three actual render textures. |
| First explicit capture scene | AppPhotoCapture.cpp217 begins a scene in RenderAddSample, after RenderStart. CommonRender and AddSample happen inside that scene; its original PC/native branch ends and presents at250–251. |

The source-level hole is therefore reachable without any UI or results setter:

```
GameApp.OnRender (outside BeginRender)
  C_PhotoCapture.OnAppRender -> RenderPhoto -> RenderSingle
    AppPhotoCapture.RenderStart
      C_AccumulateStrip.Start -> ClearTextures -> Device.Clear
    [later] RenderAddSample -> BeginScene -> CommonRender/AddSample -> EndScene
```

The lower native context's strict check at MetalContext.mm81 requires a recording
command for a draw/clear. The native upper BeginScene actually creates that
command and is not a no-op. ClearTextures in Start precedes that first scene.
The existing native CreateVolatile path at AccumulateStrip.cpp133–148 already
handles the same clear correctly outside a scene with a temporary owned
BeginScene/EndScene; Start does not. This is a concrete source-supported match
for the error, not actual first-throw proof until237 provides its stack.

## Existing test gap and repair boundary

`tools/macos/photo_test.cpp` validates real screen strips, partial final strips,
GPU average pixels, CPU maps, ownership and recreation. It deliberately starts
a device scene before calling Start in each existing sample test, so it does
not reproduce the actual application order above. Passing those pixel tests
does not validate this missing outer scene boundary. The proper regression
must also call real Start outside a scene, preserve an already active caller
scene, verify subsequent sample/copy pixels and retain strict lower checks.
Graphics240 owns that class repair; this task does not weaken the renderer or
move BeginScene into every low-level draw.

Related source limits: C_PhotoCapture::RenderPhoto/RenderSingle restore camera,
shutter and application debug state on normal completion, but these original
functions have no scoped restoration if RenderStart/RenderAddSample throws.
The current application aborts rendering and unwinds after an error. This
separate exception/retry behavior is not tested or repaired by a one-successful-
photo claim; no new image-success or retry state should be invented.

## Faster legitimate original UI reproduction

The normal Shared/Garage.lua has no original TakePhoto route. Do not invent a
garage-photo callback or call Amax.TakePhoto directly to manufacture a result.
A real single offline race already exposes the original pause-menu PhotoMode:

1. Observe actual Main0 Ingame/HUD, Racing, no popup/loading/wreck, controller
   unlocked, and original allow_pause not false. GameControls.cpp209 maps F1
   to the real Start button. Escape is also Start but is additionally Cancel,
   so a finite F1 is the unambiguous source-backed choice.
2. HUD.PauseGame1045–1067 sends the original pause and opens Ingame/Paused.
   Paused.Init58–62 locks Photo only in splitscreen/network races; a normal
   single offline race (and the corresponding Debug offline path) permits it.
   Do not change any locked branch, game mode or selection.
3. Observe the actual carousel's selected value with
   `UIButtons.GetSelection(GUI.carousel_id,2)`; its second return is the branch
   value, not its transient animation time. Read UIEnums.RwGameBranch.Photo,
   GUI.branch_name[selected] and CarouselIsAppLocked(Photo). With no subscreen,
   loading, active carousel app or retry menu, use finite ordinary Right steps
   to the actual unlocked Photo branch, then Enter. Paused.lua281–286 schedules
   the original Ingame/PhotoMode screen.
4. Observe PhotoMode's actual Camera mode and ready screen. Its normal
   MenuNext196–202 disables UI rendering, calls the real queued TakePhoto,
   changes to Viewing and later restores the UI through original FrameUpdate.
   A single ordinary Enter triggers this. Do not call any of those functions
   directly from Automation.

PhotoMode's default request uses g_PhotoSamples5, so it normally accumulates25
samples rather than postrace's1. Its initial RenderStart/ClearTextures boundary
is identical, before either first sample scene. This route can diagnose that
class without replaying three laps, while real postrace acceptance still needs
a genuine race/result/save path. No photo-save/upload key is needed for the
scene-boundary reproduction.

## MAC241 ignored finite-key operator

Added only ignored `_work/runtime/research/photo_mode_operator.py`. Parent owns
launch/input coordination. Once the root's existing game is at actual Racing
and every other input driver has stopped/its finite holds expired:

```
python3 _work/runtime/research/photo_mode_operator.py mac237 --seconds 150
```

The operator reuses the existing owned-log/tagged Observer. Before any IPC it
requires the root harness's actual StartScreen record; it never launches a game
or activates a window. It only uses bounded native F1/Right/Enter, maximum16
keys and180s. It checks Main0 context, original screen, popup absence, active/
unfinished/unlocked screen, no subscreen or GUIBank loading, actual Racing,
actual paused state, offline/nonSplit/nonwreck and each source-specific gate.
Paused must expose the actual unlocked Photo enum and a recognized branch name;
Camera must match the original runtime enum. Unknown popup/mode or rejected/
ambiguous input acknowledgement stops without retrying a possibly consumed key.

Multi-query observations are bounded rather than atomic: total freshness6s,
final context/selection/mode fences, then at most1s input expiry; the actual
command rechecks context and screen. There must be no concurrent keyboard
operator. Carousel animation ftime is excluded from action identity so time
passing alone cannot authorize another Right on an unchanged selection. No
profile/progress/result/finish/selection/camera/Lua transition setter appears.

After the Camera Enter acknowledgement it returns0 and sends **no more IPC**,
leaving the owned debugger free to capture the real first failure. That0 means
only the original finite capture key was accepted; it explicitly does not prove
photo completion, a successful frame, process exit or postrace persistence.
Pure tests cover each actual screen/branch/mode, blocked/unknown/boolean IDs,
staleness/expired/refused keys and maximum127-byte original command capacity.
Self-test and py_compile pass without IPC/game/build/GPU execution.

### First actual operator query correction

Root237's first helper attempt reached only passive HUD queries and exposed a
helper error: bare `IsNetworkRace()` is defined by Paused.lua423, not the common
HUD/PhotoMode Lua environment. The protected Lua runner reported a nil global;
no key was accepted. This is not the production BeginScene failure. The private
helper now uses the actual globally registered native
`Amax.IsGameModeSplitScreen()` and `Amax.IsNetworkRace()` getters (lua_Amax.cpp
1116/819 and registration7108/7142) for all three contexts. Other screen-specific
fields are read only under their matching screen; common GUIBank/native APIs
retain source authority. Pure tests additionally check the native registration
names against source and classify a fresh protected-Lua execution error as a
terminal query failure, rather than repeatedly treating it as transient
unavailability. The shared Observer is unchanged. Corrected pure tests and
py_compile pass; actual corrected-key/first-throw proof remains root-owned.


### Explicit ordinary return option after the fixed photo lifecycle

The default diagnostic command still stops all IPC immediately after capture
Enter acknowledgement. Explicit `--return-to-race` adds the original Back path
without invoking a Lua transition or save callback:

```
python3 _work/runtime/research/photo_mode_operator.py mac243 --seconds 180 --return-to-race
```

After capture acknowledgement, it requires two fresh consistent observations of
the actual PhotoMode Viewing enum and `GUI.enable_render_in<0`. The original
FrameUpdate224–232 restores UI rendering before that flag becomes negative.
Viewing itself was set before rendering, so it is not sufficient by itself.
This remains an observed render/UI lifecycle, not an independent GPU-pixel or
photo-persistence proof.

With all original active/unfinished/unlocked, offline/nonSplit/nonwreck,
Racing/paused, no-popup/subscreen/loading guards retained, the helper sends:

1. One finite Escape while **Viewing**. Original PhotoMode.lua185–190 changes
   to Camera through its own MenuBack handler. It never sends Enter here;
   original Enter in Viewing would attempt StartAsyncSavePhotos.
2. After observing actual **Camera** enum, one finite Escape. Original
   PhotoMode.lua164–168 opens Paused; PhotoMode.End invokes EndPhotoCam.
3. After observing ready **Paused**, active_selection=-1, no active carousel
   app/retry/loading/subscreen, one finite Escape. Original Paused.lua218–224
   handles MenuBack/Start and calls its actual ResumeGame function. Its End
   at413–420 unpauses and restores the in-game camera.
4. Observe the original ready **HUD**, `Blur2PC.RaceState()=="Racing"` and
   `Amax.IsGamePaused()==false`, then return0 and stop IPC before the parent's
   ordinary keyboard driver resumes.

Separate return phases never reuse the initial Camera-Enter capture decision,
so the helper cannot shoot a second photo while backing out. The total route
has a16-key/180s budget. Runtime guards and all queries remain within127bytes;
pure tests cover Viewing-with-UI-still-hidden refusal, wrong return phases,
actual Camera/Paused resume, blocked/paused state and no Viewing Enter. Tests
and syntax pass without actual execution by this agent. The final event names
its scope explicitly: photoGPUQualified=false and physicalRaceFinishQualified=
false. Parent owns successful actual photo presentation, continued human driving,
full career result/save-return and process-exit evidence.
