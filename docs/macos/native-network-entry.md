# MAC260/261 — Native network entry prerequisites

MAC253 and MAC256 verified an actual offline local LAN host with the original
UI/router/session path. MAC256 also left through the original menus and
confirmed desktop exit 0 in 120.24 seconds. The former native entry return 4 was
therefore a classification mismatch, not a demonstrated LAN blocker or cause
of the historical more-than-three-player issue. Remote joins remain unverified.

## Contract and narrow correction

`CheckEntryRequirements` in
`code/amax/game/GameLogic/Lua/lua_NetServices.cpp:525–591` documents return 0 as
permission to enter the mode, 1 as too many profiles, 2 as no online account,
3 as parental restriction, 4 as no service connection and 5 as generic error.
It is not a router-connect or session-creation result. Its body before this
change was byte-identical to pristine upstream commit
`1965facd3d4860447a043b1e0fea4d55c086d1f5`.

The original PC and PS3 branches apply account checks only when `online` is
true. Offline mode leaves the initial return 0 intact. macOS previously fell
through to the unknown-platform branch and returned 4 for both modes. MAC261
adds `PLATFORM_MACOS` only to the existing PC conditional and a `[Blur2PC]`
comment. It preserves the original function body, boolean validation, profile
precondition/assert, debug overrides, other platforms and source bytes/line
endings. No global PC platform define is introduced.

For a correctly booted native local profile, with debug entry overrides off:

| Request | Source outcome after correction | Meaning |
| --- | --- | --- |
| `false` | 0 | No hosted account is required for original offline LAN. This does not prove a network address, router connection, security registration or session. |
| `true` | 2 | The native credential getters return null, so the existing PC account check refuses online entry. It does not authenticate or grant service access. |
| Missing/nonboolean argument | 4, unchanged | Original invalid-input behavior is retained; this task does not redesign the status ABI. |

`C_GameProfileManager::GetDemonwareUserName/Password` at
`GameLogic/Profile/GameProfileManager.cpp:66–94` return null on native. The old
PC-only debug credential branch remains excluded. `OnlineConnectionPossible`
at `lua_NetServices.cpp:103–130` continues returning false on native.
`NetRouter_pc.cpp:355–367` still rejects native hosted authentication with
`NoServiceProvider`; no endpoint, account, credential, identity or provider code
is changed. There is no invented local-to-online identity conversion.

## All original call sites

- `Multiplayer/MpMain.lua:260–280` passes true. Return 0 enters the online task;
  1/2/3/4 select their original profile-count/account/privilege/connection
  popups. Native now takes the existing missing-account popup, not a fabricated
  success path.
- `Multiplayer/MpMain.lua:281–295` passes false. It separately requires a real
  `NetworkConnectionActive` address and only rejects entry status 1. Both the
  previous 4 and corrected 0 therefore take the same original offline task.
  The real router must still complete and the real LAN session must still be
  created. The change fixes classification without altering LAN admission,
  wire format, player limits or observed host behavior.
- `screens/NetworkBank.lua:466–490` passes true during online invite processing.
  It handles 0 through 3; 2 clears the invite and selects the original account
  popup. Its existing unhandled return-4 case would leave popup type -1. The
  native valid-boolean request now classifies missing credentials as 2 instead.
  No invite is fabricated or exercised: current `GameInvitePending` at
  `lua_NetServices.cpp:451–463` requires an actually connected service, while
  native local profiles remain offline. General unsupported-platform/invalid
  input handling is outside this correction.

The disabled profile-count check and existing debug overrides are unchanged.
No synthetic account, peer, ready state or single-racer bypass is introduced.

## Verification and root integration

Source integrity checks confirm that removing this exact conditional/comment
change restores the entire function to pristine upstream bytes; scoped diff
checks pass. There is no new mirror test of the conditional. No compile, shared
archive, game, Automation or GPU work was performed by this task's author.

The integration acceptance requires building the affected game module and both
standard apps, then
query the actual registered Lua function after ordinary local profile boot:
expect false → 0, true → 2 and unchanged real offline profile/router identity.
Debug overrides must stay off. Repeat the original ordinary LAN host/leave/
desktop path and verify real offline router completion, actual local roster,
start unavailable without a remote player, and exit 0. These are integration
acceptance requirements, not results claimed from this source-only change.
Existing native profile/router/discovery checks remain relevant; none proves
four independent actual lobby peers or a replicated LAN race.

## MAC262/263 integration and review

Both standard Debug and Speed applications build with the frozen correction.
Each passes the native profile and actual game-entrypoint checks. Independent
source review confirms invalid argument handling and debug override precedence
are unchanged; provider/router/session prerequisites remain separate.

Actual MAC264, after normal local profile boot, returns `0` for false, `2` for
true, and `4` for both nil and numeric arguments. The false result also proves
that none of the three entry override flags is enabled at that call: their
existing final priority chain would replace it with 1, 2 or 3. Source defaults
alone are insufficient because startup can load persisted debug variables.
No debug setter or online credential was used.

## MAC266 actual complete host lifecycle

A fresh signed copy of the 0.1.1/build262 development app repeats the full
original LAN route with the corrected passive reader. Actual entry results
remain false → 0, true → 2, nil/number → 4. The real offline router completes
with state 3/error 0; CustomRace hosts one real local player in a 20-slot lobby,
with zero remotes and both start gates false. Twenty ordinary LAN-menu keys
leave and return to StartScreen. Three ordinary desktop keys confirm popup204;
the process exits 0 after 147.65 seconds with no primary failure, before its
420-second guard. Strict signature and dependency/notices checks pass after
exit. The earlier MAC264 private reader failure remains a recorded baseline.

The ignored `mac266-lan-proof.json`, original logs and entry diagnostics record
the actual path. This validates classification and single-host cleanup, without
claiming online provider access, remote joining or multiplayer racing. The
local 0.1.1 ZIP passes signature, manifest and seven-alias preservation checks
after extraction; clean-Mac and notarization acceptance remain separate.
