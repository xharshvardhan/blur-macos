// [Blur2PC][macOS] Stable local slots, coherent GameController snapshots and real handle haptics.
#include <system/Debug/Debug.h>
#include "NativeControllers.h"
#include <cmath>
#include <atomic>
#include <stdexcept>
#import <GameController/GameController.h>
#import <CoreHaptics/CoreHaptics.h>
namespace Input::Mac {
struct Controllers::Impl {
    struct Slot {
        GCController* controller=nil;PadSample sample;
        CHHapticEngine* engines[2]={nil,nil};id<CHHapticPatternPlayer> players[2]={nil,nil};F32 speeds[2]={0,0};
        std::shared_ptr<std::atomic<bool>> invalidated[2];
        void Stop(){for(U32 i=0;i<2;++i){if(players[i]){players[i].isMuted=YES;[players[i] stopAtTime:0 error:nullptr];}players[i]=nil;if(engines[i])[engines[i] stopWithCompletionHandler:nil];engines[i]=nil;speeds[i]=0;invalidated[i].reset();}}
    } slots[4];
    ~Impl(){for(auto& slot:slots)slot.Stop();}
};
Controllers::Controllers():m_Impl(std::make_unique<Impl>()){}
Controllers::~Controllers()=default;
void Controllers::Poll(){@autoreleasepool{PollList((__bridge void*)[GCController controllers]);}}
static PadSample Read(GCController* controller){
    PadSample sample;GCExtendedGamepad* p=[controller capture].extendedGamepad;if(!p)return sample;sample.connected=true;
#define BUTTON(Game,Native) sample.buttons[C_Pad360::E_Button_##Game]=p.Native.isPressed?1:0;
    BUTTON(A,buttonA) BUTTON(B,buttonB) BUTTON(X,buttonX) BUTTON(Y,buttonY)
    BUTTON(DPadUp,dpad.up) BUTTON(DPadDown,dpad.down) BUTTON(DPadLeft,dpad.left) BUTTON(DPadRight,dpad.right)
    BUTTON(Start,buttonMenu) BUTTON(Back,buttonOptions) BUTTON(LeftShoulder,leftShoulder) BUTTON(RightShoulder,rightShoulder)
    BUTTON(LeftThumb,leftThumbstickButton) BUTTON(RightThumb,rightThumbstickButton)
#undef BUTTON
    sample.buttons[C_Pad360::E_Button_LeftTrigger]=p.leftTrigger.value;sample.buttons[C_Pad360::E_Button_RightTrigger]=p.rightTrigger.value;
    sample.axes={p.leftThumbstick.xAxis.value,p.leftThumbstick.yAxis.value,p.rightThumbstick.xAxis.value,p.rightThumbstick.yAxis.value};return sample;
}
void Controllers::PollList(void* array){@autoreleasepool{
    NSArray<GCController*>* available=(__bridge NSArray<GCController*>*)array;
    if(available&&![available isKindOfClass:NSArray.class])throw std::invalid_argument("Native controller list is not an NSArray");
    for(auto& slot:m_Impl->slots)if(slot.controller&&![available containsObject:slot.controller]){slot.Stop();slot.controller=nil;slot.sample={};}
    for(GCController* controller in available){if(![controller isKindOfClass:GCController.class]||!controller.extendedGamepad)continue;
        bool assigned=false;for(const auto& slot:m_Impl->slots)if(slot.controller==controller)assigned=true;
        if(!assigned)for(auto& slot:m_Impl->slots)if(!slot.controller){slot.controller=controller;break;}
    }
    for(auto& slot:m_Impl->slots)slot.sample=slot.controller?Read(slot.controller):PadSample{};
}}
const PadSample& Controllers::Get(U32 player)const{if(player>=4)throw std::out_of_range("Native controller player slot");return m_Impl->slots[player].sample;}
bool Controllers::Rumble(U32 player,F32 low,F32 high){@autoreleasepool{
    if(player>=4||!std::isfinite(low)||!std::isfinite(high)||low<0||low>1||high<0||high>1)return false;
    auto& slot=m_Impl->slots[player];if(low==0&&high==0){slot.Stop();return true;}
    for(U32 i=0;i<2;++i)if(slot.invalidated[i]&&slot.invalidated[i]->load()){slot.Stop();break;}
    GCDeviceHaptics* haptics=slot.controller.haptics;
    if(!haptics||![haptics.supportedLocalities containsObject:GCHapticsLocalityLeftHandle]||![haptics.supportedLocalities containsObject:GCHapticsLocalityRightHandle]){slot.Stop();return false;}
    const F32 speeds[2]={low,high};GCHapticsLocality localities[2]={GCHapticsLocalityLeftHandle,GCHapticsLocalityRightHandle};
    for(U32 i=0;i<2;++i){if(speeds[i]==slot.speeds[i]&&slot.players[i])continue;NSError* error=nil;
        if(!slot.engines[i]){slot.engines[i]=[haptics createEngineWithLocality:localities[i]];
            if(!slot.engines[i]){slot.Stop();return false;}
            auto invalidated=std::make_shared<std::atomic<bool>>(false);slot.invalidated[i]=invalidated;
            slot.engines[i].resetHandler=^{invalidated->store(true);};
            slot.engines[i].stoppedHandler=^(CHHapticEngineStoppedReason){invalidated->store(true);};
            if(![slot.engines[i] startAndReturnError:&error]){slot.Stop();return false;}}
        if(!slot.players[i]){
            CHHapticEventParameter* intensity=[[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticIntensity value:1];
            CHHapticEventParameter* sharpness=[[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticSharpness value:i?1:0];
            CHHapticEvent* event=[[CHHapticEvent alloc] initWithEventType:CHHapticEventTypeHapticContinuous parameters:@[intensity,sharpness] relativeTime:0 duration:GCHapticDurationInfinite];
            CHHapticPattern* pattern=[[CHHapticPattern alloc] initWithEvents:@[event] parameters:@[] error:&error];
            slot.players[i]=pattern?[slot.engines[i] createPlayerWithPattern:pattern error:&error]:nil;
            if(!slot.players[i]){slot.Stop();return false;}
            slot.players[i].isMuted=YES;
            CHHapticDynamicParameter* volume=[[CHHapticDynamicParameter alloc] initWithParameterID:CHHapticDynamicParameterIDHapticIntensityControl value:speeds[i] relativeTime:0];
            if(![slot.players[i] startAtTime:0 error:&error]||![slot.players[i] sendParameters:@[volume] atTime:0 error:&error]){slot.Stop();return false;}
            slot.players[i].isMuted=NO;
        }else{
            CHHapticDynamicParameter* volume=[[CHHapticDynamicParameter alloc] initWithParameterID:CHHapticDynamicParameterIDHapticIntensityControl value:speeds[i] relativeTime:0];
            if(![slot.players[i] sendParameters:@[volume] atTime:0 error:&error]){slot.Stop();return false;}
        }
        slot.speeds[i]=speeds[i];
    }
    return true;
}}
}
