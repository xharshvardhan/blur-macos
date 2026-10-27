// [Blur2PC][macOS] Actual AIR-failure handlers and checked application error handoff; no game/progress is started.
#include "../../code/amax/game/GameLogic/stdafx.h"
#include <GameApp.h>
#include <Game/Game.h>
#include <GameLogic/Layers/AmaxWorldLayer.h>
#include <GameLogic/RaceLogicCore/Core/RaceLogicCore.h>
#include <Game/Messaging/GameFlowMessages.h>
#include <AI/AI_Messages.h>
#include <framework/layer/LayerManager.h>
#include <array>
#include <exception>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <string>
namespace {
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
class Application final:public C_GameApp {
public:
    void CheckMainHandoff(){m_LayerManager.Update();}
    void InsertPending(Framework::C_Layer* layer){m_LayerManager.InsertLayer(layer);}
    void Cancel(Framework::C_Layer* layer){m_LayerManager.NativeQuiesceCreation();m_LayerManager.NativeDestroyLayerNow(layer);}
};
struct Counts {unsigned creates=0,deletes=0;};
class Pending final:public Framework::C_Layer {
    Counts& counts;
public:
    explicit Pending(Counts& c):C_Layer("Blocked by required AIR failure"),counts(c){}
    ~Pending()override{++counts.deletes;}
    HRESULT ProcessMessage(Framework::E_LayerMessage message)override{if(message==Framework::E_LayerMessage_Create)++counts.creates;return S_OK;}
};
template<class T,class Callback> void InvokeWithoutPromotion(T& owner,Callback callback){
    std::array<unsigned char,sizeof(T)> before{},after{};
    std::memcpy(before.data(),&owner,sizeof(owner));
    Require(callback(),"Actual initialization-failure handler did not consume the failure");
    std::memcpy(after.data(),&owner,sizeof(owner));
    Require(before==after,"Failure callback changed the actual handler's readiness/ownership state");
}
void VerifyMainHandoff(Application& app,Counts& counts,Pending* pending){
    bool received=false;
    try{app.CheckMainHandoff();}
    catch(const std::runtime_error& error){received=std::string(error.what())=="Required native race AIR resources failed to initialise";}
    Require(received,"Actual required-AIR failure did not surface on checked main application Update");
    Require(!counts.creates&&pending->GetLayerState()==Framework::E_LayerState_PendingCreate,
            "Required AIR failure promoted pending work toward Ready");
    app.Cancel(pending);Require(counts.deletes==1,"Pending work was not actually retired exactly once");
}
}
int main(){try{
    // Layer callback/destructor ownership uses the actual upper Display lease, even without drawing.
    Display::C_Device device;Display::C_DeviceParams parameters;parameters.m_Width=32;parameters.m_Height=24;
    Require(device.Create(parameters)&&device.CreateVolatile(parameters),"Actual failure-handoff fixture device initialization failed");
    const auto failure=Messaging::BuildSimpleMessage(Game::E_AIMessage_LoadFailed);
    {
        Application app;app.GetGame()->GetLevelState().mInUI=false;
        Game::C_AmaxWorldLayer world(&app.GetGame()->GetSubscriberSystem());
        Counts counts;auto* pending=new Pending(counts);app.InsertPending(pending);
        // Actual callback executes on another thread; only the existing main handoff may throw.
        bool invoked=false;std::exception_ptr callbackFailure;
        std::thread callback([&]{
            try{InvokeWithoutPromotion(world,[&]{return world.MessageHandler(&failure);});invoked=true;}
            catch(...){callbackFailure=std::current_exception();}
        });
        callback.join();if(callbackFailure)std::rethrow_exception(callbackFailure);
        Require(invoked,"Actual world failure callback did not complete");
        app.NativeReportStartupFailure("A later callback must not replace the first");
        VerifyMainHandoff(app,counts,pending);
    }
    {
        Application app;app.GetGame()->GetLevelState().mInUI=false;
        // Explicit fixture precondition: original never-Initialise RLC raw owners need static zero fill.
        // This checks the real failure callback, not arbitrary-storage RLC constructor completeness.
        static Game::Actors::RaceLogicCore core;
        Counts counts;auto* pending=new Pending(counts);app.InsertPending(pending);
        InvokeWithoutPromotion(core,[&]{return core.OnMessage(&failure);});
        Require(core.GetTrackData()==nullptr,"Failure callback manufactured initialized race data");
        VerifyMainHandoff(app,counts,pending);
    }
    Require(device.GetDisplay2().WaitIdle(),"Actual failure-handoff fixture GPU completion failed");
    device.DestroyVolatile();device.Destroy();
    std::cout<<"Actual nonUI AIR LoadFailed handlers: world callback-thread and RLC failure do not promote owner state; first error reaches main Update before pending Create/Ready; real cancellation/application/static exit PASS (not an AIR import or gameplay fixture)\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
