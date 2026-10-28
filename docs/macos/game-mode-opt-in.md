# Explicit game-bundle Game Mode opt-in

MAC342 refinement, 2026-10-28. Normal game packaging defaults to
`BLUR_BUNDLE_GAME_MODE=OFF`: it preserves the source app's existing Game Mode
support/category values, including their absence. It does not insert or remove
these keys. The current raw game app has neither key, so default delivery keeps
that policy. Game Mode is an optional packaging feature pending separate manual
fullscreen and performance qualification.

To request it explicitly when staging a new game bundle, add this argument to
the normal `tools/macos/stage_bundle.cmake` command:

```sh
-DBLUR_BUNDLE_GAME_MODE=ON
```

That option alone forwards `--game-mode` to `bundle_notices.py metadata`, which
writes the following values before ordinary signing:

```xml
<key>LSSupportsGameMode</key>
<true/>
<key>LSApplicationCategoryType</key>
<string>public.app-category.games</string>
```

The helper's Python API is backward-compatible:
`metadata(app, identifier, name, version, build, minimum, game_mode=False)`.
Passing false preserves source policy; it is not a request to disable an
existing opt-in. Only explicit true inserts/overrides support and category.
Existing identifier/name/version/minimum-OS overrides and unrelated plist fields
retain their behavior. No delivered or running app is patched in place.

Apple documents `LSSupportsGameMode` as the Boolean support key, available on
macOS 26. True makes Game Mode available, but does not prove OS activation in a
particular run. Omitting it may prevent activation. See Apple's
[LSSupportsGameMode documentation](https://developer.apple.com/documentation/bundleresources/information-property-list/lssupportsgamemode).
The category comes from Apple's supported
[application category values](https://developer.apple.com/documentation/bundleresources/information-property-list/lsapplicationcategorytype).

Apple's WWDC25 guidance describes Game Mode for fullscreen games and a banner
indicating activation. The OS/player retains control. Minimized automated
acceptance does not verify fullscreen activation. See
[Level up your games, system performance](https://developer.apple.com/videos/play/wwdc2025/209/).

## Scope and verification

The staging flow validates the actual game executable. The independent
NativeHost SDK and authored triangle/conformance client do not use this helper
and receive no opt-in. No legacy key, process priority, CPU affinity, QoS, power
mode, user setting, quality, simulation cadence or audio-format change is made.
There is no FPS, energy, latency or activation claim. Observed timing differences
among candidates remain unresolved after the same-executable metadata
comparison; the complete results are in the Apple silicon report. An unproven policy change is not the delivery default.

Pure temporary-plist tests cover default preservation of existing false/category
values, default absence without added keys, preservation of existing true/Games,
rejected-write immutability, repeated updates, existing overrides and explicit
true replacing false/another category. The source-app staging test requests ON
explicitly and checks both keys; that opt-in fixture does not change the normal
packaging default. Python syntax/pure checks pass without builds or runtime.

The original unconditional MAC342 implementation and hashes remain in ignored
`_work/publication/mac342-before-explicit-opt-in/` to preserve its test history.
Final source-app explicit-ON staging, strict signature, notice closure and
relocation pass. Default-OFF build352 preserves absent keys and matches the
compiled executable tested in actual350. ZIP extraction/relocation also passes.
Manual fullscreen OS indicator checks remain unverified. Future performance comparisons must control graphics,
workload, audio, power conditions and actual Game Mode state.
