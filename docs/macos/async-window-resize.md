# Native resize during asynchronous graph creation (MAC312)

The foreground 0.1.3 startup failure was reproduced at
`C_Context::Clear → C_LightingPrepass::Begin → C_MainView::RenderPrepass1` after
resizing/maximizing during startup. The prepass had captured view dimensions
when its graph was created, while a later viewport used the resized framework
view. Strict viewport validation correctly exposed that inconsistency.

The lifetime gap was in the original layer dispatch path. Async Create builds
the complete graph and publishes its layer as Ready. The main thread later
moves Ready layers into the Running set. Resize messages enumerate only that
Running set. Resizing between graph capture and this handoff can therefore
miss a newly created world. Resizing the shared renderer registry concurrently
with a creator or destructor also crosses the graph's ownership boundary.

`C_LayerManager::NativeResizeLease` now tries to enter a native lifecycle
transaction. Async Create holds the same transaction through Ready/failure
publication; async Destroy holds it through callback and object deletion.
The lease refuses a pending Ready layer until ordinary main-thread Update
publishes it. The application retains and coalesces the latest requested size
while either condition holds. After admission, the existing framebuffer,
renderer, framework-view and full owner Resize path commits together. The
world's existing Resize chain rebuilds both postprocess and deferred view
mappings; no prepass-only target patch or oversized viewport clamp is used.

The lease must not block: the main render loop already owns Display when it
checks a resize, while a creator may hold lifecycle ownership and wait for
Display. The try-enter/defer path avoids that inverse wait. Ready-list locks
only inspect/publicize ownership and are released before any Resize callback.
Lease destruction releases the transaction when an owner throws. Creation
failures still reach the original checked main-thread failure handoff.

Cocoa changes `CAMetalLayer.drawableSize` before invoking the resize callback.
While a graph owns creation, ApplicationMac restores the normal presentation
layer to the committed framebuffer dimensions and reapplies its presentation
interval. The layer's view scales the old image to the new bounds; subsequent
old-size frames remain presentable. Successful resize commit installs the
latest physical dimensions through the ordinary volatile-device path.

`layer_resize_test.cpp` uses the actual LayerManager worker and gated callbacks
to verify nonblocking Create/Destroy exclusion, Ready handoff, retained Resize,
failed-Create visibility, owner-exception lease release and exactly-once cleanup.
Its private CPU compile/link/run passed. `application_resize_test.mm` uses an
actual minimized native application, an async creator with the real
lifecycle→Display ownership order, two independently cached real render targets,
and two resizes while creation is gated. It checks continued old-frame presents,
latest physical-size admission, both resource extents before first graph render,
exact GPU pixels and clean shutdown. This is an authored graph-resource fixture,
not a claim of complete Amax startup coverage. Its syntax probe passed; parent
coherent builds, GPU execution and real foreground startup acceptance remain
required. No shared build or hardware execution was performed by this subagent.

The original PC contract supports target-sized viewport reset, not silent
clipping of an invalid explicit viewport:
[D3D9 SetRenderTarget](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-setrendertarget),
[D3D9 SetViewport](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-setviewport).
The rebuilt DX11 adapter's full-resource clear differs from
[D3D9 Clear](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-clear)
and cannot justify accepting stale oversized viewport state.


Parent integration: both coherent Debug/Speed builds complete. The first Speed
focused matrix passes all six selected checks, including the actual GPU resize
fixture, CPU layer lease, original input/options actions and world/resource
teardown. The Cocoa event fixture is registered as `mac_cocoa_window` and is
included in subsequent final integration, not that six-check selection.

An ABI-safe Speed negative control compiles the exact pre-repair ApplicationMac
body against the current LayerManager/header layout, replacing only that object
in a copied current application-host archive. The same actual GPU application
fixture fails cleanly with exit 1: `Resize committed while graph creator was in
flight`. The repaired coherent fixture exits 0. Strict Metal validation is on;
shared archives are unchanged. Private source/hash/link/runtime evidence is in
`_work/build/macos/mac312-old-resize-control/`. This differential establishes the
resize transaction regression, rather than assuming clipping a stale viewport
would be safe. Final integration adds required-lighting failure propagation and
completes the compact-keyboard menu alternatives before final runtime acceptance.


Final coherent source320 passes both complete148-test Debug/optimized suites,
including actual window resize, lifecycle, resource-failure and strict GPU
checks; the strict focused matrix is8/8. The final source-qualified actual LAN
startup/hosting run322 exits normally. These actual-game runs use the original
archive/renderer graph at their ordinary window settings. The specifically gated
resize-during-creation differential is the real Cocoa/GPU authored graph fixture,
not a claim that the exact timing of a human maximize was replayed in Amax.
