# File provenance

This directory is an extraction of the independently authored macOS host added
by the Blur native port (MAC327). `src/Window.mm` preserves that port's native
`MacWindow.mm` implementation with generic names and fixed-width standard types.
That implementation is absent from the pristine inherited base revision
`1965facd3d4860447a043b1e0fea4d55c086d1f5`. The public API, CMake packaging and
authored client/conformance programs are part of this extraction.

The accompanying MIT LICENSE applies to this `native_host/` directory only,
Copyright 2026 Harshvardhan. It does not license the parent engine compatibility
wrapper, inherited Mercury/Blur source, original game assets, Windows binaries,
shader microcode or any other repository content. The package contains none of
those game data files or engine serializers. Cocoa, QuartzCore and Metal are
system frameworks; no copies are bundled here. There are no vendored third-party
libraries in this package.
