# MAC384 — LAN garage car-selection identity

Found in / belongs to: native macOS. The reported manual0.1.7 run reaches the original LAN lobby and then logs `Screens\\Shared\\Garage.lua:1108: attempt to index field '?' (a nil value)` when opening car selection. The handled Lua error appears in the debug overlay rather than a gameplay popup. The source and diagnostic evidence below identify a numeric-ID contract; controls, simulation, garage filtering, unlock policy and saved selections are preserved.

## Fault and source authority

`UI::GetGarage` in `Lua_Multiplayer.cpp` builds its vehicle table using `lua_rawseti(..., pVehicleAM->ID)`, whose key is a signed32-bit int. `GameData.GetVehicle` supplies its vehicle ID through the existing `Lua::AddDataMember(U32)`/`PushInteger32` contract. The original PC Lua integer was32-bit; on the native LP64 host, direct `lua_pushinteger` of a U32 saved-car ID instead exposes high-bit IDs as positive64-bit values.

`Multiplayer.GetGarageCarByID` bypassed that existing helper in both its global and per-class paths. The original `Garage.lua` list search compares its car keys with the saved-car result. Numerically different signed/unsigned representations fail that comparison even though their32-bit bits are identical. The lobby fallback then calls `Garage_GetSelectedCar` before `GUI.current.class` initializes; the original getter indexes a missing class at line1108.

A passive old-build observation on an isolated copy of the user's existing profile gives global/class-B ID3307642456 and class-D ID4229618501. Their corresponding signed PC32 values are−987324840 and−65348795. This confirms affected high-bit saved IDs exist; the manual log does not independently record its exact lobby class/selected ID. A genuinely missing or filtered car can also reach the inherited uninitialized fallback; this change does not fabricate a replacement car or claim that separate condition is fixed.

## Repair

Both direct getter producers now use the already-established `Lua::PushInteger32`. Native results match existing garage keys and vehicle IDs. That helper's non-macOS branch retains the original `lua_pushinteger(state,bits)`. The no-profile result, numeric/nonnumber argument dispatch, class/ANY selection and profile contents stay unchanged. Lua integers and pointers remain64-bit on the host. No script, data archive, progression or save migration changes are needed.

## Regression evidence

The new `multiplayer_garage_id_test.cpp` registers the real `luaopen_multiplayer` callback table and invokes the actual getter. It checks six low/high-bit boundary values through global, class, ANY and existing nonnumber dispatch. Its independent signed `lua_rawseti` table must find the getter result and match the original class-list comparison. Missing-primary no-result behavior, unchanged in-memory selections and high-address lightuserdata are also checked. It neither initializes the OS profile service nor loads a user profile; an isolated empty persistence directory is verified.

The unchanged old producer fails this same fixture with `getter changed PC32 ID identity`; the repaired producer passes. An independent agent reviewed the final binding and fixture without production edits or game input. Both complete final Debug and optimized suites pass156/156 with `BLUR_MAC_AUDIO_MUTE=1`. The first Debug invocation omitted that required test-launch flag:155 passed and the decoder-trace fixture refused its precondition before running. That failed log is retained; the correctly muted complete rerun passes. No production audio change was made.

## Actual-game acceptance

Actual verification uses the same staged Speed application and a copied existing profile/save tree, minimized, muted before playback and serialized through the native-service slot. It creates its own LAN host session through ordinary original keys; it never joins a remote user's lobby or sets game/UI/profile state directly. The intended route is Lobby → Shift options → Car Select → change an owned allowed car → confirm → reopen and verify → cancel → ordinary lobby/browser/menu/desktop exit. Final route388 qualifies as recorded below.

Private attempts385/386 did not qualify garage navigation. Their helper sent RIGHT while the original `PreviouslyOnBlur.lua` child still owned input despite the parent StartScreen already reporting stage2/boot-ready. Its deduplication then suppressed a later action after the child retired. Both failed records remain. The corrected private readiness gate waits for the child to retire and two fresh sole-StartScreen observations; it changes no production input mapping or saved profile. Guard exits in failed runs are not ordinary-exit acceptance.

## Final acceptance and delivery

MAC388 completes that entire original route with all three helpers returning0 and ordinary game exit0 at179.158s. It opens the actual class-B garage with selected car−987324840 (U32 bits3307642456), changes to−1334278929 (U32 bits2960688367), confirms both global/class profile results and reopens that same car. It then cancels, leaves the original lobby/browser/MP flow and accepts the original desktop confirmation. The source application and original profile/save tree remain unchanged. The test uses no remote players and establishes no complete multiplayer race. No Lua runtime/handled error appears in this clean run.

Attempt387 also observed the affected car and a successful change/reopen, but private malformed/over-capacity command construction disrupted navigation/cleanup and it reached its elapsed-time guard. Those raw failures and source-qualified recovery records are retained rather than called an ordinary-exit pass. Private commands now preserve the original127-byte bound by splitting passive expressions and using short context/type guards after fresh source-qualified observations. Production scripts and input mappings remain untouched.

The qualified artifact is0.1.8/build384, with693 non-notice resources byte-identical to0.1.7. Strict signature, notices, ZIP extraction and relocation checks pass. The verified Speed app is delivered to the usual play location and the manual launcher now selects0.1.8, while0.1.7 remains available for rollback. The public preview remains Apple Silicon/macOS26+, ad-hoc signed and not notarized; a second clean Mac and remote races remain separate acceptance work.
