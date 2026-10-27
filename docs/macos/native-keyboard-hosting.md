# Native keyboard controls and LAN hosting

MAC316 fixes the compact Mac keyboard's Control/Shift actions without changing
physical key identities, saved profiles, serialized action IDs, or the selected
keyboard layout. The original PC bindings require **right** Control for the
menu's Y/Host action and neutral power-up fire, and **right** Shift for X/Refresh.
A Mac keyboard's ordinary Control key arrives as **left** Control. The native
Cocoa decoder already distinguished those keys correctly; the missing part was
an alternative at the game's action-binding boundary.

For macOS, `SetupAmaxUserControlsPC` now adds left Control to the stock
single-keyboard fire action and the primary menu's Host/Y action, and left Shift
to the primary menu's Refresh/X action. Right Control and right Shift continue to
work. The existing maximum/OR combiner means holding both sides produces one
pressed action, and releasing one side keeps the action held until both release.
The primary menu also accepts **[** / **]** for its left/right shoulder actions,
while retaining the original Insert / Forward Delete keys. Cocoa
`kVK_Help` now reaches the portable Insert key for extended keyboards. Raw
modifier, bracket and navigation-key values stay separate. Explicit replacement
bindings and the secondary split-screen keypad/menu bindings are not globally
aliased.

## Host a LAN game

Use the original menu flow:

1. Enter **Multiplayer**, select **LAN / System Link**, and press **Return**.
2. If **No LAN games found** appears, press **Return** or **Space** to close
   that popup (Escape does not dismiss it). In the LAN browser, press **Control**
   for **Host Game**. The original PC
   prompt may still say right Control; either Control works with MAC316.
3. Select **Custom Game** using the arrow keys and press **Return**. This invokes
   the game's `NetRace.CreateLanServer` and opens the lobby on success.
4. In the lobby, use **Shift** for the options/car-selection menu. Once the
   original lobby enables Start, press **F1** to launch the custom race. Whether
   macOS requires Fn with F1 depends on the keyboard's function-key setting.

In the browser, **Shift** refreshes after the current search finishes,
**Return** joins the selected discovered game, and **Escape** goes back. Host
creation, discovery, readiness, and multi-machine networking still use their
existing backend contracts; this key repair does not itself verify a second Mac
joining or completing a race.

The source chain is `GameControls.cpp`'s `kKeyLayoutMenuButtons` →
`C_UiManager`'s `E_UiButton_Option2`/Y dispatch →
`screens/Multiplayer/MpLan.lua`'s ButtonY handler →
`Multiplayer/Shared/MpModeSelect.lua`'s Custom Game branch.
`Multiplayer/MpLobby.lua` handles Start only when the local player is host,
`NetRace.CanStartCustomRace()` is true, and the original lobby state permits it.

## Keyboard map

The selected layout remains the one stored in the player's profile. In the
original Options > Keyboard screen, **Layout 2** is the WASD layout (internal
index 1). MAC316 does not reset or migrate existing profiles.

| Action | Layout 1 | Layout 2 (WASD) |
|---|---|---|
| Accelerate / brake | Q / A | W / S |
| Steer left / right | Left / Right | A / D |
| Handbrake | Down | Space |
| Fire selected power-up | Either Control | Either Control |
| Fire forward / backward | W / S | Up / Down |
| Switch selected power-up | Up | Left |
| Discard power-up | D | Right |
| Change camera | V | Z |
| Look back | Tab | Page Down |
| Navigate menus | Arrow keys | Arrow keys |
| Select / confirm | Return or Space | Return or Space |
| Back / pause | Escape | Escape |
| Menu X (refresh / options) | Either Shift | Either Shift |
| Menu Y (host / alternate option) | Either Control | Either Control |
| Start | F1 (Escape also shares Cancel) | F1 (Escape also shares Cancel) |
| Back-button action | F2 | F2 |
| Menu triggers | Page Up / Page Down | Page Up / Page Down |
| Menu shoulders | [ / ] (also Insert / Forward Delete) | [ / ] (also Insert / Forward Delete) |

Page navigation, Forward Delete, and function keys may require the keyboard's
standard macOS Fn combinations or an extended keyboard. The bracket alternatives make both menu shoulder actions available without
dedicated Insert / Forward Delete keys. Extended Insert/Help is also decoded.
There is no arbitrary substitution of driving letters or new Fn shortcut. Key events use
physical Cocoa positions; this is not character-layout translation for non-US
keyboards. Existing PC prompt text and the original keyboard-options labels are
retained, so some labels still name only the right modifier even though the
native stock action now accepts either side. Shoulder prompts may likewise name
Insert / Delete rather than the additional [ / ] keys.

## Verification boundary

The existing input fixture checks independent left/right Control and Shift
state, two-key release ordering, and focus loss. The original options-controls
fixture exercises both stock keyboard layouts, menu Host/Y and Refresh/X,
modifier and shoulder OR/debounce/release behavior, Return/Escape, camera/look-back,
explicit right-only replacement bindings, and secondary keypad preservation.
The Cocoa window fixture routes authored real `NSEventTypeFlagsChanged` events
through `BlurEngineView` into those physical states, and routes actual authored
Help/Insert and bracket keyDown/keyUp events without global aliases. These fixtures do not
replace actual LAN host/join or power-up gameplay acceptance.

Private object probes and coherent runtime results are recorded in the MAC316
ledger entry; no gameplay or LAN success should be inferred from compilation.


Final integration: both complete Debug/optimized suites pass148/148, including
original control bindings and actual authored Cocoa modifier/Help/bracket events.
MAC322 uses the frozen0.1.4/build320 executable to press original LCONTROL Host,
create the real20-capacity one-player lobby, leave and exit normally (code0,
116.661s). The isolated source profile/save is unchanged and every test launch
is muted. This verifies the Host action and original local session creation;
it does not verify a second machine joining or power-up projectile behavior.
