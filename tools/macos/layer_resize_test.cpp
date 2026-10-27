// [Blur2PC][macOS] Actual async LayerManager transactions and Ready handoff.
#include "FrameworkPch.h"
#include <framework/layer/Layer.h>
#include <framework/layer/LayerManager.h>
#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace Framework;
static void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
struct Record {std::atomic<U32> creates{0},destroys{0},deletes{0},resizes{0},updates{0};std::atomic<bool> cleanup{false};};
class Layer final:public C_Layer {
    Record& record;MT::Event *entered,*release;bool fail,gateDestroy;
public:
    Layer(Record& r,MT::Event* entered=nullptr,MT::Event* release=nullptr,bool fail=false,bool gateDestroy=false):C_Layer("resize transaction"),record(r),entered(entered),release(release),fail(fail),gateDestroy(gateDestroy){}
    ~Layer()override{++record.deletes;}
private:
    HRESULT ProcessMessage(E_LayerMessage message)override{
        if(message==E_LayerMessage_Create){++record.creates;if(entered)entered->Signal();if(release&&!record.cleanup)require(release->Wait(5000),"Create gate timed out");if(fail)throw std::runtime_error("authored Create failure");}
        if(message==E_LayerMessage_Destroy){++record.destroys;if(gateDestroy){if(entered)entered->Signal();if(release&&!record.cleanup)require(release->Wait(5000),"Destroy gate timed out");}}
        if(message==E_LayerMessage_Resize)++record.resizes;
        if(message==E_LayerMessage_Update)++record.updates;
        return S_OK;
    }
};
struct Release {MT::Event& event;Record& record;~Release(){record.cleanup=true;event.Signal();}};
static C_LayerManager::NativeResizeLease::Result status(C_LayerManager& manager){C_LayerManager::NativeResizeLease lease(manager);return lease.GetResult();}
static void waitStatus(C_LayerManager& manager,C_LayerManager::NativeResizeLease::Result expected){
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(std::chrono::steady_clock::now()<end){if(status(manager)==expected)return;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    throw std::runtime_error("Expected lifecycle/Ready status never published");
}
static void createdThenReady(){
    Record record;MT::Event entered,release;C_LayerManager manager;Release cleanup{release,record};
    auto* layer=new Layer(record,&entered,&release);manager.InsertLayerAsync(layer);require(entered.Wait(5000),"Async creator never entered");
    const auto begin=std::chrono::steady_clock::now();require(status(manager)==C_LayerManager::NativeResizeLease::Result::Busy,"Resize entered an in-flight creator");
    require(std::chrono::steady_clock::now()-begin<std::chrono::milliseconds(100),"Main resize blocked on a creator");
    // The original running-list broadcast cannot reach an uncompleted object.
    manager.SendMessageToLayers(E_LayerMessage_Resize);require(record.resizes==0,"In-flight object received Resize");
    release.Signal();waitStatus(manager,C_LayerManager::NativeResizeLease::Result::ReadyPending);
    require(record.resizes==0&&record.updates==0,"Ready object received running callbacks");
    manager.Update();{
        C_LayerManager::NativeResizeLease lease(manager);require(bool(lease),"Ordinary Ready→Running handoff did not unblock resize");
        manager.SendMessageToLayers(E_LayerMessage_Resize);
    }
    require(record.resizes==1,"Completed running layer missed the retained resize");
    // A failure from the owner while holding a lease must release the barrier.
    try{C_LayerManager::NativeResizeLease lease(manager);require(bool(lease),"Resize lease unavailable");throw std::runtime_error("authored owner failure");}catch(const std::runtime_error&){}
    require(status(manager)==C_LayerManager::NativeResizeLease::Result::Acquired,"Resize exception retained the lifecycle lease");
    manager.NativeQuiesceCreation();release.Signal();manager.NativeDestroyLayerNow(layer);manager.ShutDown();
    require(record.creates==1&&record.destroys==1&&record.deletes==1,"Successful layer did not retire exactly once");
}
static void asynchronousDestroy(){
    Record record;MT::Event entered,release;C_LayerManager manager;Release cleanup{release,record};
    // Create's event/hold expires first. The same real layer then gates Destroy.
    auto* layer=new Layer(record,&entered,&release,false,true);manager.InsertLayerAsync(layer);require(entered.Wait(5000),"Destroy fixture Create never entered");release.Signal();
    waitStatus(manager,C_LayerManager::NativeResizeLease::Result::ReadyPending);manager.Update();manager.DestroyLayerAsync(layer);manager.Update();
    require(entered.Wait(5000),"Async Destroy callback never entered");
    require(status(manager)==C_LayerManager::NativeResizeLease::Result::Busy,"Resize entered asynchronous Destroy");
    release.Signal();waitStatus(manager,C_LayerManager::NativeResizeLease::Result::Acquired);manager.NativeQuiesceCreation();manager.ShutDown();
    require(record.creates==1&&record.destroys==1&&record.deletes==1,"Async Destroy transaction escaped ownership");
}
static void failedCreate(){
    Record record;MT::Event entered,release;C_LayerManager manager;Release cleanup{release,record};
    auto* layer=new Layer(record,&entered,&release,true);manager.InsertLayerAsync(layer);require(entered.Wait(5000),"Failure creator never entered");release.Signal();
    waitStatus(manager,C_LayerManager::NativeResizeLease::Result::CreationFailed);
    bool original=false;try{manager.Update();}catch(const std::exception& e){original=std::string(e.what()).find("authored Create failure")!=std::string::npos;}
    require(original,"Creation failure was hidden by resize deferral");manager.NativeQuiesceCreation();manager.NativeDestroyLayerNow(layer);manager.ShutDown();
    require(record.creates==1&&record.destroys==0&&record.deletes==1,"Failed creator received invented Destroy or leaked");
}
int main(){try{createdThenReady();asynchronousDestroy();failedCreate();std::cout<<"Real LayerManager nonblocking Create/Destroy barrier, Ready handoff, retained Resize, creation failure and owner-exception release PASS (CPU lifecycle fixture)\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
