// [Blur2PC] Actual world interpolation implementation on authored matrices, without retail scenes.
#include <core/Core.h>
#include <core/Maths.h>
#include <core/threads.h>
#include <world_render/ModelInstance.h>
#include <renderingdata/DataTypes.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
static bool near(F32 a,F32 b){return std::abs(a-b)<.0001f;}
static Maths::HMat43 translated(F32 x){Maths::HMat43 result;result.AW(Maths::HVec3(x,2,4));return result;}
static void store(MathsStorage::MatrixRow44& destination,const Maths::HMat43& value){Maths::Store(Maths::HMat44(value),&destination);}
struct Hook {
    std::atomic<U32> begin{0},end{0};
    static void Call(void* context,bool start){auto& hook=*static_cast<Hook*>(context);if(start)++hook.begin;else ++hook.end;}
};
// [Blur2PC] The same high-speed wheel API now called by native VehicleActor::SynchData.
static Maths::HMat43 wheelPose(F32 angle,F32 x){
    Maths::HMat43 value;value.AX(Maths::HVec3(1,0,0));value.AY(Maths::HVec3(0,std::cos(angle),std::sin(angle)));value.AZ(Maths::HVec3(0,-std::sin(angle),std::cos(angle)));value.AW(Maths::HVec3(x,2,4));return value;
}
struct PoseHook {
    const MathsStorage::MatrixRow44* matrix;
    F32 beginPose=0,endPose=0,beginAlpha=0,endAlpha=0;
    static void Call(void* pointer,bool begin){auto& hook=*static_cast<PoseHook*>(pointer);if(begin){hook.beginPose=hook.matrix->_41;hook.beginAlpha=WorldRender::Blur2PCInterpRenderAlpha();}else{hook.endPose=hook.matrix->_41;hook.endAlpha=WorldRender::Blur2PCInterpRenderAlpha();}}
};
int main(){try{
    using namespace WorldRender;
    Blur2PCInterpEnable(true);Blur2PCInterpSetTickSeconds(1.f/60);
    MathsStorage::MatrixRow44 destination;const int owner=0;
    auto previous=translated(0),current=translated(8);store(destination,previous);Blur2PCInterpRecordStore(&owner,&destination,previous);
    Blur2PCInterpTick();store(destination,current);Blur2PCInterpRecordStore(&owner,&destination,current);
    const U32 tick=Blur2PCInterpCurrentTick();
    for(U32 i=0;i<=16;++i){const F32 alpha=F32(i)/16;Blur2PCInterpBeginRender(alpha);require(near(destination._41,alpha*8)&&near(destination._42,2)&&near(destination._43,4),"Original world render pose did not interpolate the stored PC matrix");Blur2PCInterpEndRender();require(near(destination._41,8)&&Blur2PCInterpCurrentTick()==tick&&Blur2PCInterpRenderAlpha()==1,"Rendering mutated the true world pose or simulation tick");}
    Blur2PCInterpTick();Blur2PCInterpTick();current=translated(12);store(destination,current);Blur2PCInterpRecordStore(&owner,&destination,current);Blur2PCInterpBeginRender(.25f);require(near(destination._41,12),"A stale world pose was interpolated across skipped ticks");Blur2PCInterpEndRender();
    Blur2PCInterpTick();current=translated(40);store(destination,current);Blur2PCInterpRecordStore(&owner,&destination,current);Blur2PCInterpBeginRender(.5f);require(near(destination._41,40),"Original teleport handling blended a discontinuous world pose");Blur2PCInterpEndRender();
    Blur2PCInterpTick();current=translated(44);store(destination,current);Blur2PCInterpRecordStore(&owner,&destination,current);Blur2PCInterpBeginRender(.5f);require(near(destination._41,42),"Consecutive world pose did not resume interpolation");auto override=translated(47);store(destination,override);Blur2PCInterpRecordStore(&owner,&destination,override);Blur2PCInterpEndRender();require(near(destination._41,47),"Render-specific world override was incorrectly restored");Blur2PCInterpForget(&owner);
    MathsStorage::MatrixRow43 palette{};const int skinOwner=1;
    Blur2PCInterpRecordTickStore43(&skinOwner,&palette,translated(0));Blur2PCInterpTick();Blur2PCInterpRecordTickStore43(&skinOwner,&palette,translated(6));Blur2PCInterpBeginRender(.5f);require(near(palette.v[3],3)&&near(palette.v[7],2)&&near(palette.v[11],4),"Original skin palette column packing/interpolation changed");Blur2PCInterpEndRender();require(near(palette.v[3],6),"Original skin palette true pose was not restored");Blur2PCInterpForget(&skinOwner);
    // Wheel turns beyond 180 degrees must keep rotor direction instead of taking the quaternion shortest arc.
    const F32 tickSeconds=1.f/60;Blur2PCInterpSetTickSeconds(tickSeconds);
    for(F32 degrees:{200.f,-240.f}){
        const F32 radians=degrees*3.14159265358979323846f/180.f;MathsStorage::MatrixRow44 wheelDestination;const int wheelOwner=2;
        auto start=wheelPose(0,0),finish=wheelPose(radians,8);store(wheelDestination,start);Blur2PCInterpRecordWheelStore(&wheelOwner,&wheelDestination,start,start,radians/tickSeconds);
        Blur2PCInterpTick();store(wheelDestination,finish);Blur2PCInterpRecordWheelStore(&wheelOwner,&wheelDestination,finish,finish,radians/tickSeconds);const U32 wheelTick=Blur2PCInterpCurrentTick();
        for(F32 alpha:{0.f,.25f,.5f,.75f,1.f}){
            Blur2PCInterpBeginRender(alpha);require(near(wheelDestination._22,std::cos(radians*alpha))&&near(wheelDestination._23,std::sin(radians*alpha))&&near(wheelDestination._41,8*alpha),"Original wheel interpolation reversed a >180-degree rotor turn or changed vehicle translation");
            Blur2PCInterpEndRender();require(near(wheelDestination._22,std::cos(radians))&&near(wheelDestination._23,std::sin(radians))&&near(wheelDestination._41,8)&&Blur2PCInterpCurrentTick()==wheelTick,"Unlocked wheel renders changed simulation time or failed to restore true wheel stores");
        }
        Blur2PCInterpForget(&wheelOwner);
    }
    MathsStorage::MatrixRow44 hookedMatrix;const int hookOwner=3;store(hookedMatrix,translated(0));Blur2PCInterpRecordStore(&hookOwner,&hookedMatrix,translated(0));Blur2PCInterpTick();store(hookedMatrix,translated(8));Blur2PCInterpRecordStore(&hookOwner,&hookedMatrix,translated(8));
    PoseHook poseHook{&hookedMatrix};Blur2PCInterpAddRenderHook(PoseHook::Call,&poseHook);Blur2PCInterpBeginRender(.25f);Blur2PCInterpEndRender();Blur2PCInterpRemoveRenderHook(PoseHook::Call,&poseHook);
    require(near(poseHook.beginPose,2)&&near(poseHook.endPose,2)&&near(poseHook.beginAlpha,.25f)&&near(poseHook.endAlpha,.25f)&&near(hookedMatrix._41,8)&&Blur2PCInterpRenderAlpha()==1,"Original render hooks did not observe the blended pose before true-pose restoration");Blur2PCInterpForget(&hookOwner);
    std::array<Hook,128> hooks;std::vector<std::thread> workers;
    for(U32 t=0;t<4;++t)workers.emplace_back([&,t]{for(U32 i=t;i<hooks.size();i+=4)Blur2PCInterpAddRenderHook(Hook::Call,&hooks[i]);});
    for(auto& worker:workers)worker.join();workers.clear();Blur2PCInterpBeginRender(.5f);Blur2PCInterpEndRender();
    for(auto& hook:hooks)require(hook.begin==1&&hook.end==1,"Concurrent original render-hook registration lost callbacks");
    for(U32 t=0;t<4;++t)workers.emplace_back([&,t]{for(U32 i=t;i<hooks.size();i+=4)Blur2PCInterpRemoveRenderHook(Hook::Call,&hooks[i]);});
    for(auto& worker:workers)worker.join();Blur2PCInterpBeginRender(.5f);Blur2PCInterpEndRender();for(auto& hook:hooks)require(hook.begin==1&&hook.end==1,"Retired world hooks were invoked again");
    RenderingData::DataTypes::SHHD_3N packed(1,-1,1);RenderingData::DataTypes::SDDD_3N packedPC(1,-1,1);static_assert(sizeof(packed)==4&&sizeof(packedPC)==4);U32 bits=0;std::memcpy(&bits,&packed,4);require(bits==(1023u|(1025u<<11)|(511u<<22)),"Native SHHD packed bit layout changed");std::memcpy(&bits,&packedPC,4);require(bits==(511u|(513u<<10)|(511u<<20)),"Native PC SDDD packed bit layout changed");
    Blur2PCInterpEnable(false);std::cout<<"Original world matrices/skin palettes/tick isolation/teleports/render overrides/full-turn wheels/hook pose ordering/concurrent hooks/packed vertex formats PASS (not a game scene)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
