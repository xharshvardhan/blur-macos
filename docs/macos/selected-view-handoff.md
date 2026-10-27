# Selected PC final-view handoff (MAC158)

The bounded MAC154 audit found two final-view branches left Windows-only in
C_GameWorld::EndRenderSplitscreenWithoutAA. Native ApplicationBase already
executes the original PC RenderSplitscreenViewWithoutAA: publish window
colour/depth/stencil globals, begin postprocessing, RestoreWindow, end
postprocessing, then clear the current view. PostProcessLayer already selects
the PC prepass-gated motion/DoF graph on native. These two omitted bodies are
PC renderer policy, with recovered retail T232 source authority, rather than
Win32 services or serialized structure sizes.

Native now selects the existing PC bodies: refresh view-dependent FX just
before the final postprocess, then restore prepass depth/stencil surface1
for exactly one view at detail2–4, which is the existing fullscreen UI stencil
contract. ViewManager borrows the world's actual LightingPrepass. No surface
is created here and no depth/cull comparison is invented; the original
GetDepthStencilSurface and SetDepthStencilSurface remain responsible. The
quality/view conditional and all original PC/console bodies remain unchanged.

This source-contract repair is distinct from an observed failed draw. It has
no claim that a lower standalone graph fixture executes this derived world
method. Four-configuration source probes and the coherent actual native game
frame under strict Metal are the validation gates; the actual frame, keyboard
lap and clean exit remain pending until that run succeeds.

All four private real GameWorld translation-unit probes pass: Debug, Speed,
DebugUBSan and SpeedUBSan. Original Windows/console bodies and native object
layouts are unchanged. Coherent actual-game validation remains pending.
