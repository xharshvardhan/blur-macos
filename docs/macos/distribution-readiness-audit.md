# MAC235 relocatable Apple Silicon app audit

Read-only inspection of the actual raw Speed231 development app. No staging,
package creation, install, build, GPU/game/input run or production edit occurred.
Current game validation and this packaging audit are separate acceptance gates.

## Actual artifact

`_work/build/macos-release/run/BlurNativeGame.app` executable SHA256:
`a2502036bb4214cbe2a508243c32f21a561ebb5e721ff8e17553a87885cdf2c6`.
`file`/`lipo` identify arm64 only; executable LC_BUILD_VERSION minOS13.0/sdk26.2.
Its nine direct Homebrew dylibs expand to21 non-system dylibs from16 packages.
All21 are arm64 and declare minOS26.0. No unresolved dependency was found on this
host. Absolute `/opt/homebrew/...` install names remain and Contents/Frameworks
is absent: this raw app cannot be described as Homebrew-independent.

| Dependency group | Actual non-system dylibs |
| --- | --- |
| FFmpeg9.0.1_1 | avformat63, avcodec63, avutil61, swscale10, swresample7 |
| Unicode/crypto/JPEG | libunistring5, libtomcrypt1, libtommath1, libgmp10, libjpeg8 |
| FFmpeg transitive closure | crypto3, ssl3, x265217, x264165, SvtAv1Enc4.2.0, opus0, mp3lame0, mpg1230, dav1d7, lzma5, vpx12 |

Ignored `_work/publication/mac235/dependency-inventory.json` records exact real
paths, versions, architectures, deployment commands and closure edges. System
SDK frameworks/libraries are excluded from copying. The compile flag13.0 does
not lower the deployment requirements of the installed26.0 libraries.

## Existing reuse fits

`tools/macos/stage_bundle.cmake` already uses CMake BundleUtilities instead of a
custom Mach-O dependency rewriter. It copies the app to a new directory, calls
fixup_bundle for transitive dependencies, derives minimumOS from bundled Mach-O
files and signs the result ad hoc. CMake documents that fixup_bundle copies and
rewrites dependencies and invokes verify_app. Reuse and validate this existing
path; no additional bundler is justified by this closure.
[CMake primary documentation](https://cmake.org/cmake/help/latest/module/BundleUtilities.html).

There is no exposed package target/install rule or packaging instruction in the
current build README. Once the app itself is validated, CPack's maintained
DragNDrop generator can produce a DMG if desired; it does not replace dependency
fixup or app signing. [CPack primary documentation](https://cmake.org/cmake/help/latest/cpack_gen/dmg.html).

## Concrete remaining gaps

| Boundary | Observed state and required next work |
| --- | --- |
| Dependency relocation | Run existing staging script on an immutable validated app copy, then independently verify every load reference is system-owned or inside the bundle; no Homebrew/build references or external symlinks. Its static scan does not itself prove all dynamically loaded behavior. |
| Minimum OS | Current complete dependency closure requires26.0. Either declare/test26.0 honestly, or rebuild every non-system library against one selected older target and rerun decode/Unicode/crypto/game checks. Editing Info.plist alone is insufficient. |
| App metadata | Raw Info.plist has blank CFBundleIdentifier, short/build versions and no LSMinimumSystemVersion. Set stable app identity/version and derived minimumOS before signing; add an icon/product name if producing a user-facing release. |
| Resources | Actual app contains693 generated shader resources,5.3MB. NativeMain resolves them through NSBundle resourcePath and original data through a separately chosen/remembered folder. Private copied-app runs establish bundle-relative shader lookup on this host, not a clean-machine dependency test. |
| User data | Profile identity defaults to Application Support/BlurRebuilt; saves use Application Support/Bizarre Creations/Blur. CLI overrides remain optional verification controls. Data validation uses the original tree names; case-sensitive data volumes need separate validation. No retail data should enter the package. |
| Signing | Current code has linker ad-hoc signature, no TeamIdentifier, unbound plist and unsealed resources. Stage script improves local integrity with inside-out dylib/app ad-hoc signing. Public Developer-ID distribution additionally needs its real identity, suitable hardened runtime, signing verification and notarization/stapling workflow; these are not present. |
| Notices/BOM | SDL3/SoundTouch/MojoShader notices are generated BESIDE the app in run/licenses. Stage script copies only the app, so those notices currently disappear. Copy them and the actual dynamic-library notices/source-version manifest into staged Resources/ThirdParty; preserve inherited source notices without inventing a blanket license. |

Apple's primary distribution guidance distinguishes ad-hoc local development
from Developer-ID signed/notarized distribution. Do not claim the present
linker signature is that workflow. [Apple signing](https://developer.apple.com/documentation/xcode/creating-distribution-signed-code-for-the-mac/),
[Apple notarization](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution).

## Exact dependency provenance work

All16 installed Homebrew packages already provide sbom.spdx.json and formula
metadata; reuse those records plus pinned CMake fetched-source notices rather
than inventing license names. The actual installed FFmpeg build enables gpl,
version3, x264 and x265; its own `ffmpeg -L` reports GPL3-or-later. It is not the
minimal base FFmpeg configuration. The package also pulls encoder/network
libraries unused by the local Bink movie boundary. A pinned minimal Bink-capable
FFmpeg build is a reasonable later reduction, but its exact configure options
must be validated against original video/audio/preload/streaming fixtures.
No configuration was changed or license compatibility conclusion assumed here.

SDL3 and SoundTouch are linked statically; MojoShader's selected translator is
also static. They need notices despite not appearing in otool. Build-time Python,
Ninja, CMake, glslang/SPIRV tools and the ffmpeg CLI are not app runtime dylibs;
the movie implementation calls FFmpeg libraries. The app does not need the
fixture-authoring ffmpeg executable bundled just because tests use it.

## Bounded next plan

1. Freeze a gameplay-validated Speed app SHA and choose the actual supported OS
   baseline. Preserve the delivered app; stage into a fresh ignored directory.
2. Complete metadata and Resources/ThirdParty manifest/notices, then use the
   existing stage_bundle.cmake with BLUR_SOURCE_APP and a new BLUR_STAGE_DIR.
3. Inspect staged complete closure/architectures/minOS/symlinks and verify its
   final signature after all modifications. Keep a machine-readable manifest.
4. Test a moved/renamed app in a path containing spaces, without build-tool or
   DYLD environment overrides, and on an Apple Silicon machine without these
   Homebrew installations. Finder data-folder selection, movies, real gameplay,
   saves/restart and clean quit remain runtime requirements.
5. Apply the selected distribution signing/notary workflow, archive the final
   signed artifact and publish its hash/build requirements. CPack is optional
   packaging reuse after the bundle passes; no package has been made by MAC235.
