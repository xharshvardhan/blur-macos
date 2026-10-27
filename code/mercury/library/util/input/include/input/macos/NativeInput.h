// [Blur2PC][macOS] Window-local events; callbacks expire safely with the manager.
#pragma once
#include <input/DeviceManager.h>
#include <functional>
namespace Input::Mac {
struct Events {
    std::function<void(U16,bool,U64,bool)> key; // native code, down, AppKit modifiers, flagsChanged
    std::function<void(F64,F64)> pointer; // window coordinates, top-left origin
    std::function<void(S32,bool)> button; // AppKit button number
    std::function<void(F64)> scroll;
    std::function<void(bool)> focus;
};
Events GetEvents(C_DeviceManager* manager);
// [Blur2PC] Explicit automation enters the real physical-key sample/mapping
// path. The 150ms tap starts on the next input update and may run minimized.
S32 KeyFromName(const char* name); // original C_Keyboard enum, -1 for unknown
bool QueueKeyTap(C_DeviceManager* manager,S32 key);
// [Blur2PC] Independent per-key elapsed holds, starting at the first sample.
// Finite duration in (0,60] seconds. Same-key tap/hold conflicts return false.
bool QueueKeyHold(C_DeviceManager* manager,S32 key,F64 seconds);

C_Device* CreateDevice(C_DeviceManager* manager,S32 user,E_Device type);
}
