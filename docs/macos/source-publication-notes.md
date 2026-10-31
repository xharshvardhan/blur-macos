# Public source snapshot

The public development repository is [xharshvardhan/blur-macos](https://github.com/xharshvardhan/blur-macos), under the personal account. Its initial tree is an independent source snapshot derived from [uKER/BlurRebuilt at 1965facd](https://github.com/uKER/BlurRebuilt/commit/1965facd3d4860447a043b1e0fea4d55c086d1f5). The original Windows README and inherited source notices are preserved. No new blanket license is assigned to inherited code.

The published tree excludes retail game installations, console avatar/DLC and trophy payloads, packaged sample media, prebuilt vendor binaries, private profiles/saves, generated applications, build output and local runtime evidence. Embedded upstream developer login overrides are removed from the publication copy; normal profile behavior is preserved. Vendor private test-key files and the upstream Windows release workflow are also excluded.

The snapshot has its own initial commit, so excluded upstream payloads and credential literals are not imported through Git history. The local development branch and original upstream history remain intact. Later public commits can build on this source snapshot.

The native game builds its runtime dependencies from pinned source or the host packages described in [dependencies.md](dependencies.md). The0.1.7 development application is now provided separately as a GitHub prerelease asset, not committed into source history. It does not include game data or the upstream prebuilt Windows toolchain. Supply your own complete original PC data separately. See [installation and current limits](installing-development-preview.md).

Validation distinguishes source checks from game proof. The Brighton/BMW
keyboard slice completed its original checkpoint/lap/finish sequence and exited 0.
The normal frontend created/reloaded a career profile, played tutorial movies,
reached the owned-car garage and shut down with exit 0. Ordinary StartScreen
desktop confirmation also exits 0; the populated 20-car shutdown completes after
the native pickup borrower repair.

A normal 20-car, three-lap career race completed its original ordered splits/laps and
RaceFinished under human keyboard control (232), then failed when the automatic
results photo cleared before BeginScene. Actual ordinary Photo Mode reproduced
the precise application call stack. The shared native photo-clear boundary now
owns a temporary scene or borrows its caller's scene, with cleanup/retry tests
and exact GPU accumulation pixels. Debug/Speed applications each pass eight
affected checks; the fixed actual Photo Mode completes capture and ordinary
return to racing. The subsequent full normal 20-car, three-lap run completes its non-medal results return,
fresh save and ordinary desktop exit. A new signed relocated app reloads that
save with 0 unread bytes and retained 60 fans/event/car record, loading all 21
non-system libraries from its bundle. See
[career-cycle-acceptance.md](career-cycle-acceptance.md).

The coverage/worker integration passes 19 affected checks in each Debug and
optimized build. Metal alpha-to-coverage implements original crowd/mirror
requests at existing sample counts, with focused GPU coverage at 1x/2x/4x. The
optimized rollback timeout was reproduced with live thread capture: resource
and layer workers shared a private quit-event name. Native per-owner signals
fix that class while preserving general named sharing. A deterministic old-code
regression fails and ten unchanged optimized World processes pass without a
test timeout increase. See [the worker audit](resource-worker-retirement-audit.md)
and [coverage validation](alpha-to-coverage.md).

Local BundleUtilities staging now includes app metadata, dependency/native
notices and installed SPDX records. Strict signatures, dependency hashes and
alias-preserving copy/ZIP checks pass. Another clean Mac, notarized distribution,
medal/unlock progression and real LAN play remain unverified. See
[development-bundle-staging.md](development-bundle-staging.md).
