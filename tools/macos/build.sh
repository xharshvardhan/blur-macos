#!/bin/sh
# [Blur2PC][macOS] Actual native game plus asset-independent engine checks.
set -eu
cd "$(dirname "$0")/../.."
if [ "$(uname -s)" != Darwin ]; then
    echo "This target requires macOS and the Apple SDK." >&2
    exit 1
fi
cmake -S . -B _work/build/macos -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64 -DBLUR_MAC_BUILD_GAME=ON
cmake --build _work/build/macos --parallel "${BLUR_MAC_JOBS:-6}"
python3 tools/macos/run_exclusive.py --timeout 900 -- ctest --test-dir _work/build/macos --output-on-failure
_work/run/BlurMacBootstrap.app/Contents/MacOS/BlurMacBootstrap --self-test
echo "Built _work/run/BlurNativeGame.app and native diagnostics."
echo "Run the game with --data-root <complete PC game-data directory>; build checks do not establish gameplay correctness."
