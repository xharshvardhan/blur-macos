// [Blur2PC][macOS] Compatibility compile anchor. The one Cocoa implementation
// is compiled by the NativeHost::Window target, consumed by blur_mac_window.
#include <framework/application/macos/MacWindow.h>
#include <type_traits>
static_assert(std::is_same_v<Framework::Mac::Window,NativeHost::Window>);
