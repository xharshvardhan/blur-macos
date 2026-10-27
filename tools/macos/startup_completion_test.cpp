// [Blur2PC][macOS] Real registered-renderer and actor-creation success/error boundaries, not a race.
#include "../../code/amax/game/GameLogic/stdafx.h"
#include <GameApp.h>
#include <Game/ActorDef/GameActorList.h>
#include <Game/ActorDef/NativeActorCreationCompletion.h>
#include <actor_system/ActorFactory.h>
#include <actor_system/ActorCreation.h>
#include <actor_system/ActorCreationList.h>
#include <display/utils/Renderer.h>
#include <framework/layer/LayerManager.h>
#include <framework/layer/Layer.h>
#include <iostream>
#include <stdexcept>
#include <thread>
namespace AmaxWorldLayer_Impl {bool NativeCreateRequiredRenderers();}
namespace {
void Require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
class Application final:public C_GameApp {
public:
    void Check(){m_LayerManager.Update();}
    void Insert(Framework::C_Layer* layer){m_LayerManager.InsertLayer(layer);}
    void Cancel(Framework::C_Layer* layer){m_LayerManager.NativeQuiesceCreation();m_LayerManager.NativeDestroyLayerNow(layer);}
};
struct LayerCounts{unsigned creates=0,destroys=0;};
class Pending final:public Framework::C_Layer {
    LayerCounts& counts;
public:
    explicit Pending(LayerCounts& c):C_Layer("Required startup completion barrier"),counts(c){}
    ~Pending()override{++counts.destroys;}
    HRESULT ProcessMessage(Framework::E_LayerMessage message)override{if(message==Framework::E_LayerMessage_Create)++counts.creates;return S_OK;}
};
void Handoff(Application& app,const char* expected){
    LayerCounts counts;auto* layer=new Pending(counts);app.Insert(layer);
    bool failed=false;try{app.Check();}catch(const std::runtime_error& error){failed=std::string(error.what())==expected;}
    Require(failed&&!counts.creates&&layer->GetLayerState()==Framework::E_LayerState_PendingCreate,"Actual main handoff promoted pending work or lost the first startup failure");
    app.Cancel(layer);Require(counts.destroys==1,"Actual blocked layer was not cancelled exactly once");
}
class Renderer final:public Display::I_Renderer {
    Display::C_Device& device;bool volatileFailure,throws;
public:
    Display2::C_VertexBuffer base,extra;unsigned releases=0,volatileReleases=0;
    Renderer(Display::C_Device& d,bool v,bool t):I_Renderer("Required world renderer proof"),device(d),volatileFailure(v),throws(t){}
    ~Renderer()override{DestroyRenderer();}
private:
    bool Finish(){if(throws)throw std::runtime_error("Authored real renderer initialization exception");return false;}
    bool OnCreate()override{Require(device.GetDisplay2().CreateVertexBuffer(32,0,&base),"Actual base allocation failed");return volatileFailure?true:Finish();}
    void OnDestroy()override{device.GetDisplay2().Destroy(&base);++releases;}
    bool OnCreateVolatile()override{Require(device.GetDisplay2().CreateVertexBuffer(32,0,&extra),"Actual volatile allocation failed");return Finish();}
    void OnDestroyVolatile()override{device.GetDisplay2().Destroy(&extra);++volatileReleases;}
};
void RenderFailure(Display::C_Device& device,bool vol,bool throws){
    Application app;Renderer renderer(device,vol,throws);bool result=true;std::exception_ptr escaped;
    std::thread worker([&]{try{result=AmaxWorldLayer_Impl::NativeCreateRequiredRenderers();}catch(...){escaped=std::current_exception();}});worker.join();
    Require(!escaped&&!result,"Actual required renderer false/exception escaped its callback or reported readiness");
    Require(!renderer.extra.IsCreated()&&(!vol?!renderer.base.IsCreated():renderer.base.IsCreated()),"Original renderer did not roll back its failed acquisition independently");
    Require(vol?renderer.volatileReleases==1:renderer.releases==1,"Failed renderer acquisition was not rolled back exactly once");
    const char* expected=throws?"Authored real renderer initialization exception":vol?"Required native world renderer CreateVolatile failed":"Required native world renderer Create failed";
    Handoff(app,expected);renderer.DestroyRenderer();Require(renderer.releases==1&&(!vol||renderer.volatileReleases==1),"Retained successful renderer base was not released exactly once");
}
struct ActorCounts{unsigned lists=0,completed=0,actors=0,creators=0;bool creatorSurvived=true;};
constexpr Actor::C_ActorTypeId probeType=0x12345001;
struct Creator final:Actor::C_ActorCreationData {
    ActorCounts* counts=nullptr;bool throwCreate=false,failCreate=false;
    static Actor::C_ActorTypeId ClassType(){return probeType;}
    const Actor::C_ActorTypeId GetType()const override{return probeType;}
    ~Creator()override{if(counts)++counts->creators;}
};
class ProbeActor final:public Actor::C_Actor {
public:
    static Actor::C_ActorTypeId TypeId(){return probeType;}
    static Actor::C_ActorTypeName TypeName(){return "Native startup fixture actor";}
    ~ProbeActor()override{const auto* c=static_cast<const Creator*>(GetCreationData().GetData());if(c&&c->counts){c->counts->creatorSurvived&=c->counts->creators==0;++c->counts->actors;}}
    void Ready(){SetPrimed();}
private:
    bool PrimeImpl()override{return false;}
    bool CreateImpl()override{const auto& c=static_cast<const Creator&>(*GetCreationData());if(c.throwCreate)throw std::runtime_error("Authored real pending actor Create exception");return !c.failCreate;}
};
class List final:public Actor::C_ActorCreationListMultiple,public Game::I_NativeActorCreationCompletion {
    ActorCounts& counts;bool throwPreload;
public:
    List(ActorCounts& c,bool failure=false):counts(c),throwPreload(failure){}
    ~List()override{++counts.lists;}
    U32 CalcNumCreators()const override{if(throwPreload)throw std::runtime_error("Authored real actor preload exception");return C_ActorCreationListMultiple::CalcNumCreators();}
    void NativeActorCreationSucceeded()override{++counts.completed;}
};
void PreloadFailure(){Application app;ActorCounts counts;Actor::C_ActorFactory factory(4);Game::C_GameActorList actors(4,1,&factory);bool result=true;std::exception_ptr escaped;std::thread worker([&]{try{result=actors.CreateActors(new List(counts,true));}catch(...){escaped=std::current_exception();}});worker.join();Require(!escaped&&!result&&counts.lists==1&&!counts.completed,"Preload exception published success, escaped worker or lost list ownership");Handoff(app,"Authored real actor preload exception");}
void PendingCase(unsigned mode){
    Application app;ActorCounts counts;Actor::C_ActorFactory factory(4);factory.Register<ProbeActor>();
    {
        Game::C_GameActorList actors(4,1,&factory);Actor::C_ActorHandle handle;
        {
            Actor::C_ActorCreationDataFactoryHelper<Creator> creator;creator->counts=&counts;creator->throwCreate=mode==1;creator->failCreate=mode==2;
            auto* list=new List(counts);list->AddActorCreationListItem(creator.Get());Require(actors.CreateActors(list),"Actual authored pending actor did not prime");handle=creator->GetActorHandle();
        }
        Require(!counts.lists&&!counts.completed&&!counts.creators&&!actors.UpdatePendingActors(),"Pending actor resources were mistaken for completed creation");
        if(mode){
            auto* actor=const_cast<ProbeActor*>(static_cast<const ProbeActor*>(actors.GetActorPtr(handle)));Require(actor&&actor->GetLifetimeState()==ActorSys::E_LifetimeState_Priming,"Actual actor did not retain its priming state");actor->Ready();
            bool drained=false;std::exception_ptr escaped;std::thread worker([&]{try{drained=actors.UpdatePendingActors();}catch(...){escaped=std::current_exception();}});worker.join();
            Require(!escaped&&drained&&counts.lists==1,"Postload completion did not retire its owned pending list");
            if(mode==1){Require(!counts.completed,"Throwing real postload phase published success");Handoff(app,"Authored real pending actor Create exception");}
            else Require(counts.completed==1,"Original Continue policy stopped tolerating an optional actor Create=false");
        }
    }
    Require(counts.lists==1&&counts.actors==1&&counts.creators==1&&counts.creatorSurvived,"Cancelling pending ownership invalidated the actual actor's strong creator reference");
    if(!mode)Require(!counts.completed,"Cancellation/destructor alone published creation success");
}
}
int main(){try{
    Display::C_Device device;Display::C_DeviceParams params;params.m_Width=32;params.m_Height=24;Require(device.Create(params)&&device.CreateVolatile(params),"Actual device lease failed");
    for(bool vol:{false,true})for(bool throws:{false,true})RenderFailure(device,vol,throws);
    Actor::C_ActorCreationDataFactory::Register<Creator>(probeType);
    PreloadFailure();for(unsigned mode=0;mode<3;++mode)PendingCase(mode);
    Require(device.GetDisplay2().WaitIdle(),"Actual device work did not retire");device.DestroyVolatile();device.Destroy();
    std::cout<<"Actual required-renderer aggregation/rollback + main first-error barrier, actor preload/pending exception, cancellation/strong creator lifetime and original optional Continue policy PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
