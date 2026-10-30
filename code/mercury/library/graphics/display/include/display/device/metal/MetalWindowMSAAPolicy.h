// [Blur2PC][macOS] Opt-in original PC display-window sample policy, independent
// of framebuffer/deferred sampling. Parse before creating or retiring resources.
#pragma once
#include <cstring>

namespace Display::Metal {
inline constexpr const char* WindowMSAAConfigurationError =
    "Invalid BLUR_MAC_WINDOW_MSAA: expected authored, 2 or 4";

// Unset/authored preserves the caller's authored selection (native default None).
// Invalid input leaves the output unchanged; the device must reject creation.
inline bool SelectWindowMultisample(const char* setting, unsigned authored,
                                   unsigned& selected) noexcept {
    if (!setting || std::strcmp(setting, "authored") == 0) selected = authored;
    else if (std::strcmp(setting, "2") == 0) selected = 2;
    else if (std::strcmp(setting, "4") == 0) selected = 4;
    else return false;
    return true;
}
}
