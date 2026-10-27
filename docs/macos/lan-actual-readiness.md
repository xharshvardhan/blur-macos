# MAC250 — Original LAN UI and actual-test readiness

This source audit and private operator record the original LAN path after
MAC243's genuine normal career race, failed-medal results/save and ordinary exit
0. MAC253 subsequently verified an actual offline local host, a real one-player
lobby and ordinary lobby leave. It exited 0 through the elapsed-time guard,
which does **not** prove the desktop-confirmation route. MAC256's fresh
host/leave/ordinary-desktop repeat is currently in progress.

No four-player join/race or repair of the user's historical three-player limit
is claimed. This audit's author performs no game launch, Automation dispatch,
network fixture, build or GPU work and changes no engine/protocol/player state.
Root owns the actual runtime and service slot.

## Original Master host route

All input must follow actual ready screens/contexts, original unlocked options,
no unknown popup/subscreen/loading and fresh selection fences. Do not invoke the
mutating functions named below directly from Automation; they are source
contracts for the original ordinary-key callbacks.

| Actual screen | Source and ordinary action |
| --- | --- |
| Intro/StartScreen | With the existing locally active profile and normal boot complete, observe stage 2 and GUI.CurrentMode/Mode.Multiplayer. Ordinary Left/Right chooses the actual Multiplayer mode; Enter at StartScreen.lua:408–424 calls the original EnterMpRaceBook. Do not use Amax.SetGameMode externally. |
| Multiplayer/MpMain | Its carousel uses GUI.carousel_id and UIEnums.RwMpBranch.Lan. Observe actual selection, no active app and ready/unlocked carousel before finite directional keys/Enter. MpMain.lua:281–295 calls actual network-valid check and CheckEntryRequirements(false), then selects SystemLink and starts its original **offline** connection task. |
| Connection popup 86 | NetworkBank.lua:129–148 and CustomPopupData.lua:2925+ own the async connection. Wait for actual original completion; no Enter/Back or result-state mutation. MpMain.ConnectionCallback at line 382 opens the chosen app. Online title-storage download is only for the Online branch. |
| Multiplayer/MpLan | Init at lines 41–55 caches actual SystemLink playlists and starts discovery. Its zero-server result opens NoLanGames (3). This known popup has no_back=true and ordinary Enter acknowledges it; do not dismiss unknown version/network/account errors. Once the actual browser is ready, ButtonY (RControl or keypad Add in GameControls.cpp:202) calls the original GoScreen(MpModeSelect). |
| Multiplayer/Shared/MpModeSelect | Observe GUI.menu_id and actual GUI.options.custom_race=2. Ordinary Left/Right then Enter selects Custom Race, qualified by actual payload/index/node count. MpModeSelect.lua:73–96 resets original custom settings, selects CustomRace, and invokes actual CreateLanServer. Only success schedules the actual MpLobby subscreen. The default Playlist (1) is a different legitimate route. |
| Multiplayer/MpLobby | Observe original SystemLink mode, NetRace.IsRunning()==true, NetRace.IsHost()==true, offline status and actual nonempty local GUI.Slots. Do not call LaunchCustomRace. A host alone normally cannot start; this is not a failed smoke test. |

The active browser/lobby may live in the original carousel app/subscreen
context, not always Main context 0. Determine contexts with the actual
UIScreen.Context and GetCurrentScreen(-1) broadcast. Popup state follows the
owning Main/context contract, not an invented separate popup screen. Previous
MAC241's bare screen-local Lua helper mistake is not repeated: use actual
registered modules and screen-owned GUI fields.

### Truthful native prerequisites and an existing guard mismatch

`NetServices.NetworkConnectionActive` at lua_NetServices.cpp:937 reads
NetPlay::C_Core.ValidLocalAddress. Native Interfaces.h enumerates **up,
non-loopback IPv4** interfaces with getifaddrs. An actual Wi-Fi/Ethernet IPv4
address is required by this gate; loopback availability alone is not proof. This
is the original IP LAN transport, not a new Bluetooth pairing transport.

`CheckEntryRequirements(false)` at lua_NetServices.cpp:536–577 still selects its
unknown-platform branch on macOS and returns 4. The PC credential-check branch
is not widened to native. MpMain's LAN callback only special-cases 1, so 4
currently continues to the offline task. This is a concrete
status-classification gap to record during a smoke, not a successful
requirements result or an established runtime blocker. No speculative source
correction is made here.

The original RouterPC.PlatformConnect at lines 119–170 does not require
username/password when onlineMode=false. Its offline StartConnect at line 487+
starts/pumps real bdNet and publishes completion only on BD_NET_DONE, without
connecting to hosted auth. The original popup 86 also permits an offline
NoServiceProvider UI fallback; therefore a popup callback/string saying success
alone is **not** proof of a working router. Native local profile identity is not
an online account. No provider/authentication success may be fabricated to enter
LAN.

Actual CreateLanServer at lua_NetRace.cpp:301–348 validates settings capacity,
gets a real machine identifier and actual active local profile, then calls
CreateLanSession. That native-original function at NetRace.cpp:270–307 requires
initialized/nonrunning state, nonzero console ID, real BD_NET_DONE, actual local
players, generated security key and successful router key registration before
setting Running and advertising. MAC253 observed the resulting real running host
and local roster; it did not packet-capture the advertisement or validate a
remote join.

## Original join route and read-only evidence

MpLan.lua:76–100 takes the actual selected enumerated row and checks both
network and playlist versions. Its original join popup 8 waits 1.5 seconds,
calls JoinLanServer, then monitors actual completion (CustomPopupData.lua:494+).
The operator should only choose a freshly observed compatible real row with
ordinary Enter. Never insert a fake enumeration record, call JoinLanServer
externally or mark a peer joined/ready. A zero-server browser cannot validate
join.

Useful copied/read-only observations, each guarded by the **actual** context and
kept inside the existing 127-byte command envelope:

- Original screen/active/popup/controller-lock/subscreen/loading state.
- StartScreen stage, actual mode; MpMain carousel selection/branch; MpLan
  current_state, sessions_found and NetRace.NumLanServersEnumerated /
  IsEnumeratingLanGames; ModeSelect actual menu selection.
- NetServices.ConnectionStatus (state/result/online), original runtime enum
  comparison and NetServices.NetworkConnectionActive.
- In the actual created lobby: Amax.GetGameMode versus SystemLink,
  NetRace.IsRunning, IsOnlineMode, IsHost, CanStartCustomRace, and the original
  UIGlobals.Multiplayer.LobbyState versus runtime enums.
- `GUI.Slots`, populated by Multiplayer.GetLobbySlots(GUI.Lobby), contains real
  empty/ref/local_player fields. UILobby.cpp:308–349 copies these from the real
  player manager and race connection; observing a nonempty local host entry is
  useful evidence. It does not create a remote player. Do not call RefreshLobby
  or other mutating native helpers as a query.

On leave, ordinary Escape in MpLobby opens ExitRaceLobby (4). Its original popup
contains actual Yes/No (or party options). Confirm only a freshly observed Yes
selection; CustomPopupData.lua:175+ invokes the original disconnect. Observe the
session stopped and normal browser/menu return, then original desktop quit and
actual exit 0. MAC253 proved session stop/browser return; the ordinary desktop
repeat is still in progress under MAC256. Do not call
NetRace.Delete/net_DisconnectFromRace or a quit setter from the operator. This
validates owner cleanup without claiming a network race.

## Concrete capacities, wire and start policy

`Network/Misc/NetDefines.h` specifies 20 maximum network vehicles, 8 party
members, and 1 local network player per process. These are source capacities,
not proof that 20 real players have joined. MpLan's ten displayed session rows
are **servers**, not a ten-player or three-player cap. Actual race maxDrivers is
loaded settings; do not enlarge it by a debug write.

Native discovery preserves PC UDP 50001, original 24-byte request / 160-byte
reply ABI, version/title/nonce/security/common-address fields and slot
admission. NetRace.cpp:2294 advertises settings.maxDrivers minus actual player
count; discovery replies only when enough advertised slots remain. Existing
network_discovery_test uses six synthetic query sockets and 20-slot boundary
checks with the real offline router/responder. This proves transport/discovery
and wire capacity, not four actual game lobby handshakes, replicated vehicles,
readiness or race completion. No source hard limit of 3 was identified in this
bounded path; the historical college lobby issue remains undiagnosed.

lua_NetRace.cpp:1821–1884 requires a local host, valid/idle vehicle entries, no
migration and **at least one remote race player** to start CustomRace. The
existing Multiplayer.AllowSingleRacer debug fallback defaults false
(RaceModeMultiplayer.cpp:79) and must stay false for real LAN validation.
MpLobby_HostCanPressStart at line 1352 consumes that real readiness. A one-host
smoke should observe start unavailable and never enable the bypass.

## Two actual apps on one Mac versus the present execution policy

The game transport has mechanisms that can support isolated endpoints without
changing its wire protocol: bdNet.cpp finds a free game UDP port; the advertised
common address includes that endpoint, and Core_pc.cpp:174 derives the console
identifier from its common-address hash. Native settings already allow distinct
Artery TCP/UDP bindings, discovery bind/target IP/port, separate absolute
BLUR_MAC_PROFILE_STORE and BLUR2PC_SAVE_ROOT. Existing Automation CLI supports
`--port`. Duplicate copied identities or default bound service ports must not be
silently reused.

Those mechanisms do not make an uncoordinated second app a supported current
test. Default Artery is TCP 6317 / UDP 6318; discovery binds 50001. The required
`run_exclusive.py` flock serializes each actual game/service command regardless
of alternate port settings. A second separately wrapped game waits for the
first; starting it outside the lock is a policy violation, not LAN evidence.
This task neither bypasses the lock nor starts a supervisor/multi-app run.

A future root-owned multi-app coordinator would have to retain **one** actual
service-slot lease for its entire owned process group, explicitly isolate all
bindings and identities, direct clients to actual endpoints, and clean up only
its children. That is a separate implementation/verification step, not an
instruction to evade the present lock. Client discovery bind 0 is useful for
querying a default host, but does not make that ephemeral client a migration-
capable default-port host. Do not claim host migration from that setup.

## Remaining bounded acceptance

MAC253 has verified the ordinary Master→SystemLink browser→CustomRace host path,
the actual offline router and local roster, unavailable race start and ordinary
lobby leave. Its elapsed-time shutdown was clean but did not exercise desktop
confirmation. MAC256 is repeating the full fresh host/leave flow and then the
original desktop confirmation, using the existing local profile, unchanged
protocol/default bindings and actual active network adapter. Until that run
finishes, this audit does not claim desktop-route completion. It also does not
claim packet-captured advertisement delivery or a remote handshake. Unknown
profile/network/import/UI failures remain failures, not bypass targets.

Four-player acceptance then requires four **actual independent** game instances
(ideally four machines on the same reachable LAN), one host plus three real
clients, distinct identities and compatible game/build/playlist versions. Prove
fresh discovery, original secure join/handshake completion, simultaneous roster
of four distinct real human peers, ready state, original start, replicated race
and clean leave/exit. Two local apps or a synthetic responder cannot establish
this. Windows interoperability and host migration require their own actual
peers/cases. No >3-player repair or improved connectivity is claimed by this
audit, by one-host smoke or by existing transport fixtures.

## MAC252 — Private ordinary-key host smoke operator

The ignored `_work/runtime/research/lan_host_operator.py` is used only under
root-owned runtime control and does not launch a game. Its pure decision/builder
self-test and Python compilation pass; those checks alone are not runtime proof.
The actual MAC253 host/leave proof and its limits are recorded below; MAC256's
fresh ordinary-desktop repeat remains in progress. Use the helper only while no
other input operator owns that game. The initial MAC253 command was:

```sh
python3 _work/runtime/research/lan_host_operator.py mac253 --seconds 300 --leave-menu > _work/runtime/verification/mac253-lan-operator.jsonl 2>&1
```

The existing root harness must first log its actual StartScreen observation. The
helper can start at Master stage 0: one ordinary Enter starts the original
profile boot. It then waits for actual stage 2, primary pad 0, locally active
and offline profile, original boot completion, unlocked controller, active
screen and no profile/loading/movie blocker. Right/Enter chooses actual
Multiplayer. It does not require a manual prebootstrap and never writes an
identity or mode.

At the ready original MpMain carousel, finite Right steps select its actual Lan
branch; Enter is permitted only with a real native network address, no existing
race session and observed entry status 0 or the separately recorded known Mac
status 4. Other entry statuses stop input. Popup 86 is automatic and receives no
key. Its fallback success is insufficient: the real router must report its
runtime Complete enum with `ConnectionStatus()` returning `online == false`
before hosting; an online provider is neither requested nor imitated. If only
the NoServiceProvider fallback runs, Host is never pressed.

The actual finished browser permits RControl (original ButtonY). Only actual
ModeSelect custom option 2 can receive its Enter. A created lobby is accepted
only after real `NetRace.IsRunning/IsHost`, offline SystemLink and CustomRace, a
nonempty original `GUI.Slots` copy, matching actual UI/network max-player counts
within 1..20, one local and zero remote players, and both `CanStartCustomRace`
and the original screen's `MpLobby_HostCanPressStart` return false. The
configured count is observed, not changed or assumed 20. This is a single-host
smoke, not security/advertisement packet capture or a remote handshake/race. No
ButtonStart is sent and AllowSingleRacer is untouched.

Default behavior stops all IPC at that verified lobby. Explicit `--leave-menu`
continues through the original leave flow: lobby Escape → **ExitRaceLobby (4)**
(observed Yes only; ordinary Up may choose Yes from the actual two-option No) →
automatic disconnect popup 6 → original MpLan return with Running=false → Escape
closes its app → ready MpMain Escape → **ExitMultiplayer (2)** Yes → original
StartScreen stage 2. Popup 6 owns EnterDisconnecting, session flush and close;
its non-ingame SystemLink branch returns to MpLan at
CustomPopupData.lua:436–456. MpMain.lua:325–333 and GUIBank.lua:876–947 own the
browser app close. ExitMultiplayer at CustomPopupData.lua:116–156 performs
original `net_MpLeave`, flush and `QuitMpGame` return bookkeeping; the operator
never calls those functions directly. Desktop confirmation/actual exit remain a
separate root-owned ordinary StartScreen route. The earlier audit's popup 2
label for ExitRaceLobby was a documentation typo corrected before any operator
run; UIEnums.lua:173–179 is authoritative.

Queries are passive original registered module getters and actual screen GUI
fields. Original popup state is read from Main context 0, even when the
underlying LAN app lives in a different actual context. Commands are capped at
127 UTF-8 bytes; key authorization expires after 1 second, full snapshots have
an 8-second bound, and fresh final screen/selection/popup fences must match. The
operator refuses unknown/out-of-phase popups, duplicate contexts, Lua query
execution errors, rejected/ambiguous key acknowledgments and log
replacement/owned process exit. A 24-key budget covers ordinary navigation and
cleanup. These policy tests are synthetic and cannot replace actual offline
host/leave/exit evidence.

### MAC253 selector observation and bounded helper correction

The first actual smoke reached a real offline Complete router (runtime state 3 /
error 0), active local profile 0, original browser and ModeSelect context 5. One
authored Down key was accepted but payload remained 1. This is not a
host/transport failure or evidence for a single-row LAN layout. Source PostInit
creates two nodes with explicit values 1 and 2; Lua_Buttons.cpp:1085–1105
returns the current item **payload**, while GetSelectionIndex at lines 1110–1123
returns its zero-based row. UINodeList defaults Horizontal at line 206;
MoveRight at line 677 advances that style. The original helper's unverified
vertical-axis assumption was incorrect. It issued no Enter on payload 1, and its
semantic duplicate fence prevented more Down taps. The owned helper then stopped
at its deadline, leaving UI unchanged.

MAC252 now queries actual selection index and node count as well as payload.
Only two actual nodes with payload 1/row 0 permit one finite Right; Enter
requires payload 2/row 1 and actual custom enum 2. Unknown or mismatched mapping
refuses input. Explicit `--resume-selector` requires a fresh ready original
ModeSelect with the real offline router, real local profile and no running
session before any key; it cannot jump/construct/select that screen. The
root-owned resume is:

```sh
python3 _work/runtime/research/lan_host_operator.py mac253 --seconds 180 --resume-selector --leave-menu > _work/runtime/verification/mac253/lan-resume-operator.jsonl 2>&1
```

The corrected pure policy/index-mismatch/127-byte builders and Python compile
pass. No IPC or runtime was performed by this audit/operator author. MAC253
subsequently verified Right selection, actual host creation and ordinary lobby
leave, as detailed below.

### MAC253 host proof and return-observer correction

MAC253 actually reached the created lobby: Running/Host true, offline SystemLink
CustomRace, real router Complete state 3 / error 0, UI and network max 20, 20
copied slots with exactly one nonempty local player and zero remotes, and both
original start-readiness getters false. This qualifies a local host smoke; it
does not qualify four-player joins or a network race. Ordinary lobby leave also
ran through actual popup 4 Yes and automatic disconnection back to the browser;
NoLanGames (3) was acknowledged, browser Escape returned to MpMain with
Running=false. The eventual application elapsed-time guard exited 0 at 601.95
seconds without a primary error. **That is not desktop-confirmation
acceptance.**

The authored return observer stalled because it additionally demanded
`GUI.CarouselActive == true`; actual returned MpMain reported false despite
active_selection=-1, Ready=true, unlocked controller and no active subscreen.
Original MpMain.MessageUpdate at lines 234–333 authorizes MenuBack by the
controller lock and active_selection branch, and does not inspect that
presentation flag. No engine/UI defect is inferred. The helper now records
CarouselActive but omits that extra requirement only for its WAIT_MP_LEFT
ordinary Escape. It retains actual
Master/screen/activity/local-profile/readiness/freshness, no active app, no
subscreen, Running=false and popup checks. The initial carousel-navigation
presentation requirement is unchanged.

A pure regression uses that actual false-CarouselActive return snapshot and
requires Escape; active app, active subscreen, locked controller, running
session and not-Ready counterexamples refuse input. Existing decisions/builders
and Python compilation pass. MAC256 owns a fresh host/leave plus separate
ordinary desktop confirmation repeat, avoiding both prior helper stalls. No IPC,
game launch or engine change was performed by this correction's author.

## MAC256 — Fresh host, original leave and desktop exit

A fresh signature-preserving copy of the staged Speed242 app repeated the full
ordinary-key route after the private helper corrections. The actual offline
router completed with state 3 and error 0; original CustomRace hosting produced
a SystemLink lobby with capacity 20, one real local player and zero remotes.
Both native and UI start gates remained false. The normal leave confirmations
closed the session, returned to MpMain and then StartScreen stage 2.

The separate desktop operator used three ordinary keys, including the original
204 confirmation. The actual process exited 0 after 120.24 seconds with no
primary failure, before its 420-second guard. The LAN portion used 20 finite
keys. Strict deep signature and dependency/notices hash verification passed
after exit. Evidence is in ignored `mac256-lan-proof.json`, original input/UI
logs and `mac256-probe-status.json`. No screenshot was produced before exit;
the earlier single MAC253 still remains the menu reference.

This supersedes MAC253's incomplete desktop-route verification. It establishes
ordinary single-host lifecycle cleanup, while remote joins, replicated racing,
four-player acceptance and the historical three-player problem remain unverified.
The native entry status 4 gap remains recorded separately from actual offline
router completion. No authentication, peer, progress or single-racer override
was introduced.
