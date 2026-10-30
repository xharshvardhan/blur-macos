# Faithful Blur remaster: direction and implementation gates

MAC353 research, refined by MAC354 on 2026-10-30. Current user direction: **keep the style similar to original Blur and improve overall graphics**. This supersedes the29October bold-redesign preference. The generated concept options are exploratory references, not the selected art style. This originated as a documentation-only research baseline. MAC380 now qualifies the first faithful rendering-quality batch: eligible texture filtering16 by default, optional window multisampling and an original-quality override. Original artistic resources remain unchanged; this is not a completed modern remaster. See [the acceptance report](faithful-remaster-acceptance.md) for final checks, actual images and adverse performance evidence. Two agents independently inspected the renderer and reusable-host/network boundaries, read-only.

## Product thesis

Faster source-based porting lowers the cost of adapting an existing game. It does not demonstrate that the original cars, environments, physics, effects, audio and balancing can all be authored in two days. Blur shipped in2010, not2000 ([original publisher announcement](https://investor.activision.com/news-releases/news-release-details/activisions-blur153-races-store-shelves)).

Our hypothesis is that cheaper iteration makes small, socially specific games more practical: a college league, a recurring friend-group championship, or a co-op event tailored to a known group. Multiplayer is already widespread; the opportunity is less friction and more local relevance, rather than multiplayer newly becoming possible. This is a product hypothesis, not a market forecast.

Candidate worlds:

- A private racing club with short cups, rivalries, shared replays and an easy return next week.
- A fictional campus-scale racing arena where friends can author routes and competition rules.
- A co-op city event where racers complete shared objectives and unlock the next route together.
- A party session that chooses among validated modifiers between races: power-up rules, routes and objectives.

Creation and live competition need different timing. Generate/edit content before a session, validate it, package it, and pin the exact version/hash for every participant. Race simulation should run without an LLM round trip in the per-frame control path. Shared seeds alone do not establish deterministic physics. Authentication, authority, state replication, prediction/interpolation, reconnect and trust remain engineering work; new transport libraries do not implement these game rules automatically.

## What the reusable package actually enables

`NativeHost` provides independent C++17 Cocoa windows, raw keyboard/mouse/focus events, Retina dimensions and a borrowed CAMetalLayer. Its installed, relocated client demonstrates independent host consumption. Blur consumes the same implementation through thin aliases.

It does not supply a renderer, asset importer, Windows ABI/API compatibility, simulation, input mappings, audio backend or race protocol. It is neither a complete engine nor an arbitrary Windows-game converter. See [its explicit contracts](../../code/mercury/library/gamesys/framework/native_host/README.md).

Promising next reuse boundaries are packaging/dependency closure and relocation checks, common-clock diagnostics, and eventually the legacy-shader adapter with an explicit standalone backend ABI and independent conformance client. Original PAK serialization, material bindings, game controls, save policy and race semantics remain game/engine adapters. Extraction should follow a second genuine consumer rather than force the entire port behind a speculative universal interface.

Existing projects to evaluate instead of duplicate:

| Need | Candidate | Integration boundary |
| --- | --- | --- |
| General cross-platform window/input/audio services | [SDL3](https://wiki.libsdl.org/SDL3/FrontPage) | Prefer existing services when an application's needs exceed our narrow host; do not replace a working host merely to adopt a name |
| Reliable/unreliable encrypted UDP transport | [GameNetworkingSockets](https://github.com/ValveSoftware/GameNetworkingSockets) | Prototype separately; transport does not recreate the original protocol, lobby provider, matchmaking or state authority, and service features have separate availability |
| Authored modern material interchange | [glTF](https://github.com/KhronosGroup/glTF) | Evaluate a new authored-content import path; importing requires mapping meshes/materials into this engine, not renaming original formats |
| Material/lighting reference implementation | [Filament PBR documentation](https://google.github.io/filament/main/filament.html) | Reference clear-coat, roughness, exposure and lighting techniques; it is not a drop-in replacement for the current renderer |
| Apple temporal reconstruction | [MetalFX](https://developer.apple.com/videos/play/wwdc2022/10103/) | Integrate only after coherent colour/depth/motion/exposure and history handling; retain native-resolution comparisons |

No new library has been integrated during this audit. Retain dependency provenance and required notices as part of the existing packaging workflow.

## Art direction — current user choice

A faithful remaster: preserve original Blur's car and track identity, proportions, environmental mood, authored time of day, palette, neon power-up colours, effect language, camera and HUD identity. Improve fidelity and readability while keeping the game immediately recognizable. The alternate illustrated, festival and monumental concepts were exploration only; no new architectural world or visual redesign is selected.

Priorities are stable edges and fine details in motion; clearer textures and surface detail; richer car paint, glass and road response; improved lighting, shadow and reflection quality; and refined original glow, smoke, sparks and trails with readable silhouettes. Preserve original effect colour meanings and avoid clipping bright neon sources. Higher texture resolution alone does not recover missing authored detail; replacement material inputs require deliberate authoring and validation.

Use matched car/track/camera/time-of-day captures and original authored resources as references. Distinguish port-fidelity corrections from optional remaster enhancements. Current native screenshots establish the port baseline, not proof of exact retail appearance; compare verified original reference captures where available. A global saturation boost, darker exposure or forced wet road is not an acceptance criterion.

No generated concept bitmap is installed as a game asset. Runtime implementation is still pending.

## Confirmed renderer seams

- `Graphics/PostFXSettings.cpp::PostFxEnvSettings::Set` selects authored grading, grain, vignette, flare and bloom-feedback settings. `Graphics/HDR.cpp::C_HighDynamicRange::SetUserParameters` uploads local values. `Graphics/Shaders/psToneMapCustom.psh` combines the legacy effects, bloom, gamma and32³ LUT.
- The original world postprocess graph includes exposure, reduced motion blur, optional depth of field and bloom. Existing shader HDR and FP16 support do not establish display HDR: `MetalPresent.mm` still uses BGRA8Unorm presentation without an established EDR/colorspace/UI paper-white policy.
- Lower Metal supports1x/2x/4x samples and resolves. The normal/depth lighting prepass is explicitly single-sample. A class existing elsewhere does not prove an active anti-aliasing integration.
- Active reduced motion blur reconstructs camera motion from depth/current and previous camera transforms. A complete moving-car/skinned-object motion-vector producer was not found in the selected graph.
- Integrated reflection routes include planar reflection, mirror and environment cubemap views. Integrated shadows include blob/drop/collision paths; unrelated shadow-map classes do not prove modern cascaded sunlight shadows.
- Authored materials expose legacy FX/specular bindings. A verified metallic/roughness/clear-coat material workflow has not been established. Some reflection targets remain8-bit intentionally: blindly enabling FP16 previously changed flare appearance.

## Implementation order

1. **Matched original-look baseline.** Select one original car and track, record camera/conditions and establish reference comparisons. Keep the existing authored colour grading and a reversible original-quality mode; fix demonstrated fidelity faults separately from enhancement work.
2. **Image stability and clarity.** Evaluate anti-aliasing and texture filtering in the actual prepass/effect graph. Temporal MetalFX needs jitter, correctly encoded depth, object as well as camera motion, exposure conventions and history resets for cuts, respawns, loads, resizing and split views. Preserve HUD sharpness and evaluate original particles/trails. Adding the framework alone establishes none of these.
3. **Material/lighting upgrades for the same scene.** Improve car paint/glass and environment surfaces while preserving their original identity. Qualify authored material inputs, reflections, shadowing and lighting in separate measured steps. Preserve track geometry, authored time of day and characteristic palette; photorealism or a new PBR label is not itself the target.
4. **Refine original effects.** Improve original power-up glow, smoke, sparks and trails while preserving their colours, shapes, gameplay meanings and road/car visibility. Compose bloom changes at their actual owners rather than globally recolouring bright surfaces.
5. **Optional HDR/EDR output.** Define linear working/output colour spaces, headroom/output conversion, tone mapping and HUD paper-white. Validate on supported displays while retaining correct SDR output and the original artistic intent.
6. **Expand content and distribution.** Apply proven workflows to more original cars/tracks only after matched visual and performance acceptance. Preserve ordinary menu/save/exit paths.

Initial performance goal, not a result: sustained60fps at a recorded2560×1440 configuration on the local M4 Pro with a representative20-car race (16.67ms nominal frame budget). Measure displayed frame pacing, CPU/GPU cost, long hitches and memory in a controlled route. Static beauty shots or generated concepts cannot establish this goal. A higher-refresh target is separate.

Acceptance for the first playable look: matched before/after camera/scene, fast steering and power-up effects, readable road/HUD, no clipped neon highlights or history trails, day/tunnel/night transitions where supported, resize, menu/save/reload and normal exit. Automated game tests stay muted and minimized; frame measurements exclude captures/readbacks and heavy parallel jobs. Use one representative still or compressed video when visual evidence is required.

## Multiplayer gate before social features

Current observed game evidence is a capacity20 host lobby with one local player and zero remote players. Real joining/racing, Windows interoperability and the historical greater-than-three-player issue are not verified.

The next credible gate requires two actual Macs with matching build/data/playlist versions and distinct real identities/profiles. Prove ordinary discovery and join, both rosters showing both players, original readiness/start with `AllowSingleRacer=false`, input visible on the opposite client, replicated gameplay, finish/results/leave and clean exit. `lua_NetRace.cpp::CanStartCustomRace` requires a remote player and valid idle vehicles with no migration. Synthetic socket tests do not satisfy that gate. Four real peers is a later, distinct test for the reported player-count issue. Hosted online service authentication currently returns `NoServiceProvider`; LAN success would not establish internet services.

The remaster, a reusable platform package and a new social game are related products with different acceptance gates. The first deliverable should make one race look and feel excellent; social-world experiments should build on verified networking and a usable authoring path.
