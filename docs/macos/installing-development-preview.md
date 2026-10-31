# Install the native Mac development preview

**[Download 0.1.8 for Apple Silicon](https://github.com/xharshvardhan/blur-macos/releases/tag/v0.1.8)** · [Direct app ZIP](https://github.com/xharshvardhan/blur-macos/releases/download/v0.1.8/BlurNativeDevelopment-0.1.8-arm64.zip) · [SHA256SUMS](https://github.com/xharshvardhan/blur-macos/releases/download/v0.1.8/SHA256SUMS.txt)

**[Download the original PC game data — MyAbandonware (third-party)](https://www.myabandonware.com/game/blur-gls)**

This is an unofficial playable development preview. The application runs natively with Cocoa/Metal. It includes its native runtime libraries; players do not need to compile the source or install Wine, Homebrew or Xcode. The GitHub “Source code” archives are for developers and do not contain the ready-to-run app.

## Requirements

- An Apple Silicon Mac and **macOS 26 or later**. This package is arm64 only; an older compilation target does not establish older macOS support. The development machine is an M4 Pro. A different clean Mac has not yet been validated.
- Your own complete original **Windows Blur game-data folder**. The Mac app ZIP does not contain or fetch the original cars, tracks, textures, movies or music. A Windows installer or executable by itself is insufficient: supply the installed folder tree.

## Get the game-data folder

The [MyAbandonware Blur download page](https://www.myabandonware.com/game/blur-gls) is the third-party source used for local testing. The [exact English Full-Rip download button](https://www.myabandonware.com/download/mc0s-blur) selects the preinstalled archive. In its Download section, the **Full-Rip of English version** option is a preinstalled data archive. Extract that archive and retain the complete `BLUR` directory containing `cache`, `levels` and `UI`. This is the directory to select in the Mac app. Link availability and other variants on the external site may change; they have not all been validated with this port.

The site and its downloads are external to this project, and this reference does not establish redistribution permission. Its Windows-specific setup instructions apply to running the Windows game; the native Mac app reads the extracted data and does not run that Windows executable. You can also use the complete installed data folder from an existing PC copy you are entitled to use.

## Install and launch

1. Download `BlurNativeDevelopment-0.1.8-arm64.zip` from the release's Assets section. Optionally download `SHA256SUMS.txt` alongside it and run `shasum -a 256 -c SHA256SUMS.txt` in that directory. The expected ZIP SHA256 is `d071b8e4646e766a21c1e424c9083a9d5eead09a7b1f14f898cfd44b4ab32169`.
2. Extract the ZIP in Finder. Open `BlurNativeGame.app`; moving it to Applications first is optional. Preserve the app as a bundle when copying it.
3. If macOS blocks launch, consult [Apple's current app-approval instructions](https://support.apple.com/en-us/102445). This preview is ad-hoc signed for integrity, **not Developer ID signed or notarized**. When you trust this exact download and macOS offers the per-app exception, its approval is in System Settings → Privacy & Security → Open Anyway. Installation through downloaded/quarantined Finder launch on another Mac remains unverified. Do not disable system-wide security settings as an installation step.
4. At first launch, select the complete game folder containing `cache`, `levels` and `UI`, rather than one of those subfolders. The app remembers it and reads the original game data in place. Keep that data available on subsequent launches.
5. Use **Options → Keyboard → Layout 2** for WASD controls. Existing saved layouts are preserved, so a previous profile may start with another layout.

The original game data is separate from the Mac application and from your saves. Use the extracted full data folder described above or the complete installed folder from your existing PC copy. The app does not run the Windows executable. Initial folder checks include nonempty `cache/root.pak`, `cache/gamedata.pak`, `UI/movies/atvi.bik`, `UI/movies/BizarreSplash.bik` and a complete level; passing those checks alone does not establish that every required archive/signature/level exists. Keep the complete tree, including signature files and original folder names. Case-sensitive data volumes remain unverified.

## Controls and LAN

W/S accelerate/brake, A/D steer and Space handbrake in Layout 2. Up/Down fire forward/backward, Left switches power-up, Right discards and either Control fires the selected power-up. Return/Space confirms; Escape backs out/pauses. Normal manual launches have sound.

See [the complete keyboard map and LAN hosting steps](native-keyboard-hosting.md). Either Control hosts and either Shift opens lobby options/refresh. LAN host-lobby creation has passed, but a second computer joining, multiplayer racing and Windows/Mac interoperability remain unverified. Hosted online services are unavailable.

Version0.1.8 also repairs the native car-ID mismatch that caused LAN car selection to log a handled Lua error. See [the repair and exact verification](lan-garage-car-selection.md).

## Saves and troubleshooting

Default profiles live under `~/Library/Application Support/BlurRebuilt`; career saves live under `~/Library/Application Support/Bizarre Creations/Blur`. Existing developer launchers may explicitly select other locations; those private paths are not required by this release. Keep existing saves backed up when trying a development preview. Do not delete profiles or saves to diagnose a launch failure.

If the folder picker rejects a selection, choose the folder one level above `cache`, `levels` and `UI` and retain the full installed data tree. For a useful bug report, include the release/build number, Mac chip, macOS version, exact error and reproduction steps. Avoid attaching game archives, profiles, account details or unrelated private logs.

The package has passed strict signature, dependency/notices, ZIP extraction and relocation checks on the development machine. Original game startup, driving, save/reload and normal exits have bounded actual-game evidence. Both final Debug/optimized suites passed156/156 with automated audio muted. This does not establish every mode, a clean second Mac or a notarized distribution workflow. Frame-time spikes and the reported perceptual race-music symptom remain limits; the separate overnight CoreAudio startup failure and successful rechecks are documented in [the audio investigation](audio-startup-wake-investigation.md).

## Quality settings and source

Version0.1.8 retains the filtering enhancement introduced in0.1.7, which strengthens filtering only for eligible original anisotropic samplers and preserves original artistic resources. Window multisampling2x/4x is optional because it increased measured GPU/frame/memory cost. To launch once with original quality from Terminal:

```sh
BLUR_MAC_TEXTURE_ANISOTROPY=authored BLUR_MAC_WINDOW_MSAA=authored /Applications/BlurNativeGame.app/Contents/MacOS/BlurNativeGame
```

Adjust the app path if installed elsewhere. Overrides affect that process and do not rewrite saved options. See [quality acceptance](faithful-remaster-acceptance.md) for actual image and cost evidence, [build instructions](../../README.macos.md) for developers, and [release staging](development-bundle-staging.md) for bundling contracts. The app's notices/SPDX records are in `Contents/Resources/ThirdParty`; no blanket license is assigned to inherited engine code or game content.
