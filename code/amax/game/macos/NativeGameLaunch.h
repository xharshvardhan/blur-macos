// [Blur2PC][macOS] Entry-point choice, configured before constructing the game.
#pragma once
namespace AmaxMac {
inline bool manualLaunch = false;
inline void SetManualLaunch(bool value) { manualLaunch = value; }
inline bool IsManualLaunch() { return manualLaunch; }
}
