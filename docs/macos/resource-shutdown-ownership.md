# Native resource shutdown ownership

MAC092, 2026-10-27. This closes an ownership boundary exposed while unwinding an
actual renderer failure; it does not prove a completed race or clean full-game
exit after integration.

## Actual observation

The frozen MAC084 application was launched under LLDB with the original private
assets and a separate `verification/mac092-data` configuration selecting DebugRb.
The active one-car Debug configuration was unchanged. The primary failure was
still the HDR attachment/sample alias. After normal layer teardown, the debugger
stopped at `BizAssertReport` with `line==32`, before the assertion in
`display2/source/device/BaseDevice.cpp:30–34`.

The failing list was **C_ResourceTracker::m_TextureList**, containing129 entries.
Other lists not yet destructed contained257 vertex buffers,205 index buffers,
249 vertex shaders and364 pixel shaders; the surface list was empty. These613
shader entries are not evidence that613 built-in shaders leaked. Their ownership
can include the original shared FX shader cache. Lists declared after the texture
list had already been destroyed and were deliberately not interpreted.

All129 texture pointers and metadata names, all vertex/index/vertex-shader
entries, and a bounded301 pixel-shader entries were recorded in the ignored
`_work/runtime/verification/mac092-lldb.log`. Names include `tide_02`,
`icon_shield_decoy`, `DefaultColour`, and full resource filenames such as
`graphics/textures/flares/lightflare_diff.texture`. One sampled texture wrapper
still held an actual native Metal texture with a strong reference. The owned
inferior was killed, LLDB quit, the exclusive slot released and TCP6317 checked
free. No assertion, tracker or resource state was altered by the diagnostic.

## Source authority and dependency edges

| Owner | Acquisition and retention | Original release contract / native implication |
| --- | --- | --- |
| Resource manager cached payloads | `util/resource/source/ResourceBase.cpp:237–251` increments/decrements real handle references; the final release calls Evict. `Manager.cpp:409–425` queues a deadline in resource-cache Update epochs. | `Manager.cpp:176–185` normally evicts one entry asynchronously per worker iteration. Update only advances the epoch, signals work and processes pre/post stages. A single application Update is not a completed eviction barrier. Preserve the refcounts and use the original Evict/PreDeleting/Deleted callbacks. |
| Loaded texture resource | `graphics/TextureResource/source/TextureResource.cpp:73–99` owns a display texture and names it with its resource filename. | FreeResourceData55–59 destroys the actual GPU texture and deletes the wrapper; a live display device is required. GetEvictDelay62–65 returns0, which still uses the asynchronous worker protocol. |
| Render-data model/archive | `gamesys/game_resource/source/ModelResource.cpp:173–179` owns RenderModel; `graphics/world_render/source/RenderModelData.cpp:383–387` owns its target Scene. Target archive texture names are assigned at `renderingdata/source/noninterface/target/archive.cpp:925/1061`. | Target Scene destruction (`target/render.cpp:193–198`) destroys resource blocks. `target/resources.cpp:15–35/52–57` releases their actual vertex/index buffers and textures. Short names therefore differ from full TextureResource filenames; metadata alone does not identify every retained parent model. |
| UI bundle | `amax/library/Ui/UiManager.cpp:943–956` loads actual BPR bundles; native `display/source/bundler/ResourceBundleSingle.cpp` lowercases bundled names. | UiManager destructor1943 calls DestroyAll. Bundle destruction39–51 destroys each owned texture. That ownership differs from deferred ResourceManager payloads; do not force-free all named textures as if they shared one owner. |
| Static FX preload manager | `graphics/fx/source/Manager.cpp:154–173` retains each newly preloaded effect at reference1; original pointer-list entries stay null for new preloads. An existing effect instead gets a separately recorded acquired reference. | ReleaseAllGlobalResources releases recorded entries, not the manager's original preload pin. The old namespace-static Manager destructor43–53 ran after device destruction and merely deleted objects; original ReleaseEffect220–246 also calls OnEffectRelease. Native shutdown must explicitly release the manager's own pin through that complete method. |
| FX shader payload | `graphics/fx/source/Shaders.cpp` shares shaders and counts references. | `System.cpp:573–591` OnEffectRelease releases every pass shader; Shaders.cpp114–150 destroys the actual GPU shader after its last shared reference. This is the owner path, not tracker removal. |

The proven dependency direction is **resource payloads before final FX cache,
then device**. `renderingdata/source/noninterface/target/render.cpp:688–691`
releases its effect through FX::ReleaseEffect. Conversely the FX subtree owns
parameter data/microcode/shaders and raw GPU sampler arguments, with no
Resource::C_Handle or TextureResource dependencies. No second resource drain is
needed merely on the assumption that FX destruction releases resource handles.

The old order destroyed the application device before CoreSystems destroyed its
ResourceManager (`amax/game/Main.cpp:256–265`, CoreSystems::Shutdown). Moving the
whole core owner earlier would also violate other borrowers. The bounded fix
therefore retires its unused payloads while retaining the core owner.

## Implemented native boundary

`Resource::C_Manager::NativeDrainUnreferencedResources()` is a main-thread
shutdown barrier after creator/client retirement. It joins the owned loader,
reports any real worker exception, and releases only zero-reference resources.
A pending load that finished or failed before Join can enter PreDeleting without
performing an unused PostLoad. Existing HasDonePostLoad still determines the
paired PreFree callback. Cache epochs advance directly to existing eviction
entry deadlines; no simulation ticks, sleeps or handle-count changes are used.
Normal deletion callbacks process dependencies that become unused while their
parent payload is freed. Remaining live handles are named and cause failure.
The original worker restarts afterward so an application retry on the same core
owner can load again; its auto-reset quit signal is consumed before restart.

`FX::Manager::NativeShutdownCache()` tracks and releases only native manager
preload ownership, through original ReleaseEffect. It preserves and reports
surviving client references, resets preload state for retry, and is idempotent.
The marker is runtime-only in ReloadableEffect; original PCFX serialization and
Windows behavior are unchanged.

ApplicationBase::Shutdown invokes the resource barrier after actual user/layer,
renderer and debug-render retirement, then the FX cache shutdown, before device
destruction. Either failure blocks device destruction with an explicit error.
No resource registry is cleared and no live client's payload is forcibly freed.

## Focused validation

The existing authored FX test now exercises preload ownership separately from a
live client, refusal to consume that client, original shader cleanup, repeat
shutdown and new preload across two lifetimes. Debug tracking also checks exact
vertex/pixel shader counts return to baseline.

`tools/macos/resource_shutdown_test.cpp` uses actual engine loading/handle/state
machinery with small authored files and real native GPU textures. It checks a
live handle survives a refused drain, long cache-delay retirement, cascading
parent/child handles, canceled pending work, a decoder returning failure, an
actual loader exception reported as failure, successful loading after that
exception, repeat shutdown, balanced payload allocation and PostLoad/PreFree,
and Debug GPU counts returning to baseline. It starts no game/Artery services.
Both fixtures pass private Debug, Speed, Debug+UBSan and Speed+UBSan runs (8/8);
the three changed production source files also pass syntax probes in all four
configurations (12/12). These checks establish the bounded owner mechanisms; the next integrated game
run must still prove the original retained resources all retire and no later
ownership boundary fails.
