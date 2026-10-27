# Native required UI initialization and ownership

MAC103, 2026-10-27. The native UI layer assigns a complete C++ manager object
before attempting resource initialization. The original Windows constructor body
and the PC font/Flask formats remain authoritative.

`C_UiManager::NativeInitialise()` performs the original initialization work and
publishes `NativeIsReady()` only at its end. It reports a named failure through
`NativeInitialisationError()`. The native layer calls it immediately after
assigning the manager and before creating its Bloom/render-buffer graph. A failed
initialization is retired by the existing layer rollback while dependencies and
the device are still alive. Exceptions do not escape a constructor containing
unowned raw acquisitions.

| Required boundary | Actual success condition | Partial ownership and failure behavior |
| --- | --- | --- |
| Common UI textures | Original `C_TextureManager::AddResourceBundle("Ui/Data/ui_common")` returns a valid bundle index after real archive/GPU creation. | Failed native initialization remains unready; actual bundle owners retire with the manager. |
| Selected PC font textures | The original Latin, Cyrillic or Baltic bundle selected by UI language loads successfully. | Other platforms' additional-font policy is unchanged; no substitute texture is selected. |
| Selected bitmap font data | `C_Font::NativeInitialise` finishes the original version/count/font reader, including actual texture-name resolution and glyph/kerning data. | Font objects and temporary arrays are staged; failure publishes none. A live font owner cannot be overwritten. The redundant native Latin load is removed. |
| UI schema | `C_UIXt::NativeLoadDefinitions` opens the actual file, obtains the application schema, imports a real `XtPack`, and the manager finds the required original `UiXtConfig` record. | A staged database/pack owns rollback. Only internally created databases are destroyed; a borrowed global database remains owned by the application. |
| Visible frontend/game bundles | Their original synchronous bundle load returns a valid index at each transition. | A named exception invalidates readiness before the caller can proceed to rendering. Unstarted managers cannot load transition bundles. |

Public manager update/render entry points reject an incomplete manager. This
fails at the required resource boundary and does not hide a later missing-texture
error with a fallback. The original PC gamer-picture service explicitly does not
retrieve local/remote pictures; avatar allocation is not promoted into a required
offline/debug prerequisite.

The bitmap parser preserves the original virtual `Read` interface and adds a
native path carrying the actual input extent. Original glyph records are 24 bytes
for versions above2 and 28 bytes for older versions; kerning records are 8 and6
bytes respectively. Compile-time checks enforce those widths and UTF16 glyphs
retain their original width. Counts, bulk-read lengths and terminated fixed-width
names are checked before use. Real failed reads and missing required texture
names fail initialization; no glyph mapping, count clamp or synthetic font data
is introduced. The font family initializes its ownership fields explicitly, so a
local or partially initialized object does not depend on static-zero storage.

Native UI schema cleanup closes the pack and an owned database before deleting
their borrowed callback context. It then resets ownership for idempotent cleanup
and retry. Unknown obsolete schema-type diagnostics retain the original reader's
behavior; this change does not create a UITextBox implementation or discard
unsupported objects speculatively.

Manager retirement destroys actual screen gadgets/message handlers while their
Lua state, styles and effects are still alive. It closes only acquired subsystem
families, clears the texture manager's Lua pointer before closing the state, and
retires its own zero-initialized alternate-texture pointers. The original
resource/family teardown APIs do the work.

A focused run additionally exposed a static-exit ownership defect:
`C_TextureManager::DestroyAll` retained null slots, so its later static destructor
logged already-retired bundles through a destroyed logger. Native DestroyAll now
removes live bundle owners once, clears retired slots and resets its original
null index/handle/count. Active Lua callers retain the existing enum-refresh
policy. This neither clears the GPU tracker nor forces another owner's resources
to be freed.

## Validation and practical limits

`tools/macos/ui_required_resources_test.cpp` uses the actual manager, C_UILua,
source-built shaders, GPU bundle reader, bitmap parser and PC Flask importer. It
checks an acquisition-free constructor; missing common/font/font-data rejection;
unready-render rejection; complete GPU retirement; source-derived bitmap
versions0–3 with UTF16 glyphs and kerning; malformed/short data and successful
retry; shared-database pack ownership; and handle invalidation after pack close.
It also exercises repeated teardown/reacquisition and the original configured
Europe/English locale default through the public determination API.

Debug and Release private runs pass and exit normally. Twenty-four exact module/PCH
syntax probes including the coordinated native locale-default translation unit pass across Debug, Release, Debug UBSan and Speed UBSan. The full
UBSan game-linked runtime closure retains the separate known missing original
PortalBlocker RTTI limitation; a source probe is not an UBSan runtime pass.

With `BLUR_MAC_UI_ROOT_PAK` set to the private original root.pak, the fixture also
loads its actual common/font BPRs into GPU resources and parses its original Latin
font DAT successfully. Extracted bytes stay under the ignored scratch directory;
none are embedded in public fixture source. This proves these initialization
boundaries, not a rendered full UI, drivable race or completed lap.

The fixture takes two arguments: an ignored scratch parent and the source-built
shader resource root (`authored-shaders` or an app's `Contents/Resources`). Link
the existing full-game/replay closure with the generated Lua bundle, the original
allocator/bootstrap archive first, and `blur_mac_assets` for the optional private
archive reader. It starts no game or Artery service; hardware runs still use the
shared exclusive wrapper. Simple2D/renderer internal GPU exception readiness and
UI-layer transition scope handling are coordinated separately with their owners.
