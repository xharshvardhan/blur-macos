// [Blur2PC][macOS] Actual layer-manager teardown/cancellation mechanics, not an Amax gameplay fixture.
#include "FrameworkPch.h"
#include <framework/layer/Layer.h>
#include <framework/layer/LayerManager.h>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <thread>
#include <string>

namespace {
void Require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
struct Record { bool alive=false; unsigned creates=0,destroys=0,deletes=0; };
class Layer final : public Framework::C_Layer {
public:
    Layer(const char* name,Record& record,std::vector<int>& order,int id,Record* dependency=nullptr)
        :C_Layer(name),record(record),order(order),id(id),dependency(dependency) { record.alive=true; }
    ~Layer() override { Require(record.alive,"Layer deleted twice");record.alive=false;++record.deletes; }
private:
    HRESULT ProcessMessage(Framework::E_LayerMessage message) override {
        if(message==Framework::E_LayerMessage_Create) ++record.creates;
        if(message==Framework::E_LayerMessage_Destroy) {
            Require(!dependency || dependency->alive,"Destruction callback lost its owning dependency");
            ++record.destroys;order.push_back(id);
        }
        return S_OK;
    }
    Record& record;std::vector<int>& order;int id;Record* dependency;
};
void VerifyCancelled(const Record& r) { Require(r.creates==0&&r.destroys==0&&r.deletes==1&&!r.alive,"Uncreated layer received callbacks or escaped cancellation"); }
void VerifyDestroyed(const Record& r) { Require(r.creates==1&&r.destroys==1&&r.deletes==1&&!r.alive,"Created layer teardown did not complete exactly once"); }
}
int main() { try {
    using namespace Framework;
    std::vector<int> order;
    C_LayerManager manager;
    manager.NativeQuiesceCreation();
    Record raw,pending,pendingAsync;
    manager.NativeDestroyLayerNow(new Layer("constructed",raw,order,0));
    auto* sync=new Layer("pending sync",pending,order,0);manager.InsertLayer(sync);manager.NativeDestroyLayerNow(sync);
    auto* async=new Layer("pending async",pendingAsync,order,0);manager.InsertLayerAsync(async);manager.NativeDestroyLayerNow(async);
    VerifyCancelled(raw);VerifyCancelled(pending);VerifyCancelled(pendingAsync);

    Record system,boot,world,race,ui,audio,book;
    auto* systemLayer=new Layer("System owner",system,order,7);
    auto* bootLayer=new Layer("Boot needs System",boot,order,1,&system);
    auto* worldLayer=new Layer("World needs UI",world,order,3,&ui);
    auto* raceLayer=new Layer("RaceLogic needs World",race,order,2,&world);
    auto* uiLayer=new Layer("UI needs Audio",ui,order,4,&audio);
    auto* audioLayer=new Layer("Audio needs System",audio,order,6,&system);
    auto* bookLayer=new Layer("RaceBook needs System",book,order,5,&system);
    C_Layer* layers[]={systemLayer,bootLayer,worldLayer,raceLayer,uiLayer,audioLayer,bookLayer};
    for(auto* layer:layers) manager.InsertLayer(layer);
    manager.Update();
    manager.NativeDestroyLayerNow(bootLayer);
    manager.NativeDestroyLayerNow(raceLayer);
    manager.NativeDestroyLayerNow(worldLayer);
    manager.NativeDestroyLayerNow(uiLayer);
    manager.NativeDestroyLayerNow(bookLayer);
    manager.NativeDestroyLayerNow(audioLayer);
    manager.NativeDestroyLayerNow(systemLayer);
    Require(order==std::vector<int>({1,2,3,4,5,6,7}),"Explicit dependency order was replaced by active pointer order");
    for(const Record* r:{&system,&boot,&world,&race,&ui,&audio,&book}) VerifyDestroyed(*r);

    // Cover both stages of asynchronous retirement, including transfer before the native creator joined.
    Record queued,transferred,drained;
    auto* a=new Layer("pending destroy",queued,order,8);manager.InsertLayer(a);manager.Update();manager.DestroyLayer(a);manager.NativeDestroyLayerNow(a);
    auto* b=new Layer("transferred async destroy",transferred,order,9);manager.InsertLayer(b);manager.Update();manager.DestroyLayerAsync(b);manager.Update();manager.NativeDestroyLayerNow(b);
    auto* c=new Layer("drained async destroy",drained,order,10);manager.InsertLayer(c);manager.Update();manager.DestroyLayerAsync(c);manager.NativeDrainPendingDestruction();manager.NativeDrainPendingDestruction();
    VerifyDestroyed(queued);VerifyDestroyed(transferred);VerifyDestroyed(drained);
    manager.ShutDown();manager.ShutDown();
    // A real callback thread publishes an initialization error; only the checked main Update unwinds.
    {
        C_LayerManager failed;
        failed.NativeQuiesceCreation();
        Record unstarted;
        auto* layer=new Layer("blocked by worker failure",unstarted,order,11);
        failed.InsertLayer(layer);
        const auto mainThread=std::this_thread::get_id();
        std::thread publisher([&]{
            Require(std::this_thread::get_id()!=mainThread,"Failure callback did not run on another thread");
            failed.NativeReportCreationFailure("Actual required-vehicle callback failure");
            failed.NativeReportCreationFailure("Later failure must not replace the first");
        });
        publisher.join();
        bool received=false;
        try { failed.Update(); }
        catch(const std::runtime_error& error) {
            received=std::this_thread::get_id()==mainThread && std::string(error.what())=="Actual required-vehicle callback failure";
        }
        Require(received,"Worker initialization failure did not surface as the first error on checked main Update");
        Require(unstarted.creates==0 && layer->GetLayerState()==E_LayerState_PendingCreate,
                "Worker initialization failure still promoted pending work toward Ready");
        failed.NativeDestroyLayerNow(layer);
        VerifyCancelled(unstarted);
        failed.ShutDown();failed.ShutDown();
    }
    std::cout<<"Original LayerManager native dependency callbacks, constructed/sync/async cancellation, pending/transferred async retirement, repeated drain/shutdown and worker-failure main-thread handoff PASS (not actual Amax Boot callback)\n";
    return 0;
} catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; } }
