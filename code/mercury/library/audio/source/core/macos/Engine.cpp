// [Blur2PC] SDL owns CoreAudio, conversion and device timing; the game owns its mixer.
#include "AudioPch.h"
#include "Engine.h"
#include <audio/macos/SDLPlayback.h>
#include <atomic>
#include <mutex>
#include <stdexcept>
#include <vector>
#include <chrono>
#include <cmath>
#include <cstdio>

struct Audio::Core::Engine::State {
    std::mutex lifecycle;
    mutable std::mutex errors;
    Audio::Mac::Playback output;
    std::thread writer;
    std::atomic<bool> stop{true},running{false};
    std::atomic<U64> submitted{0};
    Renderer* renderer=nullptr;
    U32 rate=48000;
    bool diagnostics=false;
    std::chrono::steady_clock::time_point started;
    std::chrono::steady_clock::time_point stopped;
    std::atomic<U64> producerNotReady{0},emptyQueue{0},backpressure{0},clipped{0},nonfinite{0},maximumGapNanoseconds{0};
    std::atomic<F32> peakAbsolute{0};
    std::atomic<U64> maximumQueueTarget{0},maximumPadding{0};
    std::string error;
    bool Fail(const std::string& value){std::lock_guard<std::mutex> guard(errors);error=value;return false;}
    EngineDiagnostics Snapshot(U32 channels)const{
        EngineDiagnostics result;
        result.enabled=diagnostics;result.sampleRate=rate;result.channels=channels;
        result.submittedFrames=submitted.load();result.producerNotReady=producerNotReady.load();
        result.emptyQueueObservations=emptyQueue.load();result.backpressurePolls=backpressure.load();
        result.clippedSamples=clipped.load();result.nonfiniteSamples=nonfinite.load();
        result.maximumQueueTargetFrames=maximumQueueTarget.load();result.maximumObservedPaddingFrames=maximumPadding.load();
        if(diagnostics)result.elapsedSeconds=std::chrono::duration<F64>((stopped==std::chrono::steady_clock::time_point{}?std::chrono::steady_clock::now():stopped)-started).count();
        result.maximumSubmissionGapMilliseconds=F64(maximumGapNanoseconds.load())/1000000.;
        result.peakAbsolute=peakAbsolute.load();result.output=output.Diagnostics();return result;
    }
};
Audio::Core::Engine::Engine():m_State(new State){}
Audio::Core::Engine::~Engine(){Shutdown();}
Audio::Core::Engine& Audio::Core::Engine::Instance(){static Engine engine;return engine;}
bool Audio::Core::Engine::Start(Renderer* renderer){
    auto& s=*m_State;
    if(!renderer)return s.Fail("Native audio requires an initialized renderer");
    if(!s.output.Open(s.rate,m_uChannels))return s.Fail(s.output.Error());
    s.renderer=renderer;s.stop=false;s.submitted=0;
    s.diagnostics=s.output.Diagnostics().enabled;s.started=std::chrono::steady_clock::now();s.stopped={};
    s.producerNotReady=0;s.emptyQueue=0;s.backpressure=0;s.clipped=0;s.nonfinite=0;s.maximumGapNanoseconds=0;s.peakAbsolute=0;
    s.maximumQueueTarget=0;s.maximumPadding=0;
    if(!s.output.Resume()){s.output.Close();s.renderer=nullptr;s.stop=true;return s.Fail(s.output.Error());}
    s.running=true;
    try{s.writer=std::thread(&Engine::WritePCM,this);return true;}
    catch(const std::system_error& error){s.running=false;s.output.Close();s.renderer=nullptr;s.stop=true;return s.Fail(error.what());}
}
bool Audio::Core::Engine::Init(Renderer* renderer){
    auto& s=*m_State;std::lock_guard<std::mutex> guard(s.lifecycle);
    if(s.writer.joinable())return s.Fail("Native audio output already has a renderer owner");
    ResetOutputSampleRate();Audio::Core::SetOutputSampleRate(s.rate);
    return Start(renderer);
}
void Audio::Core::Engine::Stop(){
    auto& s=*m_State;const bool hadWriter=s.writer.joinable();s.stop=true;
    if(s.writer.joinable())s.writer.join();
    if(hadWriter)s.stopped=std::chrono::steady_clock::now();
    // [Blur2PC] Optional summary only after the application writer has joined.
    // SDL's callback remains quiet; shortfall is approximate demand, not a
    // hardware-underrun measurement, and startup is included in these totals.
    if(hadWriter&&s.diagnostics){
        const auto d=s.Snapshot(m_uChannels);
        std::fprintf(stderr,"[BlurMacAudio] rate=%u channels=%u elapsed=%.3f submitted=%llu producer_not_ready=%llu queue_empty_observations=%llu backpressure_polls=%llu max_submit_gap_ms=%.3f clipped_samples=%llu nonfinite_samples=%llu peak_abs=%.6g device_rate=%u device_channels=%u device_period_frames=%u callbacks=%llu approximate_demand_shortfalls=%llu approximate_demand_bytes=%llu max_additional_bytes=%llu max_total_bytes=%llu max_queue_target_frames=%llu max_observed_padding_frames=%llu startup_included=1\n",
            d.sampleRate,d.channels,d.elapsedSeconds,(unsigned long long)d.submittedFrames,(unsigned long long)d.producerNotReady,
            (unsigned long long)d.emptyQueueObservations,(unsigned long long)d.backpressurePolls,d.maximumSubmissionGapMilliseconds,
            (unsigned long long)d.clippedSamples,(unsigned long long)d.nonfiniteSamples,double(d.peakAbsolute),
            d.output.deviceRate,d.output.deviceChannels,d.output.devicePeriodFrames,(unsigned long long)d.output.callbacks,
            (unsigned long long)d.output.demandShortfallCallbacks,(unsigned long long)d.output.demandShortfallBytes,
            (unsigned long long)d.output.maximumAdditionalBytes,(unsigned long long)d.output.maximumTotalBytes,
            (unsigned long long)d.maximumQueueTargetFrames,(unsigned long long)d.maximumObservedPaddingFrames);
    }
    s.running=false;s.output.Close();s.renderer=nullptr;
}
bool Audio::Core::Engine::Shutdown(Renderer* expected){
    auto& s=*m_State;std::lock_guard<std::mutex> guard(s.lifecycle);
    if(expected&&s.renderer!=expected)return false;
    Stop();return true;
}
bool Audio::Core::Engine::SetOutputSampleRate(U32 rate){
    auto& s=*m_State;std::lock_guard<std::mutex> guard(s.lifecycle);
    if(rate!=24000&&rate!=36000&&rate!=48000)return s.Fail("Native audio quality must be 24000, 36000 or 48000 Hz");
    if(rate==s.rate)return true;
    Renderer* renderer=s.renderer;Stop();s.rate=rate;Audio::Core::SetOutputSampleRate(rate);
    return renderer?Start(renderer):true;
}
bool Audio::Core::Engine::ConfigureChannels(U32 channels){
    auto& s=*m_State;std::lock_guard<std::mutex> guard(s.lifecycle);
    if(s.writer.joinable())return s.Fail("Cannot change a live native audio channel layout");
    if(channels!=2&&channels!=6&&channels!=8)return s.Fail("Native output supports stereo, 5.1 and 7.1 input");
    m_uChannels=channels;return true;
}
void Audio::Core::Engine::WritePCM(){
    auto& s=*m_State;
    try{
        std::vector<F32> frame(m_uChannels*uFrameLength);
        auto lastSubmission=std::chrono::steady_clock::time_point{};
        // [Blur2PC] A hardware callback can consume more than one 60-Hz tick.
        // Keep one current device period (converted to input frames), or the
        // original tick target if larger, plus at most one mixer quantum.
        // Query the logical device each iteration so default-device format/
        // period changes do not retain a stale queue target. SDL owns resampling.
        while(!s.stop){
            Audio::Mac::PlaybackQueueStatus queue;
            if(!s.output.GetQueueStatus(s.rate/60,queue))throw std::runtime_error(s.output.Error());
            // [Blur2PC] CreateRenderer (core.cpp) owns eight PCM blocks. Bound
            // additional retained resampler input by one existing producer ring,
            // not the device period itself; unsupported conversion fails instead
            // of growing a moving queue target without limit.
            if(queue.paddingInputFrames>U64(8)*uFrameLength)
                throw std::runtime_error("Native SDL resampler retained more input than the eight-buffer renderer-ring budget");
            U64 targetMaximum=s.maximumQueueTarget.load(std::memory_order_relaxed);
            while(targetMaximum<queue.targetInputFrames&&!s.maximumQueueTarget.compare_exchange_weak(targetMaximum,queue.targetInputFrames,std::memory_order_relaxed)){}
            U64 paddingMaximum=s.maximumPadding.load(std::memory_order_relaxed);
            while(paddingMaximum<queue.paddingInputFrames&&!s.maximumPadding.compare_exchange_weak(paddingMaximum,queue.paddingInputFrames,std::memory_order_relaxed)){}
            const U64 queued=queue.queuedInputFrames;
            if(s.diagnostics&&queued==0)++s.emptyQueue;
            if(queued>queue.targetInputFrames&&queue.periodReady){if(s.diagnostics)++s.backpressure;MT::Thread::Sleep(1);continue;}
            F32 *surroundLFE=nullptr,*surround=nullptr,*stereo=nullptr,*mono=nullptr;
            if(!s.renderer->GetNextFrame(&surroundLFE,&surround,&stereo,&mono)){if(s.diagnostics)++s.producerNotReady;MT::Thread::Sleep(1);continue;}
            std::fill(frame.begin(),frame.end(),0.f);
            if(!s.renderer->IsOutputMuted()){
                if(mono)MixMono(mono,frame.data(),m_uChannels);
                if(stereo)MixStereo(stereo,frame.data(),m_uChannels);
                if(surround)MixStereo(surround,frame.data(),m_uChannels);
                if(surroundLFE)MixSurroundLFE(surroundLFE,frame.data(),m_uChannels);
                for(auto& value:frame){
                    if(s.diagnostics){
                        if(!std::isfinite(value))++s.nonfinite;
                        else{
                            const F32 absolute=std::abs(value);if(absolute>1.f)++s.clipped;
                            F32 peak=s.peakAbsolute.load(std::memory_order_relaxed);
                            while(peak<absolute&&!s.peakAbsolute.compare_exchange_weak(peak,absolute,std::memory_order_relaxed)){}
                        }
                    }
                    value=(value<1.f)?((value< -1.f)?-1.f:value):1.f;
                }
            }
            if(!s.output.Queue(frame.data(),uFrameLength))throw std::runtime_error(s.output.Error());
            s.submitted+=uFrameLength;
            if(s.diagnostics){
                const auto now=std::chrono::steady_clock::now();
                if(lastSubmission!=std::chrono::steady_clock::time_point{}){
                    const U64 gap=U64(std::chrono::duration_cast<std::chrono::nanoseconds>(now-lastSubmission).count());
                    U64 maximum=s.maximumGapNanoseconds.load(std::memory_order_relaxed);
                    while(maximum<gap&&!s.maximumGapNanoseconds.compare_exchange_weak(maximum,gap,std::memory_order_relaxed)){}
                }
                lastSubmission=now;
            }
        }
    }catch(const std::exception& error){s.Fail(error.what());s.stop=true;std::fprintf(stderr,"[BlurMacAudio] output stopped: %s\n",error.what());}
    s.running=false;
}
U64 Audio::Core::Engine::SubmittedFrames()const{return m_State->submitted;}
S64 Audio::Core::Engine::QueuedFrames()const{return m_State->output.QueuedFrames();}
bool Audio::Core::Engine::IsRunning()const{return m_State->running;}
std::string Audio::Core::Engine::Error()const{std::lock_guard<std::mutex> guard(m_State->errors);return m_State->error;}
Audio::Core::EngineDiagnostics Audio::Core::Engine::Diagnostics()const{std::lock_guard<std::mutex> guard(m_State->lifecycle);return m_State->Snapshot(m_uChannels);}
#include "../OutputMix.inl"
