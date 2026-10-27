# Remaining quad producers on the selected slice (MAC165)

Source inventory and approved policy restoration, 27 October 2026. Scope is
native startup/UI through one Brighton/BMW race and shutdown. This records
source reachability and recovered PC policy, not proof that every listed
renderer has drawn in the actual game. The initial audit was read-only; parent
then approved the five narrow guard changes described below. No imported
topology, lower validation, new triangle algorithm or Windows policy changed.

## Prioritised source inventory

| Producer | Original source contract and current native selection | Slice relevance / action boundary |
| --- | --- | --- |
| `gamesys/LightTrails/source/LightTrails.cpp:471–539` | Both ribbon draw at477 and particle draw at521 use `#ifndef PLATFORM_PC`. The ribbon reserves `nodes * LIGHTTRAILS_SIZE * 2` strip vertices and writes fade factors used by the particle path. The particle path reserves/writes `nodes * (LIGHTTRAILS_SIZE - 2) * 4 * 3` QuadList vertices. Original PC explicitly skips both with the comment “knock out until we have a begin vertices method for PC”; no PC six-vertex writer exists here. At audit start native entered both console paths; it now selects both original PC exclusions. | High-confidence PC/native selector mismatch, potentially reachable main view. The approved change selects both exclusions together; changing only the particle topology or ribbon guard breaks the coupled writer/fade contract. No new light-trail implementation is justified by this source. |
| `gamesys/AuxFlares/source/AuxFlares.cpp:142–165,220–250` | `AddFlare` writes exactly four vertices; the render reservation is `batch.m_NodeCount * 4`. The complete Begin/write/End block is excluded only on PC, explicitly with the same missing-PC-BeginVertices comment. At audit start native took console QuadList; it now selects the original PC exclusion. No original PC triangle writer exists here. | High-confidence PC/native selector mismatch, potentially reachable vehicle flare pass. The approved change selects the existing PC exclusion, without manufacturing a triangle feature or reinterpreting four vertices as six. |
| `gamesys/SplashCamera/source/SplashCamera.cpp:381–500` | Console writes four QuadList corners. The recovered T107 retail-PC branch writes six TriangleList corners `(0,1,2),(3,0,2)`, retaining the documented repeated-corner alpha difference. | Actual prior SplashCamera call evidence exists (MAC155). Graphics MAC162 owns the full PC/native selector repair; its current source already excludes native from the console block. No duplicate edit in MAC165. |
| `graphics/render/source/debug/DebugRendered.cpp:1042–1128` | `Draw3dBoxLit` has two coupled PC selectors: console six quads/four vertices, PC twelve triangles/three vertices; writer switches from four corners to `(0,1,2),(2,3,0)`. At audit start native took both console branches; both now select the existing PC branch. | Source-proven mismatch but optional debug drawing, not a required normal-race feature. The approved repair widens both selectors together, preserving per-face normals/colour. |
| `gamesys/debugsystems/Source/ProfilerView.cpp:23,181–225` | `System.ProfilerView.Enable` defaults false. Registration occurs only when enabled; no marks means no draw. Enabled rendering reserves background4 + marks4 + timing8 and uses QuadList; `AddRect` writes four corners. There is no PC triangle branch. | Optional diagnostic, not selected by the normal slice. Native QuadList remains unsupported if explicitly enabled; no recovered writer permits a faithful conversion in this audit. |
| `amax/game/Game/GameWorld/View/Reflection.cpp:596–637` and `View/CubeMap.cpp:339–391` | Fake-sun QuadList reservations/writers are already inside `!PLATFORM_PC && !PLATFORM_MACOS`. | Native already matches original PC disabled draw policy. State setup outside the excluded block does not make it a quad producer on native. Do not re-enable it through a blanket conversion. |
| `graphics/display/source/render/Render.cpp:361–402` | `DrawFullscreenQuad` and its four-corner writer are inside `#ifdef PLATFORM_PS3`. | Not compiled on native/PC; not a missing native fullscreen implementation. |
| `graphics/renderingdata/source/noninterface/build/scenebuild.cpp:239,296–320` and `target/archive.cpp:1107–1118` | Export/build API validates four vertices for `PrimitiveInfo::PRIMITIVE_TYPE_QUAD`, maps it to serialized QUAD_LIST, and the target archive maps that vocabulary to Display QuadList. QUAD_STRIP already asserts unsupported. | Data/procedural-format capability, not a selected immediate quad writer. Scoped game/gamesys calls find no `Scene::AddPrimitive` producer; observed call sites are converter tools. No selected Brighton/BMW imported quad has been established. Keep archive IDs unchanged; investigate actual data before adding any conversion. |

Paths beginning `gamesys/` and `graphics/` are beneath
`code/mercury/library/`; Amax paths are beneath `code/`.

## Why the first two are relevant to the slice

`code/amax/game/Game/GameWorld/GameWorld.cpp:1033–1091` creates both original
services, configures LightTrails for the main view/Alpha pass, AuxFlares for
AlphaVehicleFlares, and inserts both into the scene graph. VehicleActor creates
trail instances for brake/rear-light parts at5751–5770 and auxiliary flare
instances from the real part paths at5808/5845/5923/5964 through
`AddAuxFlare` at9586–9625. `View/Main.cpp:843,950–954` supplies real main-view
flare settings and schedules the flare pass when its existing debug toggle is
enabled. AuxFlares gather still requires the real view flag/frustum/depth checks
at95–136, and its draw requires completed texture notification at166–170.
These facts establish reachable source paths; they do not establish a visible
batch on the selected car in a captured frame.

DebugRendered's game-library caller is
`code/amax/library/Kanzen/Camera/HollywoodVehicleCamera.cpp:463–468`, gated by
`OverrideTransformRender`, whose debug flag defaults false at60. Other scoped
callers are test sections/GravityTest. This is separate from required vehicle
geometry. ProfilerView similarly requires deliberate diagnostic activation.

## Already-covered producers and non-producers

MAC156 covers the complete dynamic subtree: eleven particle metadata policies,
thirteen CPU writers, six light shape metadata/writer pairs and Minimal's cube.
Their remaining QuadList tokens are console branches; native selects the
original PC paired counts, triangle writers and mapped-buffer lifetime. See
[the existing geometry contract](particle-geometry-contract.md). This audit
spot-checked SpriteRotatingDefault, Quad2d and Minimal against that selection;
it does not repeat MAC156's GPU validation.

The widened UI search also finds `ui/source/ui/Graphics/Simple2d.cpp:1523`, but
its native/PC branch at1237 selects six vertices and mapping `(0,1,2),(0,2,3)`;
BeginVertices at1519 selects TriangleList and the writer at1565 uses the same
mapping. Its QuadList token is an excluded console alternative, not a remaining
native producer. The UI multi-stage loops also use the selected vertex count
and mapping. Amax UI contains no further topology-token hit.

`DisplayToDisplay2.cpp:113`, DeviceTypes primitive-count helpers,
PrimitiveRender's accepted-topology assertion, archive text names and console
SDK headers are vocabulary/forwarding, not additional writers. In particular,
`DeviceTypesEnums.h:360–364` reserves real QuadList only for PPU/SPU and uses an
unsupported sentinel elsewhere. An assertion permitting that enum in an upper
batcher is not evidence that PC/native can draw it.

## Method, limits and next step

Scoped `rg` searched `QuadList`, `QuadStrip`, `QUAD_LIST`, `QUADLIST`, `QUADS`
and `PRIMITIVE_TYPE_QUAD` across Amax game/library, Mercury gamesys/graphics and
Mercury/Amax UI (`cpp/h/inl/mm`). Each direct producer was read with its guards,
reservation and writer; converter/console declarations were classified
separately. The initial full graphics/gamesys/Amax inventory had59 token hits,
including vocabulary and excluded branches. No search result alone was treated
as proof of a live draw.

The approved production change is limited to five native guard selectors in
three existing sources: both coupled original-PC LightTrails exclusions, the
complete AuxFlares original-PC draw exclusion, and both DebugRendered box
metadata/writer selectors. Each carries a `[Blur2PC]` annotation. No count or
vertex-writing algorithm changed. Default-off ProfilerView and imported
QUAD_LIST data remain separately bounded limitations. No existing focused GPU
fixture was found that directly exercises this optional lit-box helper; no
mirrored guard test or speculative GPU replacement was added.

Validation: private real translation-unit object compilation of LightTrails,
AuxFlares and DebugRendered in Debug, Speed, DebugUBSan and SpeedUBSan passes
(all12 probes). Logs/objects are private under
`_work/build/macos/physics-agent-quad-policy/`; the reproducible driver is
`_work/build/macos/physics-agent-quad-policy-probes.py`. `git diff --check` passes
for the three scoped production files.
No shared build, actual game session or GPU runtime was run for MAC165. Coherent
actual MainView/race validation remains the parent integration gate. This work
establishes neither a completed frame nor keyboard displacement, natural finish
or clean full-game exit.
