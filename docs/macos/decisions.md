# macOS decisions

## MAC-D01 — Native macOS phase (accepted by user, 2026-10-26)

Use BlurRebuilt as the technical base for native macOS work on a local branch. The user specifically excludes a Wine wrapper. New platform work lives beside the engine modules it extends; new tools live under tools/macos. Build products live under _work. No inherited Windows code or task is silently redefined.

## MAC-D02 — First native milestone (implementation choice)

Build the Mac bootstrap before the full game. It must identify itself as a bootstrap, never report gameplay as implemented, and fail explicitly on unsupported data formats. Validate actual archive layouts from the source serializers and Metal command execution/readback on the real host. Leave the full Windows build unchanged on non-Apple hosts. A working probe is not a playable game.

## MAC-D03 — Publication status

Nonprofit intent does not establish permissions. The prior source licensing audit remains unresolved. All work in this session is local; no license is assigned to inherited code, no GitHub fork is created, and no code or release is pushed. This is a record of scope, not legal clearance for local use or distribution.

## MAC-D04 — Continue source-only porting (accepted by user steering, 2026-10-26)

The Windows installation is not a prerequisite for continuing native compilation,
platform services, authored shader work or isolated engine verification. Do not
keep asking the user to transfer it while independent source work is available.
Retail assets are needed later for actual menu/track/race validation. The source
tree contains some tool/test art, but no runtime .pak archives were found under
code/. An official downloadable PC demo/data package was not located in the
initial search; the publisher's demo announcement describes in-store events.


## MAC-D05 — Complete-game active goal (explicit user request, 2026-10-26)

The active goal covers the actual native game: integrated engine and renderer,
startup, one playable race, saves and LAN verification. Source-only milestones
remain intermediate. Do not mark the goal complete for a bootstrap, component
tests or compilation alone. The user requests sustained autonomous work without
repeated small-step confirmations. Publication status in MAC-D03 is unchanged.

## MAC-D06 — Prefer existing open-source implementations (explicit user steering, 2026-10-26)

Use established open-source components wherever they improve the port and avoid
rebuilding platform machinery. Existing zlib/glslang/SPIRV-Tools/SPIRV-Cross
reuse continues. SDL3's native audio device streams are selected for the PCM
output backend because the original engine already owns its mixer; retain that
mixer and replace DirectSound output through a small adapter. Pin the upstream
release and preserve its notices automatically. Do not interrupt source work
for routine dependency choices. Track actual integration and remaining work
in dependencies.md; an acquired dependency alone is not game functionality.


## MAC-D07 — Reusable source-port toolkit scope (user request, 2026-10-28)

The user wants optimization without quality reduction and reuse beyond one game.
Audit the existing seams before extracting a generic package. Scope supported
by current work is source-available/reconstructed engine ports with explicit
engine adapters. A general Windows executable/ABI compatibility runtime is not
provided by this port. Reuse maintained SDL/shader/profiling components rather
than rebuild their functionality. Host OS, runtime ABI, renderer and asset schema remain
separate capabilities.

The staged host-package/external-client architecture in
optimization-and-reuse-audit.md is proposed, not an already extracted SDK or an
accepted replacement renderer. Routine read-only auditing and documenting this
proposal are within the user's current instruction and require no new approval.
Independent packaging, an authored second client, unchanged Blur acceptance and
a second real-engine adapter are the milestones for progressively broader reuse
claims. Keep independently authored toolkit code, inherited engine adapters and
third-party provenance explicit.

Publication record: the user subsequently authorized the personal public source
repository; public/main 178ffbbf was verified after build320 delivery. Thus the
local-only publication state in earlier D03/D05 is historical. This records that
explicit user steering and source-only publication; it assigns no blanket new
license to inherited engine code. No standalone toolkit repository or API has
been published by the audit.


## MAC-D08 — Two bounded optimizations and independent host (accepted by user, 2026-10-28)

Implement exact immutable Metal depth/stencil-state reuse and one-read native
DDS2D upload without changing shaders, effects, formats, resolution, simulation
or synchronization. Validate through the real loader/effect graph, GPU output
and muted original frontend/race/normal exit. Preserve contrary performance
results; fewer native factory calls and repeated reads do not imply a measured
FPS or resident-memory improvement.

Extract only the authored Cocoa window/event/CAMetalLayer host as a scoped MIT
CMake package with a separately installed/relocated second client. Blur consumes
the same implementation through compatibility names. Keep inherited renderer,
asset and gameplay adapters outside that license/scope. The independent host
package/client milestone from D07 is now implemented; a second real engine and
arbitrary Windows binary conversion are not established.

User authorized subagents, unchanged quality, sound-off actual-game tests and
completion without repeated small-step confirmations. Personal source-only
publication authorization remains in force. See
[qualification](optimization-host-acceptance.md).


## MAC-D09 — Hardware-guided Apple silicon optimization (user request, 2026-10-28)

Research and query the actual M4 Pro rather than select optimizations from a
chip-name string or assume Metal4 implies the newest GPU family. Use Apple's
primary architecture/resource/scheduling guidance and actual muted game
profiling. Apply the MLX principle of fitting work and ownership to unified
memory/execution; do not add a machine-learning dependency to a raster engine
without a relevant workload.

The first bounded change preserves a live encoder across identical native
attachment bindings while retaining original viewport/scissor and all required
resource/query/clear boundaries. Gate with exact GPU outputs/count controls,
complete coherent suites and original frontend/race/exit. Keep resolution,
shaders, effects, formats, synchronization, simulation and audio unchanged.

Hardware capabilities and profiler sample semantics are evidence, not FPS
claims. Keep contrary timings and the first timeout attempt. Do not infer
unique frames from duplicated HUD pairs, combine unified-memory estimates as
disjoint sums, pin hardware cores, expand the legacy worker slots or raise
thread priorities without measured evidence and ownership-compatible design.


MAC-D09 implementation refinement: preserve the pass-coalescing experiment
behind an explicit default-off switch. Its counts/pixels qualify, but the
measured race timings do not establish a gain and Apple documents a scheduling
overlap trade-off. The new default optimization instead targets the sampled
per-binding sampler key allocation, retaining exact key bytes and cold-key
ownership. No scheduler/allocator/interpolation synchronization change follows
from a three-second sample. Game Mode support is available through explicit default-OFF packaging
metadata; delivery preserves prior policy. OS fullscreen activation and benefit
are separate acceptance limits. The adverse347 and normal350 metadata-only
control do not establish a causal scheduling diagnosis.
