# MAC211 — Original normal desktop quit route

Actual MAC198 reached the normal Master menus/owned-allowed garage and completed
assertion-enabled native application shutdown with exit0. Its605.95s duration
came from the application's elapsed600s guard. The old private quit helper
waited for a separate popup Lua screen/context that never appeared, so that run
did not prove the normal desktop confirmation route.

## Source authority

| Boundary | Original source contract |
| --- | --- |
| Automation command | UIAutomation.cpp21–31 broadcasts the existing Lua command message. UIScreenControl.cpp1201 calls HandleCommand in each active handler; GUIBank.lua294–313 temporarily selects that context's GUI/SCUI. |
| Popup state | CustomPopupData.lua5925–5953 selects CurrentType/CurrentData, saves CallingGUI/SCUI and calls AddPopup. Lua_Screen.cpp176–196 initializes the separate popup control. It does not replace the underlying ordinary GetCurrentScreen value with a separately broadcast popup screen. Main0 holds CustomPopup, PopupGUI and SCUI_Popup in the actual observed route. |
| Leave SP | SpMain.lua493–498 MenuBack opens ExitApplication. Its enum is1 (UIEnums.lua174). CustomPopupData.lua76–118 provides Yes0/No1 and defaults Yes; only the original Yes callback schedules StartScreen and QuitSpGame return mode. |
| Start mode back | StartScreen.lua180–185 starts the returned SP profile at mode-select stage2. Its MenuBack at429–444 waits while GUI.camera is a table, then changes to original press-start stage0. |
| Desktop confirmation | StartScreen.lua322–327 MenuBack at stage0 opens KillApplication204. Its source-backed PC popup at CustomPopupData.lua5911–5924 has no options menu: MenuNext is Yes and runs the original UISystem.SystemQuitGame(true) callback. |
| Real popup key | TheCustomPopup.lua297–334 ignores finished/error popups, marks MenuNext finished, invokes the actual next_CB and records selected options when present. PopupEnd415–430 sends ordinary popup messages and clears CurrentData. |
| Input | NativeInput.h15–20 and NativeInput.mm123–130 queue a150ms native key tap that begins on the next input update. ESC and ENTER are existing aliases for the real portable keyboard buttons. No OS activation is needed. |

The source ordinary path is finite Back keys from garage/tier/event/SP menus,
ExitApplication1 confirmation with observed Yes selected, StartScreen stage2
Back after its original camera finishes, then stage0 Back and KillApplication204
confirmation. It is not a direct call to a screen/quit/progress setter.

## Private operator correction

Only ignored `_work/runtime/research/quit_master_frontend.py` was repaired;
production sources and `master_frontend_operator.py` remain unchanged. The
operator reuses that module's passive Observer, fresh byte-offset/tagged replies,
log identity/fatal/owner-exit checks and127-byte UTF8 command cap. It waits for
an actual root-harness StartScreen observation before dispatching any IPC.

Popup observation now reads Main0's current type/data, runtime enum identity,
underlying screen, finished/error/cancel state, sender identity and actual next
callback. Type1 also requires the original options count2, default Yes and
currently selected Yes. Type204 requires StartScreen and absent options. An
unknown popup, changed options or mismatched origin never authorizes a key.
Profile/loading/movie states and inconsistent/stale observations wait without
input. Underlying ready screens use only Back; no selection is rewritten.

Every action uses one finite ESC or ENTER Tap with a unique true acknowledgement.
Execution-time guards retain context/screen for ordinary Back, or Main0/type/
unfinished state for confirmation. The longest popup204 command is exactly127
bytes at the largest bounded tag `mffff`. The full multi-query snapshot is not
an atomic transaction: its final context/type/selection fence and one-second
input expiry bound that interval; the private operator assumes no concurrent
keyboard operator for the same game. Unknown/rejected dispatch outcomes stop
without repeating a possibly consumed key. Repeating identical actionable state
also never causes an automatic second key.

The operator has a16-key and at-most180s budget. After an acknowledged204 Tap it
sends no more IPC during teardown, observing only its owned log/status. Operator
success requires an observed process exit0 with no primary failure; the root
runtime must still compare timing/log evidence to distinguish original desktop
quit from any elapsed guard. A queued Tap acknowledgement alone is not proof
that the callback completed or that cleanup succeeded.

Invocation, after the root's actual game/harness launch:

```
python3 _work/runtime/research/quit_master_frontend.py macNNN --seconds 145
```

`--self-test` passes pure normal/popups1/204 decisions, unknown/stale/transition/
wrong-selection refusal, true integer identities, all passive command sizes,
maximum action sizes, and rejected key acknowledgements. `py_compile` passes.
No game, Automation client, shared build, archive, GPU or production source was
run/changed during MAC211. Actual desktop confirmation verification belongs to
the root's next owned native game run.

## Actual populated verification follow-up

MAC243 completes the normal20-car3lap/non-medal results/save return and then
uses the original SP exit and desktop204 confirmations (7finitekeys). Actual
process exits0/1546.35s with no primary before its3300s guard. MAC249 and the
correctly signed relocated MAC251 also quit from the owned-car garage through
ordinary Back/confirmations, with actualexit0/246.35s and0/196.35s respectively.
251 retains a valid strict bundle signature/manifest after exit. These runs
verify SP exit confirmation after populated play and the deeper garage route;
they are not elapsed-guard or external-termination exit evidence.
