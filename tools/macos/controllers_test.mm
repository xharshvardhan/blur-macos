// [Blur2PC][macOS] Real Apple snapshot objects, native slot assignment and original pad/action interfaces.
#include <system/Debug/Debug.h>
#include <input/macos/NativeInput.h>
#include <input/GameInput.h>
#include <input/GameInputMapping.h>
#include "NativeControllers.h"
#import <GameController/GameController.h>
#include <iostream>
#include <memory>
#include <vector>
#include <cmath>
#include <stdexcept>
using namespace Input;
static void require(bool v,const char* message){if(!v)throw std::runtime_error(message);}
int main(){@autoreleasepool{try{
    auto a=[GCController controllerWithExtendedGamepad],b=[GCController controllerWithExtendedGamepad],c=[GCController controllerWithExtendedGamepad],d=[GCController controllerWithExtendedGamepad],extra=[GCController controllerWithExtendedGamepad];
    require(a.isSnapshot&&a.extendedGamepad,"Apple extended controller snapshot unavailable");
    [a.extendedGamepad.buttonA setValue:1];[a.extendedGamepad.rightTrigger setValue:.75];[a.extendedGamepad.leftThumbstick setValueForXAxis:.7 yAxis:0];
    [b.extendedGamepad.buttonB setValue:1];[c.extendedGamepad.buttonX setValue:1];[d.extendedGamepad.buttonY setValue:1];[extra.extendedGamepad.buttonMenu setValue:1];
    Mac::Controllers slots;auto list=@[a,b,c,d,extra];slots.PollList((__bridge void*)list);
    require(slots.Get(0).buttons[C_Pad360::E_Button_A]==1&&slots.Get(1).buttons[C_Pad360::E_Button_B]==1&&slots.Get(2).buttons[C_Pad360::E_Button_X]==1&&slots.Get(3).buttons[C_Pad360::E_Button_Y]==1,"Native gamepad controls/player slots changed");
    require(std::abs(slots.Get(0).buttons[C_Pad360::E_Button_RightTrigger]-.75f)<1e-6f&&std::abs(slots.Get(0).axes[0]-.7f)<1e-6f,"Native trigger/stick snapshot mapping changed");
    slots.PollList((__bridge void*)@[d,c,b,a,extra]);require(slots.Get(0).buttons[0]==1&&slots.Get(3).buttons[C_Pad360::E_Button_Y]==1,"Controller enumeration reorder changed existing local players");
    slots.PollList((__bridge void*)@[a,c,d,extra]);require(slots.Get(1).buttons[C_Pad360::E_Button_Start]==1&&slots.Get(2).buttons[C_Pad360::E_Button_X]==1,"Disconnect shifted surviving controller slots");
    require(!slots.Rumble(0,1,.5f)&&slots.Rumble(0,0,0)&&!slots.Rumble(4,0,0),"Snapshot without haptics claimed a real motor output");
    C_DeviceManagerCreateParam params;std::unique_ptr<C_DeviceManager> manager(CreateDeviceManager(params));C_Device* raw=nullptr;manager->CreateDevice(0,E_Device_Pad360,&raw);std::unique_ptr<C_Device> owner(raw);auto* pad=dynamic_cast<C_Pad360*>(raw);require(pad,"Native factory did not return the original C_Pad360 interface");
    Mac::PollControllerList(manager.get(),(__bridge void*)@[a]);pad->Update();C_GameInputManager actions(4,1,1);auto* game=actions.GetGameInput(0);game->SetButtonInput(0,new C_ButtonInputFromDevice(pad,C_Pad360::E_Button_A));game->SetAxisInput(0,new C_AxisInputFromDevice(pad,C_Pad360::E_Axis_LeftStick_X));actions.Update();
    require(pad->IsConnected()&&game->GetButtonPressedDebounced(0)&&game->GetButtonDevice(0)==E_Device_Pad360&&std::abs(game->GetAxisSignedValue(0)-.6f)<1e-5f,"Apple snapshot did not reach original pad/deadzone/game-action mapping");
    [a.extendedGamepad.buttonA setValue:0];Mac::PollControllerList(manager.get(),(__bridge void*)@[a]);pad->Update();actions.Update();require(game->GetButtonReleasedDebounced(0),"Native pad release lost");
    std::vector<std::unique_ptr<C_Device>> otherPads;const S32 buttonIds[3]={C_Pad360::E_Button_B,C_Pad360::E_Button_X,C_Pad360::E_Button_Y};
    for(U32 i=1;i<4;++i){C_Device* next=nullptr;manager->CreateDevice(i,E_Device_Pad360,&next);require(next,"Native local pad slot creation failed");otherPads.emplace_back(next);actions.GetGameInput(i)->SetButtonInput(0,new C_ButtonInputFromDevice(next,buttonIds[i-1]));}
    Mac::PollControllerList(manager.get(),(__bridge void*)@[a,b,c,d]);pad->Update();for(auto& next:otherPads)static_cast<C_Pad360*>(next.get())->Update();actions.Update();
    for(U32 i=1;i<4;++i)require(actions.GetGameInput(i)->GetButtonPressedDebounced(0),"Native controller local player mapping crossed slots");
    auto events=Mac::GetEvents(manager.get());events.focus(false);manager->Update();Mac::PollControllerList(manager.get(),(__bridge void*)@[a,b,c,d]);for(auto& next:otherPads)static_cast<C_Pad360*>(next.get())->Update();actions.Update();
    for(U32 i=1;i<4;++i)require(!actions.GetGameInput(i)->GetButtonPressed(0),"Unfocused window accepted native controller input");
    Mac::PollControllerList(manager.get(),(__bridge void*)@[]);pad->Update();actions.Update();require(!pad->IsConnected()&&game->GetAxisSignedValue(0)==0,"Disconnected native pad retained input");
    slots.PollList((__bridge void*)@[]);for(U32 i=0;i<4;++i)require(!slots.Get(i).connected,"Empty controller list retained a player");
    std::cout<<"Apple controller snapshots/native four-slot stability/original pad/actions PASS (physical input/haptics unverified)\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}}
