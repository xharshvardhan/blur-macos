// [Blur2PC] Opt-in actual renderer/SDL demand measurements with silent PCM and
// an authored producer stall. No game bank, audible signal or scheduling change.
#include "AudioPch.h"
#include "source/core/macos/Engine.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

void XtLibrary_Application(){}
static void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
template<class P> static void until(P predicate,const char* reason){
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(4);
    while(!predicate()){require(std::chrono::steady_clock::now()<deadline,reason);MT::Thread::Sleep(1);}
}
struct Producer {
    std::atomic<bool> stall{false};
    std::atomic<U64> begins{0},ends{0};
    static void Begin(void* context){
        auto& self=*static_cast<Producer*>(context);++self.begins;
        if(self.stall.exchange(false))MT::Thread::Sleep(120);
    }
    static void End(void* context){++static_cast<Producer*>(context)->ends;}
};
struct SchedulerOwner {
    SchedulerOwner(){Scheduler::Init();Scheduler::SetGroupPoolSize(64);Scheduler::jqEnableWorkers(Scheduler::JQ_CORE_1|Scheduler::JQ_CORE_3);Scheduler::Start();}
    ~SchedulerOwner(){Scheduler::Shutdown();}
};
static void run(bool enabled,U32 rate=48000,bool injectStall=false){
    if(enabled)setenv("BLUR_MAC_AUDIO_DIAGNOSTICS","1",1);else unsetenv("BLUR_MAC_AUDIO_DIAGNOSTICS");
    auto& engine=Audio::Core::Engine::Instance();Producer producer;
    require(engine.SetOutputSampleRate(rate),"Original quality rate selection failed");
    {
        Audio::Core::Renderer renderer(8);
        require(renderer.Init(&producer,Producer::Begin,Producer::End,0,false),"Actual renderer init failed");renderer.Start();
        until([&]{return engine.SubmittedFrames()>=2048;},"Actual SDL writer never submitted silent PCM");
        const auto before=engine.Diagnostics();
        require(before.enabled==enabled&&before.output.enabled==enabled,"Diagnostic opt-in status disagrees");
        require(before.sampleRate==rate&&before.channels==2,"Unexpected native output format");
        if(enabled){
            require(before.elapsedSeconds>0&&before.output.deviceRate>0&&before.output.deviceChannels>0&&before.output.devicePeriodFrames>0,"Actual device format/period missing");
            until([&]{return engine.Diagnostics().output.callbacks>=2;},"Actual SDL demand callback never ran");
            const auto steadyStart=engine.Diagnostics();
            until([&]{return engine.Diagnostics().output.callbacks>=steadyStart.output.callbacks+12;},"Stable silent output demand window did not advance");
            const auto steadyEnd=engine.Diagnostics();
            std::cout<<"Stable no-stall window: input_rate="<<rate<<" callbacks="<<steadyEnd.output.callbacks-steadyStart.output.callbacks
                <<" approximate_demand_shortfalls="<<steadyEnd.output.demandShortfallCallbacks-steadyStart.output.demandShortfallCallbacks
                <<" approximate_demand_bytes="<<steadyEnd.output.demandShortfallBytes-steadyStart.output.demandShortfallBytes
                <<" producer_not_ready="<<steadyEnd.producerNotReady-steadyStart.producerNotReady
                <<" submitted_frames="<<steadyEnd.submittedFrames-steadyStart.submittedFrames
                <<" elapsed="<<steadyEnd.elapsedSeconds-steadyStart.elapsedSeconds
                <<" max_total_bytes="<<steadyEnd.output.maximumTotalBytes<<'\n';
            require(steadyEnd.output.demandShortfallCallbacks==steadyStart.output.demandShortfallCallbacks,"Steady silent output failed to satisfy actual SDL input demand");
            require(engine.QueuedFrames()<=S64(steadyEnd.maximumQueueTargetFrames+Audio::Core::uFrameLength),"Measured usable-output queue lost bounded backpressure");
            if(injectStall){
            producer.stall=true;
            until([&]{const auto d=engine.Diagnostics();return d.producerNotReady>steadyEnd.producerNotReady+10&&d.maximumSubmissionGapMilliseconds>=50&&d.submittedFrames>steadyEnd.submittedFrames+1024;},"Induced producer starvation did not reach counters and recover");
            const auto d=engine.Diagnostics();
            require(d.output.demandShortfallCallbacks>0&&d.output.demandShortfallBytes>0&&d.output.maximumAdditionalBytes>0&&d.output.maximumTotalBytes>0,"SDL approximate input demand not observed during actual consumption");
            require(d.emptyQueueObservations>0&&d.backpressurePolls>0,"Actual empty/full queue observations missing");
            require(d.clippedSamples==0&&d.nonfiniteSamples==0&&d.peakAbsolute==0,"Silent original renderer produced invalid PCM");
            std::cout<<"Observed authored120ms stall: max_submit_gap_ms="<<d.maximumSubmissionGapMilliseconds<<" producer_not_ready="<<d.producerNotReady<<" approximate_demand_shortfalls="<<d.output.demandShortfallCallbacks<<" device_period_frames="<<d.output.devicePeriodFrames<<'\n';
            }
        }else{
            require(before.producerNotReady==0&&before.emptyQueueObservations==0&&before.backpressurePolls==0&&before.maximumSubmissionGapMilliseconds==0&&before.output.callbacks==0,"Disabled diagnostics collected counters");
        }
    }
    require(!engine.IsRunning()&&producer.begins==producer.ends,"Renderer diagnostic lifetime did not join callbacks");
    const auto retired=engine.Diagnostics();MT::Thread::Sleep(3);
    require(engine.Diagnostics().elapsedSeconds==retired.elapsedSeconds,"Retired diagnostic elapsed time kept advancing");
    require(engine.Shutdown()&&engine.Shutdown(),"Diagnostic shutdown was not idempotent");
}
static void resampledPlayback(){
    // [Blur2PC] 44.1k is a raw SDL input conversion check, not a new game quality.
    // Keep the physical device open at 48k while adding the 44.1k logical stream.
    setenv("BLUR_MAC_AUDIO_DIAGNOSTICS","1",1);Audio::Mac::Playback deviceClock,output;
    require(deviceClock.Open(48000,2),"48k sibling device clock did not open");
    require(output.Open(44100,2),"44.1k silent SDL conversion open failed");
    const auto device=output.Diagnostics();
    require(device.deviceRate==48000,"44.1k fixture did not exercise actual 48k-device resampling");
    const U64 expected=(U64(device.devicePeriodFrames)*44100+device.deviceRate-1)/device.deviceRate;
    require(output.DevicePeriodInputFrames()==expected,"44.1k device-period conversion did not round up in input frames");
    std::vector<F32> silence(2*Audio::Core::uFrameLength,0);
    auto feed=[&]{
        Audio::Mac::PlaybackQueueStatus queue;
        require(output.GetQueueStatus(44100u/60,queue),"44.1k usable-output query failed");
        if(!queue.periodReady||queue.queuedInputFrames<=queue.targetInputFrames)require(output.Queue(silence.data(),Audio::Core::uFrameLength),"44.1k silent queue failed");
        require(output.QueuedFrames()<=S64(queue.targetInputFrames+Audio::Core::uFrameLength),"44.1k rounded usable-output queue bound changed");
    };
    for(U32 i=0;;++i){
        require(i<128,"44.1k paused priming exceeded bounded authored quantum count");
        Audio::Mac::PlaybackQueueStatus queue;require(output.GetQueueStatus(44100u/60,queue),"44.1k paused status query failed");
        if(queue.periodReady&&queue.queuedInputFrames>queue.targetInputFrames)break;
        feed();
    }
    require(output.Resume(),"44.1k SDL conversion did not resume");
    until([&]{feed();return output.Diagnostics().callbacks>=2;},"44.1k actual SDL callback never ran");
    const auto before=output.Diagnostics();
    until([&]{feed();return output.Diagnostics().callbacks>=before.callbacks+12;},"44.1k actual SDL demand window did not advance");
    const auto after=output.Diagnostics();
    require(after.demandShortfallCallbacks==before.demandShortfallCallbacks,"44.1k steady converted output failed SDL input demand");
    std::cout<<"44.1k SDL input/device conversion: device_rate="<<device.deviceRate<<" period="<<device.devicePeriodFrames<<" input_period_ceil="<<expected<<" stable_callbacks="<<after.callbacks-before.callbacks<<" approximate_demand_shortfalls="<<after.demandShortfallCallbacks-before.demandShortfallCallbacks<<'\n';
    require(output.Pause(),"44.1k SDL pause failed");output.Close();output.Close();
}
int main(){
    try{
        SchedulerOwner scheduler;
        run(false);run(true,24000);run(true,36000);run(true,48000,true);run(false);resampledPlayback();unsetenv("BLUR_MAC_AUDIO_DIAGNOSTICS");
        std::cout<<"Actual silent SDL/renderer diagnostics opt-in, producer starvation/recovery, approximate demand, format/period and joined restart PASS; no game-music quality claim\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
