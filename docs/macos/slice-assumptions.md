# Native one-track slice: assumptions and evidence

MAC088, read-only audit, 2026-10-27. Immediate acceptance is one original
Brighton1 route, one BMW on controller0, keyboard driving, a genuine completed
race, and clean application exit. LAN/multiplayer, expanded menus, career saves,
and distribution are deferred. Completed local work remains in place; this audit
adds no production changes, game runs, or new feature work.

The implementation must distinguish three meanings previously grouped under
`PLATFORM_PC`: Windows host APIs; the original PC asset/data contract; and retail
PC engine policy. macOS replaces the first while retaining the latter two where
actually used. Defining `PLATFORM_PC` globally would also select Win32 services
and is not an appropriate way to preserve PC data. Conversely, falling through
to a console branch is not evidence that the branch is portable.

Evidence terms: **actual** means observed in the original full game or a reader
of an identified original file; **focused** means a bounded original-component
fixture; **source** means a traced reachable path without an equivalent completed
game run. Confidence describes the stated mechanism, not completion of the slice.
The latest actual slice evidence is MAC183 on coherent Speed182: original
Brighton/BMW archives, full renderer gameplay frame, ordinary keyboard driving,
ordered splits and lap-qualified RaceFinished, then ordinary application exit0.
All705 observed Racing records used human control and sampled travel was3309.57m.
Actual179 separately verifies populated Debug shutdown with assertions enabled.
MAC174/175 repair attachment-snapshot and audio-observation ownership classes;
178 restores mixed Context/Device geometry state;181/185 retain measured original
PC cube/2D semantics with a required transactionally owned zero cube. These
repairs passed affected fixtures and the real completed race. The bounded slice
is complete; normal Master menus, career save/reload, multiplayer and distribution
remain separate acceptance work. See [slice evidence](PLAYABLE_SLICE.md).

Grouped repairs now cover the upper mask contract, all selected dynamic
particle/light topology and writer/map pairs, UI scene-borrower retirement,
required configuration and texture-map failure propagation, and original PC
final-view/HUD handoff. Source/fixture evidence is in
[colour-mask-contract.md](colour-mask-contract.md),
[particle-geometry-contract.md](particle-geometry-contract.md),
[scene-light-ownership.md](scene-light-ownership.md),
[remaining-startup-contracts.md](remaining-startup-contracts.md), and
[selected-view-handoff.md](selected-view-handoff.md). These class fixes must be
integrated into one coherent binary and pass the actual race path; isolated
checks do not advance the five acceptance gates.

| Priority / recurring assumption | Classification and source authority | Evidence / confidence | Required slice validation or proposal |
| --- | --- | --- | --- |
| P0: successful outer construction implies every child created | Runtime ownership and failure propagation, independent of host API. `render/source/postprocess/Manager.cpp:64` used short-circuit child creation but returned true; `GameWorld.cpp:1227` and `AmaxWorldLayer.cpp:560` now check the actual child/world result. `framework/source/layer/LayerManager.cpp:640` now retains failed objects without promoting them to Ready. | Actual MAC080 had all11 MotionBlur shader backings and both textures null, with no Create-entry output. Actual ghost DDS failure explained the earlier short circuit. Current complete effect-graph startup progresses beyond this. High. | Apply one invariant across the reachable graph: fail the parent, retire only its acquired resources, and surface the original error. Do not render a failed child or manufacture successful resource creation. MAC089/090 own the remaining graph inventory. |
| P0: a valid D3D pass is a valid Metal encoder without extra ownership transitions | PC engine/render API contract, not Windows UI. `effect/source/postprocess/HDRInterface.cpp:563` through `render/source/postprocess/Interface.cpp:292`; existing PC DX11 compatibility code is the authority for active-target sampling snapshots. | Actual MAC087: pixel slot1 sampled the same1x1 R32F surface bound as colour0. Metal correctly rejected the hazard. High. | Restore the complete existing pass/snapshot contract in the graphics owner; retain hazard checks. A fallback texture or relaxed validation would not establish a correct frame. |
| P0: process exit frees everything, so destructor order is irrelevant | Host lifetime differs. Original `GameApp.cpp` PC OnShutdown ends in TerminateProcess; native `Main.cpp:254` explicitly shuts down/deletes the application before core services. Native `GameApp.cpp:223` releases static ReplayDirector before C_Game. `ReplayDirector.cpp:137` releases complete forwarders/resources; `ReplaySaveManager.cpp:81` joins owned I/O first. | Actual old cleanup dereferenced a cleared Boot/System owner. MAC079/085 now reach later cleanup, but MAC084 still asserts a nonempty Display2 resource list. Original-component lifecycle and constructed-app fixtures pass; clean full-game exit does not. High. | Keep layer owners, caches, FX and device alive until their borrowers finish. Join creators/cache workers before retirement; release caches after callbacks. Trace each remaining tracked GPU resource to its owning acquisition/release. Never clear the tracker to hide ownership. |
| P1: platform identity is the host operating system | Asset contract. `common/system/source/misc/Platform.cpp:31` explicitly returns PC data identity on native. Real PAK authentication, resource platform checks, BPR, model/texture and level readers remain enabled. | Actual original root/gamedata packs securely mount and models/world resources now pass platform validation. High. | Keep native host services separate from serialized PC platform tags. Validate the selected Brighton/BMW resources through their existing readers; previous showroom success is not that validation. |
| P1: a32-bit file can be loaded by copying native structs | Disk/runtime ABI. `xt2/source/common/XtFlask.h:79` stores array length as XtU32 and pointers as component/element XtU16 pairs; `XtFlaskImport.cpp:698/731` reconstructs native pointers/arrays. `renderingdata/.../target/archive.cpp:71` keeps exported texture indices32-bit while runtime pointers use native width. `display/source/texture/metal/MetalTextureLoad.cpp:15` requires the original28-byte texture header. | Actual UI.bin/BPR and showroom render data load; authored Xt v1/v2 and texture-header/payload fixtures cover specific formats. High for these paths, incomplete for Brighton/BMW. | Preserve fixed-width fields, offsets, endian and import reconstruction. Do not interpret a stored address/index as a64-bit process pointer. Inventory any newly exercised archive version rather than assuming all versions share one layout. |
| P1: wchar_t, DWORD, long and pointers have Windows widths | Host ABI versus PC field widths. `core/Core.h:300` uses DWORD32, WCHAR/char16_t16 and uintptr_t pointers; `XtReflection.h:509` makes XtSz16 UTF16. `system/.../BinarySerialiseAutoEndian.h:30/43/103` uses exact-byte copies for packed scalars. | Actual UI/profile schema exposed UTF16 requirements; focused Unicode and packed scalar tests pass. The packed U32-at-byte1 failure was observed under UBSan. High. | Retain16-bit text and32-bit serialized scalars even though Darwin wchar_t/long and pointers differ. Byte alignment on disk does not authorize typed native dereferences. UI text conversion/formatting belongs at a host boundary, not in the serialized schema. |
| P1: one128-byte clear/cache line equals the complete object | Native runtime layout only. `ui/.../Simple2d.inl:159` clears sizeof(C_Sprite) on native; `core/.../btl/Allocators/pool.h:30` gives free links and live objects their actual size/alignment. `scenegraph/Handle.h:49` checks independent lifetime before reading a pooled object's salt. | Actual sprite matrix index0x1111 came from uncleared bytes in a136-byte sprite. Actual UBSan scene exposed misaligned intrusive links then a dead light vptr; adjacent-poison, pool and real scene/handle fixtures cover the fixes. High. | Keep runtime slot stride/alignment separate from disk layout. Poison/reuse checks are relevant to newly found fixed-size clearing/overlay sites, not a reason to memset every C++ object. A destroyed polymorphic object's salt is not safe to read, even at a recycled address. |
| P1: constructors or static zero-fill imply every ownership path is initialized | Native runtime construction/failure ownership. `XtReflection.h:700` avoids live constructors/destructors during static base-layout queries. `GameWorld.cpp:321/345` initializes early-failure scene/shadow owners; native replay initializes all owned pointer arrays explicitly. | Actual pre-main reflection destroyed an uninitialized UIText effect pointer; actual world creation failure reached uninitialized/later owners. Focused poisoned local replay construction and repeated destruction pass. High for corrected sites. | Review acquired stages in reachable Create/Destroy pairs, including partial construction, rather than blanket null guards. Namespace-static zero-fill is not a guarantee for heap/stack instances. Missing implementation is not an initialized empty object: UITextBox gaps remain documented separately. |
| P1: PC quality/view choices are Windows-only | PC engine policy. Still-PC-only members and guards form a coherent remaining graph: `GameWorld.h:327/472`, `GameWorld.cpp:452/1437/2208/2215/3480`, and `PostProcessLayer.cpp:201` select the recovered graphical-detail preset, view-dependent FX, depth restoration and prepass gating. Existing comments cite retail0x0041b9e0/0x0042311e and T232. | Source-reachable during world Create/render. Prior actual failures repeatedly came from selecting console depth/state/resource policies. These remaining sites have not been established as the current primary failure. Medium for next impact, high for branch difference. | Graphics MAC090 should evaluate this as one preset/view/pass contract. Do not widen all PC guards blindly: e.g. GameWorld's graphical-detail field itself is absent natively, so it is not simply an uninitialized native field. Preserve host-only automation/settings exclusions. |
| P1: decoded physics implies the race has simulated correctly | PC physics format and independent fixed simulation. `game_resource/source/PhysicsLevel.cpp:42` wraps Gravity Import; `gravity/source/importer/Importer.cpp:286` reads the actual version. Native `ApplicationMac.cpp:77` consumes existing1/60 cadence and independently interpolates rendering; vehicle interpolation uses the existing PC hooks. | Actual Showroom v10 reader fixture consumed the complete file:1static mesh,4925 vertices,3358 triangles; tree/indices/bounds and truncated Gravity payload rejection verified. It does not load Brighton or simulate BMW contact/steering. High for that file only. | Verify the selected route's imported scene and car on the real path. Source gap: versioned Main/Route loads discard Import(bool) at PhysicsLevel.cpp80/92 and return true; the legacy path54 propagates it. Propose a faithful owner-level error policy before editing; no failure is inferred for the valid showroom file. |
| P1: configured one lap necessarily produces a completed race | Existing game configuration, not a new feature. `GameModeDebug.cpp:315` loads NumCars directly; SetupRace129 preserves it and disables slots>=count143; `DebugCarScreen.lua:24` explicitly supports1; `RaceLogicCore.cpp:1915` has a single-car spawn path. Nothing is the LevelState default135; RaceBookLayer464 skips its Update/HUD. The original StreetRace evaluator instead calls RaceRulesManager Update92. | Source proves no minimum-two clamp and identifies the genuine ordered-lap finish chain below. Bare Debug/Nothing records laps without the active StreetRace finish evaluator. High for source mechanism; no actual driven finish. | The existing SetupRace RaceMode field can select the exported StreetRace enum while retaining the one-car Debug setup. Validate its real finish and teardown; original Debug postfinish UI falls into multiplayer screens and is not a proven offline results route. Multiplayer.AllowSingleRacer remains unchanged. |
| P1: rendering/input polls are simulation ticks | PC engine cadence and actual keyboard mapping. `GameControls.cpp:113/178` defines layout0 Q throttle/A brake/arrows steering; native Options selects PC SetupPlayerControls at `lua_Amax.cpp:3159`, steering axis alone at `RaceLogicCore.cpp:871`. Original audio DSP native path keeps24000Hz/128 samples (`dspspectralreverb.cpp:7667`), independently of output rate. | Focused original control remap and elapsed Tap/Hold tests pass; actual audio initialization progressed. No actual keyboard-driven car motion or lap yet. High for mappings/cadence, unvalidated driving integration. | Verify throttle, brake, both steering directions and release through real actions/physics. Preserve fixed simulation and render interpolation; release automation by elapsed time. Do not use A as the default-layout steering key or alter mixer rate to satisfy a console-only48000 assertion. |

The current intended entry is the original `Ui.NewLaunchMode=Debug` route.
`UISetup.lua:80` initializes the local debug profile and selects DebugScreen at106;
it is distinct from the earlier DebugRb/StartScreen/showroom route. Normal
DebugScreen Return calls LaunchGame (`DebugScreen.lua:270`) and the existing
SetupRace/LoadingGame path. No direct synthetic race state or fake finish is
required. The root's private configuration selects Brighton1, BMW/controller0,
NumCars1 and Laps1; Game.Debug.NumAdditionalCars defaults0. Configuration must be
confirmed in the live game, including RaceMode, not assumed from the XML alone.

The completion contract needs a separate check from vehicle count. Source tracing
finds the following original route, without changing any checkpoint, vehicle
position, simulation state, or finish flag:

- `GameLogic/GameModes/GameModeDebug.cpp:100–102` accepts the existing optional
  `RaceMode` member. `GameLogic/Lua/LuaVariableHelper.cpp:100–110/153–154` exports
  the named `UIEnums.RaceMode.StreetRace` value (`ui/UIDefines.h:13` names
  UIEnums). Thus `Amax.SetupRace({RaceMode=UIEnums.RaceMode.StreetRace})` is a
  source-supported configuration operation before normal DebugScreen LaunchGame,
  rather than a new race service. `lua_shared/source/LuaHelper.cpp:140–149`
  preserves existing values when a safe member is omitted. Consequently the
  subsequent normal DebugScreen setup, whose gadget table omits RaceMode, keeps
  this choice. This recommendation has not been executed or runtime validated.
- LoadingGame's existing Debug branch (`screens/Loading/LoadingGame.lua:155–158`)
  selects HUD and sends the normal RaceStarted message. StreetRace's inherited
  StartRace (`RaceModeBase.cpp:125–137`) starts the real vehicles and the mode's
  Update (`RaceModeStreetRace.cpp:73–92`) then evaluates the race rules.
- MAC095 refinement: the active vehicle lap scalar comes from AIR LogicState
  (`AI/AI_Amax.cpp:3123–3164`), shared by human vehicles before the AI-control
  conditional. Its original start/intermediate/finish-zone rules are in
  `ai/air/source/vehiclelogic/cvehiclelogic.cpp:896–1141`. The legacy RLC split
  counter/timing logs are separate and are not the direct finish-rule input.
  Neither independently emits the active StreetRace finish.
  `RaceRules/RaceRulesManager.cpp:388–406` requires
  currentLap greater than configured laps, then passes when all pass rules pass
  or no pass rules exist. Its last-survivor shortcut explicitly requires more
  than one vehicle (`:257`), so one vehicle does not immediately win. The normal
  pass message (`:430–438`) reaches StreetRace (`:335–345`), which emits the
  actual RaceFinished message. Nothing's base Update is empty and RaceBookLayer
  omits it, so merely setting Laps does not invoke this evaluator.
- This source supports a genuine one-car lap finish mechanism, not a complete
  original offline Debug results experience. HUD (`screens/Ingame/HUD.lua:762–786`)
  sends SinglePlayer to its results route but Debug into MpFinished;
  `screens/Multiplayer/Ingame/MpFinished.lua:11–18/56–79` invokes real network
  session/results methods. StreetRace SetupResults (`:556–617`) also calls
  GetEvent (`:284–299`), whose assertion expects a valid SP event, while a clear
  plain Debug LevelState starts with mSpEventId=-1 (`LevelState.cpp:184`). Do not
  claim those later screens are supported by the configuration, invoke fake
  results, or switch to SinglePlayer blindly: `GameModeSinglePlayer.cpp:49–53`
  rebuilds from an event and can change the intended car/track population.

The pass boundary is explicit: original PC authentication/import, selected
Brighton/BMW loading, countdown, the complete gameplay renderer and ordinary
keyboard motion/steering now have actual MAC172 evidence. Corrected ABI,
resource and ownership mechanisms also have focused evidence. A genuine lap
finish and successful full-game shutdown still require real-path evidence.
A successful build, an authored shader image, or an isolated reader does not
advance those latter milestones. Full-game UBSan linking also remains limited by
the missing original PortalBlockerCreate key functions documented in
[portal-blocker-audit.md](portal-blocker-audit.md); no invented definitions or
vptr-check suppression is proposed.

Other explicit limits stay outside the slice: [missing UITextBox widgets](ui-snapshot-gaps.md)
affect particular optional screens; save-envelope tests are not career-save proof;
completed service/discovery tests are not LAN play. No further network/features
or those tests are part of MAC088. Production follow-ups require a source-backed
proposal tied to one of the reachable contracts above.

### MAC093: named-platform registry allocation lifetime

The complete original-PAK graph fixture reached fourteen HDR/Bloom frames and
explicit GPU/graph teardown, then aborted at process exit. Bounded LLDB proved
`g_SmallBlockAllocator` had already run its destructor (`m_bInitialized=false`)
before the native recursive-mutex registry destroyed an expired weak control
block. That block was an interior allocation from a game small-block page;
late `operator delete` therefore fell through to libc with an invalid pointer.

Named native synchronization registries are platform infrastructure and may be
constructed before the game allocator. Their map buckets/nodes, long name keys,
and shared-state/control-block allocation now all use a libc-backed allocator.
Registries and expired entries are still destroyed normally; no static owner is
leaked, no allocator is disabled, and named in-process sharing remains intact.
Unnamed handles no longer construct an unused named registry.

The new `named_registry_allocator_test.cpp` includes the **original** game
AllocatorFunctions implementation/global new/delete and deliberately constructs
all three registry kinds before default-priority allocator destructor
registration. It checks shared mutex/event/semaphore behavior, multiple handle
lifetimes and no retained game blocks, then completes real static destruction.
Debug, Speed, Debug UBSan and Speed UBSan each pass this test and the existing
thread-behavior test. The same fixture privately linked to the previous native
thread implementation fails retained-block validation and aborts at static exit.
The actual archive-backed graph must separately exit successfully after relink;
these focused results do not establish a playable frame or race.

The active AIR lap/finish and original Debug postfinish/exit boundary are traced
further in [debug-slice-finish.md](debug-slice-finish.md) (MAC095).


### Bounded native pointer-bearing element storage audit (after MAC121)

The MAC120 trail failure exposed a runtime storage stride inherited from PC32:
`Dynamic/Particle/Manager.cpp` used eight bytes for `C_Particle_E`, while its
native pointer plus salt occupy sixteen. MAC121 owns the actual sizeof-based
stride and raw-slot initialization change. This record is transient CPU trail
state, not an Xt object, disk record or GPU vertex format.

A source-only companion inventory covered `SetElementSize`, raw byte copies,
`ElementVector`, particle/strip creation and scenegraph gather/result storage.
No second concrete pointer-stride mismatch was found in that bounded scope.
The other particle bucket sizes16–112 contain Float/U8 numeric state; the
original particle and super-effect constructors assert that `sizeof(ParticleType)`
matches the registered particle format size. Strip buckets16–80 likewise serve
numeric records; the actual Beam producer passes `sizeof(BeamParticle)`.
The pointer-bearing owners and render nodes sit outside those numeric arrays.

Particle removal copies the current `ElementVector::ElementSize`; trail removal
uses typed `C_Particle_E` assignment. Scenegraph visible nodes use typed Btl
vectors. Cached/flat-list gather results allocate/store with `sizeof(Result)`,
pointer arrays use `sizeof(pointer)` and particle vertex-build job parameters
use `sizeof(VertexBuildParams)`. The remaining U32 pointer-alignment operation
in `CachedRenderListNodeGather::FetchBits` is inside `PLATFORM_SPU`; the active
native branch returns the original pointer directly. Fixed-width UV writes,
index arrays and numeric vertex records must retain their disk/GPU widths;
this audit provides no reason to widen them.

This inventory is source evidence, not proof that every dynamic type has been
exercised. No game run, shared build or production edit was part of the companion
audit. It does not establish rendering, driving/contact, a completed lap or a
clean actual-game exit. MAC121's focused test and the next coherent native run
remain separate verification boundaries.


### MAC125: native physics container publication and import limits

The original `C_ImportScene::Import` currently has a single `return true`; native
scalar read failures throw. The discarded Main/Route bools in `PhysicsLevel.cpp`
therefore did not establish a reproducible false-return failure on their own.
No false-return importer implementation, new shape acceptance policy or fake
callback was introduced.

The native container loader now stages the original imported scenes in a pending
`C_PhysicsLevel`, initializes declared route-owner slots to null, checks each
original Import result, and publishes only a Main scene plus exactly the declared
route count. The Main requirement is grounded in the original LevelSceneManager
consumer and AmaxWorld's NOCOLMSH diagnostic. Route ownership follows the original
Header count. Header remains optional: the original default zero-route Main-only
container is retained and tested. The original raw Gravity fallback is also
retained. Windows parsing and PC field widths/shape records are unchanged.

Native structural failure returns false without publishing pending scenes;
exceptional reads unwind the same pending owner. The original SDK release policy
handles imported scenes, and caller file-version metadata is restored on all
exits. Successful prior ownership stays published across a failed retry. Original
block-header sizes are checked against their parent extent before C_FileBlock
owns a header, avoiding an invalid end seek from its destructor. Negative or
impossible route counts, duplicate Header/Main/Routes owners, excess/missing
routes and absent Main cannot report a complete native container.

`physics_level_failure_test.cpp` uses the original block writer, original Gravity
importer, authored empty-scene terminators and optional private Brighton physics
bytes. Its failures exercise real parser/control paths, not a mocked Import.
The private actual payload is never written to tracked source. These checks are
container publication/ownership evidence only. Shape-level assertions, unknown
shape-type handling and ownership of descriptors not yet attached to an imported
scene remain as recovered in the original Import implementation. This change
does not make the whole parser safe for arbitrary malformed shape streams and
does not prove physics simulation, contact, driving or a race finish.

MAC125 verification: Debug, Speed, DebugUBSan and SpeedUBSan private reader runs
all pass, including the original complete Brighton container (four routes) and
Main truncation. The same authored fixture with the previous container loader
fails its malformed-success assertion. Root MAC127 integrated Debug also passes
both reader checks within its24-check strict-Metal batch. These are load/status/
ownership results; actual MAC127 gameplay and exit remain separate root proof.
