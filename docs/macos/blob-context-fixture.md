# Blob fixture context ownership (MAC287)

The MAC279 Speed suite crash in `mac_blob_shadows` occurs during fixture setup,
before blob construction or rendering. Apple crash report
`blur_mac_blob_shadows_test-2026-10-27-151027.ips` identifies main thread
`SceneGraph::C_Context::CloneState(const C_Context*) + 40`, called by `main`,
with a read at `0x598`. This is separate from the MAC275 framework-worker
allocator finalization race; it is not a shutdown failure.

The fixture constructed a standalone SceneGraph context, called its inherited
`Initialise`, then called `CloneState(nullptr)`. The original constructor in
`graphics/scenegraph/source/Context.cpp:89–94` initializes RD/FX/Display pointers,
but does not assign its owning graph. The null-source clone is a supported
original operation: `Context.cpp:110–134` reads the **existing owning graph**'s
render flags before cloning the registered device subcontexts. The standalone
fixture had never acquired that owner. A passing Debug run does not make this
uninitialized read valid, and the precise reason it previously escaped detection
is unproven.

The original ownership path is `graphics/scenegraph/source/SceneGraph.cpp:127–130`:
`C_SceneGraph` assigns `m_pSceneGraph = this` before initializing each context.
`C_SceneGraph::GetContext` selects the actual calling scheduler thread's context
at lines 184–193. The fixture now creates a real graph and borrows its context;
the graph outlives the blob and resource-drain operations. This retains the
original null-source clone and render flags. It adds no production null guard,
synthetic owner, GPU fallback or changed shader assertions.

Validation: Debug and Speed fixture TU probes pass with their actual CMake
commands. Debug and Speed UBSan TU probes also pass using the matching blob
fixture commands plus the existing sanitized configuration flags (the sanitized
build trees do not currently configure a blob fixture target). The first attempt
to reuse the sanitized scene fixture command lacked the blob include path; that
probe-host mismatch was corrected without source changes. No private link or
runtime was launched. The parent owns coherent fixture rebuilding and serialized
GPU reruns. No runtime, game or rendering-success claim follows from source
compilation alone.

Root integration: both coherent Debug and Speed GPU reruns pass after the actual graph-owner correction (MAC284). The final MAC289 suites include this fixture; see [feedback acceptance](player-feedback-acceptance.md).
