// [Blur2PC][macOS] Native event state through original physical/game-input interfaces.
#include <system/Debug/Debug.h>
#include <input/macos/NativeInput.h>
#include <input/Keyboard.h>
#include <input/Mouse.h>
#include <input/GameInput.h>
#include <input/GameInputMapping.h>
#include <memory>
#include <thread>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace Input;
static void require(bool v,const char* message){if(!v)throw std::runtime_error(message);}
int main(){try{
    Mac::Events stale;
    {
        C_DeviceManagerCreateParam params;std::unique_ptr<C_DeviceManager> manager(CreateDeviceManager(params));
        require(manager&&manager->GetUserIndexCount()==4,"Native input manager/four-player slots failed");
        C_Device *k=nullptr,*m=nullptr;manager->CreateDevice(0,E_Device_Keyboard,&k);manager->CreateDevice(0,E_Device_Mouse,&m);
        std::unique_ptr<C_Device> keyboard(k),mouse(m);require(k&&m&&k->IsConnected()&&m->IsConnected(),"Native keyboard/mouse unavailable");
        auto events=Mac::GetEvents(manager.get());stale=events;
        C_GameInputManager controls(4,2,1);auto* game=controls.GetGameInput(0);
        game->SetButtonInput(0,new C_ButtonInputFromDevice(k,C_Keyboard::E_Button_A));
        game->SetButtonInput(1,new C_ButtonInputFromDevice(m,C_Mouse::E_Button_Left));
        auto sample=[&]{manager->Update();controls.Update();};
        events.key(0,true,0,false);sample();require(game->GetButtonPressedDebounced(0)&&game->GetButtonDevice(0)==E_Device_Keyboard,"Native key did not reach original mapping/debounce");
        const auto held=game->GetButtonPressedTime(0);const auto before=std::chrono::steady_clock::now();for(U32 i=0;i<300;++i)sample();
        const auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-before).count();require(game->GetButtonPressedTime(0)<=held+elapsed*60+3,"Held counter advanced by sample count");
        std::this_thread::sleep_for(std::chrono::milliseconds(85));sample();require(game->GetButtonPressedTime(0)>=held+4,"Held counter ignored elapsed monotonic time");
        events.key(0,false,0,false);sample();require(game->GetButtonReleasedDebounced(0)&&!game->GetButtonPressed(0),"Native key release lost");
        events.key(56,false,1ULL<<17,true);sample();require(k->GetButtonValue(C_Keyboard::E_Button_LShift)==1,"Left shift flagsChanged failed");
        events.key(60,false,1ULL<<17,true);sample();require(k->GetButtonValue(C_Keyboard::E_Button_RShift)==1,"Right shift flagsChanged failed");
        events.key(56,false,1ULL<<17,true);sample();require(k->GetButtonValue(C_Keyboard::E_Button_LShift)==0&&k->GetButtonValue(C_Keyboard::E_Button_RShift)==1,"Releasing one shift changed the other");
        events.key(60,false,0,true);sample();require(k->GetButtonValue(C_Keyboard::E_Button_RShift)==0,"Final shift release stuck");
        // Cocoa flagsChanged carries the changed physical key and the aggregate
        // modifier mask. Left/right identities must remain distinct; action
        // compatibility belongs to GameControls, never the physical sampler.
        constexpr U64 controlMask=1ULL<<18;
        events.key(59,false,controlMask,true);sample();
        require(k->GetButtonValue(C_Keyboard::E_Button_LControl)==1&&k->GetButtonValue(C_Keyboard::E_Button_RControl)==0,"Left Control was missing or globally aliased to right Control");
        events.key(62,false,controlMask,true);sample();
        require(k->GetButtonValue(C_Keyboard::E_Button_LControl)==1&&k->GetButtonValue(C_Keyboard::E_Button_RControl)==1,"Right Control flagsChanged failed");
        events.key(59,false,controlMask,true);sample();
        require(k->GetButtonValue(C_Keyboard::E_Button_LControl)==0&&k->GetButtonValue(C_Keyboard::E_Button_RControl)==1,"Left Control release changed held right Control");
        events.key(62,false,0,true);sample();
        require(k->GetButtonValue(C_Keyboard::E_Button_RControl)==0,"Final right Control release stuck");
        events.key(62,false,controlMask,true);sample();
        require(k->GetButtonValue(C_Keyboard::E_Button_LControl)==0&&k->GetButtonValue(C_Keyboard::E_Button_RControl)==1,"Right Control was globally aliased to left Control");
        events.key(59,false,controlMask,true);sample();events.key(62,false,controlMask,true);sample();
        require(k->GetButtonValue(C_Keyboard::E_Button_LControl)==1&&k->GetButtonValue(C_Keyboard::E_Button_RControl)==0,"Right Control release changed held left Control");
        events.focus(false);sample();
        require(k->GetButtonValue(C_Keyboard::E_Button_LControl)==0&&k->GetButtonValue(C_Keyboard::E_Button_RControl)==0,"Focus loss left Control held");events.focus(true);sample();
        // Extended Help/Insert, Forward Delete and the bracket keys stay
        // distinct physical inputs; only primary menu actions add alternatives.
        events.key(114,true,0,false);sample();
        require(k->GetButtonValue(C_Keyboard::E_Button_Insert)==1&&k->GetButtonValue(C_Keyboard::E_Button_Delete)==0&&k->GetButtonValue(C_Keyboard::E_Button_LBracket)==0,"Native Help did not map independently to Insert");
        events.key(114,false,0,false);events.key(117,true,0,false);sample();
        require(k->GetButtonValue(C_Keyboard::E_Button_Insert)==0&&k->GetButtonValue(C_Keyboard::E_Button_Delete)==1&&k->GetButtonValue(C_Keyboard::E_Button_RBracket)==0,"Native Forward Delete was confused with Insert or bracket");
        events.key(117,false,0,false);events.key(33,true,0,false);sample();
        require(k->GetButtonValue(C_Keyboard::E_Button_LBracket)==1&&k->GetButtonValue(C_Keyboard::E_Button_Insert)==0,"Left bracket was globally aliased to Insert");
        events.key(33,false,0,false);events.key(30,true,0,false);sample();
        require(k->GetButtonValue(C_Keyboard::E_Button_RBracket)==1&&k->GetButtonValue(C_Keyboard::E_Button_Delete)==0,"Right bracket was globally aliased to Forward Delete");events.key(30,false,0,false);sample();
        events.pointer(10,20);events.pointer(17,24);events.button(0,true);events.scroll(2);sample();
        require(game->GetButtonPressedDebounced(1)&&m->GetAxisSignedValue(C_Mouse::E_Axis_DeltaX)>0&&std::abs(m->GetAxisSignedValue(C_Mouse::E_Axis_DeltaX)/m->GetAxisSignedValue(C_Mouse::E_Axis_DeltaY)+1.75f)<0.0001f&&m->GetButtonValue(C_Mouse::E_Button_WheelUp)==1,"Native pointer/button/scroll sample failed");
        sample();require(m->GetAxisSignedValue(C_Mouse::E_Axis_DeltaX)==0&&m->GetButtonValue(C_Mouse::E_Button_WheelUp)==0&&game->GetButtonPressed(1),"Transient mouse state persisted or held button vanished");
        events.focus(false);sample();require(!game->GetButtonPressed(1),"Focus loss left held input");events.key(0,true,0,false);sample();require(!game->GetButtonPressed(0),"Unfocused window accepted key input");
        events.focus(true);events.key(0,true,0,false);sample();manager->DisableAllDevices();controls.Update();require(!game->GetButtonPressed(0),"Disabled native device supplied input");manager->EnableAllDevices();sample();require(game->GetButtonPressed(0),"Native device enable failed");
        require(controls.GetGameInput(-1)==nullptr&&controls.GetGameInput(4)==nullptr,"Invalid local player index accepted");
    }
    stale.key(0,true,0,false);stale.pointer(1,2);stale.button(0,true);stale.scroll(1);stale.focus(false);
    std::cout<<"Native input/original game mapping/elapsed hold/lifetime PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
