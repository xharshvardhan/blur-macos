# Native Options initialization and original keyboard layouts

MAC268 addresses the user's actual 0.1.1 Options stall and requested controls.
The attached frame showed a dark paused race background, the options bar, Select
and Back prompts, and no option rows. In the owned manual run, the first failure
was `Options.lua:46: attempt to call UpdateCustomKeyboardControls (a nil value)`.
The subsequent NodeListScan and SetSelection errors were consequences: Init had
stopped before constructing its menu. The UI remained in paused.lua plus
Shared/Options.lua with no popup or controller lock.

The native UI deliberately consumes the original PC UI data. Operating-system
selection must therefore not remove the portable PC keyboard callbacks that
those scripts require. `lua_Amax.cpp` now compiles and registers the existing
`UpdateCustomKeyboardControls`, `GetPCKeyboard`, and `SetSplitscreenKeyboards`
functions for macOS. The profile read, write, original input mapping refresh and
original menu flow remain in control; no screen is bypassed and no missing
function is replaced with a no-op.

`GetPCKeyboard` retains the Windows KLID query on Windows. NativeInput uses
physical Cocoa virtual key codes and the game's portable key names, rather than
Windows keyboard-layout identifiers or character translation. Its identifier is
therefore the empty string, which leaves Options_Control's physical-key labels
unchanged instead of fabricating a Windows language ID or applying its Windows
AZERTY character-label transform. This does not add character-layout-aware
remapping for non-US keyboards.

## Original PC choices

These are the two actual original layouts, from the recovered retail tables in
`GameControls.cpp` and the matching `Options_Control.lua` key rows. UI labels are
one-based: Layout 2 is the internal layout 1 requested for native play.

| Action | Layout 1 (internal 0) | Layout 2 (internal 1) |
|---|---|---|
| Accelerate / brake | Q / A | W / S |
| Steer left / right | Left / Right | A / D |
| Handbrake | Down | Space |
| Fire selected power-up | Right Control | Right Control |
| Fire forward / backward | W / S | Up / Down |
| Switch power-up | Up | Left |
| Discard power-up | D | Right |
| Change camera | V | Z |
| Look back | Tab | Page Down |
| Menu navigation | Arrows | Arrows |

New native profiles default to internal layout 1. Both the gameplay layout and
the UI's `IsCustom` selector agree; despite its name, the latter also stores the
standard 0/1 choices. Existing saved profile choices still deserialize unchanged.
The old version 1373 save conversion explicitly retains its previous layout 0.
For the existing local user profile, the acceptance run must choose Layout 2 in
the original Options > Keyboard screen and accept/save it normally. There is no
forced save migration and no global change to keyboard translation.

## Verification boundary

The existing native options-controls fixture injects real physical Cocoa key
codes through the native device manager and the original PC binding routine. It
now covers both layouts' throttle, brake, steer, handbrake, neutral/forward/back
fire, selection, discard and releases, plus independent original arrow menu
navigation. It specifically rejects arrow steering in layout 1 and preserves the
existing pad-remap regression. This fixture is input validation, not proof that
real power-up gameplay or the complete UI has run.

Coherent application acceptance now passes through ordinary Escape, the populated
three-row Options menu, all 13 Keyboard rows, Layout 2 acceptance, save, resumed
HUD and original desktop exit (MAC285). A fresh process loaded the saved layout
and exited normally (MAC283). See [the actual acceptance record](player-feedback-acceptance.md)
for the isolated profiles, loader evidence and corrected private observer guards.

## Remaining graphics contract audit

The same inventory found a separate unported graphics capability boundary.
`Options_Graphics.lua` requires `GraphicResolutionCount`, `GraphicAACount`,
`UpdateGraphicsOptions`, and `GetScreenResolution`. The dimension query is already
native. The other three remain Windows-only, as do ProfileOptions' graphics
fields. ApplicationPC implements `GetGraphicsOptions` and `ApplyGraphicsOptions`;
ApplicationMac has no corresponding apply API. Broadly enabling these branches
would not implement native resolution, sample count, fullscreen or presentation
changes. MAC271 now exposes the Graphics row only when all four callbacks are registered
functions. Native currently does not expose it. Audio remains row 3, the menu
count follows the rows actually created, and a remembered unavailable graphics
selection falls back to the original first row. Dispatch also rejects stale row
4, including capability loss after initialization. Windows with its full backend
and the console image-adjust/storage routes retain their original behavior.
Their capability/apply design remains a separate integration task; no successful
native graphics-options support is claimed.

The other reachable Options screens use existing shared callbacks: audio and
advanced audio use UpdateAudioOptions; speakers use Speakers; image adjustment
uses GammaCorrection; controller selection uses GetControllerConfigs. Parent
Options initialization also uses shared GetOptionsDefaults/Options and normal
camera, mode and social capability queries. The keyboard callbacks above were
the required missing family at the reported parent-screen failure.

MAC271 extends the existing Lua/UI fixture with the actual `Options.lua` source
executed by the original Lua 5.1 VM. A small authored UI host records created
rows, remembered selection, scale and ordinary message dispatch. Cases cover
the complete backend, each missing callback, a non-function callback, capability
loss after Init, unchanged keyboard/audio routes, frontend count scaling, stale
row fallback, and original PS3 image/Xenon storage behavior. This validates the
script contract; real menu rendering remains a separate runtime check. The
standard native game's Lua bundle custom command depends on every source Lua
file, so a normal `BlurNativeGame` build regenerates the embedded bundle.
