# macOS phase board

Phase: local `macos` branch, derived from upstream `1965facd3d4860447a043b1e0fea4d55c086d1f5`. Authorized by the user's 26 October 2026 request for a native Mac implementation based on BlurRebuilt. The inherited Windows branches and their delivery rules remain unchanged.

Current acceptance (MAC183–199): the native Brighton/BMW keyboard slice has
completed a real lap-qualified race and exited0. Normal Master profile creation,
reload, intro movies/autosave and first unlocked event selection work. Frontend shutdown194/196 exposed owned FileStreams;195/197 repair their
worker/graph/stream/file lifecycle and static mixer recreation. Actual198
Debug full frontend/garage shutdown now exits0 through the elapsed-time guard. Normal career
results, end-to-end menu exit, real LAN and distribution remain unverified.
The rows below retain earlier implementation milestones and historical blockers;
use the current acceptance paragraph and latest numbered entries for status.

| ID | Task | Owner | Status | Expected files | Blocked on |
| --- | --- | --- | --- | --- | --- |
| MAC001 | Establish native arm64 build, engine core ABI, source-compatible archive reader, Metal device/render/readback probe and explicit bootstrap application | Codex /root | done (bootstrap milestone only) | CMakeLists.txt; README.md; README.macos.md; cmake/macos.cmake; core/Core.h; engine logicalFs macos adapter; display Metal bootstrap; tools/macos; docs/macos | None for this milestone; full game remains unimplemented |
| MAC002 | Validate real retail archives and resource layouts | Codex /root + agents | secure PAK/UI textures/boot movies verified; scene work pending | Original C_PakFs, SHA1/RSA, Flask, Display bundler/movie | Actual level/race loading |
| MAC003 | Actual Display/Display2 Metal renderer and game effects/shaders | Codex /root + /root/metal_occlusion | engine GPU checks pass; real UI integration in progress | Native Metal, original renderer/FX/shaders | Actual completed game frame/race |
| MAC004 | Native platform services and complete original game build | Codex /root | Debug/Speed actual game linked; integration continues | Jam-derived engine/game sources; Cocoa/Metal/audio/input/network; native tools/CMake | Actual menu/race/save/LAN; explicit full-game UBSan source RTTI gap |
| MAC005 | Verify actual startup, race, save/load and game LAN | Codex /root + agents | in progress | Actual BlurNativeGame and ignored original data/evidence | UI render-buffer PC depth-format integration; no completed game frame yet |
| MAC006 | Compile the actual engine's allocator/endian source on macOS and verify allocation, size queries, byte order and active debug assertions without retail assets | Codex /root | done (selected core object files only) | CoreMemoryAligned.cpp; CoreMemoryNonTracked.cpp; endian.cpp; native allocation helper; cmake/macos.cmake; tools/macos/core_runtime_test.cpp; docs/macos; README.macos.md | None for source-only verification; full game remains tracked separately |
| MAC007 | Implement real native GPU occlusion counting and original query wrapper | Codex /root/metal_occlusion | in progress | Display2 Metal context/native internals; new native query headers/implementation; upper occlusion selection; focused GPU fixture | Root integrates Jam/CMake and full matrix |
| MAC008 | Port original Gravity/RacingPhysics sources with faithful math and lifetime behavior | Codex /root/physics_port | in progress | Physics/gravity and Physics/RacingPhysics; focused fixtures | Root owns shared pointer-width helpers/build integration |
| MAC009 | Port original dynamic particles and crowd bodies; check concurrent native pools | Codex /root/dynamic_port | in progress | Graphics/dynamic; GameSys/crowd_system; focused pool fixtures | Root owns common iterator/scheduler helpers/build integration |
| MAC010 | Port original game resource and actor/model utility compilation | Codex /root/physics_port | in progress | gamesys game_resource, actor_system, animated_model, game_context, lod, resource_utils | Root owns source-list/build integration |

| MAC011 | Port original UI/Lua and UTF-16/save services | Codex /root/physics_port | in progress | UI, Lua helper/vendor integration, Core UTF-16, authored runtime fixtures | Root integrates source lists and native Movie dependency |
| MAC012 | Native region copies and original photo accumulation | Codex /root/metal_occlusion | in progress | Display2 GPU copies, Render photo accumulation, GPU fixture | Root integration matrix |
| MAC013 | Port original Amax startup/game application bodies | Codex /root/dynamic_port | in progress | Amax Jam sources, scoped dependent headers, text fixture | Root links remaining original game modules |
| MAC014 | Native movie decoding/playback with public FFmpeg/SDL | Codex /root/metal_occlusion | in progress | graphics/movie, native backend and authored fixtures | Root dependency/source integration |

| MAC015 | Port original Amax UI | Codex /root/physics_port | in progress | Amax/library/Ui sources and headers | Root source/build linkage |
| MAC016 | Native multiple render targets and attachment transitions | Codex /root/metal_occlusion | in progress | Native Metal lower/upper context/resources, GPU fixture | Root Deferred manager integration |
| MAC017 | Port original Amax game logic229 | Codex /root/dynamic_port | in progress | Amax/game/GameLogic source/header closure | Root shared/platform modules |
| MAC018 | Integrate original game-support libraries and actual game link closure | Codex /root | in progress | native source generator/CMake; vehicle/shared actors/debug/render support; native main later | Parallel game/UI/backend ports |

| MAC019 | Native MSAA resolve and recovered depth-bias behavior | Codex /root/metal_occlusion | in progress | Metal resources/context/device/capabilities and GPU fixtures | Integrated regression matrix |
| MAC020 | Original GameAudio/RaceBook/Kanzen and full audio-system/bank services | Codex /root/physics_port | in progress | GameAudio19, RaceBook7, Kanzen4, native original Audio queues/loader, fixtures | Root archive/test integration |
| MAC021 | Native original network/Demonware transports and wire ABI | Codex /root/dynamic_port | in progress | Network25, AmaxNetwork22, Unix vendor dependencies, Darwin services, crypto and loopback fixtures | Parent source/dependency integration |

| MAC022 | Native existing vehicle/HUD/wheel/light render interpolation | Codex /root/physics_port | source/fixture complete; parent rebuild pending | VehicleActor, UI anchors/panel/trails, actual world fixture | Integrated game scenes remain unverified |
| MAC023 | Real Display bundler and cube/3D file-loader closure | Codex /root/metal_occlusion | in progress | shared Display bundler/readers, native DDS loader/GPU uploads | Actual game-link integration |
| MAC024 | Native original PC Flask/Xt and RenderingData asset ABI | Codex /root/physics_port | in progress | Xt2 Flask/types, RenderingData archive, authored pack fixture | Parent integration |

| MAC025 | Original Amax native startup/resource/offline lifecycle | Codex /root/dynamic_port | source complete; runtime integration in progress | Main, GameApp, BootLayer, GameAppResources | Original assets and full-game validation |
| MAC026 | Native Movie idle/deferred clip and named preload lifecycle | Codex /root/metal_occlusion | in progress | Movie.cpp, Movie.h, MacMovie.cpp, movie_test.cpp | Actual Amax startup exposed empty-clip renderer creation |
| MAC027 | Native game controller/subscriber ownership during shutdown | Codex /root/physics_port | in progress | Amax GameApp destructor and actual automation handlers | Actual failed-start teardown exposed nine live subscriber lists |

The inherited `tools/agent_context.py context` was attempted and rejects this newly introduced phase name. This board and LEDGER.md are the macOS task records. No inherited Windows task is claimed or modified.

| MAC028 | Original optimized physics/animation inline link closure | Codex /root/physics_port | source/private matrix complete | EventLogger.cpp, SPUScene.cpp | Full game runtime |
| MAC029 | Original exporter compressed texture mip-tail layout | Codex /root/metal_occlusion | real-data Debug/UBSan verified | ResourceBundleSingle.cpp, display_file_test.cpp | Integrated matrix |
| MAC030 | Native original PAK SHA1/RSA authentication | Codex /root/physics_port | real-data secure mounts verified | PakSha1, RSASecurity, pak_auth fixtures | Integrated matrix |
| MAC031 | Opt-in actual completed-game-frame capture and elapsed shutdown | Codex /root/metal_occlusion + /root | integrated; fixtures pending parent run | RunDiagnostics, NativeMain, ApplicationMac, CMake | Actual frame after startup blocker |
| MAC032 | Actual game build/shader resources and complete private runtime data | Codex /root | linked; real startup in progress | native CMake, shader compiler/paths, NativeMain; ignored runtime data | Actual menu/race/save/LAN |
| MAC033 | UI/FX teardown assertion ownership audit | Codex /root/physics_port | audit complete | No speculative UI edits | Root native PC FX preload ownership guard |
| MAC034 | Existing Artery Automation v2 native test client | Codex /root/dynamic_port | in progress | tools/macos/automation_* | Parent integration |
| MAC035 | Native original Lua Tap/RaceState automation hooks | Codex /root/physics_port | in progress | NativeInput, UiManager, input fixture | Parent game build |
| MAC036 | Actual game native presentation interval | Codex /root/metal_occlusion | in progress | Native Display interval mapping/fixture | Actual startup request |
| MAC037 | Quiesce original async layer creation before application teardown | Codex /root | in progress | LayerManager, ApplicationBase, framework fixture | Failed actual Run while UI/audio creation pending |

| MAC038 | Audit absent PortalBlocker creator RTTI/key-function source | Codex /root/physics_port | in progress | Source/Jam audit first | Actual full-game UBSan link |
| MAC039 | Original Artery service cleanup after automation disconnect | Codex /root/dynamic_port | in progress | Native Artery shutdown/poll boundary | Real Automation fixture final join |

| MAC040 | Native pool slot alignment for object and free-list pointer | Codex /root/physics_port | in progress | Btl allocator pool header, pool fixture | Actual UBSan scene misalignment |
| MAC041 | Native manual folder selection and remembered game-data root | Codex /root/dynamic_port | in progress | NativeMain.mm only | Parent visibility wiring after game startup |

| MAC042 | Actual minimized game/Automation startup probe | Codex /root/dynamic_port | completed diagnostic only | Ignored mac036 probe/log/stack/status | Revealed MAC043, no frame/race |
| MAC043 | Original PC UI depth-stencil attachment selection | Codex /root/metal_occlusion | source ready; actual rebuild pending | UiRenderBuffer.cpp, original renderer fixture | Native wrongly selected console D24FS8 |
| MAC044 | Audit active startup/UI console branches against PC-data behavior | Codex /root/dynamic_port | in progress | Read-only scoped source audit first | Evidence before changes |
| MAC045 | Audit unregistered UITextBox Flask schema warning | Codex /root/physics_port | in progress | Existing UI schema/registration audit first | Determine active use before changes |

| MAC046 | Native transient light-handle lifetime after pool destruction/reuse | Codex /root/physics_port | source ready; fixtures in progress | SceneGraph handle/light/drop-shadow headers; scene fixture | Actual UBSan stale-handle vptr access |

| MAC047 | Actual original game UI/debug-menu/race probe after PC path fixes | Codex /root/dynamic_port | in progress | Fresh ignored app snapshot and actual game/Automation evidence | No successful menu/race claimed yet |

| MAC048 | Original PC spectral-reverb dry path on native mixer cadence | Codex /root/physics_port | source ready; matrix in progress | dspspectralreverb.cpp, actual audio-core fixture | Original console48000 assertion on PC24000/128 |
| MAC049 | Original platform identity for native PC asset validation | Codex /root/metal_occlusion | source ready; actual-resource checks in progress | system misc Platform.cpp, platform/resource fixture | Native fell off GetPlatform without return |

| MAC050 | Audit native nonvoid fallthrough warnings and repair active diagnostic platform label | Codex /root/dynamic_port | source/four-config syntax complete | DebugLayer.cpp; ignored warning inventory | Actual next startup probe |

| MAC051 | Actual original Showroom load after model/audio identity fixes | Codex /root/dynamic_port | completed diagnostic only | Ignored actual probe/log/stack | Depth target dimensions/format mismatch, no completed frame |
| MAC052 | Original larger-depth attachment contract and actual mismatch diagnostics | Codex /root/metal_occlusion | in progress | MetalContext.mm; genuine GPU regression fixture | Need exact actual attachment sizes/formats |
| MAC053 | Native registration of original Amax.GetScreenResolution | Codex /root/dynamic_port | source/four syntax complete | lua_Amax.cpp | Actual GUIBank nil function removed in source |
| MAC054 | Original PC Showroom physics deserialization audit/runtime fixture | Codex /root/physics_port | in progress | Gravity/gamephysics actual reader and optional real-data fixture | Source-authoritative file ABI |

| MAC055 | Actual shared-depth mismatch sizes and Lua-resolution verification | Codex /root/dynamic_port | completed diagnostic only | Ignored actual probe/log/stack/status | Revealed valid larger-depth case; no frame |
| MAC057 | Actual original UI draw under Metal Validation after depth fix | Codex /root/dynamic_port | completed diagnostic only | Ignored actual probe/crash/status | Fragment uniform404/416 ABI mismatch |
| MAC058 | Compiled Metal reflection uniform-buffer ABI sizes/alignment | Codex /root/metal_occlusion | in progress | MetalContext pipeline reflection/upload; exact GPU fixture | Actual UI fragment buffer404logical/416required |

| MAC059 | Native bounded elapsed-time key hold for actual driving verification | Codex /root/physics_port | in progress | NativeInput, UiManager, original mapping fixture | Hold throttle while tapping steering, no synthetic race state |

| MAC060 | Actual UI draw after compiled-uniform sizing fix | Codex /root/dynamic_port | completed diagnostic only | Ignored actual probe/log/status | Original blend Default/Unknown sentinel rejected; no frame |
| MAC061 | Native original PC Default/Unknown blend sentinel semantics | Codex /root/metal_occlusion | source ready; real GPU fixture in progress | MetalGameStates.cpp, Context_Base.cpp, focused fixture | Actual Simple2D invalidation call |

| MAC062 | Actual UI/world progression after blend fix | Codex /root/dynamic_port | completed diagnostic only | Ignored actual crash/log/status | Reduced/PostProcess creation failure then null bloom |
| MAC063 | Original PC render-target depth and null-address budget semantics | Codex /root/metal_occlusion | source ready; real manager fixture in progress | ReducedRender Manager.cpp, MetalRenderTextures.mm | Native console D24FS8 and budget wrongly treated as external memory |

| MAC064 | Actual run after original render-target allocation fixes | Codex /root/dynamic_port | completed diagnostic only | Ignored actual probe/log | Invalid Simple2D pre-transform index; no completed frame |
| MAC065 | Capture primary game assertion before unwinding | Codex /root/dynamic_port | completed diagnostic | Bounded LLDB actual app runs | Proven Simple2D matrix index and deferred-lighting Default address request |
| MAC066 | Native LP64 complete UI sprite clearing | Codex /root/physics_port | in progress | Simple2d.inl; adjacent poisoned-sprite fixture | Actual index0x1111 from partial128-byte clearing of136-byte sprites |
| MAC067 | Native original default texture-address contract | Codex /root/metal_occlusion | in progress | DisplayToDisplay2 mapping; GPU address fixture | Actual deferred lighting requests Default |
| MAC068 | Audit actual menu/race/save/LAN verification sequence | Codex /root/dynamic_port | in progress | Read-only original source/Lua audit | No synthetic state transitions |

| MAC069 | Actual game progression after sprite and sampler fixes | Codex /root | completed diagnostic only | Private actual game snapshot/log | Next failure missing/dimension shader texture, no complete frame |
| MAC070 | Original native PC compressed-profile save format and ownership | Codex /root/dynamic_port | in progress | SaveInfoInterfaces.inl; focused real save boundary fixture | Actual career save/load remains unverified |
| MAC071 | Audit actual native Brighton race load and vehicle/input PC branches | Codex /root/physics_port | in progress | Read-only source and prior real logs | Avoid speculative fixes |
| MAC072 | Exact actual shader texture binding diagnosis | Codex /root + /root/metal_occlusion | in progress | Bounded before-unwind LLDB, renderer reflection audit | Must identify used binding and actual source draw |

| MAC073 | Base-owned rollback when native Artery startup fails | Codex /root | source ready | CoreSystems.cpp native Initialise | Actual LLDB immediate retry entered derived Shutdown before resource manager creation |

| MAC075 | Exact-byte unaligned auto-endian scalar serialization | Codex /root | source ready | BinarySerialiseAutoEndian.h, scalar boundary regression | Actual Amax save envelope UBSan U32 store at byte1 |

| MAC074 | Original PC keyboard/options/steering/fire input branches on Mac | Codex /root/physics_port | source/private matrix complete; integrated build pending | lua_Amax.cpp, RaceLogicCore.cpp, VLC_Pickups.cpp, real binding fixture | Does not yet prove driving |
| MAC076 | Actual game run after motion-blur/depth/input/save fixes | Codex /root/dynamic_port | in progress | Fresh private snapshot of root delivered actual app | Normal observed-screen inputs only; capture70, quit100, outer150 |
| MAC077 | Audit static ReplayDirector destruction after game deletion | Codex /root/physics_port | completed read-only | Original Main/GameApp/ReplayDirector lifecycle | Original PC TerminateProcess bypasses destructors; native returns |
| MAC078 | Native replay ownership/subscription cleanup before game deletion | Codex /root/physics_port | in progress | ReplayDirector and GameApp only | Balance actual resources and live subscriber owners; no static game dereference |

| MAC079 | Native dependency-aware game layer shutdown | Codex /root/dynamic_port | in progress | GameApp OnShutdown; LayerManager native lifecycle if required | Actual BootLayer/system pointer crash after caught Run failure |
| MAC080 | Actual motion-blur shader creation versus binding diagnosis | Codex /root/physics_port | in progress | Bounded LLDB frozenMAC076app | Original Bloom L8 texture rejects and manager short-circuits later creates |

| MAC081 | Actual full effect graph after original L8 DDS support | Codex /root | build in progress | Native DDS/Utils, original PostProcess/GameWorld failure propagation, root game integration | MAC080 confirms all11 blur shader backings and2textures null, effect Create skipped |

| MAC082 | Audit original concurrent LAN instance configuration | Codex /root/physics_port | completed read-only | Artery/discovery/game UDP/profile source | Game UDP already selects free ports; Artery/discovery binds collide |
| MAC083 | Explicit isolated native service/discovery instance settings | Codex /root/physics_port | in progress | Original Artery and LAN discovery native configuration/tests | Preserve default ports, actual message/security/identity semantics |

| MAC084 | Actual native menu/debug/race probe after L8 and teardown fixes | Codex /root/dynamic_port | waiting root build | Latest actual game snapshot, observed input, one completed-frame capture | Root Debug3+Speed6 builds |
| MAC085 | Native global cache worker/references shutdown ordering | Codex /root/dynamic_port | source/private probes complete | GameApp wait/release, game Main | Join caching before layers, release refs after callbacks while device/FX live |
| MAC086 | Reject failed layer creation instead of promoting it to Ready | Codex /root | pending implementation after current Debug build | LayerManager native failure state and regression | Observed failure propagation must reach main run safely |

2026-10-27 user scope/priorities reset: first audit recurring platform/format, LP64 and ownership assumptions together; validate the real archive/startup/full effect graph; deliver one Brighton1 track, one BMW, keyboard, completed race and clean exit. Defer normal menu/save/LAN/distribution expansion. Completed LAN source stays local, no further LAN work.

| MAC087 | Actual HDR read/write attachment alias evidence | Codex /root/dynamic_port | completed diagnostic | FrozenMAC084 before-unwind LLDB | HDR luminance slot1 samples same1x1 R32F colour0; no completed frame |
| MAC088 | Grouped reachable OS/PC-format/LP64/ownership assumption audit | Codex /root/physics_port | in progress | docs/macos/slice-assumptions.md; source evidence first | Actual one-track/car startup and race slice |
| MAC089 | Actual archive/startup/vehicle initialization failure and ownership audit | Codex /root/dynamic_port | in progress | docs/macos/slice-initialization.md | Required resources fail before Ready/render |
| MAC090 | Complete game renderer/effect graph and resource ownership audit/validation | Codex /root/metal_occlusion | in progress | Real PAK-backed game graph, original PC renderer contract | Class-level alias snapshot/source-format/init-status fixes |

| MAC091 | Transactional GameWorld acquisition and teardown | Codex /root/dynamic_port | source/focused Debug verified | GameWorld/View ownership, world_transaction_test.cpp | Actual pre-physics bool/throw rollback exits0 with original Main bootstrap object linked first; later-stage rollback/gameplay unverified |
| MAC092 | Drain cache owners while their GPU dependency is alive | Codex /root/physics_port | in progress | Resource Manager, FX preload ownership, ApplicationBase | Original eviction/release protocol, no forced live-reference frees |
| MAC093 | Native platform registry versus game allocator lifetime | Codex /root | verified all4 + actual graph | NativeThreads registry allocation domain, actual allocator fixture | LLDB proves registry weak controlblocks free after global allocator destructor |

| MAC094 | Integrate grouped renderer/cache/layer fixes and probe actual Brighton/BMW startup | Codex /root | Debug game build in progress | CMake fixture integration and private frozen actual app | Complete real graph now14frames+staticexit0; actual track/car/frame/race stillpending |

| MAC096 | Actual direct-debug GPU frame and ID roundtrip diagnosis | Codex /root | captured actual menu; no clean exit | Frozen MAC094 app, real Lua data/UI queries | Route U32 positive4112414174 vs signedselector-182553122 prevents thumbnail lookup |
| MAC097 | Original PC32 Lua integer and high-bit identifier roundtrip | Codex /root/physics_port | audit/implementation | Native Lua ABI and relevant numeric-ID boundaries | Actual confirmed Lua/table/selector mismatch; no script bypass |
| MAC098 | Retained GPU owner diagnosis after clean debug-menu shutdown | Codex /root | bounded LLDB running | Frozen MAC094 app only | MAC092 cache drains pass but actual menu-only exit still retains resources |
| MAC099 | Enforce actual original Lua-command message capacity | Codex /root | implementing | native automation client/test; UI command constructor safe failed state; private harness | Long diagnostic command exceeded actual128-byte UI message; invalid object contained uninitialized command |

| MAC100 | UI GPU acquisition readiness and paired lifetime | Codex /root | implementing | UiLayer.cpp, LightPaintSystem.{cpp,h}, focused actual UI callback fixture | MAC098 retained exactly2VBs, symbol addresses equal gSparkleBuffers[0/1]; UI Create allocates but only race transition destroys |

| MAC101 | Actual game integration after world, PC32-ID, UI GPU lifetime fixes | Codex /root | initial Debug build passed; final incremental pending | Full actual game + focused CMake closure | Strict actual track/car/prepass probe next |
| MAC102 | Global game allocator constructor/destructor registration order | Codex /root | implementing | AllocatorFunctions.inl, actual allocator static-owner regression | World fixture ray-group CPU vector retains capacity until static exit after allocator; fix class lifetime order |
| MAC103 | Required UI texture bundle readiness and constructor ownership | Codex /root/physics_port | read-only audit until snapshot | UiManager.{cpp,h}, coordinate UiLayer readiness | Required common/font/frontend/game bundles currently ignore invalidindex |

| MAC105 | Vehicle damage volume/pool/model required-initialization class | Codex /root/metal_occlusion | source frozen; focused GPU tests | DamageLayer.cpp, VehicleModel.cpp | Actual BMW CreateVolume null single-buffer pointer; preserve real material encoding |
| MAC106 | Simple2D required GPU/status and exception-safe ownership | Codex /root/metal_occlusion | audit/implementation | Simple2d/renderer source, coordinate MAC103 | Manual tracker push can bypass pop on GPU exception |
| MAC107 | UI region reader returns initialized source default on native | Codex /root | four-config syntax + Debug/Speed default regression pass | UILocaliser.cpp native-only reader | Existing ctor/Determine already explicitly default toEurope; native reader returned uninitialized enum |

| MAC108 | Coherent actual-game integration and one-track/car probe | Codex /root | fixtures pass; actual world/race running, physics stack failure | cmake/macos.cmake fixture/bootstrap order; UiLayer message RAII; actual frozen app | Required UI/original damage F32/world rollback verified together before natural race |

| MAC109 | Late Darwin malloc deallocation versus retired game allocator metadata | Codex /root | verified four configs + integrated Debug | AllocatorFunctions.inl; real allocator regression | LLDB proves libc++ late string ptr is malloc_size32, not SBA; preserve early-bootstrap game owner ordering |

| MAC110 | Read-only actual vehicle/race/road telemetry for native keyboard verification | Codex /root/dynamic_port | source/four-config compile verified; live validation pending | UiManager.cpp native automation helper/registration; keyboard-race-telemetry.md | Actual published expected car/AIR fields only; no progress/control mutation; root game rebuild/probe |

| MAC111 | Original PC commit/reserve contract for native CPU worker stacks | Codex /root/physics_port | source frozen; four-config runtime + pre-fix differential pass | NativeThreads.cpp; thread_stack_test.cpp | Actual MAC108 JQ Worker0 guard fault entering nonrecursive CollideMesh; PC64KiB request commits within default1MiB reserve |

| MAC112 | Coherent original PC quality/view/prepass policy on native | Codex /root/metal_occlusion + root host support | implementing/probing | GameWorld preset-only, ViewManager/View/Main/Mirror/Reflection/CubeMap; lower TextureRender/view format propagation; root ApplicationMac2/GameApp2 + application fixture | Existing default2 preset chain, complete enabled prepass remains actual-game gate |

| MAC114 | Actual Brighton/BMW enabled full graph and keyboard slice | Codex /root | preparing real frozen app probe | CMake integration of stack/view/AIR tests; native actual app harness/real capture | All14 Debug integrated checks passed; real frame/motion/natural finish/exit required |

| MAC113 | Fail required native race AIR initialization before readiness; audit real Debug input/finish flow | Codex /root/dynamic_port | source/eight object probes + real callback Debug exit0 verified | AmaxWorldLayer.cpp/RaceLogicCore.cpp LoadFailed handlers; air_startup_failure_test.cpp | Actual natural lap/finish remains pending; fixture does not import AIR data or run a race |

| MAC116 | Native scenegraph/audio logical worker ownership | Codex /root/dynamic_port | integrated Debug CTest17/17 + source12/12 probes + private five Debug exit0 runs; frozen | SceneGraphMaxContexts.h, SceneGraph.cpp, RenderTask.h; audio ObjectManager.h/cpp; worker_storage_test.cpp | Native scheduler main0 plus workers1..7 requires eight independent contexts/scratch regions; preserve policies/legacy capacities |

| MAC117 | Coherent eight-slot worker ownership actual-game integration | Codex /root | Debug full game rebuilt; fixtures integrating | CMake particle/scene/audio storage targets; actual frozen app | Real particle high-worker failure addressed across all reachable per-worker ownership domains; enabled first frame/motion/finish/exit pending |

| MAC118 | Native scheduler completion-event retirement lifetime | Codex /root/dynamic_port | integrated Debug CTest17/17 + source8/8 probes + scheduler/stack all4 runtime8/8 + worker Debug5/5 exit0; frozen | scheduler/jobqueue.cpp, jobqueue_kernel.cpp; worker_storage_test.cpp | Flush observed zero before final Event::Signal and DestroyGroup destroyed mutex; pair completion/retirement under existing pool lock, preserve nonnative flow |

| MAC119 | Semantic vector storage exchange preserves derived native tail fields | Codex /root/metal_occlusion | final source frozen; all4 focused runtime exit0; final full-game integration pending | btl/Containers/vector.h; vector_swap_test.cpp | Actual queue4→186 across radix sort: raw24-byte base swap overwrote counter offset20; member/storage exchange preserves layout; staged-live static reset covers constructor failure |

| MAC122 | Coherent final container/trail storage integration and actual Brighton slice | Codex /root | coherent Debug build +20/20 focused PASS; actual frame blocked by MAC124 FX ownership | cmake/macos.cmake, private build/probe, macOS evidence docs | Final MAC119 header + MAC121 transient trail stride/initialization; full game startup/render/motion/natural finish/exit gates |

| MAC120 | Actual game after primary semantic vector fix | Codex /root | failed actual probe; boundary documented | frozen actual app, mac120-game.log/status/crash | Full real startup now reaches trail update; native16-byte record stored at PC8-byte stride, MAC121 owns fix; no gameplay frame/finish/exit |

| MAC121 | Native transient particle trail reference stride and complete raw-slot initialization | Codex /root/metal_occlusion | source/fixture frozen; Debug integrated + Speed/DebugUBSan private exit0; SpeedUBSan stale Portal archive blocks pre-main | dynamic particle Manager.cpp, SuperSpawn.cpp; particle_trail_test.cpp | Actual GetSalt this0xcdcdcdcd00000000: native pointer+salt16 bytes stored at original PC8-byte stride; no disk/GPU record changes |

| MAC125 | Required level physics Import result/ownership propagation | Codex /root/physics_port | source frozen; four-config reader + integrated Debug PASS | PhysicsLevel.cpp/.h if required; focused original-reader fixture | Main/route Import bool ignored by container loader; preserve PC format |

| MAC123 | Preserve required renderer and explicit actor-list startup completion failures | Codex /root/dynamic_port | integrated Debug CTest24/24 + four-config source8/8 + actual fixture/static exit0; frozen | AmaxWorldLayer.cpp, GameActorList.cpp, NativeActorCreationCompletion.h; startup_completion_test.cpp | Preserve original Continue policy; false/throw/cancellation never publishes success; native main first-error and pre-instantiation renderer rollback |

| MAC124 | Actual GameApp whole-render FX ownership on native | Codex /root/metal_occlusion | frozen; all4 probes + Debug strictMetal exit0; root24/24 integrated pass | GameApp.cpp; game_render_ownership_test.cpp | Boot false flag survives Debug launch without frontend world; native RAII spans original whole OnRender, serializer exclusion + actual sampler GPU draw/throw/retry verified |

| MAC126 | Original PC blob channel/shader/stencil/cull lifecycle policy on native | Codex /root/metal_occlusion | frozen; all4 source/fixture probes + real blob GPU/PAK Debug exit0; actual game pending | BlobShadows.h/cpp, MetalGameDevice.cpp; blob_shadows_test.cpp | Original PC channel/shader/stencil/cull/unbind chain and coherent reverse-depth default; preserve PS3-only GCM controls |

| MAC127 | Coherent startup/render-policy class integration and actual Brighton slice | Codex /root | completed diagnosis: deferred unattached depth; superseded by131/139 | cmake/macos.cmake, production Debug binary, focused/in-game evidence | MAC123 explicit completion,124 wholeFX renderowner,125 physics transaction,126 PCblob chain; actualframe/input/naturalfinish/exit stillrequired |

| MAC128 | PC attachment-conditioned native depth/stencil draw contract | Codex /root/physics_port + root actual diagnosis | source frozen; four-config GPU runtime + pre-fix differential PASS | MetalContext.mm; depth_attachment_test.cpp; depth-attachment-contract.md | Actual MAC129 deferred spotlight-volume pass has requested depth with null attachment; preserve requests, derive effective tests, retain real attachment/clear validation; actual game rerun remains separate |

| MAC130 | Scene view registration/materialization/removal lifetime audit | Codex /root/dynamic_port | source frozen;16/16 source probes + actual Debug lifecycle fixture/static exit0 | SceneGraph RendererInterface/SceneGraph; Dynamic View acquired groups; view_slots_test.cpp | Live11/10 mismatch confirmed; reserved slots/stable-index materialization/matching permutation + rollback; coherent integration and actual game exit pending |

| MAC131 | Coherent PC depth/attachment/shadow integration and real playable slice | Codex /root | 39/41 checks; actual mirror sampler bias failure; superseded by139 | cmake/macos.cmake, native Debug actualgame+GPU regressions; privateprobe | Final126nativePCdepthdefault +128effectiveattachmentstate; no frame/input/finish/cleanexitclaim fromfocusedchecks |

| MAC133 | Native framework BasicRender pixel regression after PC depth policy | Codex /root/metal_occlusion | fixture frozen; all4 probes and full Debug strictMetal exit0 | application_test.mm only | Authored final backbuffer draw now initializes its own depth via actual PC clear; no production depth disable or BasicRender policy change |

| MAC134 | Original PC sampler mip LOD bias through native shader ABI | Codex /root/metal_occlusion | frozen; all4 GPU strictMetal exit0 and693/693 real loader PASS; coherent game pending | native Metal shader/context and shared source integration; legacy profile correction; sampler_lod_test | Actual MirrorView pixel2 encoded16; original S8/16 semantics, explicit clamps and per-stage ABI; precise anisotropic-gradient/provenance limits |

| MAC136 | Final native game message-subscription ownership audit | Codex /root/dynamic_port | source frozen;24/24 probes + actual Debug owner fixture/static exit0 | Messaging single/multi expected-pool APIs; RaceBook/world/race/AIR lifecycle; subscription_owner_test.cpp | Actual MAC137 maps all172 subscriptions/27 types; entered owner retirement preserves borrowed pools/per-race behavior; coherent actual unwind pending; Audio separately owned |

| MAC139 | Coherent sampler/view/message/audio owner integration and actual Brighton slice | Codex /root | build +45/45 PASS; actual vertex texture dimension failure/late UI queue cleanup crash | Jam MetalSamplerLod, legacy shader generation, native fixture CMake, actual Debug game + strictMetal checks | Frozen130/133/134/136/138; actual GPU frame, keyboard motion, natural finish and clean exit remain required |

| MAC138 | Explicit native game-audio subsystem ownership and loader retirement | Codex /root/physics_port | source frozen; all-four source probes, private Debug/Speed + integrated Debug lifecycle PASS | GameAudioLayer.cpp; InterfaceController.h/cpp; Music/UI subsystem h/cpp; audio_subsystem_ownership_test.cpp | Preserve borrowed Audition objects; join loaders, destroy eleven explicit game owners with live dependencies/System last, balance15 subscriptions; real fresh-game exit and loaded-bank behavior remain separate |

| MAC140 | Required FX default texture initialization and publication transaction | Codex /root/metal_occlusion | frozen; all4 source probes + actual Debug/DebugUBSan FX failure/retry/GPU exit0; integration pending | FX System.cpp/default owner lifecycle, Fx.cpp Initialise tracker scope, fx_test.cpp | Actual native startup source audit found ignored required3D creation/map failure; no incomplete default publication, real failure/retry and GPU maps required |

| MAC144 | Selected postprocess resize/creation and paint failure propagation | Codex /root/metal_occlusion | frozen; all16 probes + real PAK graph false/throw/retry strictMetal Debug exit0; integration pending | PostProcessLayer.cpp, render postprocess Manager.cpp, PaintEditor.cpp, selected graph fixture | Required resize bool ignored, throwing volatile tracker scope, unmatched paint failure pop; preserve actual rollback and render readiness |

| MAC143 | Reflected vertex-sampler dimensions preserve original PC default policy | Codex /root/physics_port | source/fixture frozen; all-four probes + Debug strictMetal mixed-FX GPU PASS | MetalShaders.h/.mm, FxContext.cpp; vertex_fx_texture_test.cpp | Actual shader589 expects3D at0/2D at1; native blanket substitution binds default3D to1; strict explicit mismatches remain rejected |

| MAC145 | Coherent required-resource/default-dimension/deferred-UI integration and actual slice | Codex /root | build/47 checks PASS; actual shared-beforepass error diagnosed146, shutdownowner147 | native CMake47checks + actualgame; frozen140/142/143/144 production | Actualgamepass must show realGPUtrack/car, keyboard motion, natural ordered lap/finish, clean normalexit; prior diagnostics remain qualified |

| MAC142 | Deferred UI message queue final/failed owner retirement | Codex /root/dynamic_port | source/fixture frozen;12/12 probes + standard-link Debug ownership/payload/explicit-owner exit0 | Mercury ButtonMessaging h/inl/cpp + ButtonController h/cpp; Amax UiManager h/cpp; deferred_ui_messages_test.cpp | Actual139 pool assertion at static exit causes logging UAF; cancel owned copies/late posts before Lua, retain reinit/borrowed message semantics; coherent game exit pending |

| MAC146 | Preserve native shared/local sampler cache validity across node and pass selection | Codex /root/physics_port | source/fixture frozen; all4 probes + Debug shared/node/cache GPU PASS; old-order differential reproduces slot0 | FxContext.cpp; vertex_fx_texture_test.cpp | Actual145 shader589 slot0 remains2D after shared node binding beforePrefetch; rematerialize shared vertex samplers with actual pass and repair real mask conflicts |

| MAC147 | Component-creator Sorcery storage/global Xt shutdown audit | Codex /root/dynamic_port + /root/metal_occlusion (audio pack scope) | read-only source inventory complete; MAC148 live throw pending | component-creator-lifetime.md; actual MAC146 source/stack | Distinguish live records vs component claims; no pool/assert/allocator bypass; app/global pack/schema owners require paired retirement |

| MAC149 | Coherent pass-bound sampler/shared-cache and global Xt owner integration | Codex /root | Debug build/48 strictMetal checks PASS; actual colour-mask + one UI light owner boundary | CMake48checks, actual native Debug game; frozen146 and staged147 | Actualone-track/car GPUframe+keyboard+naturalfinish+normalexit remainrequired; baseline145/148 evidence retained |

| MAC152 | Actual scene point-light owner retirement audit | Codex /root/physics_port | exact MAC155 ownership proof; source/fixture frozen; all4 probes + Debug fullgame lifecycle exit0 | SceneGraph light manager; VehicleActor/Wheel, LightSpecialFX, UIPerks2 and world/UI teardown | Actual149 world destructor has1 allocated point light; identify original owner and required pre-scene release, no pool/assert bypass |

| MAC150 | Upper Context colour-mask default contract | Codex /root/metal_occlusion | source/fixture frozen; all4 probes + Debug/UBSan strictMetal GPU exit0; actual155 sentinel/caller confirmed | Context_Base.cpp; MetalGameContext h/cpp; blend_mode_test.cpp | Original PC Context routes Device default resolution; preserve lower literal mask rejection and actual per-device policy; actual149 numeric mask still uncaptured |

| MAC151 | Before-unwind actual mask and scene-light owner proof | Codex /root | diagnostic complete: actual particle QuadList; light count1, exact gadget handle pending155 | ignored verification/mac151 scripts/evidence; root slice docs | Capture upper sentinel/caller and remaining light vs UIPerks2 handle before class fixes; no lower validation/pool assertion bypass |

| MAC155 | Actual particle/splash trigger and exact UI-light handle proof | Codex /root | diagnostic complete: actual DefaultUINT_MAX/SplashCamera; exact UIPerks light match | ignored verification/mac155 evidence | Finite realQ1; explicit gadget handle and scene active-light fields, no inferred pointer match |

| MAC156 | Native original-PC dynamic geometry topology/map policy | Codex /root/metal_occlusion | source/fixture frozen;72 probes all4 + standard Debug strictMetal GPU exit0 | 11particle shape headers/13writers; sixlight shape header/writers; Minimal.cpp; DVB Lock/Unlock; Manager UseSoft; particle_geometry_test.cpp | Actual151 QuadList from SpriteRotating; complete original PC paired counts/writers, no lower topology bypass; coherent actualgame pending |

| MAC154 | Remaining native required configuration/default-map startup boundaries | Codex /root/dynamic_port | complete; 20 object probes PASS; standard strictMetal fixture exit0; coherent51/51PASS | remaining-startup-contracts.md; RaceBookLayer.cpp; MultiplayerData.cpp/h; Dynamic Renderer.cpp; focused fixture | Offline required config failures must stop before consumers; exact white/grey defaults need initialized texels; root158 final-view selectors and159 failure guards integrated; cube skip-clear content policy alone deferred |

| MAC158 | Final original-PC view FX and prepass-to-HUD handoff | Codex /root | source frozen; all4 private translation-unit probes PASS; coherent actual run pending | GameWorld.cpp EndRenderSplitscreenWithoutAA only; selected-view-handoff.md | Original retailT232 body and ApplicationBase PC RestoreWindow chain authoritative; no invented Metal depth state, final actual frame required |

| MAC159 | Required world texture creation/map failure before publication | Codex /root | source frozen; all4 combined translation-unit probes PASS; coherent actual run pending | GameWorld.cpp CreateVehicleLightCubeMaps + required window-depth map only | Native retains current contents policy; reject failed creation/map before assert/memset/unlock; existing NativeDestroy owns every partial texture |

| MAC153 | Coherent grouped policy/startup/ownership integration and actual Brighton slice | Codex /root | coherent Debug549steps +51strictMetal checks PASS; Release build and actual160 running | native CMake51checks; Debug/Speed actualgame bundles; private real-path operator/evidence | Frozen150/152/156/158/159 +154required boundaries; completedframe+ordinarykeyboard+naturalfinish+normalexit required |

| MAC160 | Actual coherent Debug fullgraph/input/populated-world exit | Codex /root | actualfailed IncompleteDrawState; allworld/UIcleanup passes, lateRaceBookselfpool crash | originalgame-test archives; fresh153copy; ignoredverification/mac160 | No frame/input/naturalfinish/normalexit claim untilactualproof; optimizednaturalrace follows |

| MAC162 | Actual post-dynamic draw shader/declaration/target readiness audit | Codex /root/metal_occlusion | source/fixture frozen;12 probes all4 PASS; real Debug Manager shader GPU draw/retirement exit0 | Particle Manager.cpp required shader Create/Recreate/rollback; SplashCamera PC triangle selector; particle_geometry_test.cpp extension | No lower incomplete-state guard relaxation or guessed rebinding; distinguish null binding from uncreated owned wrapper |

| MAC161 | Before-unwind actual incomplete draw and final-exit owner proof | Codex /root | complete; controlleddiagnosticexit0, normalgameexitunverified | ignoredverification/mac161 | Actualparticlevertex/pixelNULL withvaliddeclaration; lateRaceBookownedselfpool base/member lifetime; no guard/logger bypass |

| MAC165 | Cross-engine quad inventory and original-PC producer policy | Codex /root/physics_port | production frozen;12 real-TU probes all4 PASS | quad-producer-inventory.md; LightTrails/AuxFlares.cpp; DebugRendered.cpp paired selectors | Native selects original PC excluded trail/flare draws and exact box triangle count/writer; profiler/imported quad limitations documented; coherent actual frame/lap pending |

| MAC164 | Coherent required-particle-shader/self-pool/cross-engine PC policy integration | Codex /root | coherent Debug152steps/52checks PASS; Speed81steps/2changedchecks PASS; actual167 running | CMake52checks; Debug+Speed native game; freshactual167aftertests | Retainstrictdraw/quad/poolvalidation; actualcompletedframe+keyboard+naturalfinish+normalexit pending |

| MAC163 | RaceBook owned self-pool and final profile/vehicle retirement | Codex /root/dynamic_port | production + fixture frozen; 12 object probes PASS; faithful private runtime/static exit0; coherent integration pending | RaceBookManager.cpp/h; GameApp.cpp destructor hook; racebook_owner_test.cpp; racebook-owner-lifetime.md | Actual160/161 static self-subscription crash proven; retain data through Audio, retire exact owned state before global Xt, no pool/logger bypass |

| MAC166 | Broader backend-guard shader startup audit | Codex /root | bounded read-only inventory complete | shader-backend-guards.md; ignored mac166-backend-guards.txt | 75PCD3D11/PS3GCM guards classified; selectedparticlefalseReady162fixed, BasicRender explicitcheckedfallback, defaultVS nativeowntransaction, unusedSkyDome/sample shadow gaps deferred |

| MAC167 | Actual optimized native full renderer and natural keyboard race | Codex /root | actualSpeed crash27.49s audioVAG source-pointer truncation; noPNG/input/naturalfinish | source164Speedfreshcopy;originalgame-test data;ignoredverification/mac167 | Observe actualmotion/steer/orderedcheckpoints/lap/finish andnormalexit, never assigngameprogress |

| MAC169 | Original ADPCM/VAG native stream pointer alignment | Codex /root/physics_port | frozen; all4 source/fixture compile+link+CPU runtime PASS | decoderadpcm4bit.cpp paired mono/stereo guards; adpcm_stream_test.cpp; adpcm-stream-pointer-contract.md | Actual optimized low32 memmove source matches native console pointer truncation; original PC full-pointer branch selected; coherent actual game pending |

| MAC168 | Actual optimized VAG fetch fault proof | Codex /root | diagnostic complete; worker memmove low32source, controlledexit0 | ignoredverification/mac168;ownedprocesscrashIPS | x1fa9b0080/x0full64dest/x2bytes128; optimizedlocals unavailable, nofulloriginalsourcepointerclaim |

| MAC170 | Native audio runtime scratch alignment/producer-consumer contract | Codex /root/dynamic_port | production frozen; four-config probes/private CPU fixtures finishing | audio_core.h/Object.cpp/Instance.cpp/core.cpp; audio_scratch_test.cpp | Native alignof-derived prefixes+roundedtotal sizes, original32bitcounts/windows/wirelayout preserved; no sourcecausation claim for167 |

| MAC171 | Coherent native audio pointer/alignment integration | Codex /root | Debug217/Speed218builds +10affectedchecks each PASS; actual172frame/motion, thenattachmenthazard | Debug/Speed game; originalaudio_core/systembank/output +newadpcm_stream/audio_scratch +ownerchecks | Frozen169+170, actual172 optimizedkeyboardrace followsvalidations |

| MAC170 | Native audio runtime scratch alignment and caller pointer audit | Codex /root/dynamic_port | source + fixture frozen; 16 object probes PASS; Debug/UBSan runtime exit0; coherent integration pending | audio_core.h; Object.cpp; Instance.cpp; core/core.cpp; audio_scratch_test.cpp; audio-runtime-layout.md | Aligned record-specific runtime prefixes/sizes, original PC wire fields unchanged; actual Instance/core/streamed PCM checks; decoder169 disjoint |

| MAC174 | Actual attachment sampling/snapshot routing and ownership audit | Codex /root/metal_occlusion | source/fixture frozen;8 probes + all4 private standard GPU closures strictMetal exit0 | MetalGameContext h/cpp; colour_feedback_test.cpp; native/PC snapshot route/ownership audit | Preserve strict raw feedback guard; distinguish binding-time colour snapshot contract from depth/vertex/late target alias |

| MAC172 | Actual completed native frame and keyboard-motion validation | Codex /root | completedframe+motion/steer proven; naturalfinish blockedon174attachmentcontract | optimized171privateapp;originalBrighton/BMW;motion-proof.json | Read-only car/AIR data withnormalfinitekeys; actualerrorcleanup exit1 withoutlatecrash, clean0exitstillpending |

| MAC173 | Actual moving-effect attachment alias before-unwind proof | Codex /root | complete: pixel10 water samples activecolour0; lateAudio pollpoolassert | ignoredverification/mac173 | Stage/slot/shader/surface/targetidentity+caller required; retainedstrictlowerhazardguard |

| MAC177 | Coherent binding/loaded-audio ownership integration and actual race | Codex /root | Debug580/Speed708 builds and14affectedchecks each PASS | standard Debug/Speed app, affected real checks, private ordinary-key operator | Completed real frame/motion established172; original natural lap/finish and successful normal exit remain required |

| MAC176 | Natural keyboard slice operator/read-only route convention | Codex /root/physics_port | frozen; all4 TU+helper compile PASS, private CPU checks exit0 | UiManager.cpp VehicleState adjacency only; route_adjacency_test.cpp; keyboard-slice-operator.md | Existing original AIR GetIndexOffset replaces hand-rolled0 seam; no game progress/input policy writes; actual lap/finish pending |

| MAC175 | Audio entity poll and callback final-owner lifecycle | Codex /root/dynamic_port | source + fixture frozen; 8 object probes PASS; standard coherent177 Debug affected14/14PASS | InterfaceController.cpp/h; audio_subsystem_ownership_test.cpp; audio-observation-ownership.md | Cancel exact controller-owned poll entries while entity refs live; defer callback reuse until actual producer shutdown; borrowed subsystem isolation retained |

| MAC178 | Native upper context/device routing, cache and ownership audit | Codex /root/metal_occlusion | source/fixture frozen; seven baseline failures reproduced,8 probes + four strictMetal GPU closures exit0 after repair | Context_Base.cpp native VB/indexed overloads; game_context_test.cpp; context-device-contract-audit.md | Coherent integration root-owned; target scheduling/cache, current-view and unreached Default gaps remain separate unmodified follow-ups |

| MAC179 | Actual populated Debug binding/ownership regression | Codex /root | actual81m keyboard motion and ordinaryassert-enabledexit0; no lapfinishclaim | actualoriginalarchives, fullgraph, finiteQ5, ordinarymanualquit | Assert-enabled actual shutdown verifies175 loaded48-band producer lifetime; no naturalfinishclaim fromearlyquit |

| MAC180 | Actual original one-track keyboard natural race | Codex /root | actual~1km/start+split1; cubebindingfailure181, cleanerror1 | originalBrighton/BMW, ordinaryfinitekeys, originalcheckpoint/lap rules, terminalwatch | Require genuine LapInc:0:1 +RaceFinished +successfulnormalexit; earlyerror/timeout/wrecknotacceptance |

| MAC182 | Coherent PC cube/2D semantics and current-binding integration | Codex /root | coherentDebug/Speedbuilds and13affectedchecks each PASS | native game, affected GPU/FX checks and original keyboard race | No lowerdimension/feedback/incomplete-state guard relaxations; require actual naturalfinish+exit |

| MAC181 | Original-PC both-stage cube/2D FX sampler contract | Codex /root/physics_port | frozen; all4 compile PASS, Debug strictMetal GPU exit0;2 failing controls verified | FxContext.cpp; MetalShaders.h/mm; vertex_fx_texture_test.cpp; cube-sampler-contract.md | Actual180 PSs8 cube/2D; existing PC67/native54 measured RGBAzero, no lower guard relaxation; dynamic185 owns required zeroCube |

| MAC185 | Required FX PC 2D-to-cube zero-texture owner | Codex /root/dynamic_port | source + fixture frozen; 8 object probes PASS; strictMetal private FX runtime/static exit0; coherent integration pending | FX System.cpp; fx_test.cpp; fx-zero-cube-owner.md | Measured PC RGBA zero all6faces, required initialization/transactional publication and retirement; upper adapter181 disjoint, strict lower guard unchanged |

| MAC183 | Actual keyboard lap after grouped PC sampler/binding contracts | Codex /root | PASS actual571.51s exit0;705human records;3309.57m;original ordered splits/lap/finish | originalBrighton/BMW, fullarchive/renderer, ordinarykeyboardoperator | Actual originalLapInc:0:1 +RaceFinished +normalexit0 required; previous1km/firstsplit notfinish |

| MAC188 | Normal menu/showroom readiness audit and actual failed-movie owner correction | Codex /root/metal_occlusion | production/fixture frozen;12 probes all4 + Debug/Speed strictMetal video-only UI runtime exit0 | UI Movie.cpp; Lua_System.cpp; GUIBank SetupMovie; ui_movie_test.cpp; menu-renderer-assumptions.md | Coherent192 integration; actual source asset mapping/191 IO separate; optional Debug audio stress CoreAudio/global-delete crash retained for193 |

| MAC193 | Debug UI movie audio stress CoreAudio/global allocator lifetime audit | Codex /root/metal_occlusion | bounded experiment complete; rare failure unresolved; candidate NOT integrated | coreaudio-disposal-audit.md; unhooked checked SDL patch proposal; private pinned3.4.18 archives/stress | Baseline/candidate each12 UI processes +12 immediate-owner processes PASS; no controlled negative reproduction or audio-tail proof; preserve allocator ownership and production SDL |

| MAC187 | Normal career/profile save and restart contract | Codex /root/dynamic_port | frozen; 28/28 object probes +192 Debug/Speed 12/12 affected checks each PASS; actual194 full career reload PASS; clean frontend exit195 pending | SaveInfoInterfaces.inl; lua_GameProfile.cpp; SaveSystem.h/cpp/pc.cpp; one UiManager destructor hook; ui_save/amax_save_envelope/profile_save_owner fixtures; career-save-reload-flow.md | Original normal UI create/load/autosave, native offline identity, PC serialization and genuine on-disk/restart evidence; disjoint186 UI flow |

| MAC189 | Actual fresh Master menu/profile/save startup | Codex /root | failed idle missingAmax movie readiness +camera snapshot doubleowner;188/186 repair | originalMaster Legal/Start/profile/mode UI, observe-only harness +ordinarykeys | NoDebugRb shortcuts/fakeevents/progress/unlocks;186flow/187save/188resources sourceaudits parallel |

| MAC186 | Normal Master menu/SP flow and camera snapshot ownership | Codex /root/physics_port | frozen;16 object probes all4cfg PASS; original snapshot lifecycle Debug exit0 | normal-menu-flow.md; camera2 Camera.h/.cpp; camera_ownership_test.cpp | Native independent nested owners +fullvalue copy, Windows/layout unchanged; original menu flow source map; actual Master/profile/event/result/shutdown proof root-owned |

| MAC190 | ActualMaster ordinaryPressStart/profile/save path | Codex /root | actual genuine2027byte career save created; later existingBlurAttract filesystemOpen failure; reload pending | sameisolated189identity/emptysaveroot, ordinarykeyboardReturn | Previous189idlefailedmovie188/camerasnapshot186; knownfaultsrepairparallel, noassetrename/fakeprogress |


| MAC192 | Coherent normal-menu camera/save/movie integration | Codex /root | Debug+Speed game/fixtures build PASS;12/12 affected checks each PASS | CMake camera_ownership/ui_movie/profile_save_owner +existing save/resource fixtures; auto Lua regeneration | Actual194 restart/read/menu proven; shutdown195/196 pool assertion pending; optional Debug CoreAudio stress193 separate |

| MAC194 | Actual coherent Debug Master/profile restart | Codex /root | actual career reload+movie/autosave+hub/unlocked event PASS; exit250 Audio PagedArray assert | independent189 data/identity/save roots; fresh192appcopy; observe-only +ordinaryinput | Original version1378 full career decode/normalmenu/ordinaryexit required |


| MAC195 | Master frontend audio streaming final-owner retirement | Codex /root/dynamic_port | frozen;24/24 source probes PASS;coherent197 Debug standard stream leaf exit0 .69s | System.cpp/ObjectManager.cpp; core WaveServer/filestream/filestreammanager cpp+h; audio_stream_retirement_test.cpp; audio-frontend-retirement.md | Exact196 FileStream owner;real blocked/fullqueue/retire/recreate proof;actual Master normal-exit root198 pending |

| MAC196 | Before-assert exact frontend audio pool proof | Codex /root | complete: actual FileStream* pool at FileStreamManager destructor; ownedinferiorkilled | fresh192appcopy +real Master/profile; __assert_rtn stack | Identify pool type/live owners before focused class repair |


| MAC191 | Existing loose movie Open failure diagnostic/audit | Codex /root/physics_port | frozen;8 object probes all4cfg PASS; existing filesystem regression Debug exit0 | NativeFileSystem.cpp Open diagnostic only; file-open-failure-audit.md; unchanged filesystem_test | Actual190 Speed null vs194 Debug success remains unknown; operation/route/exception+captured system code diagnostic preserves all IO decisions |

| MAC200 | Normal career race setup/config authority audit | Codex /root/physics_port | read-only audit complete; no correction required by scoped values | career-race-config-audit.md; BOARD/LEDGER only | menu189 Master bypasses debug frontend; genuine event laps/population/owned allowed car/profile controls, no progress or production writes |

| MAC203 | Independent native audio stream final-owner review | Codex /root/physics_port | read-only review complete; no further blocking defect identified after195 legacy-return correction | frozen195 System/WaveServer/filestream/manager +audio_stream_retirement_test.cpp reads; BOARD/LEDGER only | Source ordering/accepted ownership coherent; focused fixture omits renderer Init, actual full frontend/renderer shutdown remains required |


| MAC197 | Coherent native stream/graph/file/mixer owner integration | Codex /root | Debug+Speed app/fixtures build PASS;19/19 affected checks each PASS | CMake new audio_stream_retirement leaf;195 source including ObjectManager static owner reset;191 failure diagnostics | Actual198 full Master renderer/menu clean exit required; fixtures not game proof |

| MAC198 | Actual Debug Master/menu/garage and normal shutdown | Codex /root | actual Debug full menu/garage +application shutdown PASS exit0/605.95s; desktopquitUI separate | same genuine189profile/save root, fresh197 appcopy;199ordinarykeyhelper | Original tutorial movie/save/owned-allowedGarage, actual successful shutdown required |
