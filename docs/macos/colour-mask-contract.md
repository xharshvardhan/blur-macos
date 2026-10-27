# Colour write mask source contract (MAC150)

The actual MAC149 run reaches race telemetry and then reports `Invalid colour
write mask`; no numeric mask or before-unwind caller has yet been captured by
this audit. The following source route is a candidate, not a diagnosis from
that error string alone. No production changes are made here.

## Width and state boundaries

Upper Display channel bits are R1/G2/B4/A8 on PC and native. The PS3 branch has
separate GCM values. `E_ColourWriteEnable_Default` is UINT_MAX in upper Display;
this is a policy sentinel, not a literal GPU mask. `C_DeviceBase` initializes
the actual default to RGB7. Blindly truncating the sentinel to15 would enable
alpha writes, changing that policy.

Lower Metal accepts literal0..15, including zero. Its single-target state
setter updates all current colour masks, and its indexed overload accepts only
target0..3 plus literal0..15. `C_ColourState::SetColourWriteEnable` simply stores
a U32 until applied through the same validated setter. These four small channel
bits and U32 retain their widths on native; the enum's `1L` shifts do not imply
an LP64 widening fault here. No complement/negative-mask caller was found in
the selected render/effect/scenegraph source inventory.

Upper native device scalar/indexed setters already resolve Default using the
real per-device default, then call lower Metal. The original PC singlethreaded
context forwards to that upper device. The recovered DX11 Display2 bridge also
forwards to Game()->SetColourWriteEnable; its native device resolves Default
before masking the descriptor's channel bits. The low-nibble conversion in a
PC D3D descriptor must not be mistaken for the upper sentinel's policy.

## Concrete remaining context route

Native `C_ContextBase::SetColourWriteEnable(flags)` forwards flags directly to
lower Metal, so upper Default would reach the strict literal-mask guard.
`SplashCamera.cpp::Render` explicitly restores this sentinel through
`pDisplay->SetColourWriteEnable(Default)` at its end. Its preceding draw uses
All, then restores the caller's PC default policy. This is the sole explicit
context-pointer Default restore found; other selected pass/effect restores
call Device(), whose native resolver already handles it.

Native `MetalGameContext.h` also exposes an indexed MRT mask overload which
forwards directly. No current caller passing Default to that added overload
was found, but it is a distinct upper policy surface to consider if the
identified class is repaired. Lower Display2 must continue rejecting arbitrary
high bits rather than treating every malformed value as Default.

## Required proof and proposed regression

Root's MAC151 LLDB capture must identify actual value and caller. UINT_MAX from
the SplashCamera context route would support restoring upper-context policy
resolution while retaining lower literal validation. A poisoned or unrelated
value requires tracing its producer instead.

The existing native blend-state fixture can cover real GPU output for all16
literal masks, Default restoring RGB with alpha preserved, a changed per-device
default (so the test cannot pass via hardcoded RGB), cache consistency and
rejected arbitrary high-bit masks. Actual SplashCamera/race execution remains
a separate integration gate.

## Upper Context inventory and source-authorized repair

MAC151 stopped earlier at an independent particle QuadList assertion, so it did
not capture a numeric colour mask. Root authorized repairing the independently
proven PC upper-context contract rather than attributing that stop to this
candidate. Native scalar Context now forwards to the actual upper Device
setter, as the original PC Context does. The native indexed extension forwards
to the Device indexed overload; both resolve the same real per-device Default
and update its cache only after validated lower acceptance. The lower literal
mask/target guards are unchanged. This is not a claim that MAC151 reproduced
SplashCamera or that every actual149 error is now explained.

The broader native upper-context inventory distinguishes other state policies:

| Surface | Current contract | Selected source evidence |
|---|---|---|
| Colour scalar/indexed | Default resolves through Device; literal0..15 retained | SplashCamera scalar restore; indexed added native API |
| Depth enable/write/compare, fill | Context casts literal state; Device resolves policy defaults | All selected explicit Default restores found use Device(), including deferred lights/mask and scene graph fill restoration; Context Default remains an unexercised API gap |
| Cull Default/Reversed | Resolves context-local cull state cloned from Device | Sky and particle Context restores rely on local state; mirror context can override it; do not replace by global resolution |
| Cull Front/Back | Device maps aliases relative to its default; Context accepts only literal/default/reversed | No selected Context alias caller found |
| BlendMode Default/Unknown | Shared Context applies original default-factor table; Device Unknown only invalidates cache | Existing GPU regression covers both distinct contracts; wholesale rerouting would change behavior |
| Alpha/blend boolean, blend factors | Literal enums only; Device conversions also reject unrecognized sentinels | No established accepted BoolDefault/factor-Unknown contract |
| Sampler Default address/filter | Device triple methods resolve per-slot defaults; Context full state is literal | Selected Default calls use triple methods fixed MAC067; no individual-axis/filter Default caller found |
| Alpha-to-coverage / point sprites | Explicit unsupported requested feature diagnostics | These are capability limits, not evidence that Default should be silently accepted |

The extended existing blend_mode_test uses real GPU readback for all16 scalar
and indexed literal masks, then RGB/Alpha/None/All per-device defaults. It also
checks cache preservation after invalid high-bit masks/target and confirms that
lower Display2 still rejects upper Default. All12 private source probes (three
TUs across Debug/Speed/DebugUBSan/SpeedUBSan) pass. Runtime results are recorded
in the ledger after the exclusive GPU queue completes.

MAC155 subsequently captured the exact actual path before unwind:
`SetColourWriteEnable(4294967295)` → upper `C_ContextBase` →
`SplashCamera::Render:504` → application OnRender. This confirms the source
candidate independently of the earlier MAC151 QuadList stop. Debug and
DebugUBSan private extended blend fixtures both pass with Metal validation,
exit0; UBSan uses halt_on_error. Logs are
`_work/build/macos/occlusion-agent-colour-mask/{macos,macos-ubsan}/runtime.log`.
Root coherent source integration and actual continued race remain separate.
