# UI snapshot gaps (MAC045)

The retail PC UI pack uses `UITextBox`, but this source snapshot contains no
implementation, Xt declaration, registration, or Jam member for that widget.
This is a functional gap: the pack contains 11 such widgets, rather than merely
an unused schema entry. No substitute class, registration alias, or additional
object filtering was added during this audit.

## Evidence

The audit read `/gamedata/xt/ui.bin` through `NativePakArchive`, which verifies
its stored entry SHA1. The original authenticated engine mount/read was also
validated separately by the MAC030 fixture. No extracted bytes are included in
this repository.

The inspected Flask v2 file is 1,004,480 bytes; SHA256:
`e7fd10138524b7e0912561a797cd037a3a6d82b0859750f74ebf8f8ea9f67691`.
Its schema contains 135 types, 434 references, 201 records and 1,706 components.
`UITextBox` is schema index 30, has disk size 148, derives from `UIDummy`, and
adds six fields: maximum characters, hidden state, clear-on-return state,
default localized text, font height and backing colour. No `UITextBox` is a
root reference. Seven component arrays contain the following objects:

| Screen record name | Widget count |
| --- | ---: |
| `Account\NetLogin.lua` | 2 |
| `SinglePlayer\SpDlcEntry.lua` | 1 |
| `FriendsList.lua` | 1 |
| `Shared\Facebook.lua` | 1 |
| `Account\NetRegister.lua` | 4 |
| `Shared\Twitter.lua` | 1 |
| `TheCustomPopup.lua` | 1 |

The inventory follows the actual fixed-width structs in
`code/mercury/library/common/xt2/source/common/XtFlask.h` and the actual v2
string decoder in adjacent `XtFlask.cpp:11–30`. Component counts and enclosing
record names were decoded from the original file; screen names above are
metadata, not claims that every corresponding online service still works.

Whole-source case-insensitive searches, the UI Jam list and the UI registration
list in `code/mercury/library/ui/source/ui/Game/UIXt.cpp:109–220` contain no
`UITextBox`. Searching `origin/master` yields the same absence. Existing font
style, font shader and static-text implementation/registration is present;
this warning is not evidence that the whole font subsystem is missing. No
existing source member was found that could be fixed by adding it to Jam.

## Effect on loading

`code/mercury/library/common/xt2/source/game/XtFlaskImport.cpp:465–475` emits
this warning for an unmatched schema type. `UIXt.cpp:234–236` forwards it as
a log warning. It does not by itself reject the whole pack.

The importer skips native allocation for the unmatched type at
`XtFlaskImport.cpp:841–844`, assigns its components `gXtNullType` and count 0 at
lines 930–936, and its pointer decoder at lines 696–711 leaves references to
these missing elements null. `UIXt.cpp:1054–1060` and 1114–1125 skip null
screen elements during gadget creation and Lua export.

Other screen records can therefore load, and actual native startup completed
the UI-manager constructor after this warning. The affected text-entry widgets
are nevertheless absent. This does not prove that the seven screens remain
usable or that the custom popup cannot be reached during a later flow.

## Recovery boundary

Recover the matching widget implementation and its schema/factory integration
from authoritative source history or analyze the matching original binary.
The field layout establishes the serialized data contract, not editing,
focus, keyboard, rendering, Lua or notification behavior. It is insufficient
to invent a faithful replacement. Keep the warning visible until that behavior
has been recovered and tested.
