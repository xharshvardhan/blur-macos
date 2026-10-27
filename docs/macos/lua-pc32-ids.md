# PC32 identifiers at the native Lua boundary

MAC097, 2026-10-27. This corrects the original PC numeric identifier contract;
it does not change the interpreter's host pointer or integer ABI.

## Actual failure and source authority

The frozen MAC096 Debug-menu probe observed a valid Brighton route as
`Amax.GetLevelData().Route == 4112414174`, the matching entry from
`Amax.GetRoutes(496308235)` as `4112414174`, and the actual route spinner's
selection as `-182553122`. These values have identical 32-bit representations.
The spinner's stored `C_VariableItem::m_Value` was confirmed to be the latter
signed value. The Lua route-matching loop consequently failed numerical
equality and supplied a missing thumbnail to the renderer. This observation
establishes the route-selection failure; it does not prove a completed race.

The original source explains the whole boundary class:

| Boundary | Source authority | Required native behavior |
| --- | --- | --- |
| Lua integer ABI | `code/vendor/lua/Lua-5.1.4/src/luaconf.h:139-143`, `lua.h:103`, `lapi.c:323-330` | `lua_Integer` remains `ptrdiff_t`; native host integers/pointers stay 64-bit. |
| ID table producers | `code/mercury/library/util/lua_shared/source/LuaHelper.cpp`, `AddDataMember(U32)`; `code/amax/game/GameLogic/Lua/lua_Amax.cpp:GetLevelData`; `lua_GameData.cpp:GetRoutes` | Original PC32 `lua_pushinteger` exposed a high-bit U32 as a signed 32-bit integer. Make that conversion explicit on macOS. |
| Spinner item ownership | `code/mercury/library/ui/include/ui/Game/Gadgets/BaseUIVariableSelect.h:40,153-155`; `BaseUIVariableSelect.cpp:SetCurrentItemByValue` | Items remain S32; retain their exact bits when accepting either signed or unsigned numeric aliases. |
| Gadget inputs/outputs | `ui/source/ui/Game/Lua_Interaface/Lua_Buttons.cpp`: `AddItem`, `SetSelection`, `GetSelection`, `SetParent`, gadget creation/find/clone/node IDs and `GetXtVar` | Avoid ARM floating-to-unsigned conversion for negative ID aliases; preserve existing lookup, value and coercion contracts. |
| Other direct ID producers | `code/amax/game/GameLogic/Lua/lua_Amax.cpp`: `GetVehicleByID`, `GetCurrentDebugEventID`, `GetCurrentCarID`, respray IDs, city/route localisation names | Restore the same U32/XtId producer contract where the common table helper is bypassed. |

## Implementation and boundaries

`Lua::PushInteger32` exposes a U32 bit pattern as its original signed PC32 value.
`ToU32Bits`/`ToS32Bits` accept finite Lua numbers by truncation and modulo 2^32,
with only in-range final casts. Numeric strings and nonnumeric-to-zero Lua
coercion remain available where the original API used them. Safe optional
`GetDataMember` still ignores absent or nonnumeric members. Nonfinite integer
inputs produce an explicit Lua error.

`NumberToU32Bits` provides the same conversion without a Lua-state dependency;
it returns false for nonfinite input without changing its output. The actual
spinner's `SetCurrentItemByValue` uses it and leaves the selection unchanged
when it fails. No artificial item, clamping, route substitution or Lua-script
special case was added.

Direct gadget integer inputs now use these helpers, including parent/child
lookups. `GetXtVar` only instantiates its integer helper for the original
I8/U8/I16/U16/I32/U32 atoms; a native compile-time bound excludes pointer-sized
fields. Its inverse uses the original `lua_tointeger` then fixed-width integer
cast, for which negative-to-U32 modulo conversion is defined.

Floating slider values, F32/F64 table members, positions, node F64 values and
original `lua_pushnumber` producers remain unchanged. In particular,
`Lua::Call` input format `u` originally used `lua_pushnumber`; it keeps that
positive-number contract. Its explicitly integer result formats use the safe
bit conversion. Since their extraction happens after `lua_pcall`, a nonfinite
integer result is rejected by the C++ bool API before changing outputs; result
stack entries are removed and a subsequent finite call remains usable. Real pointers continue through lightuserdata. Existing S64
scoreboard outputs retain the native Lua integer width. The vendor Lua
configuration is unchanged. Windows branches preserve the original casts and
producer calls.

Remaining boundaries outside this slice include direct numeric casts in other
Amax Lua APIs and deferred friend/online identities. The audit does not claim
that all arbitrary nonfinite inputs throughout the game are validated, nor
that existing `lua_pushnumber` IDs should be globally rewritten. Each retains
its original source contract until actual use establishes a required change.

## Verification and integration

`tools/macos/lua_id_test.cpp` runs the original interpreter, an authored in-memory
UIEnums script pack, the original factory-created `UIVariableSpinner`, and real
`UIButtons.AddItem`, `SetSelection`, `GetSelection` and slider callbacks. It checks
high-bit IDs, both aliases of the observed route, `-1`, optional member/string
coercion, nonfinite input/return rejection and finite retry, real LP64 pointer identity, and fractional slider
values. It does not load proprietary game files or start networking/display.
Its host implements diagnostic fixture services; unexpected rendering/input
requests throw. Actual UI objects are destroyed through their original owner
before the Lua state closes, including on a failed assertion.

Debug and Release private runs pass. Replacing only the U32 table producer with
its pre-fix native `lua_pushinteger` call makes the same real spinner/route
assertion fail with exit1; restoring the producer passes. Four configurations
(Debug, Release, Debug UBSan, Speed UBSan) pass exact module/PCH source probes
for all four modified translation units (16/16).

The complete UI callback table and default creator function reference the real
game closure. The full UBSan fixture link remains blocked by the independently
audited missing original `Components::C_PortalBlockerCreate` RTTI implementation;
no replacement definition or sanitizer suppression was added. This is not an
UBSan runtime pass.

The runtime fixture takes no arguments. Compile it with the original GameLogic
include/PCH ordering, link the existing full-game library closure and generated
`amax_lua_bundle.cpp.o`, and retain the normal dead-strip option. It creates no
Artery service. The private build helpers and logs are ignored under
`_work/build/macos/physics-agent-id-*`; the fixture and this document are public
source. A fresh integrated actual-game run is still required to prove corrected
menu-to-race launch and finish.
