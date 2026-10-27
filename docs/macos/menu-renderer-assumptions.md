# Normal menu renderer and widget assumptions (MAC188)

Read-only source audit, 27 October 2026. The natural one-track keyboard race is separately proven by root MAC183: original start, split1, split2, LapInc1 and RaceFinished, followed by ordinary quit/exit0 at571.51s. That launch used the developer race entry. It does not prove the normal Master frontend, profile prompts, showroom or return to menu. Root owns that actual menu validation; MAC186 owns the normal Lua flow and MAC187 profile/save ownership.

The initial audit performed no production changes, shared builds, archive reads or runtimes. Root's actual MAC189 subsequently identified a failed-movie readiness fault, authorizing the narrow correction and validation below. The inherited `agent_context.py context --task MAC188` command was attempted and rejects the new macOS worktree phase; this board/ledger is its task record.

## Required renderer and resource boundaries

| Owner/boundary | Current selected native behavior | Evidence limit |
| --- | --- | --- |
| `C_UiLayer::Create` (`UiLayer.cpp:223`) | Retains the manager owner before calling `NativeInitialise`; creates Bloom, FE render buffer and FE postprocess buffer; checks initial LightPaint readiness; loads required frontend BPR. A throwing/failed create resumes only its acquired loader pause, destroys owned UI resources and propagates failure. | Earlier required-resource and owner fixtures exercise false/throw/retirement. An ordinary Master menu frame is a separate actual path. |
| `C_UiManager::NativeInitialise` (`UiManager.cpp:836`) | Acquires Lua/contexts/Simple2D; requires common and locale-selected font BPR, bitmap font data, real UI Flask pack and `UiXtConfig`. NativeReady is published only at the end. Public update/render entry points call NativeRequireReady. | This is manager readiness, not a promise that every optional widget or IPF shape exists. See [ui-required-resources.md](ui-required-resources.md). |
| Frontend visible textures (`UiManager.cpp:1706`) | `NativeRequireBundle("UI/Data/ui_frontend")` invalidates readiness and throws on a real bundle failure. Game transition similarly requires ui_game. No default visible texture substitutes a missing bundle. | A valid bundle index does not certify every later gadget name or optional artwork reference. |
| UI GPU creation (`UiLayer.cpp:146`, `:311`, `:423`) | Requires actual psBloomComposite, selected postprocess manager Create/CreateVolatile and render-buffer Create/CreateVolatile. FE composite uses backbuffer dimensions, RGBA8 and original PC D24S8, native sample None. | The actual `C_UiRenderBuffer` GPU fixture covers creation, None/2x/4x matched attachments, clears/resolves, resize and retirement; it does not drive all normal screen scripts. |
| UI render ownership (`GameApp.cpp:1017`) | Existing whole native OnRender FX mutex scope encloses BeginFrame, UI RenderOnce, scene rendering and fullscreen composition, with RAII release on failure. UI transition creation owns recursive device access in its own scope. | Do not flip lock types or bypass resource serialization to advance a menu. |
| Frontend draw sequence (`UiLayer.cpp:890`) | RenderOnce first calls UI PreRender, FE composite and FE postprocess. FE composite binds its real colour/depth target, clears colour/depth/stencil and invokes UI RenderAA. FE postprocess/Bloom produces the borrowed bloom result. Later RenderFullscreen draws 2D background, premultiplied composite with Bloom, then non-AA UI. | The bloom texture is borrowed from the ready graph and produced before the normal composite consumes it. Do not synthesize it or reuse an arbitrary previous target. |
| Showroom world policy (`GameWorld/View/Main.cpp:410`, `:1134`) | Native selects original PC preset policy. `inUI` requires vehicle prepass and full model distance; the prepass still draws frontend depth. Original PC excludes deferred lights while mInUI, rather than disabling the whole frontend world/render graph. Mirror/reflection policies remain the existing preset chain. | A completed Brighton race proves the race graph. Showroom models, cameras and screen selection still need actual Master-flow evidence. No prepass/mirror/attachment policy was changed for this audit. |
| FE→race→FE transitions (`UiLayer.cpp:1140–1250`) | Existing messages clear borrowed world/minimap pointers, switch frontend/game bundles and shapes, destroy the old mode's GPU graph and acquire the selected new one. Native required graph calls propagate failure; Destroy retires owned managers/buffers/minimaps/LightPaint. | Successful game shutdown is not yet evidence of every normal frontend return/resize path. Required failures must remain visible before rendering an incomplete graph. |

The FE graph is not the in-game HUD graph: in-game composite is quarter-width/height and depthless; its colour-only clear is source-authoritative. A normal menu failure must be traced against the selected FE owner rather than globally disabling depth, postprocessing or borrowing game targets.

### Concrete source gaps to observe before changing policy

1. `LoadFrontendShapes` (`UiManager.cpp:1582`) ignores the individual IPF load results and marks its load flag true. However, `UIAdobeSceneManager.cpp:78` currently returns **false even after a successful import and handle publication**. Blindly promoting that bool to a required-success predicate would reject valid scenes. If a normal menu has missing required shapes, capture its actual requested name/import failure/handle first and repair the real status contract and ownership together; do not insert a guessed shape or declare all IPFs optional.
2. On return to FE, `UiLayer.cpp:1187` calls `LightPaint_Init` without the explicit `LightPaint_IsCreated` check used by initial Create. This is a concrete asymmetry in required-readiness publication, not a reproduced allocation failure. It does not justify a speculative transition refactor before actual menu/return evidence.
3. MAC178's target scheduling/cache and local/current-view differences remain documented separately in [context-device-contract-audit.md](context-device-contract-audit.md). They are not diagnosed normal-menu failures and are not changed here.

## The11 missing UITextBox objects

[ui-snapshot-gaps.md](ui-snapshot-gaps.md) records the original authenticated UI pack/type evidence: the source has no matching implementation, Xt registration or Jam member. The existing PC importer skips allocation for the unknown type, assigns its components null type/count0, and leaves references to those elements null. UI screen creation/export skips null elements. This is existing recovery behavior, not an instruction to alias UITextBox to UIKeyboard, static text or a dummy.

The prior MAC045 private schema parser was rerun read-only by the MAC186 owner (`_work/build/macos/physics-agent-ui-schema-current.log`); it gives these actual names. Proprietary pack bytes remain ignored/private.

| Screen record | Count and gadget name | Relation to normal local play |
| --- | --- | --- |
| Account/NetLogin.lua |2 `textbox` | Account text entry; not on the selected ordinary offline-SP route. Missing functionality remains real. |
| Account/NetRegister.lua |4 `textbox` | Account registration; same boundary. |
| SinglePlayer/SpDlcEntry.lua |1 `textbox` | DLC code entry; not required for the selected existing local track/event. Do not claim DLC works. |
| FriendsList.lua |1 `textbox` | Friends/network text entry; distinct from ordinary local navigation. |
| Shared/Facebook.lua |1 `message` | Social text entry; absent source widget prevents a faithful completeness claim. |
| Shared/Twitter.lua |1 `message` | Social text entry; same boundary. |
| TheCustomPopup.lua |1 `textbox` | **Generic, reachable in local UI.** Its presence cannot be dismissed as online-only. |

The generic popup is record164/ref356, component16, under `SCUI_Popup`; the missing widget is named `textbox`, backing colour `!255 0 0 0`, colour style Main_White. It is not the popup's `title`, `message`, `options`, `next` or `back` gadget.

`popups/TheCustomPopup.lua` and `CustomPopupData.lua` contain no access to `textbox`. The ordinary StartScreen Escape quit confirmation uses KillApplication's existing title/message and next/back text; MenuNext invokes `UISystem.SystemQuitGame(true)` (`CustomPopupData.lua:5911`). Thus the source supports a non-text-entry quit confirmation using the implemented gadgets. This does **not** prove its actual layout/focus or every custom popup: original missing-element parents, timelines and later feature callbacks remain possible dependencies and must be observed rather than replaced.

MAC186 identifies the normal fresh-store success path as Profile/AsyncLoad's own created-profile message followed by Return and actual AsyncSave. No generic text-entry popup is inherent to that success branch. Generic custom popups are still used for quit, errors/network conditions and other ordinary UI branches; they remain a required actual observation boundary. The account/DLC/social gaps must remain disclosed even if local play succeeds.

## Normal controls and selected screen assumptions

The selected main entry is Master→Intro/Legal→Intro/StartScreen, not the obsolete alternate Intro/MainMenu script with RaceWorld paths. MAC186 records the full ordinary single-player screen chain and first-event behavior; do not force a developer screen or unlock/save progression merely to get past a menu.

The existing native keyboard route is complete at the source boundary:

- Framework creates the actual keyboard/mouse/pad devices for native (`ApplicationBase.cpp:1243`). NativeInput maps macOS Return, Escape and arrows to original keyboard button enums.
- Original PC menu layouts in `GameControls.cpp:196` select Return/keypad Enter/Space for Confirm, Escape/keypad0 for Cancel, and arrows/keypad8/2/4/6 for directions. Native selects that PC layout.
- `UiManager::ProcessController` preserves profile AllowPadInput gates, active screen-context message masks, screen transition timing, debouncing and directional repeat. Confirm/Cancel become actual MenuNext/MenuBack; native also supplies original PC input-device metadata to Lua.
- StartScreen treats keyboard/mouse as profile slot0. Its original-PC branch advances only on MenuNext and uses MenuBack for quit, even though Escape also maps to ButtonStart (`StartScreen.lua:323`). Native publishes PC platform enums through ButtonController.

The Windows-only optional controller-glyph atlas registration/input-glyph switching remains excluded from native. It is a pad-hint visual boundary, not evidence that keyboard menu navigation is absent; it should not be silently claimed as implemented. Native keyboard hints use the existing font/resource route. Physical OS focus and actual selected gadget/context still need the ordinary menu run; Lua handlers alone do not prove input delivery.

## Actual MAC189 failed-movie boundary and correction

Root's actual Master run reached the showroom/Start screen, then at idle attempted `Ui/Movies/AmaxAttract.bik`. The file failed to open; native GPU validation later rejected unbound pixel texture0 in shader690. Original `UI::Movie::Create` ignored Open=false and marked the player active; `SetupMovie` always added its movie graphic; GetTexture exposed an unopened player's empty wrapper. The original UI Update already defines failed open as MovieFinished, so no placeholder or invented skip mechanism is needed.

Root approved and source-froze this native-only class repair:

- `ui/Graphics/Movie.cpp` stages the actual player; only a successful real open/first decoded frame/output texture publishes active playback. Missing/corrupt/open exceptions retire the candidate and queue the original MovieFinished once. Native fields are initialized for local/partial owners, and device acquisition in Update uses RAII.
- UI Pause cannot reactivate failed playback and forwards pause state to the native elapsed-media clock. A decode/upload failure terminates once even for a looping movie. GetTexture returns actual created GPU storage; a genuine last decoded frame may remain until original screen teardown. No synthetic movie or replacement pixel is created.
- Native `Lua_System::InitMovie` reports actual readiness, rejects invalid slot narrowing/indices and honors the original duplicate live-owner rejection. `GUIBank::SetupMovie` omits its movie graphic only on explicit false. Existing Windows and non-creating preview nil returns retain legacy behavior.

The asset-name question is separate: original local data contains BlurAttract.bik, while three source references name AmaxAttract. This correction does not rename a source asset or infer matching retail Lua authority. Root MAC190 also observed an Open failure for the existing BlurAttract path before decoder entry; MAC191 owns that distinct filesystem investigation. Missing input and an actual codec failure must remain distinguishable.

### Focused verification and remaining limits

`tools/macos/ui_movie_test.cpp` uses the actual UI wrapper, real native Lua InitMovie registration, exact source SetupMovie function, original deferred message queue and real decoded GPU textures. A descriptor collector observes whether that helper publishes its graphic; it is explicitly **not** a full gadget renderer. GPU checks sample real output through the source-built Display texture shader and compare every authored pixel, not the full FE compositor.

Twelve exact production/fixture object probes pass across Debug, Speed, Debug UBSan and Speed UBSan. Private Debug and Speed full-game/bootstrap-first closures both exit0 under strict Metal for three missing/corrupt-open→valid retry cycles, one completion per failed request and normal end, duplicate/invalid-slot rejection, first/last GPU pixels, elapsed pause/resume, repeated teardown and preview compatibility. The standard existing movie_test.py authors its media. No full-game UBSan runtime is claimed: that closure retains the separately documented CMake exclusion.

The default is authored video-only isolation. Optional `BLUR_MAC_UI_MOVIE_AUDIO=1` preserves a real PCM/CoreAudio stress variant. That variant passed Speed, but Debug crashed on asynchronous system audio disposal: PageUsage::GetBit→BlockAllocator::Free→global delete→AQMEIO_Base→AudioQueueObject destructor on a dispatch worker. The owned IPS is retained under `_work/build/macos/occlusion-agent-ui-movie/macos/crash.ips`; this is an unresolved audio/allocator lifetime observation, not hidden by the video-only result. Root MAC193 owns its follow-up through this agent. Logs and exact probes remain in that ignored private directory.

Source/fixture are frozen for root coherent192 integration. No renderer target/view/default policy, missing-widget behavior, shape loader, profile or normal progression code changed. Actual normal-menu navigation/profile reload and full return/results flow remain root verification; source readiness and the focused UI movie test do not establish them.
