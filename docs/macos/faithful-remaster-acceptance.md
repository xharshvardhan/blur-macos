# Faithful remaster: first image-quality batch

MAC355/356/359,2026-10-30. User approved continuing overnight while preserving original Blur's style. This report distinguishes a rendering-quality increment from new assets, material reauthoring or a completed modern remaster. Historical candidate and overnight-stop sections below are retained; the resumed final qualification selects0.1.7/build380 with filtering16 and original window sampling.

## Initial candidate360 (historical)

Candidate0.1.7/build360 has game-launch defaults of eligible authored anisotropy16 and display-window MSAA2. These defaults are provisional until actual cost comparisons finish. The lower renderer/device APIs retain their authored defaults; explicit per-process environment choices override the game defaults and never change saved options.

Texture quality is restricted to samplers already requesting anisotropic min/mag filtering, with no point-filter axis and with mip filtering enabled. The quality floor never reduces a higher authored maximum and never rewrites point/ordinary-linear filters, no-mip controls, addressing, gamma, signed LOD bias or native cache ownership. The effective maximum stays in the existing eight-word sampler key; cold insertion owns a copy. Set `BLUR_MAC_TEXTURE_ANISOTROPY=authored|4|8|16` before launch. Optional shutdown-only counters use `BLUR_MAC_TEXTURE_QUALITY_STATS=1`; disabled counters perform no timing or I/O. See [texture contract](remaster-texture-quality.md).

MSAA uses the original display-window colour/depth/stencil-mask and resolve path. It leaves the presentation backbuffer and deferred normal/depth prepass single-sample. Original mirror and reflection views may inherit the application window sample count; this is not a main-view-only memory/cost change. Set `BLUR_MAC_WINDOW_MSAA=authored|2|4` before launch. The native authored default remainsNone. Original PC D3D11 defaults already distinguish2x display windows from a non-MSAA backbuffer; this is source-grounded integration, not a new temporal-AA algorithm. See [window contract](remaster-window-aa.md).

Malformed/unsupported window requests fail before old volatile resources are retired. A native-only shared-device parameter transaction restores authoritative metadata on failure/exception. That does not reconstruct an old GPU graph after a later destructive allocation failure. The new capability query answers live-device sample-count/store-and-resolve support, not every texture format/allocation or concurrent-destroy safety.

All693 non-notice resource files match0.1.6 byte for byte. No original textures, geometry, effects, shader binaries, colour grading, audio, controls or simulation are replaced. A shader-HDR/FP16 path is not native display HDR/EDR. PBR, temporal reconstruction, new assets, remote multiplayer and another Mac remain separate work.

## Independent image and failure controls

The real GPU texture fixture uses an authored256-square stripe texture, exact box-filtered mips and a16:1 derivative footprint. Independent minor-axis band truth gives authored mean absolute error127.5/contrast0 versus enhanced error0/contrast255 on the local GPU. This deliberately controlled signal establishes detail retention for that footprint, not a universal score on game textures.4/8/16 selections match the corresponding native authored maximum exactly. Point, ordinary-linear, mixed-point and no-mip controls remain byte-identical. It also checks higher authored maxima, global/per-slot state, each shader stage, explicit mip/bias/address paths and diagnostic-off output. Stages are exercised in separate contexts; this is not a new simultaneous-stage/sRGB pixel oracle. Existing sampler-key reuse tests supply their separate cache/stage controls.

The expanded upper-window fixture checks colour/depth/mask sample agreement, caller copyback, early rejection without retiring live owners, and native internal metadata rollback. A genuine unsupported D16 window-depth request fails after framebuffer creation, checks partial-owner cleanup and CanRender=false, then retries D24S8 and renders. A same-sized original-shader triangle comparison independently requires binary1x edges, fractional equal-RGB2x edge coverage, and exact analytically interior/exterior pixels, without fixing driver sample positions or counts.

The first focused Speed run passed3/3 before the additional geometric-edge oracle. Final coherent qualification of that oracle and both full suites is recorded below when complete. No failed initial attempt is relabelled as a pass.

## Actual original-game evidence

The original Master frontend,20-car human Focus race, ordinary Photo Mode and original race/menu/desktop exit are used with copied accepted285 profiles/saves. Automated sound is muted before playback, windows minimized and native service runs serialized. Source game data and user saves remain unchanged. Captures are single native completed-frame readbacks; no generated image or edited frame is an acceptance artifact. Captures are not performance runs.

MAC361 exposed an authored harness mistake: it treated20cars as career eventId20. The real independently validated frontend mapping is event key4013244786 → eventId4597671, car331339493. That failed capture proof is retained. Supplemental ordinary Camera→Paused→HUD→race/menu/desktop cleanup all returned0; no unknown-screen input or direct game-state setter was used. The corrected harness pins actual scene identity, validates it independently in Photo Camera and records primary pose/clock/filter stability within each run.

MAC362 original352 and MAC363 enhanced360 both qualify their native2560×1440 Photo Camera captures. They have the same validated scene/car and unchanged artistic resources. AI motion/camera entry across separate runs is not deterministic; no pixel-difference or exact cross-run camera-pose claim is made. The images are evidence of actual graph appearance, not a substitute for the controlled GPU oracles.

Both games exited0 without fatal errors after accepted original popup204 desktop confirmations. Their v2 wrapper reported ordinaryExitQualified=false because it published owned process status only after waiting for a helper that itself required that status. Raw proofs and helper-15 records are preserved. Separate supplemental proofs require the exact source-qualified popup, guarded command/tag, typed acceptance, qualified capture, source/state hashes and normal owned exit. They make no exact runtime claim: old wrapper seconds included helper grace. Future v3 runs atomic-publish actual exit status before a bounded helper wait and measure runtime before that grace.

MAC364 is a separate parked30s diagnostic, not a timing comparison. It completed normal original exit with audio muted/source state unchanged. Across startup/frontend/race/shutdown, the real context made17,834,993 sampler lookups;1,353,719 were eligible and promoted, with highest eligible authored maximum8 and effective maximum16. The early empty context had zero lookups. These are lifetime lookup counts, not counts limited to the parked interval, unique textures, road-specific coverage, GPU time or FPS.

## Initial coherent checks and pending selection (historical)

Final source freeze contains53 paths (12 changed) and preserves all unrelated previously accepted source. Both complete Debug and Speed suites pass155/155 (177.12s/107.70s). Three focused fixtures pass in each lower UBSan build (six checks), and14 focused Metal-validation tests pass. This includes the final geometric-edge oracle and real late-failure/retry path. Actual isolated HUD comparisons use the same360 executable with authored/authored,16/authored,authored/2 and16/2 startup settings after builds/checks finish. They retain the existing duplicate-aware HUD summary method and exclude captures/readbacks/heavy builds/extra measurement IPC from quiet windows. Current baseline358 and all contrary results must remain in the final report. Default selection, package integrity, standard delivery and publication remain pending until that evidence is reviewed.


## Overnight stop: release held

The allowed overnight window ended before all cost comparisons qualified.365
(authored/authored) and366(16/authored) passed complete actual driving/menu exit.
367 completed its stationary window but stopped during a later passive telemetry
bookend; no moving-window or ordinary-exit qualification is assigned. Later HUD
records/guard cleanup establish application progress, not later physics or a
causal AA regression.369 and368 failed at audio initialization before racing.
The independent real-output silence fixture reports AudioQueueStart -66681
(kAudioQueueErr_CannotStart in the installed SDK), and the accepted352 app also
fails that audio-init path. The cause is not established. No dummy backend,
service restart or global setting change was used to bypass the failure.

Candidate360 remains held; exact signed0.1.6/build352 is restored in the standard
play location and manual launcher. No0.1.7 distribution or public implementation
commit was made. All source, GPU/capture and failure evidence is preserved.
The overnight automation was deleted. Remaining work is native-audio availability
and the two complete2x-only/combined driving-cost comparisons before final
default selection, packaging, delivery and publication.

## MAC380 final qualification — 2026-10-30

After the user explicitly resumed work, the unchanged real CoreAudio probe376 and actual original20-car driving runs377/378 passed. The final0.1.7/build380 keeps eligible texture anisotropy16 but **original/authored window sampling by default**. Explicit2x/4x window multisampling remains optional. The lower renderer defaults remain authored, and explicit process settings win. Original assets, effects, colour treatment, simulation, audio implementation and saved options are preserved; this is an incremental rendering-quality batch, not a completed asset/material remaster.

Both final complete Debug/Speed suites pass155/155 (136.73s/64.86s). The53-path source freeze preserves52 renderer/fixture paths from the candidate360 qualification; only NativeMain's default/help changed. The six focused lower UBSan and14 strict Metal checks therefore remain applicable. All693 non-notice resources are byte-identical to0.1.6.

The cost matrix below uses the same360 executable with explicit settings. Final380 differs only in its game entry default/help and has its own default-settings actual run381. Each qualified run used real CoreAudio muted before playback, minimized windows, a serialized native-service slot, the original20-car scene,90s no-input and45s W-driving windows, then ordinary race/menu/desktop exit0. Source data and accepted saves remain unchanged. Heavy builds/captures and extra telemetry IPC were excluded from quiet measurement windows.

| Run | Texture / window samples | No-input interval / GPU ms | Driving interval / GPU ms | HUD graphics MB, no-input / driving |
| --- | --- | --- | --- | --- |
|365|authored / authored|17.130 /7.270|16.345 /7.590|572.73 /574.91–581.98|
|366|16 / authored|17.545 /7.010|16.880 /7.095|572.73–574.48 /574.91–581.39|
|377|authored /2|20.530 /10.225|20.210 /9.835|648.41–664.84 /649.53–657.91|
|378|16 /2|25.090 /9.580|24.962 /11.050|647.69 /650.69–673.53|
|381, final defaults|16 / authored|16.430 /7.170|20.380 /8.170|557.95–573.27 /560.95–568.48|

These are medians of per-emission HUD sample medians, not reconstructed unique-frame statistics or derived FPS. Duplicate/overlapping driver samples are retained. GPU milliseconds are not occupancy; HUD memory is not process RSS and categories must not be summed as independent allocations. Separate sessions, AI trajectories and host variance limit causal comparison. Earlier baseline358 had22.13/7.39ms no-input and29.92/8.94ms driving; it does not establish an FPS win. Maximum reported intervals for365/366/377/378 were50.74/59.50/86.97/91.82ms no-input and48.98/71.81/119.86/115.06ms driving. Final381 maxima were46.97 and76.91ms: hitches remain.

The AA rows show higher measured cost, so the final default preserves original window sampling. Filtering improves the independent controlled detail oracle and has verified nonzero actual use; no universal scene-detail score or speedup is claimed. The actual362/363 images and separate exit proofs remain relevant appearance evidence, with their nondeterministic cross-run camera/AI limitations. Failed361/367/368/369/370/371 proofs remain retained. The passive-observation budget fix is private harness code, not a game/physics change.

Correlated audio logs locate a15s physical AudioQueue start timeout during background wake; the exact internal macOS trigger is unproven. Accepted0.1.6 failed the same path overnight. Current probe376 and actual377/378/381 pass without a production audio change, service restart, dummy backend or OS routing change. This is not a listening-quality verdict for earlier race-music crackle. See [audio startup investigation](audio-startup-wake-investigation.md).

## MAC380 delivery

Qualified0.1.7/build380 is installed in the standard play location and the manual user launcher now selects its immutable development package. Exact executable SHA256 is13f7f50071822e090b2a278d8294cb451e455e256aec3a668c2723b71eed490e. Strict ad-hoc signature, bundled notices, ZIP extraction and relocation checks pass. Accepted0.1.6 remains available for rollback; no user game data or profiles are bundled or reset. No audible test or automatic foreground launch was made.

Publication contains only12 frozen source/fixture/build paths and10 authored documentation paths through the separate personal publication index, preserving profile redactions. Game data, saves, builds, private logs and unrelated changes are excluded. Notarization, another clean Mac, complete remaster coverage and remote multiplayer remain unverified.
