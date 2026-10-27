// [Blur2PC][macOS] Original framework services over real clocks, files and workers.
#include <core/Core.h>
#include <core/Maths.h>
#include <core/containers.h>
#include <core/threads.h>
#include <core/threads/misc.h>
#include <system/time/Time.h>
#include <system/debug/Debug.h>
#include <system/serialise/C_FileSystem.h>
#include <framework/timer/Timer.h>
#include <framework/application/LoadDebugVars.h>
#include <framework/application/ApplicationParams.h>
#include <framework/layer/Layer.h>
#include <framework/layer/LayerManager.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

static void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
static void timers() {
    Framework::C_Timer timer;
    timer.ForceFrameTime(1.f / 60.f);
    for (int i = 0; i < 120; ++i) timer.Update();
    require(std::abs(timer.GetElapsedTime() - 2.f) < 0.0001f, "forced simulation ticks lost elapsed time");
    timer.TogglePaused();
    const float elapsed = timer.GetElapsedTime();
    timer.Update();
    require(timer.IsPaused() && timer.GetElapsedTime() == elapsed, "paused timer advanced simulation time");
    timer.SetTimerScale(0.5f);
    require(std::abs(timer.GetFrameTime() - 1.f / 120.f) < 0.00001f, "timer scale changed forced tick duration");
    timer.SetTimerScale(-1.f);
    require(timer.GetFrameTime() == 0.f, "negative timer scale was not clamped");
    timer.TogglePaused(); timer.SetTimerScale(1.f); timer.ForceFrameTime(-1.f);
    std::this_thread::sleep_for(std::chrono::milliseconds(15)); timer.Update();
    require(timer.GetFrameTime() >= 0.01f && timer.GetFrameTime() <= 1.f / 15.f,
            "framework timer did not use native elapsed time");
}
static void settings(const std::filesystem::path& root) {
    std::filesystem::create_directories(root);
    const auto previous = std::filesystem::current_path();
    struct Restore { std::filesystem::path previous; ~Restore() { std::filesystem::current_path(previous); } } restore{previous};
    std::filesystem::current_path(root);
    Sys::SetDefaultFileSystem(&Sys::C_FileSystem::Instance());
    Debug::Bool enabled("Native.Framework.Enabled", true, Debug::Persist());
    Debug::Int count("Native.Framework.Count", 73, Debug::Persist());
    Debug::Float ratio("Native.Framework.Ratio", 0.25f, Debug::Persist());
    Debug::String label("Native.Framework.Label", "native fixture", Debug::Persist());
    Framework::SaveDebugVars();
    require(std::filesystem::file_size("debug_values.xml") > 0, "framework did not save debug settings");
    enabled = false; count = -10; ratio = 0.75f; label = "changed";
    Framework::LoadDebugVars();
    require(enabled.Value() && count.Value() == 73 && ratio.Value() == 0.25f &&
            std::strcmp(label.Value(), "native fixture") == 0, "framework settings round trip changed values");
}
struct LayerState {
    std::atomic<int> created{0}, destroyed{0}, freed{0}, updates{0};
    std::atomic<U64> createThread{0};
    std::vector<int> order;
};
class Layer : public Framework::C_Layer {
public:
    Layer(LayerState& state, int priority) : C_Layer("native fixture"), state(state), priority(priority) {}
    ~Layer() override { ++state.freed; }
    S32 GetLayerPriority() const override { return priority; }
    HRESULT ProcessMessage(Framework::E_LayerMessage message) override {
        if (message == Framework::E_LayerMessage_Create) { state.createThread = MT::GetThreadId(); ++state.created; }
        if (message == Framework::E_LayerMessage_Destroy) ++state.destroyed;
        if (message == Framework::E_LayerMessage_Update) { ++state.updates; state.order.push_back(priority); }
        return S_OK;
    }
private:
    LayerState& state;
    int priority;
};
template<class F> static void pump(Framework::C_LayerManager& manager, F done) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!done()) {
        require(std::chrono::steady_clock::now() < deadline, "framework layer transition timed out");
        manager.Update(); std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
static void layers() {
    {
        LayerState cancelled;
        Framework::C_LayerManager manager;
        manager.DestroyLayer(new Layer(cancelled,0));
        auto* pending=new Layer(cancelled,0);manager.InsertLayer(pending);
        manager.DestroyLayer(pending);manager.Update();manager.ShutDown();
        require(cancelled.freed==2&&cancelled.created==0&&cancelled.destroyed==0,
                "Failed startup did not cancel unstarted synchronous layers without lifecycle callbacks");
    }
    {
        // A failed render can stop the main loop while original UI/audio creation
        // is still running. Quiescing must retire it before owner teardown.
        struct BlockingLayer final : Layer {
            std::atomic<bool>& entered; std::atomic<bool>& release;
            BlockingLayer(LayerState& state,std::atomic<bool>& e,std::atomic<bool>& r)
                :Layer(state,0),entered(e),release(r){}
            HRESULT ProcessMessage(Framework::E_LayerMessage message) override {
                if(message==Framework::E_LayerMessage_Create){entered=true;while(!release.load())std::this_thread::yield();}
                return Layer::ProcessMessage(message);
            }
        };
        LayerState state;std::atomic<bool> entered{false},release{false};
        Framework::C_LayerManager manager;
        auto* inFlight=new BlockingLayer(state,entered,release);manager.InsertLayerAsync(inFlight);
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(!entered&&std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
        if(!entered){release=true;throw std::runtime_error("Async creator never entered");}
        auto* queued=new Layer(state,1);manager.InsertLayerAsync(queued);
        std::thread complete([&]{std::this_thread::sleep_for(std::chrono::milliseconds(10));release=true;});
        manager.NativeQuiesceCreation();complete.join();manager.NativeQuiesceCreation();
        require(inFlight->GetLayerState()==Framework::E_LayerState_Running,"Quiescing did not publish completed creation");
        manager.DestroyLayer(queued);manager.DestroyLayer(inFlight);manager.Update();manager.ShutDown();
        require(state.freed==2&&state.destroyed==state.created,"Quiesced failure cleanup lost a layer or mismatched callbacks");
    }
    for (int cycle = 0; cycle < 3; ++cycle) {
        LayerState state;
        {
            Framework::C_LayerManager manager;
            auto* later = new Layer(state, 5); auto* first = new Layer(state, -5);
            manager.InsertLayer(later); manager.InsertLayer(first); manager.Update();
            require(state.created == 2 && state.createThread == MT::GetThreadId(), "synchronous layer create used wrong thread");
            manager.SendMessageToLayers(Framework::E_LayerMessage_Update);
            require(state.order == std::vector<int>({-5, 5}), "framework dispatch lost layer priority ordering");
            first->DisableLayer(); manager.SendMessageToLayers(Framework::E_LayerMessage_Update);
            require(state.updates == 3, "disabled layer received update");
            manager.Pause(42); manager.Pause(99); manager.Resume(42);
            require(manager.IsPaused() && !manager.IsPaused(42) && manager.IsPaused(99), "nested layer pause IDs changed");
            manager.Resume(99); require(!manager.IsPaused(), "layer resume did not clear pause state");
            manager.DestroyLayer(first); manager.DestroyLayer(later); manager.Update();
            require(state.destroyed == 2 && state.freed == 2, "synchronous layer destroy leaked callbacks or objects");
            auto* async = new Layer(state, 0); manager.InsertLayerAsync(async);
            pump(manager, [&] {
                manager.SendMessageToLayers(Framework::E_LayerMessage_Update);
                return state.updates >= 4;
            });
            require(state.createThread != MT::GetThreadId() && async->GetLayerState() == Framework::E_LayerState_Running,
                    "asynchronous layer did not load on native worker");
            manager.DestroyLayerAsync(async);
            // Let the worker consume the request-time notification before Update publishes the actual destroy queue.
            // Publication must supply its own wake; the normal transition deadline still checks liveness.
            std::this_thread::sleep_for(std::chrono::milliseconds(10));manager.Update();
            pump(manager, [&] { return state.freed == 3; });
            require(state.destroyed == 3, "asynchronous layer lost destroy callback");
            // Shutdown must drain both accepted and not-yet-created requests.
            for (int i = 0; i < 20; ++i) manager.InsertLayerAsync(new Layer(state, i));
            manager.ShutDown(); manager.ShutDown();
            require(state.freed==23&&state.destroyed==state.created,"Initial framework shutdown did not drain its layers before restart");
            manager.NativeRestart();auto* restarted=new Layer(state,0);manager.InsertLayerAsync(restarted);
            pump(manager,[&]{return restarted->GetLayerState()==Framework::E_LayerState_Running;});
            require(state.createThread!=MT::GetThreadId(),"Restarted framework layer used the caller instead of its native worker");
            manager.DestroyLayerAsync(restarted);pump(manager,[&]{return state.freed==24;});manager.ShutDown();
        }
        require(state.freed == 24 && state.destroyed == state.created,
                "framework shutdown lost a pending/ready/running layer or called destroy without create");
    }
}
int main(int argc, char** argv) {
    try {
        require(argc == 2, "fixture directory missing");
        require(FAILED(E_FAIL) && SUCCEEDED(S_OK) && SUCCEEDED(S_FALSE), "native HRESULT sign contract changed");
        Framework::C_ApplicationParams parameters;
        require(parameters.m_InputUserCount == 1 && parameters.m_ViewCount == 4 && !parameters.m_NetworkStackAlreadyStarted,
                "original application defaults changed on native platform");
        timers(); settings(std::filesystem::path(argv[1]) / "framework-fixture"); layers();
        std::cout << "Original framework timer, persistent settings and layer lifecycle PASS\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
