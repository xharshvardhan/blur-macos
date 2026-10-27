// [Blur2PC][macOS] Synthesized automation keys traverse original device/action
// mappings; elapsed-time release remains independent of input/render poll count.
#include <system/Debug/Debug.h>
#include <input/macos/NativeInput.h>
#include <input/Keyboard.h>
#include <input/GameInput.h>
#include <input/GameInputMapping.h>
#include <chrono>
#include <iostream>
#include <memory>
#include <limits>
#include <stdexcept>
#include <thread>
using namespace Input;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
    require(Mac::KeyFromName("rcontrol")==C_Keyboard::E_Button_RControl && Mac::KeyFromName("ENTER")==C_Keyboard::E_Button_Return &&
            Mac::KeyFromName("ESC")==C_Keyboard::E_Button_Escape && Mac::KeyFromName("F12")==C_Keyboard::E_Button_F12 &&
            Mac::KeyFromName("not-a-key")==-1 && Mac::KeyFromName(nullptr)==-1,"automation key names changed");
    require(!Mac::QueueKeyTap(nullptr,C_Keyboard::E_Button_Return),"tap accepted missing input manager");
    C_DeviceManagerCreateParam params;std::unique_ptr<C_DeviceManager> manager(CreateDeviceManager(params));
    C_Device* raw=nullptr;manager->CreateDevice(0,E_Device_Keyboard,&raw);std::unique_ptr<C_Device> keyboard(raw);
    require(raw!=nullptr,"actual native keyboard unavailable");
    auto events=Mac::GetEvents(manager.get());auto controls=std::make_unique<C_GameInputManager>(4,4,0);auto* game=controls->GetGameInput(0);
    game->SetButtonInput(0,new C_ButtonInputFromDevice(raw,C_Keyboard::E_Button_RControl));
    game->SetButtonInput(1,new C_ButtonInputFromDevice(raw,C_Keyboard::E_Button_Return));
    game->SetButtonInput(2,new C_ButtonInputFromDevice(raw,C_Keyboard::E_Button_Q));
    game->SetButtonInput(3,new C_ButtonInputFromDevice(raw,C_Keyboard::E_Button_A));
    auto sample=[&]{manager->Update();controls->Update();};
    require(!Mac::QueueKeyTap(manager.get(),-1)&&!Mac::QueueKeyTap(manager.get(),C_Keyboard::E_Button_Count),"invalid enum tap accepted");
    events.focus(false); // Explicit automation must work with the game minimized.
    require(Mac::QueueKeyTap(manager.get(),Mac::KeyFromName("RCONTROL")),"background RCONTROL tap rejected");
    require(!Mac::QueueKeyTap(manager.get(),Mac::KeyFromName("RETURN")),"pending tap was overwritten");
    const auto start=std::chrono::steady_clock::now();
    sample();require(game->GetButtonPressedDebounced(0)&&game->GetButtonDevice(0)==E_Device_Keyboard,"tap bypassed original physical/action mapping");
    for(int i=0;i<100;++i)sample();
    if(std::chrono::steady_clock::now()-start<std::chrono::milliseconds(120))
        require(game->GetButtonPressed(0),"tap release advanced by poll count");
    std::this_thread::sleep_for(std::chrono::milliseconds(170));sample();
    require(!game->GetButtonPressed(0)&&game->GetButtonReleasedDebounced(0),"tap did not release after 150ms elapsed");
    // A queued tap must still get a visible press if input sampling is delayed.
    require(Mac::QueueKeyTap(manager.get(),Mac::KeyFromName("RETURN")),"RETURN tap rejected after release");
    std::this_thread::sleep_for(std::chrono::milliseconds(170));sample();require(game->GetButtonPressedDebounced(1),"hitch discarded queued key before first sample");
    std::this_thread::sleep_for(std::chrono::milliseconds(170));sample();require(game->GetButtonReleasedDebounced(1),"RETURN release lost");
    // Do not erase a real held key when the same synthesized tap expires.
    events.focus(true);events.key(62,true,0,false);sample();require(game->GetButtonPressedDebounced(0),"physical RCONTROL missing");
    require(Mac::QueueKeyTap(manager.get(),C_Keyboard::E_Button_RControl),"physical-key overlay tap rejected");sample();
    std::this_thread::sleep_for(std::chrono::milliseconds(170));sample();require(game->GetButtonPressed(0),"automation release erased real held key");
    events.key(62,false,0,false);sample();require(game->GetButtonReleasedDebounced(0),"physical release lost after automation");
    require(!Mac::QueueKeyHold(nullptr,C_Keyboard::E_Button_Q,1)&&!Mac::QueueKeyHold(manager.get(),-1,1)&&
            !Mac::QueueKeyHold(manager.get(),C_Keyboard::E_Button_Count,1),"hold accepted absent manager or invalid key");
    for(F64 invalid:{0.,-1.,60.0001,std::numeric_limits<F64>::infinity(),-std::numeric_limits<F64>::infinity(),std::numeric_limits<F64>::quiet_NaN()})
        require(!Mac::QueueKeyHold(manager.get(),C_Keyboard::E_Button_Q,invalid),"hold accepted an invalid elapsed duration");
    // Authored throttle/steer bindings use the original keyboard/action classes.
    events.focus(false);
    require(Mac::QueueKeyHold(manager.get(),C_Keyboard::E_Button_Q,.55),"background throttle hold rejected");
    require(!Mac::QueueKeyTap(manager.get(),C_Keyboard::E_Button_Q)&&!Mac::QueueKeyHold(manager.get(),C_Keyboard::E_Button_Q,.2),"same-key hold conflicts were accepted");
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    const auto heldStart=std::chrono::steady_clock::now();sample();
    require(game->GetButtonPressedDebounced(2)&&game->GetButtonDevice(2)==E_Device_Keyboard,"delayed hold lost its first physical/action sample");
    require(Mac::QueueKeyTap(manager.get(),C_Keyboard::E_Button_A),"steer tap could not coexist with throttle hold");
    require(!Mac::QueueKeyHold(manager.get(),C_Keyboard::E_Button_A,.2),"hold overwrote a same-key pending tap");
    sample();require(game->GetButtonPressed(2)&&game->GetButtonPressedDebounced(3),"independent hold/tap mappings did not coexist");
    for(int i=0;i<100;++i)sample();
    if(std::chrono::steady_clock::now()-heldStart<std::chrono::milliseconds(500))require(game->GetButtonPressed(2),"hold release advanced by input poll count");
    std::this_thread::sleep_for(std::chrono::milliseconds(170));sample();
    require(game->GetButtonReleasedDebounced(3),"steer tap lost its independent150ms release");
    if(std::chrono::steady_clock::now()-heldStart<std::chrono::milliseconds(500))require(game->GetButtonPressed(2),"steer release erased throttle hold");
    std::this_thread::sleep_until(heldStart+std::chrono::milliseconds(600));sample();
    require(game->GetButtonReleasedDebounced(2)&&!game->GetButtonPressed(3),"throttle did not release at its own elapsed duration");
    // Physical ownership acquired during an overlay must survive its expiration.
    events.focus(true);require(Mac::QueueKeyHold(manager.get(),C_Keyboard::E_Button_Q,.08),"hold rejected after previous expiration");sample();
    events.key(12,true,0,false);std::this_thread::sleep_for(std::chrono::milliseconds(100));sample();
    require(game->GetButtonPressed(2),"hold release erased a physical key pressed during automation");
    events.key(12,false,0,false);sample();require(game->GetButtonReleasedDebounced(2),"physical Q release lost after hold overlay");
    // Maximum-duration state belongs to this manager and must not leak to a new owner.
    require(Mac::QueueKeyHold(manager.get(),C_Keyboard::E_Button_Q,60)&&Mac::QueueKeyTap(manager.get(),C_Keyboard::E_Button_Return),"valid maximum-duration hold or independent tap rejected");sample();
    controls.reset();keyboard.reset();manager.reset();
    std::unique_ptr<C_DeviceManager> fresh(CreateDeviceManager(params));C_Device* freshRaw=nullptr;fresh->CreateDevice(0,E_Device_Keyboard,&freshRaw);std::unique_ptr<C_Device> freshKeyboard(freshRaw);
    events.focus(true);events.key(12,true,0,false); // Weak callbacks from the destroyed owner must expire.
    fresh->Update();require(freshRaw->GetButtonValue(C_Keyboard::E_Button_Q)==0&&freshRaw->GetButtonValue(C_Keyboard::E_Button_Return)==0,"destroyed owner's hold/tap/callback state leaked into a new manager");
    std::cout<<"Native Lua tap/hold pipeline: background keys, original mappings, independent throttle/steer timers, duration bounds, delayed first samples, physical ownership and teardown PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
