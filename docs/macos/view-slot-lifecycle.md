# MAC130 — Scene view registration and renderer materialization

The actual MAC132 before-unwind snapshot confirms eleven scene views but only
ten DynamicEffect renderer slots. The first Main view being removed is index0
and initialized. The final LightTrails view is index10, uninitialized, with no
renderer views. The other ten scene views are initialized. Evidence is the
JSON at `_work/runtime/verification/mac132-game.log` around312, followed by the
actual ViewManager teardown stack. The diagnostic killed its owned inferior;
it is not evidence of clean game exit.

The original scene registers a view immediately in `C_SceneGraph::AddView`,
while `C_View::Initialise` creates renderer children only when explicitly
called or on the first `BeginFrame`. `T_Renderer::AddView` originally appends
children in initialization order. `RemoveView` nonordered-erases the scene
and renderer vectors using the scene index. Consequently, lazy registration,
out-of-order initialization and early cancellation violate the shared index
assumption. In the observed case, removing Main moves lazy LightTrails into
index0 in the scene but moves an initialized cube view into index0 in the
renderer. A later removal indexes beyond the renderer vector. Renderer owner
reordering, forcing a first frame, or skipping the missing slot would leave
the index contract broken.

Native registration now reserves an empty slot in every typed renderer before
publishing the scene view. Materialization fills that parent's existing index,
independent of initialization order. Disabled views retain their reserved empty
slot. Removing either a materialized or lazy view retires the same indexed
slot and applies the original nonordered permutation. The remaining parent's
index and child identity therefore stay aligned.

Registration failure removes only the already reserved prefix before deleting
the unpublished scene view. Materialization failure destroys the failed
child's acquired resources and all children already created for that parent,
clears the parent's raw child references, and retains the reserved slots.
`m_Initialised` is published only by the existing successful return path, so a
failed view can either retry or be cancelled. Native Dynamic view constructors
initialize scheduler handles to NONE; Destroy releases acquired handles once
and resets them. This makes partial Create rollback and repeated cleanup safe
without modifying scheduling or frame cadence.

The repository has one typed scene renderer: DynamicEffect. LightTrails,
AuxFlares and CrowdFlags inherit the untyped I_Renderer and have no child-view
Add/Remove overrides; their native reserve/reset hooks remain empty, matching
their lack of a typed slot table. Existing Windows/console methods and virtual
layout are unchanged by native guards. The native I_Renderer interface adds
virtual hooks, requiring a coherent rebuild of all native consumers. Normal
GameWorld creation registers its renderers before `CreateViews`; dynamic
registration of a new typed renderer after views already exist remains outside
the recovered lifecycle contract and is not newly claimed as supported.

`tools/macos/view_slots_test.cpp` uses real SceneGraph/T_Renderer methods,
real scheduler groups, actual Dynamic views, actual Display/FX services and the
real LayerManager first-error channel. Its authored child renderer injects
failure at acquisition boundaries; it does not replace production ownership.
It checks:

- lazy cancellation and initialization out of registration order;
- disabled slots and matching nonordered removals;
- reservation failure without publishing a scene view;
- failure after one renderer child succeeded, exact group/child rollback,
  successful retry and one-time initialization;
- actual Dynamic view early Main removal with lazy final LightTrails;
- never-created and repeated actual Dynamic cleanup;
- real layer Create exception reaching main Update before Ready/Running/render,
  then one-time layer/view cancellation and true process exit.

Four affected production objects compile in Debug, Speed and both UBSan
configurations:16/16 private probes pass. The focused Debug fixture compiles,
links and runs through the exclusive wrapper with process exit0. Its private
link replaces SceneGraph, RendererInterface, Dynamic View and Dynamic Renderer
objects to keep the instantiated native typed vtable coherent. No other scene
renderer family is instantiated by that private fixture. Integration requires
the normal full-game/bootstrap-first closure and GameLogic includes before
Amax, no arguments. Root coherent build/CTest and a clean actual-game exit
remain pending. This lifecycle fixture is not a completed frame, driving or
race-finish result.
