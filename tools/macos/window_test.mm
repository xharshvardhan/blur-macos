// [Blur2PC][macOS] Actual minimized Cocoa window/layer, event routing and engine presentation.
#include <framework/application/macos/MacWindow.h>
#include <display2/device/Device.h>
#include <input/macos/NativeInput.h>
#include <input/Keyboard.h>
#include <input/Mouse.h>
#include <input/GameInput.h>
#include <input/GameInputMapping.h>
#import <Cocoa/Cocoa.h>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
static void require(bool v,const char* message){if(!v)throw std::runtime_error(message);}
int main(){@autoreleasepool{try{
    Input::C_DeviceManagerCreateParam inputParams;std::unique_ptr<Input::C_DeviceManager> inputManager(Input::CreateDeviceManager(inputParams));
    Input::C_Device *k=nullptr,*m=nullptr;inputManager->CreateDevice(0,Input::E_Device_Keyboard,&k);inputManager->CreateDevice(0,Input::E_Device_Mouse,&m);
    std::unique_ptr<Input::C_Device> keyboard(k),pointer(m);require(k&&m,"Cocoa input devices failed");auto input=Input::Mac::GetEvents(inputManager.get());
    Input::C_GameInputManager actions(4,2,0);auto* gameInput=actions.GetGameInput(0);
    gameInput->SetButtonInput(0,new Input::C_ButtonInputFromDevice(k,Input::C_Keyboard::E_Button_A));
    gameInput->SetButtonInput(1,new Input::C_ButtonInputFromDevice(m,Input::C_Mouse::E_Button_Left));
    Framework::Mac::Window window;Framework::Mac::WindowConfig config;config.width=160;config.height=90;config.title="Blur native window test";
    Framework::Mac::WindowCallbacks callbacks;U32 resizes=0,closes=0;std::vector<Framework::Mac::KeyEvent> keys;std::vector<Framework::Mac::MouseEvent> mouse;
    callbacks.resized=[&](U32 w,U32 h){require(w&&h&&window.IsCreated(),"Resize callback preceded host ownership");++resizes;};
    callbacks.close=[&]{++closes;};callbacks.key=[&](const auto& e){if(e.text=="throw")throw std::runtime_error("callback exception");input.key(e.code,e.down,e.modifiers,e.modifiersOnly);keys.push_back(e);};
    callbacks.mouse=[&](const auto& e){mouse.push_back(e);input.pointer(e.x,e.y);if(e.kind==Framework::Mac::MouseEvent::Down||e.kind==Framework::Mac::MouseEvent::Up)input.button(e.button,e.kind==Framework::Mac::MouseEvent::Down);if(e.kind==Framework::Mac::MouseEvent::Scroll)input.scroll(e.scrollY);};
    callbacks.focused=input.focus;
    require(window.Create(config,callbacks)&&!window.Create(config),"Cocoa host create/ownership failed");window.PollEvents();
    auto native=(__bridge NSWindow*)window.NativeWindow();auto view=native.contentView;
    const auto deadline=[NSDate dateWithTimeIntervalSinceNow:2];
    while(!native.isMiniaturized&&[deadline timeIntervalSinceNow]>0){window.PollEvents();[[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];}
    if(!native.isMiniaturized||native.isKeyWindow||window.PixelWidth()<160||window.PixelHeight()<90||!resizes)
        std::cerr<<"Host state: minimized="<<bool(native.isMiniaturized)<<" key="<<bool(native.isKeyWindow)<<" pixels="<<window.PixelWidth()<<'x'<<window.PixelHeight()<<" resizes="<<resizes<<'\n';
    require(native.isMiniaturized&&!native.isKeyWindow&&window.PixelWidth()>=160&&window.PixelHeight()>=90&&resizes,"Automated host was not minimized or had invalid Retina dimensions");
    bool foreign=false;std::thread worker([&]{Framework::Mac::Window other;foreign=!other.Create(config);});worker.join();require(foreign,"Foreign thread created an AppKit window");
    Display2::C_DeviceManager manager;Display2::C_DeviceManagerParams managerParams;
    require(Display2::CreateDeviceManager(&manager,managerParams)&&!manager.GetDeviceName().empty(),"Shared engine manager did not acquire a real GPU");
    Display2::C_DeviceParams params;Display2::SetupDefaultDeviceParams(&manager,params);params.m_PresentationLayer=window.MetalLayer();params.m_Width=window.PixelWidth();params.m_Height=window.PixelHeight();params.m_VSync=false;
    Display2::C_Device failed;auto invalid=params;invalid.m_Width=0;
    require(!Display2::CreateDevice(&manager,&failed,invalid)&&!failed.IsCreated(),"Failed native presentation setup retained a partially created device");
    Display2::C_Device device;require(Display2::CreateDevice(&manager,&device,params)&&device.IsCreated()==2,"Shared engine factory failed to attach Cocoa host");
    Display2::Device(&device);require(Display2::DeviceExists()&&Display2::Device()==&device,"Shared engine device registration failed");
    Display2::C_Surface surface;require(device.CreateSurface(window.PixelWidth(),window.PixelHeight(),Display2::E_Flags_Colour,Display2::E_SurfaceFormat_A8R8G8B8,Display2::E_Multisample_None,nullptr,&surface),"Cocoa host backbuffer allocation failed");
    device.SetColourSurface(0,&surface);device.SetViewport(Display2::C_Viewport(0,0,surface.GetWidth(),surface.GetHeight()));
    for(U32 frame=0;frame<6;++frame){device.BeginScene();device.Clear(Display2::E_Clear_Colour,0xff123456+frame,1,0);device.EndScene();
        require(device.PresentSurface(&surface)&&device.WaitIdle(),"Minimized Cocoa host stopped engine presentation");window.PollEvents();}
    auto event=[NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:NSEventModifierFlagShift timestamp:0 windowNumber:native.windowNumber context:nil characters:@"a" charactersIgnoringModifiers:@"a" isARepeat:NO keyCode:0];
    input.focus(true);[view keyDown:event];inputManager->Update();actions.Update();require(gameInput->GetButtonPressedDebounced(0),"Actual Cocoa key did not reach original game actions");
    auto release=[NSEvent keyEventWithType:NSEventTypeKeyUp location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:native.windowNumber context:nil characters:@"a" charactersIgnoringModifiers:@"a" isARepeat:NO keyCode:0];[view keyUp:release];inputManager->Update();actions.Update();require(gameInput->GetButtonReleasedDebounced(0),"Actual Cocoa key release did not reach game actions");
    require(keys.size()==2&&keys[0].down&&!keys[1].down&&keys[0].text=="a"&&keys[0].modifiers==NSEventModifierFlagShift,"Cocoa keyboard events lost their fields");
    auto flags=[&](U16 code,NSEventModifierFlags mask){
        auto change=[NSEvent keyEventWithType:NSEventTypeFlagsChanged location:NSZeroPoint modifierFlags:mask timestamp:0 windowNumber:native.windowNumber context:nil characters:@"" charactersIgnoringModifiers:@"" isARepeat:NO keyCode:code];
        [view flagsChanged:change];inputManager->Update();actions.Update();
        require(keys.back().modifiersOnly&&keys.back().code==code&&keys.back().modifiers==mask,"Actual Cocoa flagsChanged lost physical key identity or modifiers");
    };
    flags(59,NSEventModifierFlagControl);
    require(k->GetButtonValue(Input::C_Keyboard::E_Button_LControl)==1&&k->GetButtonValue(Input::C_Keyboard::E_Button_RControl)==0,"Cocoa left Control was not independent");
    flags(62,NSEventModifierFlagControl);flags(59,NSEventModifierFlagControl);
    require(k->GetButtonValue(Input::C_Keyboard::E_Button_LControl)==0&&k->GetButtonValue(Input::C_Keyboard::E_Button_RControl)==1,"Cocoa Control sided release lost held right key");
    flags(62,0);
    require(k->GetButtonValue(Input::C_Keyboard::E_Button_RControl)==0,"Cocoa Control release stuck");
    flags(56,NSEventModifierFlagShift);
    require(k->GetButtonValue(Input::C_Keyboard::E_Button_LShift)==1&&k->GetButtonValue(Input::C_Keyboard::E_Button_RShift)==0,"Cocoa left Shift was not independent");
    flags(60,NSEventModifierFlagShift);flags(56,NSEventModifierFlagShift);
    require(k->GetButtonValue(Input::C_Keyboard::E_Button_LShift)==0&&k->GetButtonValue(Input::C_Keyboard::E_Button_RShift)==1,"Cocoa Shift sided release lost held right key");flags(60,0);
    auto physical=[&](U16 code,NSString* text,bool down){
        auto keyEvent=[NSEvent keyEventWithType:down?NSEventTypeKeyDown:NSEventTypeKeyUp location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:native.windowNumber context:nil characters:text charactersIgnoringModifiers:text isARepeat:NO keyCode:code];
        if(down)[view keyDown:keyEvent];else[view keyUp:keyEvent];inputManager->Update();actions.Update();
    };
    physical(114,@"",true);
    require(k->GetButtonValue(Input::C_Keyboard::E_Button_Insert)==1&&k->GetButtonValue(Input::C_Keyboard::E_Button_Delete)==0,"Actual Cocoa Help/Insert keyDown lost native mapping");
    physical(114,@"",false);require(k->GetButtonValue(Input::C_Keyboard::E_Button_Insert)==0,"Actual Cocoa Help/Insert keyUp stuck");
    physical(33,@"[",true);physical(30,@"]",true);
    require(k->GetButtonValue(Input::C_Keyboard::E_Button_LBracket)==1&&k->GetButtonValue(Input::C_Keyboard::E_Button_RBracket)==1&&k->GetButtonValue(Input::C_Keyboard::E_Button_Insert)==0&&k->GetButtonValue(Input::C_Keyboard::E_Button_Delete)==0,"Actual Cocoa brackets were globally aliased to shoulder keys");
    physical(33,@"[",false);physical(30,@"]",false);
    auto click=[NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:NSMakePoint(10,12) modifierFlags:0 timestamp:0 windowNumber:native.windowNumber context:nil eventNumber:1 clickCount:1 pressure:1];[view mouseDown:click];
    require(mouse.size()==1&&mouse[0].kind==Framework::Mac::MouseEvent::Down&&mouse[0].button==0&&mouse[0].x>=0&&mouse[0].y>=0,"Cocoa mouse event routing failed");
    inputManager->Update();actions.Update();require(gameInput->GetButtonPressedDebounced(1),"Actual Cocoa click did not reach original game actions");
    auto failure=[NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:native.windowNumber context:nil characters:@"throw" charactersIgnoringModifiers:@"throw" isARepeat:NO keyCode:0];[view keyDown:failure];
    bool propagated=false;try{window.PollEvents();}catch(const std::runtime_error& e){propagated=std::string(e.what())=="callback exception";}require(propagated,"Callback exception escaped through AppKit or disappeared");
    const U32 oldWidth=window.PixelWidth();[native setContentSize:NSMakeSize(192,108)];window.PollEvents();require(window.PixelWidth()!=oldWidth&&resizes>=2,"Cocoa resize did not update physical dimensions");
    require(device.SetPresentationLayer(window.MetalLayer(),window.PixelWidth(),window.PixelHeight())&&!device.PresentSurface(&surface),"Cocoa resize accepted stale backbuffer dimensions");
    [native performClose:nil];window.PollEvents();require(window.CloseRequested()&&closes==1&&window.IsCreated(),"Window close did not defer shutdown to the engine");
    require(device.SetPresentationLayer(nullptr,0,0),"Host layer detachment failed");device.Destroy(&surface);window.Destroy();require(!window.IsCreated(),"Cocoa host did not release its window");
    Display2::DestroyDevice(&manager,&device);require(!device.IsCreated()&&!Display2::DeviceExists(),"Shared engine device shutdown retained a global/volatile device");
    Display2::DestroyDeviceManager(&manager);require(!manager.IsCreated(),"Native manager shutdown retained the adapter");
    Framework::Mac::Window reentrant;Framework::Mac::WindowCallbacks shutdown;
    shutdown.key=[&](const auto&){reentrant.Destroy();};require(reentrant.Create(config,shutdown),"Reentrant host creation failed");
    auto closeView=((__bridge NSWindow*)reentrant.NativeWindow()).contentView;[closeView keyDown:event];
    require(!reentrant.IsCreated(),"Input callback could not release its own host safely");
    std::cout<<"Actual minimized Cocoa host: Retina dimensions, six engine presents, keyboard/mouse callbacks, callback failures, resize and close PASS (platform host, not game startup)\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}}
