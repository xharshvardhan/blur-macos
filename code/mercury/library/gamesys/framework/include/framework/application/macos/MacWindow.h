// [Blur2PC][macOS] Cocoa host for the engine; main-thread lifetime/event operations.
#pragma once
#include <core/Core.h>
#include <functional>
#include <memory>
#include <string>
namespace Framework::Mac {
namespace Detail {struct WindowState;}
struct WindowConfig {std::string title="Blur native development";U32 width=1280,height=720;bool startMinimized=true;};
struct KeyEvent {U16 code=0;U64 modifiers=0;bool down=false,repeat=false,modifiersOnly=false;std::string text;};
struct MouseEvent {
    enum Kind {Move,Down,Up,Scroll} kind=Move;
    F64 x=0,y=0,scrollX=0,scrollY=0;S32 button=0;bool preciseScroll=false;
};
struct WindowCallbacks {
    std::function<void(U32,U32)> resized;
    std::function<void(bool)> focused;
    std::function<void()> close;
    std::function<void(const KeyEvent&)> key;
    std::function<void(const MouseEvent&)> mouse;
};
class Window {
public:
    Window();~Window();
    Window(const Window&)=delete;Window& operator=(const Window&)=delete;
    bool Create(const WindowConfig& config,const WindowCallbacks& callbacks={});
    void Destroy();void PollEvents();
    bool IsCreated()const;bool CloseRequested()const;
    U32 PixelWidth()const;U32 PixelHeight()const;
    void* NativeWindow()const;void* MetalLayer()const;
private:
    std::shared_ptr<Detail::WindowState> m_State;
};
}
