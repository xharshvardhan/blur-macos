# Native Brighton/BMW startup publication audit (MAC122/123)

The current actual path has reached real World/RaceLogic loading and
StreamerStarted, with original simulation/particle work active. MAC120's trail
stride failure and MAC119's particle queue corruption were runtime storage
faults, not proof of missing original assets. MAC121/122 integrate those fixes;
actual completed-frame, keyboard motion, natural lap/finish and clean-exit gates
remain separate. This audit read the original startup contracts without changing
preset2, the track, car, original Continue policy or console behavior.

| Class / boundary | Concrete current source | Result / ownership |
|---|---|---|
| Registered renderer acquisition → World Ready | AmaxWorldLayer.cpp calls Device CreateRenderers/CreateRenderersVolatile; DeviceBase.cpp257/284 aggregates actual renderer false results. Native I_Renderer false/throw already rolls back its failed own acquisition. | MAC123 now consumes both aggregate results through NativeCreateRequiredRenderers and existing main first-error channel before InstantiateLevel/AIR/RLC. Layer unwinds acquired debug/subscription state and fully created transactional World before level/LUT/advertising release. No placeholder renderer or volatile promotion after base failure. |
| Pending actor phase → actor-studio completion | GameActorList.cpp previously discarded CreatePostLoad result; the level-list destructor notified mActorsLoaded. | MAC123 explicit native runtime side interface calls completion only after successful original preload/postload phases. Cancellation/destruction alone never calls success. Actual false/throw publishes first error without escaping the callback worker; owned lists retire exactly once. |
| Optional actor versus required capability | ActorCreationList.cpp67 config defaults E_OnFail_Continue; ActorList.cpp786–798 logs/kills failed actors but returns false only under explicit Abort. | Original Continue retained, including an actor Create=false. No recovered source classification makes every level actor mandatory. Required BMW model/physics/vehicle and AIR/RLC paths retain their separate stricter native checks. Missing mandatory subsets beyond these remain unknown. |
| Archive parse success → usable physics contents | Initial audit found versioned Main/Route Import bools discarded; LevelResource propagates the PhysicsLevel aggregate. | Companion MAC125 now checks native Main/Route Import results and ownership, with four-config reader and integrated Debug verification. No format replacement or synthetic geometry; this is no longer an outstanding gap. |
| Global resource completion → resource validity | GameAppResources.cpp1338/1349 tests SpecialFX bag IsWaitingForLoad; ResourceBag.cpp93–104 only waits for initial(-1) notification,115–125 records any outcome;130–143 returns resource only for DataReady. BootLayer.cpp615 and UiLayer.cpp1277 consume completion. | Queue/bag drain is not aggregate success. Root owns companion required-resource audit; bag may include optional effects and needs a source-backed required subset. |
| Particle mesh/texture preload → cache completeness | GameAppResources.cpp468 deletes failed texture and continues;477 discards UserParticleLibrary::Open bool. UserParticleLibrary.cpp44–60 returns false on open/deserialization. | Companion required-resource audit, outside MAC123. Full selected runtime graph remains the evidence for which assets are actually consumed. |

Two apparent bool/return gaps are **not** failures to propagate:

- GameWorld's variable named streamerSuccess comes from
  LevelSceneManager::UpdateStreaming → WorldRender::C_Scene::UpdateStreaming
  (Scene.cpp136–147), which ORs **data still loading**. False means no stream data
  remains pending. Treating it as failure would invert the actual contract.
- VehicleModelManager::Create (VehicleModelManager.cpp98–139) currently returns
  true after shared parameter/sampler publication; Initialise is void.
  BlobShadows::Create(settings,context) at1204 is a void settings assignment,
  distinct from the real registered renderer's OnCreate/OnCreateVolatile GPU
  acquisition. Their signatures do not justify guessed failure branches.

PS3 stream/resource overlap, Xenon/PS3 outro memory budgeting, disabled cutscene
code, and converter-only XMem contexts are outside the active native PC-data
preset2 path. No blanket PC-guard widening or console policy change was made.

MAC123 verification: two original CPPs in four configs compile8/8. The real
startup_completion fixture uses actual registered GPU renderers and the exact
production aggregation helper, actual native application/LayerManager first-error
handoff, and actual C_GameActorList/factory/creator references. It checks base and
volatile false/throw rollback, main handoff before pending Create/Ready, preload
and pending postload exceptions, cancellation with an actor still Priming, and
independent strong creator lifetime through actor destruction. It also proves
original optional Create=false Continue behavior remains. This is bounded
mechanism coverage, not a populated-world renderer-failure injection or race.
Root's coherent Debug full-game/fixture build and strict Metal CTest24/24 passed,
including true startup_completion process/static exit0. Source/fixture frozen.
