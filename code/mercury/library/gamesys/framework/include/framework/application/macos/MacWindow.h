// [Blur2PC][macOS] Thin compatibility names for the independently built host.
#pragma once
#include <core/Core.h> // Preserve historical engine-client scalar declarations.
#include <native_host/Window.h>
namespace Framework::Mac {
using WindowConfig=NativeHost::WindowConfig;
using KeyEvent=NativeHost::KeyEvent;
using MouseEvent=NativeHost::MouseEvent;
using WindowCallbacks=NativeHost::WindowCallbacks;
using Window=NativeHost::Window;
}
