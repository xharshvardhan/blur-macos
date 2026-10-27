# Required lighting resources during resize (MAC312 follow-up)

The native application resize barrier runs the original full world resize chain.
The lighting stage must report a required allocation failure before a later frame
uses incomplete render targets. `Graphics/Lighting/Prepass.cpp::Resize` previously
ignored both `CreateVolatile()` results, unlike the checked postprocess stage.

The native branch now throws a distinct error for failed deferred point-light
resources and failed deferred-manager render targets. The original destroy,
view setup and creation order remains intact. Windows and console branches keep
their recovered behavior. This changes readiness reporting, not render formats,
viewport dimensions, simulation timing or fallback policy.

## Unpublished wrapper ownership

The PC-and-other `Deferred::C_Manager::CreateRenderTargets` branch creates a new
colour render-texture wrapper, depth-surface wrapper and depth-texture wrapper
before calling the corresponding device creation API. A false result previously
returned without publishing or deleting that new wrapper. Native code now keeps
each of these three wrappers in a `unique_ptr` until successful publication in
the view. The original renderer rollback owns earlier published resources. The
same local wrapper ownership also applies when creation throws.

Existing false-return and success paths retain their explicit device-ownership
release. This patch does not make the whole creation callback exception-safe:
`CreateRenderTargets` still lacks a scope release for an exception, and
`OnCreateVolatile` uses manual tracker PUSH/POP. No injected exception acceptance
is claimed. Those existing exception-scope gaps are separately reported to the
integrator; the regression below targets genuine device false returns.

## Focused verification

`tools/macos/world_transaction_test.cpp::LightingResizeProof` uses the original
lighting prepass, deferred manager, point-light renderer, native GPU and mounted
original shader resources. It checks the required point-light base absence,
then creates the real bases and successfully resizes. It replaces setup index 0
with actual invalid colour or depth formats to make the native device reject
creation. Both failures must surface the upper required-target error, leave
volatile readiness false, and restore the tracked GPU resource baseline. The
test restores original setups, retries successfully, and destroys repeatedly
back to its initial baseline. No allocator fault hook, fake target or viewport
clipping is used.

Private compile probes passed for all three changed translation units in Debug,
Speed, Debug UBSan and Speed UBSan: **12/12 PASS**. Results are preserved at
`_work/build/macos/lighting-resize-agent/source-probes.json`. The existing
`mac_world_transaction` target and its original-data, authored-shader and scratch
arguments are unchanged. Coherent GPU execution is owned by the parent and is
pending at this source freeze. These compile results do not prove a foreground
manual resize or actual-game exit.

The first coherent Speed run rejected the fixture's healthy precondition before
its injected failures: the standalone view had index 0, causing the original
`SetupViews` mappings `+index` and `-index` to alias the lighting-only setup. The
fixture now temporarily uses the application's nonzero view-index convention
(index 1), restores it afterwards, and queries normals/depth through that
positive index. This was an authored host prerequisite error; no production
resource check was weakened. Corrected runtime acceptance remains pending.

Parent coherent Speed strict Metal rerun with nonzero view identity passes
`mac_world_transaction` (exit0, 1.45 seconds). This includes healthy real
lighting resources, both genuine lower creation rejections, upper error
propagation, retry and cleanup to the resource baseline. Complete suites are
recorded in final integration below/ledger; this focused result is not manual
foreground resize acceptance.
