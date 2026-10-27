# Backend guard startup audit (MAC166, read-only)

The actual MAC161 particle draw has a real declaration/target and null vertex
and pixel shader bindings. MAC162 traces their required owner to a native-
excluded creation block that nevertheless reports success. Backend identity
checks need to distinguish a shader data/API choice from an OS service.

A bounded scan across game, game libraries, graphics and gamesys records75
PCD3D11/PS3GCM guard sites in the private mac166-backend-guards.txt inventory.
Most matches are backend header aliases or PS3 CG-header inclusion, rather
than omitted allocation. These must not all be widened to macOS.

The selected ParticleManager required set was the concrete whole-creation
falseReady gate; MAC162 now creates/validates16VS+7selectedPS and owns false/
throw rollback. Dynamic light manager shader creation is unconditional and
bool-checked. Display BasicRender has an explicit file-loading else branch
with every required result checked, so its embedded-function guard is not
a missing allocation. Default VertexShaders has its own transactional native
file-loading branch; native required destruction is separately selected.
Font/TextureRender and OcclusionQuery explicitly include native creation.
The initial PS3-only conditions in primitive/debug/depth renderer files are
CG-source inclusion, while their actual function-creation calls remain live.

Render::Sky::C_SkyDome has a similar legacy PS3-only shader creation block,
but bounded owner lookup finds its construction only in sample/test-section
applications rather than the selected actual GameWorld. Sample standard/
variance shadow renderers have commented shader creation and are likewise
not constructed by the selected game path in this lookup. These are explicit
limits for future samples, not grounds to mutate unused renderers or claim
coverage. The actual game uses its own sky/lighting pipeline.

This is source classification, not proof of all shader permutations or a
replacement for the full archive-backed renderer run. MAC162's actual manager
fixture checks required creation, failure/retry/recreate, real GPU bindings
and retirement; coherent actual-game completed-frame and natural-race gates
remain pending. No production code, public format or platform-wide PC define
was changed by this audit.
