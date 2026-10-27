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
enum : U16 { kVK_ANSI_A=0x00,kVK_ANSI_S=0x01,kVK_ANSI_D=0x02,kVK_ANSI_Q=0x0c,kVK_ANSI_W=0x0d,kVK_Space=0x31,kVK_Shift=0x38,kVK_Control=0x3b,kVK_RightShift=0x3c,kVK_RightControl=0x3e,kVK_Return=0x24,kVK_Escape=0x35,kVK_ANSI_KeypadPlus=0x45,kVK_ANSI_KeypadMinus=0x4e,kVK_ANSI_Z=0x06,kVK_ANSI_V=0x09,kVK_Tab=0x30,kVK_Help=0x72,kVK_ForwardDelete=0x75,kVK_ANSI_LeftBracket=0x21,kVK_ANSI_RightBracket=0x1e,kVK_PageDown=0x79,kVK_LeftArrow=0x7b,kVK_RightArrow=0x7c,kVK_DownArrow=0x7d,kVK_UpArrow=0x7e };
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
    auto reset=[&]{for(U16 code:{U16(kVK_ANSI_Q),U16(kVK_ANSI_W),U16(kVK_ANSI_A),U16(kVK_ANSI_S),U16(kVK_ANSI_D),U16(kVK_Space),U16(kVK_Control),U16(kVK_RightControl),U16(kVK_Shift),U16(kVK_RightShift),U16(kVK_Return),U16(kVK_Escape),U16(kVK_ANSI_KeypadPlus),U16(kVK_ANSI_KeypadMinus),U16(kVK_ANSI_Z),U16(kVK_ANSI_V),U16(kVK_Tab),U16(kVK_PageDown),U16(kVK_Help),U16(kVK_ForwardDelete),U16(kVK_ANSI_LeftBracket),U16(kVK_ANSI_RightBracket),U16(kVK_LeftArrow),U16(kVK_RightArrow),U16(kVK_UpArrow),U16(kVK_DownArrow)})events.key(code,false,0,false);sample();};
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
        const U16 change=layout?kVK_LeftArrow:kVK_UpArrow,discard=layout?kVK_RightArrow:kVK_ANSI_D;
        key(change,true);require(input->GetButtonPressedDebounced(Game::E_Button_SwitchPickup),"Original PC pickup-selection binding missing");
        require(input->GetButtonPressed(Game::E_UiButton_PadOrAxis_Up)==!layout&&input->GetButtonPressed(Game::E_UiButton_PadOrAxis_Left)==bool(layout),"Layout selection changed original arrow menu navigation");
        if(layout)require(std::abs(input->GetAxisSignedValue(Game::E_Axis_Steering))<1e-6f,"Layout1 pickup-selection arrow also steered the car");
        key(change,false);require(input->GetButtonReleasedDebounced(Game::E_Button_SwitchPickup),"Pickup-selection release lost");
        key(discard,true);require(input->GetButtonPressedDebounced(Game::E_Button_DiscardPickup),"Original PC discard binding missing");
        if(layout){require(input->GetButtonPressed(Game::E_UiButton_PadOrAxis_Right),"Layout1 discard arrow lost ordinary menu navigation");require(std::abs(input->GetAxisSignedValue(Game::E_Axis_Steering))<1e-6f,"Layout1 discard arrow also steered the car");}
        key(discard,false);require(input->GetButtonReleasedDebounced(Game::E_Button_DiscardPickup),"Pickup discard release lost");
        const U16 handbrake=layout?kVK_Space:kVK_DownArrow;
        key(handbrake,true);require(input->GetButtonPressed(Game::E_Button_Handbrake)&&!input->GetButtonPressed(Game::E_Button_Brake),"Original PC handbrake missing or crossed brake");key(handbrake,false);
        constexpr U64 controlMask=1ULL<<18,shiftMask=1ULL<<17;
        auto modifier=[&](U16 code,U64 flags){events.key(code,false,flags,true);sample();};
        modifier(kVK_Control,controlMask);
        require(input->GetButtonPressedDebounced(Game::E_Button_UsePickup)&&input->GetButtonPressedDebounced(Game::E_UiButton_Option2),"Left Control did not reach stock fire and menu Host/Y");
        require(keyboard->GetButtonValue(Input::C_Keyboard::E_Button_RControl)==0,"Action compatibility rewrote physical right Control");
        modifier(kVK_RightControl,controlMask);
        require(input->GetButtonPressed(Game::E_Button_UsePickup)&&!input->GetButtonPressedDebounced(Game::E_Button_UsePickup)&&!input->GetButtonPressedDebounced(Game::E_UiButton_Option2),"Second held Control retriggered fire or Host");
        modifier(kVK_Control,controlMask);
        require(input->GetButtonPressed(Game::E_UiButton_Option2)&&!input->GetButtonReleasedDebounced(Game::E_UiButton_Option2),"One Control release dropped the other held Host action");
        modifier(kVK_RightControl,0);
        require(input->GetButtonReleasedDebounced(Game::E_Button_UsePickup)&&input->GetButtonReleasedDebounced(Game::E_UiButton_Option2),"Final Control release lost fire/Host release");
        modifier(kVK_RightControl,controlMask);require(input->GetButtonPressedDebounced(Game::E_Button_UsePickup)&&input->GetButtonPressedDebounced(Game::E_UiButton_Option2),"Original right Control bindings regressed");modifier(kVK_RightControl,0);
        modifier(kVK_Shift,shiftMask);require(input->GetButtonPressedDebounced(Game::E_UiButton_Option1),"Left Shift did not reach Refresh/X");
        require(keyboard->GetButtonValue(Input::C_Keyboard::E_Button_RShift)==0,"Menu compatibility rewrote physical right Shift");
        modifier(kVK_RightShift,shiftMask);require(!input->GetButtonPressedDebounced(Game::E_UiButton_Option1),"Second held Shift retriggered Refresh");
        modifier(kVK_Shift,shiftMask);require(input->GetButtonPressed(Game::E_UiButton_Option1),"One Shift release dropped the other held Refresh action");
        modifier(kVK_RightShift,0);require(input->GetButtonReleasedDebounced(Game::E_UiButton_Option1),"Final Shift release lost Refresh release");
        key(kVK_ANSI_LeftBracket,true);require(input->GetButtonPressedDebounced(Game::E_UiButton_LeftShoulder)&&!input->GetButtonPressed(Game::E_UiButton_RightShoulder),"Primary left bracket did not reach only the left shoulder");
        key(kVK_Help,true);require(input->GetButtonPressed(Game::E_UiButton_LeftShoulder)&&!input->GetButtonPressedDebounced(Game::E_UiButton_LeftShoulder),"Original Insert/Help retriggered held left shoulder");
        key(kVK_ANSI_LeftBracket,false);require(input->GetButtonPressed(Game::E_UiButton_LeftShoulder),"Bracket release lost held Insert shoulder");key(kVK_Help,false);require(input->GetButtonReleasedDebounced(Game::E_UiButton_LeftShoulder),"Final Insert/Help release lost shoulder release");
        key(kVK_ANSI_RightBracket,true);require(input->GetButtonPressedDebounced(Game::E_UiButton_RightShoulder)&&!input->GetButtonPressed(Game::E_UiButton_LeftShoulder)&&!input->GetButtonPressed(Game::E_UiButton_RightThumb),"Primary right bracket crossed shoulder or globally aliased Forward Delete");
        key(kVK_ForwardDelete,true);require(input->GetButtonPressed(Game::E_UiButton_RightShoulder)&&!input->GetButtonPressedDebounced(Game::E_UiButton_RightShoulder)&&input->GetButtonPressed(Game::E_UiButton_RightThumb),"Original Forward Delete shoulder/thumb contract changed");
        key(kVK_ANSI_RightBracket,false);require(input->GetButtonPressed(Game::E_UiButton_RightShoulder),"Bracket release lost held Forward Delete shoulder");key(kVK_ForwardDelete,false);require(input->GetButtonReleasedDebounced(Game::E_UiButton_RightShoulder),"Final Forward Delete release lost shoulder release");
        key(kVK_Return,true);require(input->GetButtonPressedDebounced(Game::E_UiButton_Confirm),"Return lost original menu Select");key(kVK_Return,false);
        key(kVK_Escape,true);require(input->GetButtonPressedDebounced(Game::E_UiButton_Cancel)&&input->GetButtonPressedDebounced(Game::E_UiButton_Start),"Escape lost original menu Back/pause");key(kVK_Escape,false);
        const U16 camera=layout?kVK_ANSI_Z:kVK_ANSI_V,rear=layout?kVK_PageDown:kVK_Tab;
        key(camera,true);require(input->GetButtonPressedDebounced(Game::E_Button_ChangeView),"Profile camera binding changed");key(camera,false);
        key(rear,true);require(input->GetButtonPressedDebounced(Game::E_Button_RearView),"Profile look-back binding changed");key(rear,false);
        // Replacing a stock action with an explicit binding must remain exact.
        input->SetButtonInput(Game::E_Button_UsePickup,new Input::C_ButtonInputFromDevice(keyboard.get(),Input::C_Keyboard::E_Button_RControl));sample();
        modifier(kVK_Control,controlMask);require(!input->GetButtonPressed(Game::E_Button_UsePickup)&&input->GetButtonPressed(Game::E_UiButton_Option2),"Stock compatibility leaked into an explicit right-only driving binding");modifier(kVK_Control,0);
        modifier(kVK_RightControl,controlMask);require(input->GetButtonPressed(Game::E_Button_UsePickup),"Explicit right-only driving binding failed");modifier(kVK_RightControl,0);
        if(layout){key(kVK_ANSI_Q,true);require(!input->GetButtonPressed(Game::E_Button_Throttle),"Old layout0 throttle leaked into layout1");key(kVK_ANSI_Q,false);}
    }
    // Secondary split-screen menus retain their separate keypad actions.
    reset();Game::SetupAmaxUserControlsPC(input,pad.get(),keyboard.get(),mouse.get(),1,true,&config,1);sample();
    events.key(kVK_Control,false,1ULL<<18,true);events.key(kVK_Shift,false,1ULL<<17,true);sample();
    require(!input->GetButtonPressed(Game::E_UiButton_Option1)&&!input->GetButtonPressed(Game::E_UiButton_Option2),"Primary modifier alternatives leaked into secondary menu keys");
    require(!input->GetButtonPressed(Game::E_Button_Handbrake),"Left Control was globally aliased into split-screen right-Control handbrake");
    reset();key(kVK_ANSI_LeftBracket,true);key(kVK_ANSI_RightBracket,true);
    require(!input->GetButtonPressed(Game::E_UiButton_LeftShoulder)&&!input->GetButtonPressed(Game::E_UiButton_RightShoulder),"Primary bracket alternatives leaked into secondary menu keys");
    reset();key(kVK_Help,true);require(!input->GetButtonPressed(Game::E_UiButton_LeftShoulder),"New physical Insert changed secondary keypad shoulder mapping");key(kVK_Help,false);
    reset();key(kVK_ANSI_KeypadPlus,true);require(input->GetButtonPressed(Game::E_UiButton_Option2),"Secondary keypad Host/Y changed");key(kVK_ANSI_KeypadPlus,false);
    key(kVK_ANSI_KeypadMinus,true);require(input->GetButtonPressed(Game::E_UiButton_Option1),"Secondary keypad Refresh/X changed");key(kVK_ANSI_KeypadMinus,false);
    std::cout<<"Native profile/options binding selection: original layout0 Q/A/arrows and layout1 WASD, dedicated fire/switch/discard/handbrake keys, independent arrow menu navigation, pad-remap regression and keyboard ownership PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
