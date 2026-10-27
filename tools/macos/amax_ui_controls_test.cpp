// [Blur2PC] Original UI controller configuration against authored physical pad states.
#include "Stdafx.h"
#include <Ui/UiXtConfig.h>
#include <input/Pad360.h>
#include <input/GameInputMapping.h>
#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
class AuthoredPad final:public Input::C_Device {
public:
    AuthoredPad(Input::C_DeviceManager* manager,S32 player):C_Device(manager,player,Input::C_Pad360::E_Button_Count,0,0){m_Connected=true;m_DeviceType=Input::E_Device_Pad360;}
    void Update() override{}
    void Press(S32 button){ResetValues();m_ButtonValues[button]=1.0f;}
    void Release(){ResetValues();}
};
int main(){try{
    Input::C_DeviceManagerCreateParam params;std::unique_ptr<Input::C_DeviceManager> devices(Input::CreateDeviceManager(params));
    require(devices&&devices->GetUserIndexCount()==4,"Original UI controls require four native player slots");
    Input::C_GameInputManager controls(4,Game::E_Button_Count,0);
    UI::C_ControlConfig config;
    const std::array<Game::E_Button,10> actions{{Game::E_Button_Throttle,Game::E_Button_Brake,Game::E_Button_Handbrake,Game::E_Button_GearUp,Game::E_Button_GearDown,Game::E_Button_ChangeView,Game::E_Button_ResetVehicle,Game::E_Button_UsePickup,Game::E_Button_DiscardPickup,Game::E_Button_SwitchPickup}};
    config.m_GameFunctionA=actions[0];config.m_GameFunctionB=actions[1];config.m_GameFunctionX=actions[2];config.m_GameFunctionY=actions[3];config.m_GameFunctionLT=actions[4];config.m_GameFunctionRT=actions[5];config.m_GameFunctionLB=actions[6];config.m_GameFunctionRB=actions[7];config.m_GameFunctionLThumb=actions[8];config.m_GameFunctionRThumb=actions[9];
    const std::array<S32,10> buttons{{Input::C_Pad360::E_Button_A,Input::C_Pad360::E_Button_B,Input::C_Pad360::E_Button_X,Input::C_Pad360::E_Button_Y,Input::C_Pad360::E_Button_LeftTrigger,Input::C_Pad360::E_Button_RightTrigger,Input::C_Pad360::E_Button_LeftShoulder,Input::C_Pad360::E_Button_RightShoulder,Input::C_Pad360::E_Button_LeftThumb,Input::C_Pad360::E_Button_RightThumb}};
    std::array<std::unique_ptr<AuthoredPad>,4> pads;
    for(S32 player=0;player<4;++player){
        pads[player]=std::make_unique<AuthoredPad>(devices.get(),player);auto* input=controls.GetGameInput(player);
        input->SetButtonInput(Game::E_Button_PhotoRoll,new Input::C_ButtonInputFromDevice(pads[player].get(),Input::C_Pad360::E_Button_Back));
        auto* photo=input->GetButtonInput(Game::E_Button_PhotoRoll);config.RemapGameControls(input,pads[player].get());
        require(photo==input->GetButtonInput(Game::E_Button_PhotoRoll),"UI remap replaced protected photo controls");
        require(input->GetButtonInput(Game::E_Button_UseShield)==nullptr,"Unassigned UI control retained an unrelated mapping");
    }
    for(S32 player=0;player<4;++player)for(std::size_t mapping=0;mapping<buttons.size();++mapping){
        pads[player]->Press(buttons[mapping]);controls.Update();
        for(S32 other=0;other<4;++other)for(std::size_t action=0;action<actions.size();++action)
            require(controls.GetGameInput(other)->GetButtonPressed(actions[action])==(other==player&&action==mapping),"Original UI pad mapping crossed actions or player slots");
        require(controls.GetGameInput(player)->GetButtonDevice(actions[mapping])==Input::E_Device_Pad360,"UI mapping discarded physical device source");
        pads[player]->Release();controls.Update();
    }
    // Mappings own references to inputs, so clear them before the authored devices are released.
    for(S32 player=0;player<4;++player)for(U32 action=0;action<Game::E_Button_Count;++action)controls.GetGameInput(player)->SetButtonInput(action,nullptr);
    std::cout<<"Original Amax UI ten-button/four-player/remap/photo controls PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
