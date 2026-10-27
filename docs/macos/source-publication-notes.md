# Public source snapshot

The public development repository is [xharshvardhan/blur-macos](https://github.com/xharshvardhan/blur-macos), under the personal account. Its initial tree is an independent source snapshot derived from [uKER/BlurRebuilt at 1965facd](https://github.com/uKER/BlurRebuilt/commit/1965facd3d4860447a043b1e0fea4d55c086d1f5). The original Windows README and inherited source notices are preserved. No new blanket license is assigned to inherited code.

The published tree excludes retail game installations, console avatar/DLC and trophy payloads, packaged sample media, prebuilt vendor binaries, private profiles/saves, generated applications, build output and local runtime evidence. Embedded upstream developer login overrides are removed from the publication copy; normal profile behavior is preserved. Vendor private test-key files and the upstream Windows release workflow are also excluded.

The snapshot has its own initial commit, so excluded upstream payloads and credential literals are not imported through Git history. The local development branch and original upstream history remain intact. Later public commits can build on this source snapshot.

The native game builds its runtime dependencies from pinned source or the host packages described in [dependencies.md](dependencies.md). This source snapshot does not provide an installer, game data or the upstream prebuilt Windows toolchain. Supply your own complete original PC data separately when running the native application.

Validation distinguishes source checks from game proof. The stream/audio/save/UI integration passed 19 affected checks in each configuration. An original Brighton/BMW keyboard race completed its natural checkpoint/lap sequence and exited 0. A populated normal Debug frontend has also created/reloaded a career profile, played its tutorial movies, reached the owned-car garage and shut down with exit 0. Normal career race/results progression, real LAN play, portable packaging and SP exit confirmation after populated play remain separate verification work. A separate StartScreen run verified ordinary desktop confirmation and actual exit 0.

The next normal career event reached its race introduction but stopped on a
missing stencil attachment. The PC borrowed-depth pass contract is now
restored and both coherent builds pass 14 affected renderer checks. The actual
normal-career rerun reaches HUD/Racing with 20 cars and ordinary keyboard motion,
and its subsequent populated shutdown completes with exit 0 after the native
pickup job/object/actor-lookup lifetime repair. Normal career completion/results
are not established.

The later coverage/worker integration passes19 affected checks in each Debug
and optimized build. Metal alpha-to-coverage implements the original crowd and
mirror request at existing sample counts, with focused GPU coverage at1x/2x/4x.
The optimized rollback timeout was subsequently reproduced with live thread
capture: resource and layer workers shared a private quit-event name. Native
per-owner quit signals fix that class while preserving general named sharing;
a deterministic old-code regression fails and ten unchanged optimized World
rollback processes now pass. The original timeout remains recorded without a
test timeout increase. See [the worker ownership audit](resource-worker-retirement-audit.md)
and [coverage validation](alpha-to-coverage.md). Full normal career results and
save-return acceptance remain separate from these component results.
