// [Blur2PC][macOS] Real layer-manager failure ownership and worker-to-main error handoff.
#include "FrameworkPch.h"
#include <framework/layer/Layer.h>
#include <framework/layer/LayerManager.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <stdexcept>
#include <string>
#include <iostream>

namespace {
void Require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
struct Record {
    std::atomic<unsigned> creates{0},destroys{0},deletes{0},updates{0},renders{0},rollbacks{0};
    std::atomic<bool> resource{false};
};
enum class Result {Success,Failure,Exception};
class Layer final:public Framework::C_Layer {
    Record& record;Result result;MT::Event* entered;MT::Event* release;
public:
    Layer(const char* name,Record& record,Result result=Result::Success,MT::Event* entered=nullptr,MT::Event* release=nullptr)
        :C_Layer(name),record(record),result(result),entered(entered),release(release){}
    ~Layer()override{Require(!record.resource,"Failed layer retained resources or successful layer skipped Destroy");++record.deletes;}
private:
    HRESULT ProcessMessage(Framework::E_LayerMessage message)override{
        using namespace Framework;
        if(message==E_LayerMessage_Create){
            ++record.creates;record.resource=true;
            if(entered)entered->Signal();
            if(release)Require(release->Wait(5000),"Creator release timed out");
            if(result!=Result::Success){
                // The callback owns rollback; the manager must not invent a successful Destroy.
                record.resource=false;++record.rollbacks;
                if(result==Result::Exception)throw std::runtime_error("original creator exception");
                return E_FAIL;
            }
        }
        if(message==E_LayerMessage_Destroy){Require(record.resource,"Destroy was sent to a failed/uncreated layer");record.resource=false;++record.destroys;}
        if(message==E_LayerMessage_Update)++record.updates;
        if(message==E_LayerMessage_RenderFullscreen)++record.renders;
        return S_OK;
    }
};
std::string Error(Framework::C_LayerManager& manager){try{manager.Update();}catch(const std::exception& e){return e.what();}throw std::runtime_error("Failed Create was promoted instead of reported");}
void CheckFailed(const Record& r){Require(r.creates==1&&r.rollbacks==1&&r.destroys==0&&r.deletes==1&&!r.resource&&r.updates==0&&r.renders==0,"Failed layer ownership/rollback/callback counts disagree");}
void CheckCancelled(const Record& r){Require(r.creates==0&&r.rollbacks==0&&r.destroys==0&&r.deletes==1&&!r.resource,"Unstarted work escaped cancellation");}

void SyncFailure(Result failure){
    using namespace Framework;
    C_LayerManager manager;manager.NativeQuiesceCreation();
    Record failed,cancelled,cancelledAsync;
    auto* layer=new Layer("sync failure",failed,failure);
    auto* pending=new Layer("sync unstarted",cancelled);
    auto* async=new Layer("async unstarted",cancelledAsync);
    manager.InsertLayer(layer);manager.InsertLayer(pending);manager.InsertLayerAsync(async);
    const auto error=Error(manager);
    Require(error.find("sync failure")!=std::string::npos,"Creation error lost layer name");
    Require(error.find(failure==Result::Failure?"HRESULT 0x80004005":"original creator exception")!=std::string::npos,"Creation error lost actual HRESULT/exception");
    Require(layer->GetLayerState()==E_LayerState_Constructed&&pending->GetLayerState()==E_LayerState_PendingCreate,"Failed/successor state was promoted");
    manager.SendMessageToLayers(E_LayerMessage_Update);manager.SendMessageToLayers(E_LayerMessage_RenderFullscreen);
    Require(Error(manager)==error,"First failure changed before teardown");
    manager.NativeDestroyLayerNow(layer);manager.NativeDestroyLayerNow(pending);manager.NativeDestroyLayerNow(async);
    manager.ShutDown();manager.ShutDown();CheckFailed(failed);CheckCancelled(cancelled);CheckCancelled(cancelledAsync);

    // Real worker restart must clear only the old error after all old objects were retired.
    manager.NativeRestart();Record restarted;auto* good=new Layer("restart success",restarted);manager.InsertLayer(good);manager.Update();
    manager.SendMessageToLayers(E_LayerMessage_Update);manager.SendMessageToLayers(E_LayerMessage_RenderFullscreen);
    Require(restarted.creates==1&&restarted.updates==1&&restarted.renders==1,"Restart retained failure or stopped successful callbacks");
    manager.NativeQuiesceCreation();manager.NativeDestroyLayerNow(good);manager.ShutDown();manager.ShutDown();
    Require(restarted.destroys==1&&restarted.deletes==1,"Restarted layer teardown was not exactly once");
}

void AsyncFailure(Result failure){
    using namespace Framework;
    C_LayerManager manager;
    MT::Event entered("FailureFixtureEntered"),release("FailureFixtureRelease");
    Record failed,cancelled,syncCancelled;
    auto* layer=new Layer("async failure",failed,failure,&entered,&release);
    manager.InsertLayerAsync(layer);Require(entered.Wait(5000),"Actual async Create callback never ran");
    manager.InsertLayerAsync(new Layer("async remains queued",cancelled));
    release.Signal();
    std::string error;const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(error.empty()&&std::chrono::steady_clock::now()<until){
        try{manager.Update();}catch(const std::exception& e){error=e.what();}
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Require(error.find("async failure")!=std::string::npos,"Async failure did not reach main Update");
    Require(error.find(failure==Result::Failure?"HRESULT 0x80004005":"original creator exception")!=std::string::npos,"Async failure lost the callback result");
    manager.InsertLayer(new Layer("sync remains queued",syncCancelled));
    Require(Error(manager)==error,"A later failure changed the first error");
    manager.SendMessageToLayers(E_LayerMessage_Update);manager.SendMessageToLayers(E_LayerMessage_RenderFullscreen);
    manager.NativeQuiesceCreation();
    Require(layer->GetLayerState()==E_LayerState_Constructed,"Failed async layer was promoted during quiescence");
    manager.NativeDestroyLayerNow(layer);manager.ShutDown();manager.ShutDown();
    CheckFailed(failed);CheckCancelled(cancelled);CheckCancelled(syncCancelled);
}

void ReadyBeforeFailure(){
    using namespace Framework;
    C_LayerManager manager;manager.NativeQuiesceCreation();manager.ShutDown();
    Record ready,failed,cancelled;
    MT::Event entered("ReadyFixtureEntered"),release("ReadyFixtureRelease");
    auto* good=new Layer("ready predecessor",ready);
    auto* bad=new Layer("later failed creator",failed,Result::Failure,&entered,&release);
    manager.InsertLayerAsync(good);manager.InsertLayerAsync(bad);manager.InsertLayerAsync(new Layer("local batch cancellation",cancelled));
    manager.NativeRestart();Require(entered.Wait(5000),"Second actual creator never ran");
    Require(good->GetLayerState()==E_LayerState_Ready,"First successful callback was not staged Ready");
    release.Signal();manager.NativeQuiesceCreation();
    Require(good->GetLayerState()==E_LayerState_Ready,"Failure promoted a successful but unstarted predecessor");
    Require(Error(manager).find("later failed creator")!=std::string::npos,"Quiescing lost the actual failure");
    manager.NativeDestroyLayerNow(good);manager.NativeDestroyLayerNow(bad);manager.ShutDown();manager.ShutDown();
    Require(ready.creates==1&&ready.destroys==1&&ready.deletes==1&&ready.updates==0&&ready.renders==0,"Ready predecessor did not receive exactly its successful lifecycle");
    CheckFailed(failed);CheckCancelled(cancelled);
}
}
int main(){try{
    SyncFailure(Result::Failure);SyncFailure(Result::Exception);
    AsyncFailure(Result::Failure);AsyncFailure(Result::Exception);ReadyBeforeFailure();
    std::cout<<"Native LayerManager real sync/async HRESULT and exception handoff, failed callback rollback/ownership, no failed update/render, ready cleanup, pending cancellation, repeated shutdown/restart PASS\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
