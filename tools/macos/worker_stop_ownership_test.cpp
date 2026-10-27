// [Blur2PC][macOS] Actual Resource/Layer quit owners must not consume each other's stop signal.
#include "FrameworkPch.h"
#include <framework/layer/Layer.h>
#include <framework/layer/LayerManager.h>
#include <resource/Resource.h>
#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void SignalIsolation(){
    // These are the real worker classes/ShouldQuit implementations, without starting an unbounded old-code join.
    Resource::C_LoadingThread resource,otherResource;
    Framework::C_LayerManagerThread layer(nullptr),otherLayer(nullptr);
    layer.SendQuitMessage();
    Require(!resource.ShouldQuit(),"Layer quit was consumed by Resource worker");
    Require(!otherLayer.ShouldQuit(),"Layer quit was consumed by another Layer owner");
    Require(layer.ShouldQuit()&&!layer.ShouldQuit(),"Owned layer quit was lost or stopped being auto-reset");
    resource.SendQuitMessage();
    Require(!layer.ShouldQuit()&&!otherResource.ShouldQuit(),"Resource quit escaped its owning worker");
    Require(resource.ShouldQuit()&&!resource.ShouldQuit(),"Owned resource quit was lost or stopped being auto-reset");
    otherLayer.SendQuitMessage();otherResource.SendQuitMessage();
    Require(!layer.ShouldQuit()&&!resource.ShouldQuit(),"Other workers' stop signals escaped their ownership");
    Require(otherLayer.ShouldQuit()&&otherResource.ShouldQuit(),"Independent pending stop signals coalesced across owners");
    // General named MT events retain their original desktop contract: this repair belongs to the private owners.
    MT::Event named("NativeWorkerFixtureSharedEvent"),same("NativeWorkerFixtureSharedEvent");
    named.Signal();Require(same.Wait(0)&&!named.Wait(0),"Native generic named-event behavior changed");
}
std::atomic<unsigned> loads{0},posts{0},frees{0};
class Payload final:public Resource::C_Resource<Payload>{
public:
    static Util::C_String StaticGetResourceTypeName(){return "NativeWorkerStopOwnership";}
    bool LoadResourceData()override{++loads;return true;}
    bool PostLoadResourceData()override{++posts;return true;}
    bool FreeResourceData()override{++frees;return true;}
};
struct Counts{std::atomic<unsigned> creates{0},destroys{0},deletes{0};};
class Layer final:public Framework::C_Layer{
    Counts& counts;MT::Event& entered;
public:
    Layer(Counts& counts,MT::Event& entered):C_Layer("worker-stop async payload"),counts(counts),entered(entered){}
    ~Layer()override{++counts.deletes;}
private:
    HRESULT ProcessMessage(Framework::E_LayerMessage message)override{
        if(message==Framework::E_LayerMessage_Create){++counts.creates;entered.Signal();}
        if(message==Framework::E_LayerMessage_Destroy)++counts.destroys;
        return S_OK;
    }
};
void LoadProof(Resource::C_Manager& manager,unsigned serial){
    const unsigned before=loads.load(),postBefore=posts.load();
    Resource::C_Handle<Payload> resource;
    Require(resource.Load(("worker-stop-"+std::to_string(serial)+".bin").c_str()),"Real Resource load could not queue");
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(!static_cast<Payload*>(resource)->IsDataReady()&&std::chrono::steady_clock::now()<until){
        manager.PartialUpdate();std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Require(static_cast<Payload*>(resource)->IsDataReady()&&loads==before+1&&posts==postBefore+1,
            "Resource worker stopped when another owner quiesced or lost its real PostLoad");
    resource=static_cast<Payload*>(nullptr);
    Require(manager.NativeDrainUnreferencedResources(),"Original resource callback did not retire/restart");
}
void ActualWorkers(){
    // Exercise the same coexistence as the World fixture: resource owner is entered before the application's layer owner.
    Resource::C_Manager resources;
    Framework::C_LayerManager manager;
    for(unsigned cycle=0;cycle<12;++cycle){
        if(cycle)manager.NativeRestart();
        Counts counts;MT::Event entered;
        auto* layer=new Layer(counts,entered);manager.InsertLayerAsync(layer);
        Require(entered.Wait(3000),"Actual asynchronous Layer callback did not run after worker restart");
        // An accepted resource load plus callback completion establishes the resource worker is active as well.
        LoadProof(resources,cycle*2);
        manager.NativeQuiesceCreation();manager.NativeQuiesceCreation();
        Require(counts.creates==1&&layer->GetLayerState()==Framework::E_LayerState_Running,
                "Layer quiescence lost accepted callback readiness");
        LoadProof(resources,cycle*2+1); // Layer quit must not terminate the still-owned Resource worker.
        manager.NativeDestroyLayerNow(layer);manager.ShutDown();manager.ShutDown();
        Require(counts.destroys==1&&counts.deletes==1,"Actual layer payload did not retire exactly once");
    }
    Require(resources.NativeDrainUnreferencedResources(),"Final Resource worker owner did not drain");
    Require(loads==24&&posts==loads&&frees==loads,"Actual Resource payload callbacks were not balanced");
    // Stop may be requested before the new worker gets its first time slice.
    for(unsigned cycle=0;cycle<32;++cycle){
        Framework::C_LayerManager early;early.NativeQuiesceCreation();early.NativeQuiesceCreation();
        early.ShutDown();early.ShutDown();early.NativeRestart();early.NativeQuiesceCreation();early.ShutDown();
    }
}
}
int main(int argc,char** argv){try{
    Require(argc==1||(argc==2&&std::string(argv[1])=="--isolation-only"),"Only --isolation-only is supported");
    SignalIsolation();
    if(argc==1)ActualWorkers();
    std::cout<<"Actual worker quit ownership: cross-family/same-family reciprocal stop isolation, named-event contract"
             <<(argc==1?", real async callback/resource load after quiescence, 12 restart lifetimes and 32 early-stop/restart lifetimes":" (isolation only)")
             <<" PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
