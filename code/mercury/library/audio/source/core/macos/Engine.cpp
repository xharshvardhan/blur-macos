// [Blur2PC] SDL owns CoreAudio, conversion and device timing; the game owns its mixer.
#include "AudioPch.h"
#include "Engine.h"
#include "DecoderTrace.h"
#include <audio/macos/SDLPlayback.h>
#include <atomic>
#include <algorithm>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <vector>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

// [Blur2PC] Decoder jobs belong to the renderer producer, not the SDL writer.
// Fixed process-lifetime storage avoids an enabled-pointer/refcount UAF during
// quality restarts. Begin/End are serialized by the actual renderer ownership;
// End runs only after producer and its original scheduler groups have joined.
namespace Audio { namespace Mac {
static_assert(std::atomic<std::uint64_t>::is_always_lock_free&&std::atomic<bool>::is_always_lock_free,"Native decoder observations require lock-free counters");
std::atomic<bool> g_DecoderTraceEnabled{false};
namespace {
constexpr std::uint64_t DecoderTraceCapacity=8192;
struct DecoderTraceSlot {
    std::atomic<bool> committed{false};
    std::uint64_t steadyNS=0,ordinal=0;
    DecoderZeroFill data{};
};
struct DecoderTraceState {
    std::atomic<const void*> owner{nullptr};
    std::atomic<std::uint64_t> attempts[2]{},streamed[2]{},success[2]{},zeros[2]{};
    std::atomic<std::uint64_t> reasons[3]{},zeroCount{0},reserved{0},overflow{0};
    DecoderTraceSlot slots[DecoderTraceCapacity];
    char path[4096]{};
    std::uint64_t session=0;
};
DecoderTraceState g_DecoderTrace;
std::uint64_t DecoderSteadyNS(){return std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
}
bool BeginDecoderTrace(const void* owner){
    if(g_DecoderTrace.owner.load(std::memory_order_acquire)){
        std::fprintf(stderr,"[BlurMacAudio] Decoder trace already has a live renderer owner\n");return false;
    }
    const char* path=std::getenv("BLUR_MAC_AUDIO_DECODER_TRACE_FILE");
    if(!path||!*path)return true;
    if(!owner||*path!='/'||std::strlen(path)>=sizeof(g_DecoderTrace.path)){
        std::fprintf(stderr,"[BlurMacAudio] Decoder trace requires an owner and absolute path shorter than 4096 bytes\n");return false;
    }
    const void* empty=nullptr;
    if(!g_DecoderTrace.owner.compare_exchange_strong(empty,owner)){
        std::fprintf(stderr,"[BlurMacAudio] Decoder trace already has a live renderer owner\n");return false;
    }
    const int fd=::open(path,O_WRONLY|O_CREAT|O_APPEND,0600);
    if(fd<0){std::fprintf(stderr,"[BlurMacAudio] Decoder trace open failed: %s\n",std::strerror(errno));g_DecoderTrace.owner=nullptr;return false;}
    ::close(fd);
    auto& s=g_DecoderTrace;std::strcpy(s.path,path);s.session=DecoderSteadyNS();
    for(unsigned i=0;i<2;++i){s.attempts[i]=0;s.streamed[i]=0;s.success[i]=0;s.zeros[i]=0;}
    for(auto& reason:s.reasons)reason=0;
    s.zeroCount=0;s.reserved=0;s.overflow=0;
    for(auto& slot:s.slots)slot.committed.store(false,std::memory_order_relaxed);
    g_DecoderTraceEnabled.store(true,std::memory_order_release);return true;
}
void CountDecoderAttempt(std::uint32_t channels,bool streamed){
    const unsigned i=channels==2;g_DecoderTrace.attempts[i].fetch_add(1,std::memory_order_relaxed);
    if(streamed)g_DecoderTrace.streamed[i].fetch_add(1,std::memory_order_relaxed);
}
void CountDecoderSuccess(std::uint32_t channels){g_DecoderTrace.success[channels==2].fetch_add(1,std::memory_order_relaxed);}
void RecordDecoderZeroFill(const DecoderZeroFill& event){
    auto& s=g_DecoderTrace;s.zeros[event.channels==2].fetch_add(1,std::memory_order_relaxed);
    for(unsigned i=0;i<3;++i)if(event.reason&(1u<<i))s.reasons[i].fetch_add(1,std::memory_order_relaxed);
    const auto ordinal=s.zeroCount.fetch_add(1,std::memory_order_relaxed);
    // Retain all first 64 decisions, then every 64th. Counts remain unsampled.
    if(ordinal>=64&&ordinal%64)return;
    const auto index=s.reserved.fetch_add(1,std::memory_order_relaxed);
    if(index>=DecoderTraceCapacity){s.overflow.fetch_add(1,std::memory_order_relaxed);return;}
    auto& slot=s.slots[index];slot.steadyNS=DecoderSteadyNS();slot.ordinal=ordinal;slot.data=event;
    slot.committed.store(true,std::memory_order_release);
}
void EndDecoderTrace(const void* owner){
    auto& s=g_DecoderTrace;if(s.owner.load(std::memory_order_acquire)!=owner||!owner)return;
    g_DecoderTraceEnabled.store(false,std::memory_order_release);
    // Actual owner joined all record writers before this point; no reset races.
    const int fd=::open(s.path,O_WRONLY|O_CREAT|O_APPEND,0600);bool good=fd>=0;
    auto line=[&](const char* bytes,int length){if(length<0||length>=2048){good=false;return;}for(int offset=0;good&&offset<length;){const auto n=::write(fd,bytes+offset,size_t(length-offset));if(n<0&&errno==EINTR)continue;if(n<=0){good=false;break;}offset+=int(n);}};
    char buffer[2048];const auto reserved=s.reserved.load();const auto retained=std::min(reserved,DecoderTraceCapacity);
    std::uint64_t first=0,last=0,committed=0;
    for(std::uint64_t i=0;i<retained;++i)if(s.slots[i].committed.load(std::memory_order_acquire)){const auto t=s.slots[i].steadyNS;if(!first||t<first)first=t;if(t>last)last=t;++committed;}
    int n=std::snprintf(buffer,sizeof(buffer),"{\"type\":\"audio_decoder_session\",\"session\":%llu,\"steady_ns\":%llu,\"capacity\":%llu,\"zero_fill_count\":%llu,\"sampled_reservations\":%llu,\"retained\":%llu,\"overflow\":%llu,\"first_retained_ns\":%llu,\"last_retained_ns\":%llu,\"mono_attempts\":%llu,\"mono_streamed_attempts\":%llu,\"mono_successful_decode\":%llu,\"mono_zero_fill\":%llu,\"stereo_attempts\":%llu,\"stereo_streamed_attempts\":%llu,\"stereo_successful_decode\":%llu,\"stereo_zero_fill\":%llu,\"unprimed\":%llu,\"no_data\":%llu,\"insufficient_bytes\":%llu,\"sampling_first\":64,\"sampling_stride\":64}\n",
        (unsigned long long)s.session,(unsigned long long)DecoderSteadyNS(),(unsigned long long)DecoderTraceCapacity,(unsigned long long)s.zeroCount.load(),(unsigned long long)reserved,(unsigned long long)committed,(unsigned long long)s.overflow.load(),(unsigned long long)first,(unsigned long long)last,
        (unsigned long long)s.attempts[0].load(),(unsigned long long)s.streamed[0].load(),(unsigned long long)s.success[0].load(),(unsigned long long)s.zeros[0].load(),(unsigned long long)s.attempts[1].load(),(unsigned long long)s.streamed[1].load(),(unsigned long long)s.success[1].load(),(unsigned long long)s.zeros[1].load(),(unsigned long long)s.reasons[0].load(),(unsigned long long)s.reasons[1].load(),(unsigned long long)s.reasons[2].load());line(buffer,n);
    for(std::uint64_t i=0;i<retained;++i){const auto& slot=s.slots[i];if(!slot.committed.load(std::memory_order_acquire))continue;const auto& e=slot.data;
        n=std::snprintf(buffer,sizeof(buffer),"{\"type\":\"audio_decoder_zero_fill\",\"session\":%llu,\"steady_ns\":%llu,\"ordinal\":%llu,\"channels\":%u,\"stream_index\":%u,\"stream_handle\":%u,\"sample_counter\":%u,\"sample_index\":%u,\"loop_end\":%u,\"source_position\":%u,\"bytes_consumed\":%u,\"bytes_available\":%u,\"samples_required\":%u,\"bytes_required\":%u,\"reason\":%u,\"pitch\":%.9g,\"primed\":%s,\"playing\":%s,\"finished\":%s,\"loop\":%s}\n",
            (unsigned long long)s.session,(unsigned long long)slot.steadyNS,(unsigned long long)slot.ordinal,e.channels,e.streamIndex,e.streamHandle,e.sampleCounter,e.sampleIndex,e.loopEnd,e.sourcePosition,e.bytesConsumed,e.bytesAvailable,e.samplesRequired,e.bytesRequired,e.reason,double(e.pitch),e.primed?"true":"false",e.playing?"true":"false",e.finished?"true":"false",e.loop?"true":"false");line(buffer,n);
    }
    if(fd>=0)::close(fd);
    if(!good)std::fprintf(stderr,"[BlurMacAudio] Decoder trace flush failed\n");
    s.owner.store(nullptr,std::memory_order_release);
}
} }

struct Audio::Core::Engine::State {
    std::mutex lifecycle;
    mutable std::mutex errors;
    Audio::Mac::Playback output;
    std::thread writer;
    std::atomic<bool> stop{true},running{false};
    std::atomic<U64> submitted{0};
    Renderer* renderer=nullptr;
    U32 rate=48000;
    bool diagnostics=false,trace=false;
    std::chrono::steady_clock::time_point started;
    std::chrono::steady_clock::time_point stopped;
    std::atomic<U64> producerNotReady{0},emptyQueue{0},backpressure{0},clipped{0},nonfinite{0},maximumGapNanoseconds{0};
    std::atomic<F32> peakAbsolute{0};
    std::atomic<U64> maximumQueueTarget{0},maximumPadding{0};
    // [Blur2PC] Writer-only, once-per-second signal statistics. No PCM samples
    // or assets are written; fixed storage is allocated before output resumes.
    struct PCMStat {
        U64 steadyNS=0,submitted=0,nonfinite=0;
        F64 monoEnergy=0,stereoLeftEnergy=0,stereoRightEnergy=0,surroundLeftEnergy=0,surroundRightEnergy=0,surroundLFEEnergy=0;
        F64 stereoLegacyRightError=0,stereoActualLeftError=0,stereoActualRightError=0;
        F64 stereoRightBoundary=0,outputRightBoundary=0;
        bool stereoPresent=false,mixApplied=false;
    };
    static constexpr U64 PCMCapacity=512;
    std::unique_ptr<PCMStat[]> pcm;
    std::string pcmPath;
    U64 pcmCount=0,pcmOverflow=0,pcmNextNS=0,pcmSession=0;
    F32 pcmPreviousStereoRight=0,pcmPreviousOutputRight=0;
    bool pcmPreviousStereoValid=false,pcmPreviousOutputValid=false;
    static U64 SteadyNS(){return U64(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
    bool PreparePCM(){
        pcm.reset();pcmPath.clear();pcmCount=pcmOverflow=pcmNextNS=0;pcmPreviousStereoValid=pcmPreviousOutputValid=false;
        const char* path=std::getenv("BLUR_MAC_AUDIO_PCM_STATS_FILE");if(!path||!*path)return true;
        if(*path!='/')return Fail("BLUR_MAC_AUDIO_PCM_STATS_FILE must be absolute");
        pcmPath=path;pcm=std::make_unique<PCMStat[]>(PCMCapacity);pcmSession=SteadyNS();return true;
    }
    PCMStat* SamplePCM(const F32* mono,const F32* stereo,const F32* surround,const F32* surroundLFE,bool mixApplied){
        if(!pcm)return nullptr;
        PCMStat* result=nullptr;const U64 now=SteadyNS();
        if(now>=pcmNextNS){pcmNextNS=now+1000000000ull;
            if(pcmCount<PCMCapacity){
                auto& entry=pcm[pcmCount++];result=&entry;entry.steadyNS=now;entry.submitted=submitted.load(std::memory_order_relaxed);entry.stereoPresent=stereo!=nullptr;entry.mixApplied=mixApplied;
                auto energy=[&](const F32* samples,U32 count){F64 sum=0;if(samples)for(U32 i=0;i<count;++i){if(std::isfinite(samples[i]))sum+=F64(samples[i])*samples[i];else ++entry.nonfinite;}return sum/count;};
                entry.monoEnergy=energy(mono,uFrameLength);entry.stereoLeftEnergy=energy(stereo,uFrameLength);entry.stereoRightEnergy=energy(stereo?stereo+uFrameLength:nullptr,uFrameLength);
                entry.surroundLeftEnergy=energy(surround,uFrameLength);entry.surroundRightEnergy=energy(surround?surround+uFrameLength:nullptr,uFrameLength);entry.surroundLFEEnergy=energy(surroundLFE,6*uFrameLength);
                if(stereo){for(U32 i=0;i<uFrameLength;++i){const F64 difference=F64(stereo[i+1])-stereo[i+uFrameLength];if(std::isfinite(difference))entry.stereoLegacyRightError+=difference*difference/uFrameLength;}
                    if(pcmPreviousStereoValid&&std::isfinite(stereo[uFrameLength]))entry.stereoRightBoundary=F64(stereo[uFrameLength])-pcmPreviousStereoRight;}
            }else ++pcmOverflow;
        }
        if(stereo){pcmPreviousStereoRight=stereo[2*uFrameLength-1];pcmPreviousStereoValid=std::isfinite(pcmPreviousStereoRight);}else pcmPreviousStereoValid=false;
        return result;
    }
    void FlushPCM(){
        if(!pcm)return;
        const int fd=::open(pcmPath.c_str(),O_WRONLY|O_CREAT|O_APPEND,0600);bool good=fd>=0;
        auto writeLine=[&](const char* bytes,int length){if(!good)return;int offset=0;while(offset<length){const auto n=::write(fd,bytes+offset,size_t(length-offset));if(n<0&&errno==EINTR)continue;if(n<=0){good=false;break;}offset+=int(n);}};
        char line[2048];int n=std::snprintf(line,sizeof(line),"{\"type\":\"audio_pcm_session\",\"session\":%llu,\"steady_ns\":%llu,\"rate\":%u,\"channels\":%u,\"capacity\":%llu,\"samples\":%llu,\"overflow\":%llu,\"interval_ns\":1000000000,\"pcm_written\":false}\n",(unsigned long long)pcmSession,(unsigned long long)SteadyNS(),rate,pcmChannels,(unsigned long long)PCMCapacity,(unsigned long long)pcmCount,(unsigned long long)pcmOverflow);writeLine(line,n);
        for(U64 i=0;i<pcmCount;++i){const auto& e=pcm[i];n=std::snprintf(line,sizeof(line),"{\"type\":\"audio_pcm_stats\",\"session\":%llu,\"steady_ns\":%llu,\"submitted_frames\":%llu,\"mono_energy\":%.17g,\"stereo_left_energy\":%.17g,\"stereo_right_energy\":%.17g,\"surround_left_energy\":%.17g,\"surround_right_energy\":%.17g,\"surround_lfe_energy\":%.17g,\"legacy_stereo_right_mean_square_error\":%.17g,\"actual_stereo_left_mean_square_error\":%.17g,\"actual_stereo_right_mean_square_error\":%.17g,\"stereo_right_boundary_delta\":%.17g,\"output_right_boundary_delta\":%.17g,\"stereo_present\":%s,\"mix_applied\":%s,\"nonfinite_observations\":%llu}\n",(unsigned long long)pcmSession,(unsigned long long)e.steadyNS,(unsigned long long)e.submitted,e.monoEnergy,e.stereoLeftEnergy,e.stereoRightEnergy,e.surroundLeftEnergy,e.surroundRightEnergy,e.surroundLFEEnergy,e.stereoLegacyRightError,e.stereoActualLeftError,e.stereoActualRightError,e.stereoRightBoundary,e.outputRightBoundary,e.stereoPresent?"true":"false",e.mixApplied?"true":"false",(unsigned long long)e.nonfinite);writeLine(line,n);}
        if(fd>=0&&::close(fd)!=0)good=false;if(!good)std::fprintf(stderr,"[BlurMacAudio] PCM statistics write failed: %s\n",pcmPath.c_str());pcm.reset();pcmPath.clear();
    }
    U32 pcmChannels=0;
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
    if(!s.PreparePCM())return false;s.pcmChannels=m_uChannels;
    if(!s.output.Open(s.rate,m_uChannels))return s.Fail(s.output.Error());
    s.renderer=renderer;s.stop=false;s.submitted=0;
    s.diagnostics=s.output.Diagnostics().enabled;s.trace=s.output.TraceEnabled();s.started=std::chrono::steady_clock::now();s.stopped={};
    s.producerNotReady=0;s.emptyQueue=0;s.backpressure=0;s.clipped=0;s.nonfinite=0;s.maximumGapNanoseconds=0;s.peakAbsolute=0;
    s.maximumQueueTarget=0;s.maximumPadding=0;
    s.output.ObserveProducer(Audio::Mac::ProducerPhase::Idle,nullptr,0,0,0,Audio::Mac::ProducerEvent::Start);
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
    s.output.ObserveProducer(Audio::Mac::ProducerPhase::Stopped,nullptr,s.submitted.load(),s.producerNotReady.load(),s.backpressure.load(),Audio::Mac::ProducerEvent::Stop);
    s.running=false;s.output.Close();s.FlushPCM();s.renderer=nullptr;
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
        auto producerWaitStart=lastSubmission;bool producerStarved=false,primed=false;
        U64 observedNotReady=0,observedBackpressure=0;
        auto observe=[&](Audio::Mac::ProducerPhase phase,const Audio::Mac::PlaybackQueueStatus* queue=nullptr,Audio::Mac::ProducerEvent event=Audio::Mac::ProducerEvent::None,U64 detail=0){
            if(s.trace)s.output.ObserveProducer(phase,queue,s.submitted.load(std::memory_order_relaxed),observedNotReady,observedBackpressure,event,detail);
        };
        // [Blur2PC] A hardware callback can consume more than one 60-Hz tick.
        // Keep one current device period (converted to input frames), or the
        // original tick target if larger, plus at most one mixer quantum.
        // Query the logical device each iteration so default-device format/
        // period changes do not retain a stale queue target. SDL owns resampling.
        while(!s.stop){
            Audio::Mac::PlaybackQueueStatus queue;
            observe(Audio::Mac::ProducerPhase::QueueQuery);
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
            if(queued>queue.targetInputFrames&&queue.periodReady){if(s.trace&&!primed){primed=true;observe(Audio::Mac::ProducerPhase::BackpressureSleep,&queue,Audio::Mac::ProducerEvent::Primed);}if(s.diagnostics)++s.backpressure;++observedBackpressure;observe(Audio::Mac::ProducerPhase::BackpressureSleep,&queue);MT::Thread::Sleep(1);continue;}
            F32 *surroundLFE=nullptr,*surround=nullptr,*stereo=nullptr,*mono=nullptr;
            observe(Audio::Mac::ProducerPhase::AcquireFrame,&queue);
            if(!s.renderer->GetNextFrame(&surroundLFE,&surround,&stereo,&mono)){
                if(s.diagnostics)++s.producerNotReady;++observedNotReady;
                if(s.trace){const auto now=std::chrono::steady_clock::now();if(producerWaitStart==std::chrono::steady_clock::time_point{})producerWaitStart=now;
                    const U64 duration=U64(std::chrono::duration_cast<std::chrono::nanoseconds>(now-producerWaitStart).count());
                    if(!producerStarved&&duration>queue.periodNanoseconds){producerStarved=true;observe(Audio::Mac::ProducerPhase::ProducerSleep,&queue,Audio::Mac::ProducerEvent::ProducerStarved,duration);}}
                observe(Audio::Mac::ProducerPhase::ProducerSleep,&queue);MT::Thread::Sleep(1);continue;
            }
            if(s.trace&&producerWaitStart!=std::chrono::steady_clock::time_point{}){
                if(producerStarved)observe(Audio::Mac::ProducerPhase::Mix,&queue,Audio::Mac::ProducerEvent::ProducerRecovered,U64(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-producerWaitStart).count()));
                producerWaitStart={};producerStarved=false;
            }
            observe(Audio::Mac::ProducerPhase::Mix,&queue);
            std::fill(frame.begin(),frame.end(),0.f);
            const bool mixApplied=!s.renderer->IsOutputMuted();
            auto* pcm=s.SamplePCM(mono,stereo,surround,surroundLFE,mixApplied);
            if(mixApplied){
                if(mono)MixMono(mono,frame.data(),m_uChannels);
                if(stereo){
                    F32 before[2*uFrameLength];
                    if(pcm)for(U32 i=0;i<uFrameLength;++i){before[2*i]=frame[i*m_uChannels];before[2*i+1]=frame[i*m_uChannels+1];}
                    MixStereo(stereo,frame.data(),m_uChannels);
                    if(pcm)for(U32 i=0;i<uFrameLength;++i){const F64 left=F64(frame[i*m_uChannels]-before[2*i])-stereo[i],right=F64(frame[i*m_uChannels+1]-before[2*i+1])-stereo[i+uFrameLength];if(std::isfinite(left))pcm->stereoActualLeftError+=left*left/uFrameLength;else ++pcm->nonfinite;if(std::isfinite(right))pcm->stereoActualRightError+=right*right/uFrameLength;else ++pcm->nonfinite;}
                }
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
            if(s.pcm){if(pcm&&s.pcmPreviousOutputValid)pcm->outputRightBoundary=F64(frame[1])-s.pcmPreviousOutputRight;s.pcmPreviousOutputRight=frame[(uFrameLength-1)*m_uChannels+1];s.pcmPreviousOutputValid=true;}
            observe(Audio::Mac::ProducerPhase::Submit,&queue);
            if(!s.output.Queue(frame.data(),uFrameLength))throw std::runtime_error(s.output.Error());
            s.submitted+=uFrameLength;
            if(s.diagnostics||s.trace){
                const auto now=std::chrono::steady_clock::now();
                if(lastSubmission!=std::chrono::steady_clock::time_point{}){
                    const U64 gap=U64(std::chrono::duration_cast<std::chrono::nanoseconds>(now-lastSubmission).count());
                    if(s.diagnostics){U64 maximum=s.maximumGapNanoseconds.load(std::memory_order_relaxed);
                        while(maximum<gap&&!s.maximumGapNanoseconds.compare_exchange_weak(maximum,gap,std::memory_order_relaxed)){} }
                    if(s.trace&&gap>queue.periodNanoseconds)observe(Audio::Mac::ProducerPhase::Submit,&queue,Audio::Mac::ProducerEvent::SubmissionGap,gap);
                }else observe(Audio::Mac::ProducerPhase::Submit,&queue,Audio::Mac::ProducerEvent::FirstSubmission);
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
