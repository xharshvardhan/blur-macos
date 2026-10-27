// [Blur2PC][macOS] The original PC binding routine selected by native Options.
// Exercises actual native keyboard events and original action mappings, not driving physics.
#include <core/Core.h>
#include <core/Maths.h>
#include <core/containers.h>
#include <core/threads.h>
#include <input/macos/NativeInput.h>
#include <input/Keyboard.h>
#include <input/Device.h>
#include <input/DeviceManager.h>
#include <input/GameInput.h>
#include <input/GameInputMapping.h>
#include <display.h>
#include <Ui/UiXtConfig.h>
#include <Game/GameControls/GameControls.h>
// Cocoa virtual key codes from the public macOS HIToolbox/Events.h.
// Avoid Carbon umbrella imports: its global Component typedef conflicts with game namespaces.
enum : U16 { kVK_ANSI_A=0x00,kVK_ANSI_S=0x01,kVK_ANSI_D=0x02,kVK_ANSI_Q=0x0c,kVK_ANSI_W=0x0d,kVK_LeftArrow=0x7b,kVK_RightArrow=0x7c,kVK_DownArrow=0x7d,kVK_UpArrow=0x7e };
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
    Input::C_DeviceManagerCreateParam params;std::unique_ptr<Input::C_DeviceManager> manager(Input::CreateDeviceManager(params));
    Input::C_Device *keyRaw=nullptr,*mouseRaw=nullptr,*padRaw=nullptr;
    manager->CreateDevice(0,Input::E_Device_Keyboard,&keyRaw);std::unique_ptr<Input::C_Device> keyboard(keyRaw);
    manager->CreateDevice(0,Input::E_Device_Mouse,&mouseRaw);std::unique_ptr<Input::C_Device> mouse(mouseRaw);
    manager->CreateDevice(0,Input::E_Device_Pad360,&padRaw);std::unique_ptr<Input::C_Device> pad(padRaw);
    require(keyboard&&mouse&&pad,"Original native physical interfaces unavailable");
    Input::C_GameInputManager controls(4,Game::E_UiButton_Count,Game::E_UiAxis_Count);auto* input=controls.GetGameInput(0);
    UI::C_ControlConfig config = UI::C_ControlConfig();config.m_GameFunctionRT=Game::E_Button_Throttle;config.m_GameFunctionLT=Game::E_Button_Brake;
    auto events=Input::Mac::GetEvents(manager.get());
    auto sample=[&]{manager->Update();controls.Update();};
    auto reset=[&]{for(U16 code:{U16(kVK_ANSI_Q),U16(kVK_ANSI_W),U16(kVK_ANSI_A),U16(kVK_ANSI_S),U16(kVK_ANSI_D),U16(kVK_LeftArrow),U16(kVK_RightArrow),U16(kVK_UpArrow),U16(kVK_DownArrow)})events.key(code,false,0,false);sample();};
    auto key=[&](U16 code,bool down){events.key(code,down,0,false);sample();};
    for(S32 layout:{0,1}){
        reset();Game::SetupAmaxUserControlsPC(input,pad.get(),keyboard.get(),mouse.get(),layout,false,&config,0);sample();
        const U16 throttle=layout?kVK_ANSI_W:kVK_ANSI_Q,brake=layout?kVK_ANSI_S:kVK_ANSI_A;
        const U16 left=layout?kVK_ANSI_A:kVK_LeftArrow,right=layout?kVK_ANSI_D:kVK_RightArrow;
        key(throttle,true);require(input->GetButtonPressed(Game::E_Button_Throttle),"Original keyboard throttle missing before remap");
        // Demonstrate the exact console operation that native Options used before MAC074.
        config.RemapGameControls(input,pad.get());sample();
        require(!input->GetButtonPressed(Game::E_Button_Throttle),"Console pad remap unexpectedly retained keyboard throttle");
        // UI::SetupPlayerControls delegates here with profile keyboard/pad configuration.
        Game::SetupAmaxUserControlsPC(input,pad.get(),keyboard.get(),mouse.get(),layout,false,&config,0);sample();
        require(input->GetButtonPressed(Game::E_Button_Throttle)&&input->GetButtonDevice(Game::E_Button_Throttle)==Input::E_Device_Keyboard,"PC profile binding refresh did not restore keyboard throttle");
        key(throttle,false);key(brake,true);require(input->GetButtonPressed(Game::E_Button_Brake)&&!input->GetButtonPressed(Game::E_Button_Throttle),"Profile layout brake/throttle crossed actions");
        key(brake,false);key(left,true);require(std::abs(input->GetAxisSignedValue(Game::E_Axis_Steering)+1.f)<1e-6f,"Profile layout left steering missing");
        require(std::abs(input->GetAxisSignedValue(Game::E_Axis_DigitalSteering))<1e-6f,"Keyboard steering unexpectedly duplicated the pad digital axis");
        key(left,false);key(right,true);require(std::abs(input->GetAxisSignedValue(Game::E_Axis_Steering)-1.f)<1e-6f,"Profile layout right steering missing");
        key(right,false);require(std::abs(input->GetAxisSignedValue(Game::E_Axis_Steering))<1e-6f,"Profile layout steering release missing");
        const U16 forward=layout?kVK_UpArrow:kVK_ANSI_W,backward=layout?kVK_DownArrow:kVK_ANSI_S;
        key(forward,true);
        require(input->GetButtonPressedDebounced(Game::E_Button_UsePickupForward)&&!input->GetButtonPressed(Game::E_Button_UsePickupBackward),"Original PC forward pickup-fire binding missing or crossed");
        key(forward,false);require(input->GetButtonReleasedDebounced(Game::E_Button_UsePickupForward),"Forward pickup-fire release lost");
        key(backward,true);
        require(input->GetButtonPressedDebounced(Game::E_Button_UsePickupBackward)&&!input->GetButtonPressed(Game::E_Button_UsePickupForward),"Original PC backward pickup-fire binding missing or crossed");
        key(backward,false);require(input->GetButtonReleasedDebounced(Game::E_Button_UsePickupBackward),"Backward pickup-fire release lost");
        if(layout){key(kVK_ANSI_Q,true);require(!input->GetButtonPressed(Game::E_Button_Throttle),"Old layout0 throttle leaked into layout1");key(kVK_ANSI_Q,false);}
    }
    std::cout<<"Native profile/options binding selection: original layout0 Q/A/arrows and layout1 WASD, dedicated pickup-fire keys, pad-remap regression and keyboard ownership PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
