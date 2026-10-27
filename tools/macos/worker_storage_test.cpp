// [Blur2PC][macOS] Original scheduler logical IDs own distinct actual scene/audio storage.
#include "../../code/amax/game/stdafx.h"
#include "../../code/mercury/library/audio/AudioPch.h"
#include "../../code/mercury/library/audio/source/ObjectManager.h"
#define SCENEGRAPH_ALLOW_INCLUDE
#include <scenegraph/SceneGraphImpl.h>
#include <scenegraph/RenderTask.h>
#include <fx/Fx.h>
#include <system/scheduler/jobqueue.h>
#include <atomic>
#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
void Require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
template<class F> void Reject(F call){bool rejected=false;try{call();}catch(const std::out_of_range&){rejected=true;}Require(rejected,"Invalid scene context ID was accepted");}
struct State {
    SceneGraph::C_SceneGraph* graph;
    std::array<SceneGraph::C_Context*,8> contexts{};
    std::array<F32*,8> buffers{};
    std::atomic<unsigned> entered{0};
    std::atomic<bool> release{false},failed{false};
    std::array<std::atomic<unsigned>,8> calls{};
    State(){for(auto& count:calls)count.store(0);}
};
void CompletionWorker(Scheduler::Task* task){++*static_cast<std::atomic<unsigned>*>(task->Output);}
void Worker(Scheduler::Task* task){
    auto& state=*static_cast<State*>(task->Output);
    const int id=Scheduler::GetWorkerID();
    if(id<1||id>7){state.failed=true;return;}
    try {
        state.contexts[id]=state.graph->GetContext();
        state.buffers[id]=Audio::ObjectManager::GetDynamicBuffers();
        for(unsigned i=0;i<Audio::Core::uFrameLength;++i)state.buffers[id][i]=F32(id*1000+i);
        ++state.calls[id];++state.entered;
        const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(10);
        while(!state.release&&std::chrono::steady_clock::now()<until)std::this_thread::yield();
        if(!state.release)state.failed=true;
        for(unsigned i=0;i<Audio::Core::uFrameLength;++i)if(state.buffers[id][i]!=F32(id*1000+i))state.failed=true;
    }catch(...){state.failed=true;++state.entered;}
}
}
int main(){try {
    static_assert(SCENEGRAPH_MAX_CONTEXTS==8&&Audio::ObjectManager::DynamicBufferWorkerSlots==8,"Native logical worker capacity must include ID7");
    std::cerr<<"worker-storage: policies/init"<<std::endl;
    Scheduler::Policy policies[8][8]={};Scheduler::PolicyHandle handles[8]={};
    for(int id=1;id<8;++id){policies[id][id]=1;handles[id]=Scheduler::CreatePolicy(policies[id]);}
    Scheduler::Init();Scheduler::SetGroupPoolSize(64);Scheduler::jqEnableWorkers(0xfe);Scheduler::Start();
    struct SchedulerScope{bool active=true;void Stop(){Scheduler::Shutdown();active=false;}~SchedulerScope(){if(active)Scheduler::Shutdown();}} scheduler;
    std::cerr<<"worker-storage: scheduler started"<<std::endl;
    Require(Scheduler::GetNumWorkers()==7&&Scheduler::GetWorkerID()==0,"Fixture requires seven native workers and main ID0");
    Display::C_Device device;device.AddWindow(Maths::C_Rect(Maths::HVec2(0,0),Maths::HVec2(1,1)));
    Display::C_DeviceParams params;params.m_Width=32;params.m_Height=24;
    Require(device.Create(params)&&device.CreateVolatile(params),"Real display device failed");
    std::cerr<<"worker-storage: device created"<<std::endl;
    FX::Initialise();
    {
        std::cerr<<"worker-storage: scene creation"<<std::endl;
        SceneGraph::C_SceneGraph graph;
        std::cerr<<"worker-storage: audio creation/init"<<std::endl;
        Audio::ObjectManager audio;
        audio.Init(Audio::Core::uFrameLength*sizeof(F32),Audio::ISystem::eDistribUpdate_Enabled,handles[1],-1,handles[1]);
        std::cerr<<"worker-storage: publishing jobs"<<std::endl;
        State state;state.graph=&graph;state.contexts[0]=graph.GetContext();state.buffers[0]=Audio::ObjectManager::GetDynamicBuffers();
        for(unsigned i=0;i<Audio::Core::uFrameLength;++i)state.buffers[0][i]=F32(i);
        auto group=Scheduler::CreateGroup(true);const Scheduler::jqModule module(Worker,"Native distinct logical worker storage");
        for(int id=1;id<8;++id)Scheduler::AddTask(module,nullptr,&state,group,handles[id]);
        const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(10);
        while(state.entered<7&&std::chrono::steady_clock::now()<until)std::this_thread::yield();
        const bool concurrent=state.entered==7;
        std::cerr<<"worker-storage: concurrent entered="<<state.entered.load()<<std::endl;
        state.release=true;Scheduler::Flush(group,false,15000);Scheduler::DestroyGroup(group);
        std::cerr<<"worker-storage: jobs joined"<<std::endl;
        Require(concurrent&&!state.failed,"All seven real workers did not concurrently retain separate storage");
        for(int id=0;id<8;++id){
            Require(state.contexts[id]&&state.buffers[id],"Worker storage pointer is null");
            Require(state.buffers[id]==state.buffers[0]+id*Audio::Core::uFrameLength,"Worker audio scratch region has wrong stride");
            if(id)Require(state.calls[id]==1,"A job did not execute exactly once on its required logical worker");
            for(int other=0;other<id;++other)Require(state.contexts[id]!=state.contexts[other]&&state.buffers[id]!=state.buffers[other],"Logical workers alias scene/audio storage");
            for(unsigned i=0;i<Audio::Core::uFrameLength;++i)Require(state.buffers[id][i]==F32(id*1000+i),"Concurrent scratch writes changed another worker's samples");
        }
        Reject([]{SceneGraph::CheckedContextIndex(-1);});Reject([]{SceneGraph::CheckedContextIndex(8);});
        SceneGraph::C_RenderTask renderTask;Reject([&]{renderTask.GetContext(-1);});Reject([&]{renderTask.GetContext(8);});
        std::cerr<<"worker-storage: rapid event-group retirement"<<std::endl;
        const Scheduler::jqModule completion(CompletionWorker,"Native completion retirement proof");
        std::atomic<unsigned> completions{0};
        for(unsigned cycle=0;cycle<256;++cycle){
            auto retiring=Scheduler::CreateGroup(true);
            for(int id=1;id<8;++id)Scheduler::AddTask(completion,nullptr,&completions,retiring,handles[id]);
            Scheduler::Flush(retiring,false,5000);Scheduler::DestroyGroup(retiring);
            Require(completions==(cycle+1)*7,"Event-group retirement published before actual callbacks completed");
        }
        std::cout<<"Original scheduler slots0..7: seven concurrent jobs, distinct actual scene contexts/audio scratch, sample integrity and checked invalid IDs passed\n";
    }
    std::cerr<<"worker-storage: scene/audio destroyed"<<std::endl;
    FX::Shutdown();Require(device.GetDisplay2().WaitIdle(),"Actual GPU wait failed");device.DestroyVolatile();device.Destroy();
    std::cerr<<"worker-storage: FX/device destroyed"<<std::endl;
    scheduler.Stop();
    std::cout<<"Native 256 immediate completion-event retire/reuse cycles and scheduler shutdown passed"<<std::endl;
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
