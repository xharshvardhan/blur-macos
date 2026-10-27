# Normal career cycle and relocated reload acceptance

MAC243 uses the real Master frontend, original PC archives, existing genuine
local profile and career-completion overrides disabled. It selects original
event 4597671 (raw career key 4013244786), owned car 331339493, 20 cars and 3 laps.
It runs the coherent Speed242 arm64 application, SHA-256
`aedb97560f058cbc7effa5bfd1d1302e9cd88413103b3e8b7e8660b9a3860c56`.

| Gate | Actual evidence |
| --- | --- |
| Real game/render/input | Original car/track/HUD/pickups/mirror in one completed GPU frame; finite ordinary Q/A/Left/Right input. |
| Physical completion | Original start, ordered splits 1/2 for each lap, completed laps 1/2/3, raw lap 4 and RaceStateFinished/globalRaceFinished. All 2,881 observed Racing snapshots have configuredAI/aiToggle/automatedHuman false. |
| Result semantics | Actual position 20, state `none`, UI `RaceFailed`. The original rules check completed laps before unmet pass rules; this is full-distance completion with no medal. VehicleState 1 means RACING, not itself finish proof. |
| Original photo/results | Automatic results photo completes; ordinary Enter/Up/Enter opens popup 159 and selects its actual Return 1 option. No result/selection/progress setter or direct callback. |
| Fresh save/return | New FINISHED SAVING/local_error 0, FinishedSuccess, inactive LoadSave/unlocks and normal SP hub. Save changes from 2,028 to 2,207 bytes; liveGameInfo is rank 1/fans 60/stars 0. No claim that all 60 fans were newly earned. |
| Ordinary exit | Original SP exit and StartScreen desktop 204 confirmations, 7 finite keys, actual process exit 0 after 1,546.35 seconds with no primary error, before the 3,300-second guard. |

The earlier MAC232 run already completed the physical race but failed at a photo
clear outside BeginScene. MAC237 ordinary Photo Mode captured that exact
application class before unwind. The shared clear boundary now owns a temporary
scene or borrows the caller's scene. The old regression fails cleanly; fixed
GPU checks pass all four configurations and coherent Debug/Speed each pass 8
affected integration checks. MAC243 additionally takes a manual photo and
returns through original Camera/Paused/HUD before its race. See
[photo-scene-ownership.md](photo-scene-ownership.md) and
[career-results-flow.md](career-results-flow.md).

## Relocated bundle and disk reload

MAC248 stages the same coherent game through existing CMake BundleUtilities.
Development metadata, 21 actual non-system dylibs, 3 native notices and 16 package
notice/SPDX sets are inside the app. Its actual minimum OS is 26.0.

MAC251 starts a fresh signature-preserving copy with dyld override paths unset
and runtime PATH limited to system directories. The actual loader maps all 21
non-system libraries from inside that app and none from Homebrew. The original
save loader reports version 1378, 51,227 decoded bytes and 0 unread. Read-only native
career APIs report the same rank 1/fans 60/stars 0, event 4597671/state `none`,
lastVehicle 331339493 and 0 stars. The post-race save hash is unchanged.
It reaches the original owned/allowed garage without launching another race,
then ordinary 9-key menu/desktop exit ends with process exit 0 after 196.35 seconds
and no primary error. Strict
signature and dependency/notices manifest verification pass before and after
exit. An earlier MAC249 private copy flattened 7 library aliases; its real reload
worked but its resource seal failed. That is retained as a failed copy baseline,
and the corrected copy and CLI regression preserve every alias without re-signing.

The local development ZIP is also extracted with Apple ditto and checked for
strict signatures, notice/dependency hashes and aliases. No original game data,
profiles or saves are included. The four upstream
developer login literals removed from the public source are also absent from
this native executable. This is same-host relocation,
not another clean Mac, a notarized release or multiplayer acceptance. See
[development-bundle-staging.md](development-bundle-staging.md).

## Scope and evidence

This verifies physical completion and the original non-medal results/save cycle.
Winning medals, unlocking tiers, full content parity and a real multiplayer race
remain separate checks. No forced AI/progress/finish or single-racer LAN override
is used. The native source remains a development port.

Ignored evidence under `_work/runtime/verification/` includes
`mac243-career-proof.json`, original notification/input/results/quit logs,
`mac243-game-frame.png`, `mac251-reload-proof.json`, loader logs and save backups.
The screenshot proves rendered content, not lap completion or save integrity.
Local staging/ZIP proofs live under ignored `_work/publication/`.
