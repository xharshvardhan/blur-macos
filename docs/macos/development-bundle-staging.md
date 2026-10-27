# MAC247 development app staging

The existing BundleUtilities staging path now produces a named, versioned local
Apple Silicon development app with its dependency notices inside the bundle.
This does not establish clean-machine launch, notarization or public binary
release readiness. No game data is included.

## Usage

With a raw built BlurNativeGame.app and its adjacent `licenses` directory:

```sh
cmake -DBLUR_SOURCE_APP=/absolute/path/BlurNativeGame.app \
  -DBLUR_STAGE_DIR=/absolute/path/to/a/new-stage-directory \
  -P tools/macos/stage_bundle.cmake
```

The source app is read only. The destination must be new and outside it;
existing destinations and symlinks are rejected. A failed stage is left for
inspection, never reported as successful; use another new destination to retry.
The raw-app preflight excludes unexpected payloads, game archives/movies/levels,
profiles/saves and loose build products. It accepts the actual executable,
metadata, generated shaders, optional icon and existing signature resources.
Already bundled apps are rejected: provenance must come from the raw build.

Development metadata defaults and optional CMake overrides:

| Variable | Default |
| --- | --- |
| BLUR_BUNDLE_IDENTIFIER | io.github.xharshvardhan.blur-macos.development |
| BLUR_BUNDLE_NAME | Blur Native Development |
| BLUR_BUNDLE_VERSION | 0.1.0 |
| BLUR_BUNDLE_BUILD_VERSION | 1 |
| BLUR_NATIVE_LICENSE_DIR | licenses beside the source app |

Identifier/name/version overrides are validated and written only in the staged
Info.plist. Actual dependency deployment commands determine
LSMinimumSystemVersion; it is not a user-declared compatibility override.
The staging tools require CMake 3.25+, Python 3.9+ and Apple's CLI Mach-O/signing
tools on the build host. They are not runtime dependencies of the resulting app.

## Reused tool and provenance boundary

CMake BundleUtilities supplies the original resolved dependency keys and remains
the only dependency copier/rewriter. `fixup_bundle` includes its standalone
verification. No custom install-name or binary rewriting implementation was
added. [Maintained upstream documentation](https://cmake.org/cmake/help/latest/module/BundleUtilities.html).

The small `bundle_notices.py` helper consumes those original keys before fixup:

- Copies SDL3, SoundTouch and MojoShader notices from beside the raw app into
  `Contents/Resources/ThirdParty/Native`.
- Locates each actual Homebrew Cellar package and copies its installed SPDX
  document and license/copying/notice/copyright/author files, preserving bytes
  and package-relative filenames. Missing package provenance, SPDX or notices
  fails clearly; no license permission or blanket application license is inferred.
- Writes `ThirdParty/manifest.json` with original executable/dependency hashes,
  relative Cellar/package versions, copied notice hashes and final signed
  dependency hashes. Personal source/build paths are not put in this manifest.
  Installed SPDX files remain original provenance documents, including any
  package-manager paths or NOASSERTION declarations they contain.

This manifest covers the staged external-library closure and the three adjacent
native notices. It does not claim a complete source-unit/application SBOM or
resolve inherited source licensing. Original source notices remain unchanged.

After BundleUtilities fixes the copied dependencies, inspection requires arm64
Mach-O files, sufficient deployment metadata, no external runtime load/search
paths and no unexpected Frameworks payload. Nested dylibs are signed ad hoc,
then their hashes/notices are sealed before signing the app. Strict deep
signature verification and a final manifest check must pass. The original
executable hash is recorded; a final app cannot embed a hash of its own changing
code signature as a sealed resource.

## Actual bounded verification

`stage_bundle_test.py` stages and checks a real app without launching it:

```sh
python3 tools/macos/stage_bundle_test.py \
  --source-app /absolute/path/BlurNativeGame.app \
  --stage-root /absolute/path/to/a/new-test-stage
```

The test also relocates the signed stage with `shutil.copytree(...,
symlinks=True)`, compares alias targets/content/modes, and verifies the copied
signature and complete notice/dependency manifest. To run that check alone on
an existing signed stage, without rebuilding or staging again:

```sh
python3 tools/macos/stage_bundle_test.py \
  --staged-app /absolute/path/to/stage/BlurNativeGame.app \
  --stage-root /absolute/path/to/a/new-relocation-test
```

Preserve symlinks when moving or copying an app: use Finder, Apple's `ditto`, or
Python `shutil.copytree(source, destination, symlinks=True)`. Python's default
`copytree` dereferences library aliases; that changes sealed resources and can
invalidate the copied signature even when the original stage remains valid.
Verify the resulting copy with `codesign --verify --deep --strict` and
`bundle_notices.py verify --app ...`. Do not re-sign a damaged copy as a repair;
copy again from the valid stage with symlink preservation.

Actual frozen Speed242 executable
`aedb97560f058cbc7effa5bfd1d1302e9cd88413103b3e8b7e8660b9a3860c56`
was staged into ignored `_work/publication/mac247-stage/BlurNativeGame.app`.
The CLI test passes with:

- 21 dependency records across 16 actual Homebrew packages, 57 installed
  notice/SPDX files plus 3 native notices;
- derived macOS 26.0 and explicit test identifier/name/version overrides;
- no external runtime dependency/search paths or escaping bundled symlinks;
- strict signature verification and notice/dependency hash verification;
- unchanged source app tree hash
  `2a4ce175ab5a8153a8b331f2864f5b87ab3bc0a44e9b08b4569e5df366329a41`;
- rejection of an existing destination, missing SPDX, missing package notices,
  invalid identifier and an authored forbidden retail-payload path.

The positive test/log/verification JSON and staged app are private ignored
outputs. The additional forbidden-payload preflight test was run independently
without repeating the full stage, since production staging code was unchanged.
No engine source, CMake game targets, shared archives, original dependencies or
source app were modified. No installer, game, GPU or Artery session was launched.

The bounded MAC247 follow-up copied the actual default-metadata MAC248 stage to
new ignored `_work/publication/mac247-followup-copy/BlurNativeGame.app` with all
seven library symlinks preserved. Both source and relocated strict signatures,
complete manifest/notice/dependency hashes and copied content/mode equality
passed; the original stage remained unchanged and neither app was re-signed.
Proof is in that new directory's `verification.json`. This CLI check preserves
the original MAC247 proof and does not claim a relocated game launch.

MAC251 passed a relocated launch on this same host, including the real saved
career reload, owned/allowed garage and ordinary exit. See
[career-cycle-acceptance.md](career-cycle-acceptance.md) for its evidence and
limits. A separate clean Apple Silicon host still needs validation with original
data selected separately, including movies/gameplay/save/reload/quit. The chosen
public signing/notary workflow also remains required. Ad-hoc signature
verification is local integrity evidence, not Developer ID or notarization
evidence.

## Local development 0.1.2 (MAC289)

The final feedback build is version0.1.2/build289, arm64 and derived macOS26+.
Its new local ZIP passes strict app signatures, dependency/notices manifest,
symlink-preserving extraction and relocation checks. It contains no original
game data or profiles. ZIP SHA256 is
`1bc041c4a6f856719da7378c610e2e2bc94d5a3e5c8c439d9196f19dc838fe40` (27427394 bytes).
This is a local ad-hoc development package, not a notarized or public binary
release. Both141-test suites pass; actual menu/save/reload and renderer results
are recorded in [feedback acceptance](player-feedback-acceptance.md).
