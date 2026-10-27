// Cocoa host API; no engine headers or asset/runtime dependencies.
#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace NativeHost {
namespace Detail { struct WindowState; }
// Initial dimensions are Cocoa content points. PixelWidth/Height and resize
// callbacks report backing pixels, including Retina backing-scale changes.
struct WindowConfig {
    std::string title="Native host";
    std::uint32_t width=1280,height=720;
    bool startMinimized=true;
};
// Raw Cocoa physical key code and modifier mask. Text is UTF-8; it is not an
// input-method/marked-text implementation. modifiersOnly denotes flagsChanged.
struct KeyEvent {
    std::uint16_t code=0;
    std::uint64_t modifiers=0;
    bool down=false,repeat=false,modifiersOnly=false;
    std::string text;
};
struct MouseEvent {
    enum Kind { Move,Down,Up,Scroll } kind=Move;
    double x=0,y=0,scrollX=0,scrollY=0;
    std::int32_t button=0;
    bool preciseScroll=false;
};
struct WindowCallbacks {
    std::function<void(std::uint32_t,std::uint32_t)> resized;
    // This callback reports focus. The client owns held-input release policy.
    std::function<void(bool)> focused;
    std::function<void()> close;
    std::function<void(const KeyEvent&)> key;
    std::function<void(const MouseEvent&)> mouse;
};
// Window lifetime and event polling belong to the Cocoa main thread. Create
// rejects other threads; Destroy/PollEvents throw for a live foreign-thread use.
// AppKit callbacks retain state through dispatch and capture the first C++
// exception for PollEvents. A callback may Destroy its own Window. Initial
// resize runs during Create after publication; its exception propagates directly.
// Close requests remain advisory until the client calls Destroy.
class Window {
public:
    Window();
    ~Window();
    Window(const Window&)=delete;
    Window& operator=(const Window&)=delete;
    bool Create(const WindowConfig&,const WindowCallbacks& callbacks={});
    void Destroy();
    void PollEvents();
    bool IsCreated()const;
    bool CloseRequested()const;
    std::uint32_t PixelWidth()const;
    std::uint32_t PixelHeight()const;
    // Borrowed NSWindow/CAMetalLayer handles; valid only while this owner lives.
    // No Metal device, renderer, input mapping or simulation is supplied here.
    void* NativeWindow()const;
    void* MetalLayer()const;
private:
    std::shared_ptr<Detail::WindowState> m_State;
};
} // namespace NativeHost
