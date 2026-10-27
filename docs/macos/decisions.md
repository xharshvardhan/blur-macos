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
