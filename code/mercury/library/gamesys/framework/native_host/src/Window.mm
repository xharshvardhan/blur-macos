// Cocoa event/window ownership for source-based native macOS applications.
// Extracted from the Blur native host adapter; see PROVENANCE.md.
#include <native_host/Window.h>
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cmath>
#include <algorithm>
#include <exception>
#include <stdexcept>
namespace NativeHost::Detail {struct WindowState;}
@interface NativeHostWindowDelegate:NSObject<NSWindowDelegate>{@public NativeHost::Detail::WindowState* state;}
@end
@interface NativeHostContentView:NSView{@public NativeHost::Detail::WindowState* state;}
@end
namespace NativeHost::Detail {
struct WindowState:std::enable_shared_from_this<WindowState> {
    NSWindow* window;NativeHostContentView* view;CAMetalLayer* layer;NativeHostWindowDelegate* delegate;
    WindowCallbacks callbacks;std::exception_ptr error;bool closing=false,focused=false;std::uint32_t width=0,height=0;
    template<class F>void dispatch(F&& fn){auto lifetime=shared_from_this();try{fn();}catch(...){if(!error)error=std::current_exception();}}
    void resize(){
        const auto rect=[view convertRectToBacking:view.bounds];
        const std::uint32_t w=std::uint32_t(std::max(1.0,std::round(rect.size.width))),h=std::uint32_t(std::max(1.0,std::round(rect.size.height)));
        layer.contentsScale=window.backingScaleFactor;layer.drawableSize=CGSizeMake(w,h);
        if(w!=width||h!=height){width=w;height=h;if(callbacks.resized)callbacks.resized(w,h);}
    }
    void focus(bool active){if(active!=focused){focused=active;if(callbacks.focused)callbacks.focused(active);}}
    void key(NSEvent* event,bool down){if(callbacks.key){KeyEvent value;value.code=event.keyCode;value.modifiers=event.modifierFlags;
        value.down=down;value.repeat=down&&event.isARepeat;value.text=event.characters.UTF8String?:"";callbacks.key(value);}}
    void mouse(NSEvent* event,MouseEvent::Kind kind){if(callbacks.mouse){MouseEvent value;value.kind=kind;
        auto p=[view convertPointToBacking:[view convertPoint:event.locationInWindow fromView:nil]];
        value.x=p.x;value.y=height-p.y;if(kind==MouseEvent::Down||kind==MouseEvent::Up)value.button=std::int32_t(event.buttonNumber);
        if(kind==MouseEvent::Scroll){value.scrollX=event.scrollingDeltaX;value.scrollY=event.scrollingDeltaY;value.preciseScroll=event.hasPreciseScrollingDeltas;}
        callbacks.mouse(value);}}
    ~WindowState(){if(delegate)delegate->state=nullptr;if(view)view->state=nullptr;window.delegate=nil;[window orderOut:nil];[window close];}
};
static void mainThread(){if(![NSThread isMainThread])throw std::logic_error("Cocoa window operations require the main thread");}
}
@implementation NativeHostWindowDelegate
-(BOOL)windowShouldClose:(NSWindow*)sender{(void)sender;if(state)state->dispatch([&]{state->closing=true;if(state->callbacks.close)state->callbacks.close();});return NO;}
-(void)windowDidResize:(NSNotification*)note{(void)note;if(state)state->dispatch([&]{state->resize();});}
-(void)windowDidChangeBackingProperties:(NSNotification*)note{(void)note;if(state)state->dispatch([&]{state->resize();});}
-(void)windowDidBecomeKey:(NSNotification*)note{(void)note;if(state)state->dispatch([&]{state->focus(true);});}
-(void)windowDidResignKey:(NSNotification*)note{(void)note;if(state)state->dispatch([&]{state->focus(false);});}
@end
@implementation NativeHostContentView
-(BOOL)acceptsFirstResponder{return YES;}
-(void)keyDown:(NSEvent*)event{if(state)state->dispatch([&]{state->key(event,true);});}
-(void)keyUp:(NSEvent*)event{if(state)state->dispatch([&]{state->key(event,false);});}
-(void)flagsChanged:(NSEvent*)event{if(state)state->dispatch([&]{if(state->callbacks.key){NativeHost::KeyEvent value;value.code=event.keyCode;value.modifiers=event.modifierFlags;value.modifiersOnly=true;state->callbacks.key(value);}});}
#define MOUSE_EVENT(Method,Kind) -(void)Method:(NSEvent*)event{if(state)state->dispatch([&]{state->mouse(event,NativeHost::MouseEvent::Kind);});}
MOUSE_EVENT(mouseDown,Down) MOUSE_EVENT(mouseUp,Up) MOUSE_EVENT(rightMouseDown,Down) MOUSE_EVENT(rightMouseUp,Up)
MOUSE_EVENT(otherMouseDown,Down) MOUSE_EVENT(otherMouseUp,Up) MOUSE_EVENT(mouseMoved,Move)
MOUSE_EVENT(mouseDragged,Move) MOUSE_EVENT(rightMouseDragged,Move) MOUSE_EVENT(otherMouseDragged,Move) MOUSE_EVENT(scrollWheel,Scroll)
#undef MOUSE_EVENT
@end
namespace NativeHost {
Window::Window()=default;Window::~Window(){Destroy();}
bool Window::Create(const WindowConfig& config,const WindowCallbacks& callbacks){@autoreleasepool{
    if(m_State||![NSThread isMainThread]||!config.width||!config.height||config.width>16384||config.height>16384)return false;
    NSString* title=[NSString stringWithUTF8String:config.title.c_str()];if(!title)return false;
    [NSApplication sharedApplication];
    auto state=std::make_shared<Detail::WindowState>();state->callbacks=callbacks;
    state->window=[[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,config.width,config.height)
        styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskMiniaturizable|NSWindowStyleMaskResizable backing:NSBackingStoreBuffered defer:NO];
    if(!state->window)return false;
    state->window.releasedWhenClosed=NO;state->window.title=title;state->window.acceptsMouseMovedEvents=YES;
    state->view=[[NativeHostContentView alloc] initWithFrame:NSMakeRect(0,0,config.width,config.height)];state->view->state=state.get();
    state->view.autoresizingMask=NSViewWidthSizable|NSViewHeightSizable;state->view.wantsLayer=YES;
    state->layer=[CAMetalLayer layer];state->view.layer=state->layer;state->window.contentView=state->view;
    state->delegate=[NativeHostWindowDelegate new];state->delegate->state=state.get();state->window.delegate=state->delegate;
    [state->window makeFirstResponder:state->view];m_State=state;state->resize();
    if(m_State!=state)return false;
    if(config.startMinimized)[m_State->window miniaturize:nil];else[m_State->window makeKeyAndOrderFront:nil];
    return true;
}}
void Window::Destroy(){@autoreleasepool{if(m_State){Detail::mainThread();m_State.reset();}}}
void Window::PollEvents(){@autoreleasepool{Detail::mainThread();if(!m_State)return;
    auto state=m_State;
    while(auto event=[NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES]){[NSApp sendEvent:event];if(m_State!=state)break;}
    [NSApp updateWindows];if(state->error){auto error=state->error;state->error=nullptr;std::rethrow_exception(error);}
}}
bool Window::IsCreated()const{return bool(m_State);}
bool Window::CloseRequested()const{return m_State&&m_State->closing;}
std::uint32_t Window::PixelWidth()const{return m_State?m_State->width:0;}std::uint32_t Window::PixelHeight()const{return m_State?m_State->height:0;}
void* Window::NativeWindow()const{return m_State?(__bridge void*)m_State->window:nullptr;}
void* Window::MetalLayer()const{return m_State?(__bridge void*)m_State->layer:nullptr;}
}
