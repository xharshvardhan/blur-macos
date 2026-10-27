// [Blur2PC][macOS] Opt-in observation of completed engine frames; disabled by default.
#pragma once
#include <string>
namespace Framework::Mac {
// Configure on the application thread before Run. Empty path or negative capture
// delay disables capture; negative quit delay disables shutdown. Zero is due on
// the first completed frame. Each configured action is attempted at most once.
void ConfigureRunDiagnostics(const std::string& pngPath,double captureAfterSeconds,double quitAfterSeconds);
// Absolute elapsed wall time since Run began, called after the frame/endRender
// and render-thread ownership have retired. This does not advance game state.
void TickRunDiagnostics(double elapsedSeconds);
}
