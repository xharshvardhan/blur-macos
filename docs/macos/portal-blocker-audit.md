# MAC038: missing portal-blocker creator implementation

The checked-in snapshot does not define `Components::C_PortalBlockerCreate`'s declared constructor or virtual destructor. This is an upstream source gap, rather than a missing native Jam member. No replacement definitions, RTTI suppression or component-registration changes were made.

## Source evidence

Paths below are repository-relative; line numbers refer to the audited `macos` working tree.

| Evidence | Path and lines |
|---|---|
| Creator declares an out-of-line constructor and virtual destructor; fields are `XtBoundingBox m_BoundingBox` and `bool m_bStartClosed`. | `code/mercury/library/gamesys/shared_actors/include/shared_actors/Components/Creation/RenderComponentsCreate.h`, 127–139 |
| Corresponding implementation contains other creators and ends without either portal-blocker creator definition. | `code/mercury/library/gamesys/shared_actors/source/Components/Creation/RenderComponentsCreate.cpp`, 1–149 |
| That implementation is already an actual Jam source. The generated native source manifest also includes it. | `code/mercury/library/gamesys/shared_actors/jamfile.jam`, 84 |
| Runtime component remains registered. | `code/mercury/library/gamesys/shared_actors/source/Components/SharedComponents.cpp`, 100–104 |
| Runtime implementation consumes creator fields and creates a real deferred portal blocker. | `code/mercury/library/gamesys/shared_actors/source/Components/Impl/RenderComponents.cpp`, 1048–1083 |
| Runtime getter downcasts creation data to this creator; UBSan's dynamic-type checks require its RTTI. | `code/mercury/library/gamesys/shared_actors/include/shared_actors/Components/Impl/RenderComponents.h`, 318 |
| Older duplicate creator declaration has implicit special members, but the whole declaration is inside `#if 0`; it is not an implementation alternative. | `code/mercurygame/game/Game/ActorComponents/Creation/RenderComponentsCreate.h`, 78–107 |
| Only `PortalBlockerComp` Xt schema/registration found in the repository is comment-disabled. | `code/mercurygame/game/Game/ActorComponents/Creation/RenderComponentsCreate.cpp`, 74–83 |
| Older duplicate runtime code is also `#if 0`-disabled. | `code/mercurygame/game/Game/ActorComponents/Impl/RenderComponents.cpp`, 529–627 |

A source search confirms that all creator occurrences are declarations, downcasts/field consumption, or disabled annotations:

```sh
rg -n 'C_PortalBlockerCreate|PortalBlockerComp' code
```

The same missing definitions are present in the unchanged `origin/master` snapshot. Local history is a shallow checkout containing only `1965facd`; this audit does not claim to cover unavailable upstream history or retail disassembly. No matching game executable/PDB was available in the inspected private installation for recovering the missing special-member behavior.

## Object/link evidence

```sh
/usr/bin/nm -C _work/build/macos-ubsan/libblur_mac_shared_actors.a \
  | rg 'PortalBlockerCreate|PortalBlocker::'
```

The archive defines the real runtime component's constructor, destructor and methods, but reports:

```text
                 U typeinfo for Components::C_PortalBlockerCreate
```

It has no creator constructor/destructor or creator RTTI definition. The full Debug UBSan link log, `_work/build/macos-ubsan/mac004-game-default-build.log:5586`, attributes the unresolved typeinfo references to `RenderComponents.cpp.o`; the optimized UBSan log reports the same source. Non-sanitized Debug does not require these downcast-check RTTI references and links without resolving the missing special members. This is not evidence that the creator implementation exists or is usable.

## Recovery boundary

The source establishes that a portal-blocker runtime component was implemented. It does not establish the intended creator constructor defaults or prove that retail game data never uses the component. The disabled older declaration is insufficient authority for inventing current constructor/destructor definitions, changing the class to defaulted special members, or deleting its runtime registration.

A faithful completion needs an actual implementation from matching source history or recovered constructor/destructor behavior from an authoritative binary. Until then, the complete game UBSan link retains this explicit blocker; component tests keep their checks enabled.
