// Authored external client. Uses only the installed public host API and AppKit.
#include <native_host/Window.h>
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#include <atomic>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static NSEvent* key(NSWindow* window,NSEventType type,unsigned short code,NSString* text,NSEventModifierFlags flags=0,bool repeat=false){
    return [NSEvent keyEventWithType:type location:NSZeroPoint modifierFlags:flags timestamp:0
        windowNumber:window.windowNumber context:nil characters:text charactersIgnoringModifiers:text isARepeat:repeat keyCode:code];
}
int main(){@autoreleasepool{try{
    static_assert(std::is_same_v<decltype(NativeHost::KeyEvent{}.code),std::uint16_t>);
    static_assert(std::is_same_v<decltype(NativeHost::KeyEvent{}.modifiers),std::uint64_t>);
    NativeHost::Window window;NativeHost::WindowConfig config;
    config.width=160;config.height=90;config.title="NativeHost independent conformance";
    require(config.startMinimized,"Automated client must default to minimized");
    auto invalid=config;invalid.width=0;require(!window.Create(invalid),"Invalid dimensions were accepted");
    unsigned resizes=0,closes=0;std::vector<NativeHost::KeyEvent> keys;
    std::vector<NativeHost::MouseEvent> mice;std::vector<bool> focus;
    bool held=false; // Client-owned release policy, deliberately outside host.
    NativeHost::WindowCallbacks callbacks;
    callbacks.resized=[&](auto width,auto height){require(width&&height&&window.IsCreated(),"Resize preceded owner publication");++resizes;};
    callbacks.close=[&]{++closes;};
    callbacks.focused=[&](bool active){focus.push_back(active);if(!active)held=false;};
    callbacks.key=[&](const auto& event){keys.push_back(event);if(event.text=="first")throw std::runtime_error("first callback error");if(event.text=="second")throw std::runtime_error("second callback error");if(!event.modifiersOnly)held=event.down;};
    callbacks.mouse=[&](const auto& event){mice.push_back(event);};
    require(window.Create(config,callbacks)&&!window.Create(config),"Create/duplicate ownership contract failed");
    window.PollEvents();
    NSWindow* native=(__bridge NSWindow*)window.NativeWindow();NSView* view=native.contentView;
    const auto until=[NSDate dateWithTimeIntervalSinceNow:2];
    while(!native.isMiniaturized&&until.timeIntervalSinceNow>0){window.PollEvents();[[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];}
    require(native.isMiniaturized&&!native.isKeyWindow,"Default test window was not minimized");
    auto verifyPixels=[&]{const auto backing=[view convertRectToBacking:view.bounds];CAMetalLayer* layer=(__bridge CAMetalLayer*)window.MetalLayer();
        require(window.PixelWidth()==std::uint32_t(std::round(backing.size.width))&&window.PixelHeight()==std::uint32_t(std::round(backing.size.height)),"Backing pixel conversion differs from AppKit");
        require(layer.drawableSize.width==window.PixelWidth()&&layer.drawableSize.height==window.PixelHeight(),"Layer drawable dimensions differ from host pixels");};
    verifyPixels();require(resizes>=1,"Initial resize callback missing");
    bool foreignCreate=false,foreignPoll=false,foreignDestroy=false;
    std::thread worker([&]{NativeHost::Window other;foreignCreate=!other.Create(config);
        try{window.PollEvents();}catch(const std::logic_error&){foreignPoll=true;}
        try{window.Destroy();}catch(const std::logic_error&){foreignDestroy=true;}});worker.join();
    require(foreignCreate&&foreignPoll&&foreignDestroy&&window.IsCreated(),"Cocoa main-thread contract failed");
    [view keyDown:key(native,NSEventTypeKeyDown,0,@"a",NSEventModifierFlagShift,true)];
    [view keyUp:key(native,NSEventTypeKeyUp,0,@"a")];
    require(keys.size()==2&&keys[0].code==0&&keys[0].text=="a"&&keys[0].repeat&&keys[0].down&&keys[0].modifiers==NSEventModifierFlagShift&&!keys[1].down,"Raw key event fields changed");
    [view flagsChanged:key(native,NSEventTypeFlagsChanged,59,@"",NSEventModifierFlagControl)];
    [view flagsChanged:key(native,NSEventTypeFlagsChanged,62,@"",NSEventModifierFlagControl)];
    require(keys[2].modifiersOnly&&keys[2].code==59&&keys[3].modifiersOnly&&keys[3].code==62,"Physical left/right modifier identity changed");
    [view keyDown:key(native,NSEventTypeKeyDown,0,@"a")];require(held,"Client key state was not held");
    // Authored notifications traverse the actual delegate; they do not claim
    // physical user focus or operating-system keyboard automation coverage.
    [native.delegate windowDidBecomeKey:[NSNotification notificationWithName:NSWindowDidBecomeKeyNotification object:native]];
    [native.delegate windowDidResignKey:[NSNotification notificationWithName:NSWindowDidResignKeyNotification object:native]];
    require(focus.size()>=2&&focus[focus.size()-2]&&!focus.back()&&!held,"Focus callback/client release policy failed");
    NSEvent* mouse=[NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:NSMakePoint(10,12) modifierFlags:0 timestamp:0 windowNumber:native.windowNumber context:nil eventNumber:1 clickCount:1 pressure:1];
    [view mouseDown:mouse];require(mice.size()==1&&mice[0].kind==NativeHost::MouseEvent::Down&&mice[0].button==0&&mice[0].x>=0&&mice[0].y>=0,"Mouse callback lost pixel/button fields");
    [view keyDown:key(native,NSEventTypeKeyDown,0,@"first")];[view keyDown:key(native,NSEventTypeKeyDown,0,@"second")];
    bool first=false;try{window.PollEvents();}catch(const std::runtime_error& e){first=std::string(e.what())=="first callback error";}
    require(first,"First callback exception was lost or crossed AppKit");window.PollEvents();
    const auto old=window.PixelWidth();[native setContentSize:NSMakeSize(192,108)];window.PollEvents();
    require(window.PixelWidth()!=old&&resizes>=2,"Resize callback/pixels did not advance");verifyPixels();
    [native performClose:nil];window.PollEvents();require(window.CloseRequested()&&window.IsCreated()&&closes==1,"Advisory close destroyed host early");
    window.Destroy();window.Destroy();require(!window.IsCreated()&&!window.NativeWindow()&&!window.MetalLayer(),"Teardown retained public handles");
    require(window.Create(config),"Destroyed owner could not recreate");window.Destroy();
    NativeHost::Window reentrant;NativeHost::WindowCallbacks destroyOnKey;
    destroyOnKey.key=[&](const auto&){reentrant.Destroy();};require(reentrant.Create(config,destroyOnKey),"Reentrant client create failed");
    NSWindow* again=(__bridge NSWindow*)reentrant.NativeWindow();NSView* againView=again.contentView;
    [againView keyDown:key(again,NSEventTypeKeyDown,0,@"a")];require(!reentrant.IsCreated(),"Callback self-destruction failed");
    NativeHost::Window initial;NativeHost::WindowCallbacks destroyInitially;
    destroyInitially.resized=[&](auto,auto){initial.Destroy();};require(!initial.Create(config,destroyInitially)&&!initial.IsCreated(),"Initial callback destruction republished a dead owner");
    NativeHost::Window initialError;NativeHost::WindowCallbacks throwInitially;
    throwInitially.resized=[](auto,auto){throw std::runtime_error("initial resize error");};
    bool propagated=false;try{initialError.Create(config,throwInitially);}catch(const std::runtime_error& e){propagated=std::string(e.what())=="initial resize error";}
    require(propagated&&initialError.IsCreated(),"Initial callback exception/ownership contract changed");initialError.Destroy();
    std::cout<<"PASS installed host conformance: minimized window, Retina dimensions, raw authored key/mouse/modifier events, focus callback/client release, first-error handoff, resize, main-thread checks, reentrancy and teardown\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}}
