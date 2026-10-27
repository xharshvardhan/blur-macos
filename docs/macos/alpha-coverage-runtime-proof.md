# MAC227 actual alpha-to-coverage request proof

Fresh Debug218 normal Master career reproduces the exact upper native
alpha-to-coverage rejection before unwind. The caller is the crowd flag renderer
inside the rear-view mirror, not a guessed FX material or foliage binding.

## Exact captured state

- `Display::C_ContextBase::SetAlphaToMaskEnable`, Context_Base.cpp485:
  requested **E_Bool_True**, not Default.
- Current upper context multisample metadata: None.
- Actual lower colour0: A8R8G8B8,1152x192, **samples1**; colour1-3 null.
- Actual lower depth/stencil: D24S8,1152x192, **samples1**.
- Command buffer and render encoder are both nonnull. Bound flag VS/PS native
  identities1086/1088; the actual crowd object has one flag.
- Pure SB capture errors empty. All threads and selected40-frame bounded stack
  collected; no target expression or rendering-state mutation was used.

Actual caller chain:

`CrowdFlags.cpp:527 RenderMain` → `Render:513` → SceneGraph RenderAbstracts /
RenderTask / RenderGroup → `GameWorld/View/Mirror.cpp:239 C_MirrorView::Render`
→ `C_View::PreRenderSplitscreen` → world / layers / application OnRender.

At the source call, the renderer has selected its real flag shaders and enabled
alpha test(ref0), then directly requests alpha-to-mask. Subsequent flag parameter
setup, texture binds and draw have not executed. In particular, unexecuted local
numTotalVertices/numIndicesPerFlag values are not valid geometry evidence.

The lower attachments establish sample count independently of the upper cached
metadata. No corresponding alpha-to-coverage pipeline has been created at this
stop: the upper guard throws before forwarding/integrating that requested state.
MAC228 owns the source-backed feature/adapter repair; no guard was changed here.

## Reproduction and input limits

Binary SHA256 matches Debug218:
`4247044b97b9c0daea3fac42810d554d4744291d227e93b5abd23ec7404bdb64`.
Original Master189 data/profile/save, CompleteCareer0, ObserveOnly1 and strict
Metal debug layer. The ordinary menu helper selected real event4597671 and an
owned allowed car. No event, AI, vehicle pose, lap, checkpoint, progress or
result setter was sent.

The initial helper raced log creation and refused before IPC; it was restarted
only after the harness confirmed StartScreen. At255s actual HUD/Racing/20 was
observed, the retry helper was retired and MAC224 finite ordinary keys started.
Its226 telemetry records reach raw lap1 of configured3, with PLAYER0 and all
configuredAI/aiToggle/automatedHuman false on accepted actions. Raw lap crossing
alone is not a completed career-lap or finish claim; no qualified finish or
normal results/exit was established by this diagnostic.

## Cleanup and evidence

After capture LLDB explicitly killed only owned inferior91179 (status9), then
quit. Wrapper diagnostic exit0 at367.93s, qualified_game_exit=false and
slot_released=true. Exact private-app process scan found no leftover; owned menu
observer and keyboard operator ended. The capture stopped before the application
catch, so the harness primary field is null despite the proven exact throw.

Ignored evidence: `_work/runtime/verification/mac227/alpha-proof.json`, compact
`alpha-summary.json`, source proof, command/capture scripts, helper records and
logs. Actual log/probe status have the MAC227 prefix. No screenshot or proprietary
asset bytes were added to tracked documentation. Production, fixture, CMake and
shared archives were untouched. Root's coherent repair and actual normal-career
retry remain required.
