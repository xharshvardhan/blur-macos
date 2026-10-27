// [Blur2PC][macOS] SDL owns hardware output, buffering, rate conversion and channel conversion.
#include <audio/macos/SDLPlayback.h>
#include <SDL3/SDL.h>
#include <limits>
#include <mutex>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
namespace Audio::Mac {
struct Playback::State {
    static_assert(std::atomic<U64>::is_always_lock_free,"Audio callback diagnostics require lock-free counters");
    mutable std::mutex mutex;SDL_AudioStream* stream=nullptr;bool ownsAudioRef=false;
    U32 rate=0,channels=0;std::string backend,error;
    bool diagnostics=false;
    // [Blur2PC] Allocate before Resume; immutable slots are never reused. Both
    // producers reserve a unique slot, publish last, and never wait for a reader.
    static constexpr U64 TraceCapacity=8192;
    struct TraceEvent {
        std::atomic<U64> committed{0};
        U64 steadyNS=0,kind=0,detail=0,additional=0,total=0,phase=0,phaseNS=0;
        U64 queueNS=0,queued=0,available=0,target=0,period=0,submitted=0,notReady=0,backpressure=0;
    };
    std::unique_ptr<TraceEvent[]> trace;
    std::string tracePath;
    U64 traceSession=0;
    std::atomic<U64> traceCallbacks{0},traceCount{0},traceOverflow{0},phase{0},phaseNS{0},queueNS{0};
    std::atomic<U64> queued{0},available{0},target{0},period{0},submitted{0},notReady{0},backpressure{0};
    static U64 Now(){return U64(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
    void Record(U64 kind,U64 detail=0,U64 additional=0,U64 total=0){
        if(!trace)return;
        const U64 index=traceCount.fetch_add(1,std::memory_order_relaxed);
        if(index>=TraceCapacity){traceOverflow.fetch_add(1,std::memory_order_relaxed);return;}
        auto& e=trace[index];e.kind=kind;e.detail=detail;e.additional=additional;e.total=total;
        e.phase=phase.load(std::memory_order_relaxed);e.phaseNS=phaseNS.load(std::memory_order_relaxed);
        e.queueNS=queueNS.load(std::memory_order_relaxed);e.queued=queued.load(std::memory_order_relaxed);
        e.available=available.load(std::memory_order_relaxed);e.target=target.load(std::memory_order_relaxed);e.period=period.load(std::memory_order_relaxed);
        e.submitted=submitted.load(std::memory_order_relaxed);e.notReady=notReady.load(std::memory_order_relaxed);e.backpressure=backpressure.load(std::memory_order_relaxed);
        e.steadyNS=Now();e.committed.store(1,std::memory_order_release);
    }
    void FlushTrace(){
        if(!trace)return;
        const int fd=::open(tracePath.c_str(),O_WRONLY|O_CREAT|O_APPEND,0600);
        bool good=fd>=0;
        auto line=[&](const char* bytes,int length){
            if(!good||length<=0)return;
            int offset=0;while(offset<length){const auto n=::write(fd,bytes+offset,size_t(length-offset));if(n<0&&errno==EINTR)continue;if(n<=0){good=false;break;}offset+=int(n);}
        };
        char buffer[1536];
        int n=std::snprintf(buffer,sizeof(buffer),"{\"type\":\"audio_trace_session\",\"session\":%llu,\"steady_ns\":%llu,\"rate\":%u,\"channels\":%u,\"device_rate\":%u,\"device_period_frames\":%u,\"capacity\":%llu,\"reserved\":%llu,\"overflow\":%llu,\"callback_demand_approximate\":true,\"writer_snapshot_approximate\":true}\n",(unsigned long long)traceSession,(unsigned long long)Now(),rate,channels,deviceRate,devicePeriodFrames,(unsigned long long)TraceCapacity,(unsigned long long)traceCount.load(),(unsigned long long)traceOverflow.load());line(buffer,n);
        const U64 count=std::min(traceCount.load(),TraceCapacity);
        for(U64 i=0;i<count;++i){
            const auto& e=trace[i];if(!e.committed.load(std::memory_order_acquire))continue;
            n=std::snprintf(buffer,sizeof(buffer),"{\"type\":\"audio_event\",\"session\":%llu,\"sequence\":%llu,\"steady_ns\":%llu,\"kind\":%llu,\"detail_ns\":%llu,\"additional_bytes\":%llu,\"total_bytes\":%llu,\"producer_phase\":%llu,\"phase_ns\":%llu,\"queue_sample_ns\":%llu,\"queued_input_frames\":%llu,\"available_output_frames\":%llu,\"target_input_frames\":%llu,\"period_output_frames\":%llu,\"submitted_frames\":%llu,\"producer_not_ready\":%llu,\"backpressure_polls\":%llu}\n",(unsigned long long)traceSession,(unsigned long long)i,(unsigned long long)e.steadyNS,(unsigned long long)e.kind,(unsigned long long)e.detail,(unsigned long long)e.additional,(unsigned long long)e.total,(unsigned long long)e.phase,(unsigned long long)e.phaseNS,(unsigned long long)e.queueNS,(unsigned long long)e.queued,(unsigned long long)e.available,(unsigned long long)e.target,(unsigned long long)e.period,(unsigned long long)e.submitted,(unsigned long long)e.notReady,(unsigned long long)e.backpressure);line(buffer,n);
        }
        if(fd>=0&&::close(fd)!=0)good=false;
        if(!good)std::fprintf(stderr,"[BlurMacAudio] trace write failed: %s\n",tracePath.c_str());
        trace.reset();tracePath.clear();
    }
    U32 deviceRate=0,deviceChannels=0,devicePeriodFrames=0;
    std::atomic<U64> callbacks{0},shortfalls{0},shortfallBytes{0},maxAdditional{0},maxTotal{0};
    static void Maximum(std::atomic<U64>& value,U64 candidate){
        U64 previous=value.load(std::memory_order_relaxed);
        while(previous<candidate&&!value.compare_exchange_weak(previous,candidate,std::memory_order_relaxed)){}
    }
    // [Blur2PC] Runs under SDL's audio lock: no application lock, allocation,
    // logging or PCM generation. Observe the documented approximate demand.
    static void SDLCALL Demand(void* context,SDL_AudioStream*,int additional,int total){
        auto& s=*static_cast<State*>(context);
        if(s.trace){const U64 callback=s.traceCallbacks.fetch_add(1,std::memory_order_relaxed);if(additional>0||callback==0)s.Record(callback==0?3:4,0,additional>0?U64(additional):0,total>0?U64(total):0);}
        if(!s.diagnostics)return;
        s.callbacks.fetch_add(1,std::memory_order_relaxed);
        if(additional>0){s.shortfalls.fetch_add(1,std::memory_order_relaxed);s.shortfallBytes.fetch_add(U64(additional),std::memory_order_relaxed);Maximum(s.maxAdditional,U64(additional));}
        if(total>0)Maximum(s.maxTotal,U64(total));
    }
    U32 UpdateDevicePeriod(){
        SDL_AudioSpec actual{};int frames=0;
        if(!stream||!SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(stream),&actual,&frames)){
            Fail(stream?SDL_GetError():"Native PCM output is closed");return 0;
        }
        if(actual.freq<=0||actual.channels<=0||frames<=0){Fail("Native output returned an invalid device period/format");return 0;}
        const U64 converted=(U64(frames)*rate+U32(actual.freq)-1)/U32(actual.freq);
        if(!converted||converted>std::numeric_limits<U32>::max()){Fail("Native output device period exceeds the input frame range");return 0;}
        deviceRate=U32(actual.freq);deviceChannels=U32(actual.channels);devicePeriodFrames=U32(frames);
        return U32(converted);
    }
    bool Fail(const char* text){error=text;return false;}
    void Close(){if(stream){SDL_SetAudioStreamGetCallback(stream,nullptr,nullptr);SDL_DestroyAudioStream(stream);}stream=nullptr;FlushTrace();rate=channels=0;backend.clear();if(ownsAudioRef)SDL_QuitSubSystem(SDL_INIT_AUDIO);ownsAudioRef=false;}
};
Playback::Playback():m_State(std::make_unique<State>()){}
Playback::~Playback(){Close();}
bool Playback::Open(U32 rate,U32 channels){
    auto& s=*m_State;std::lock_guard<std::mutex> guard(s.mutex);
    if(s.stream)return s.Fail("Native PCM output is already open");
    if(!rate||rate>U32(std::numeric_limits<int>::max())||(channels!=1&&channels!=2&&channels!=4&&channels!=6&&channels!=8))return s.Fail("Unsupported native PCM sample rate or channel layout");
    if(!SDL_InitSubSystem(SDL_INIT_AUDIO))return s.Fail(SDL_GetError());s.ownsAudioRef=true;
    SDL_AudioSpec spec{};spec.format=SDL_AUDIO_F32;spec.channels=int(channels);spec.freq=int(rate);
    s.stream=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,nullptr,nullptr);
    if(!s.stream){s.error=SDL_GetError();s.Close();return false;}
    // [Blur2PC] Automated runs can mute only this stream before it resumes.
    // Mixer/decoder/device timing remain active; system volume is untouched.
    const char* muteFlag=std::getenv("BLUR_MAC_AUDIO_MUTE");
    if(muteFlag&&std::strcmp(muteFlag,"1")==0&&!SDL_SetAudioStreamGain(s.stream,0.f)){
        s.error=SDL_GetError();s.Close();return false;
    }
    s.rate=rate;s.channels=channels;const char* backend=SDL_GetCurrentAudioDriver();s.backend=backend?backend:"";s.error.clear();
    const char* diagnosticFlag=std::getenv("BLUR_MAC_AUDIO_DIAGNOSTICS");
    s.diagnostics=diagnosticFlag&&std::strcmp(diagnosticFlag,"1")==0;
    s.callbacks=0;s.shortfalls=0;s.shortfallBytes=0;s.maxAdditional=0;s.maxTotal=0;
    const char* traceFile=std::getenv("BLUR_MAC_AUDIO_TRACE_FILE");
    if(traceFile&&*traceFile){
        if(*traceFile!='/'){s.error="BLUR_MAC_AUDIO_TRACE_FILE must be absolute";s.Close();return false;}
        try{s.tracePath=traceFile;s.trace=std::make_unique<State::TraceEvent[]>(State::TraceCapacity);}
        catch(...){s.Close();throw;}
        s.traceSession=State::Now();s.traceCallbacks=0;s.traceCount=0;s.traceOverflow=0;s.phase=0;s.phaseNS=0;s.queueNS=0;
        s.queued=0;s.available=0;s.target=0;s.period=0;s.submitted=0;s.notReady=0;s.backpressure=0;
        s.Record(1);
    }
    s.deviceRate=0;s.deviceChannels=0;s.devicePeriodFrames=0;
    if(!s.UpdateDevicePeriod()){s.Close();return false;}
    if(s.diagnostics||s.trace){
        SDL_SetAudioStreamGetCallback(s.stream,&State::Demand,&s);
    }
    return true;
}
bool Playback::Queue(const F32* samples,U32 frames){
    auto& s=*m_State;std::lock_guard<std::mutex> guard(s.mutex);
    if(!s.stream)return s.Fail("Native PCM output is closed");const U64 bytes=U64(frames)*s.channels*sizeof(F32);
    if((frames&&!samples)||bytes>U64(std::numeric_limits<int>::max()))return s.Fail("Invalid native PCM block");
    if(!SDL_PutAudioStreamData(s.stream,samples,int(bytes)))return s.Fail(SDL_GetError());return true;
}
bool Playback::Resume(){auto& s=*m_State;std::lock_guard<std::mutex> guard(s.mutex);if(!s.stream)return s.Fail("Native PCM output is closed");s.Record(2);return SDL_ResumeAudioStreamDevice(s.stream)||s.Fail(SDL_GetError());}
bool Playback::Pause(){auto& s=*m_State;std::lock_guard<std::mutex> guard(s.mutex);if(!s.stream)return s.Fail("Native PCM output is closed");return SDL_PauseAudioStreamDevice(s.stream)||s.Fail(SDL_GetError());}
bool Playback::Clear(){auto& s=*m_State;std::lock_guard<std::mutex> guard(s.mutex);if(!s.stream)return s.Fail("Native PCM output is closed");return SDL_ClearAudioStream(s.stream)||s.Fail(SDL_GetError());}
void Playback::Close(){auto& s=*m_State;std::lock_guard<std::mutex> guard(s.mutex);s.Close();}
bool Playback::IsOpen()const{auto& s=*m_State;std::lock_guard<std::mutex> guard(s.mutex);return s.stream!=nullptr;}
S64 Playback::QueuedFrames()const{auto& s=*m_State;std::lock_guard<std::mutex> guard(s.mutex);if(!s.stream)return -1;const int bytes=SDL_GetAudioStreamQueued(s.stream);return bytes<0?-1:S64(bytes)/(s.channels*sizeof(F32));}
U32 Playback::SampleRate()const{auto& s=*m_State;std::lock_guard<std::mutex> guard(s.mutex);return s.rate;}
U32 Playback::Channels()const{auto& s=*m_State;std::lock_guard<std::mutex> guard(s.mutex);return s.channels;}
U32 Playback::DevicePeriodInputFrames()const{auto& s=*m_State;std::lock_guard<std::mutex> guard(s.mutex);return s.UpdateDevicePeriod();}
bool Playback::GetQueueStatus(U32 cadenceFrames,PlaybackQueueStatus& status)const{
    auto& s=*m_State;std::lock_guard<std::mutex> guard(s.mutex);
    // [Blur2PC] Query the device before locking the stream: SDL's device mixer
    // can hold its device lock while obtaining stream data in the other order.
    if(!s.UpdateDevicePeriod())return false;
    if(!SDL_LockAudioStream(s.stream))return s.Fail(SDL_GetError());
    struct Unlock {SDL_AudioStream* stream;~Unlock(){SDL_UnlockAudioStream(stream);}} unlock{s.stream};
    // SDL stream locks are recursive; obtain one consistent queue/usable-output
    // snapshot. No application lock is ever acquired by the demand callback.
    SDL_AudioSpec input{},output{};
    if(!SDL_GetAudioStreamFormat(s.stream,&input,&output))return s.Fail(SDL_GetError());
    if(input.freq!=int(s.rate)||input.channels!=int(s.channels)||input.format!=SDL_AUDIO_F32||output.freq<=0||output.channels<=0||!SDL_AUDIO_FRAMESIZE(output))
        return s.Fail("Native PCM stream returned an invalid or externally changed format");
    const int queued=SDL_GetAudioStreamQueued(s.stream),available=SDL_GetAudioStreamAvailable(s.stream);
    if(queued<0||available<0)return s.Fail(SDL_GetError());
    PlaybackQueueStatus next;
    next.queuedInputFrames=U64(queued)/SDL_AUDIO_FRAMESIZE(input);
    next.availableOutputFrames=U64(available)/SDL_AUDIO_FRAMESIZE(output);
    next.periodOutputFrames=(U64(s.devicePeriodFrames)*U32(output.freq)+s.deviceRate-1)/s.deviceRate;
    if(!next.periodOutputFrames||next.periodOutputFrames>std::numeric_limits<U32>::max())return s.Fail("Native output period exceeds the stream frame range");
    const U64 periodInput=(next.periodOutputFrames*s.rate+U32(output.freq)-1)/U32(output.freq);
    const U64 visibleInput=(next.availableOutputFrames*s.rate+U32(output.freq)-1)/U32(output.freq);
    // SDL reports usable output after accounting for its current resampler
    // history/padding. Measure that retained input instead of guessing a filter
    // constant; strict >target retains the bounded mixer-quantum headroom.
    next.paddingInputFrames=next.queuedInputFrames>visibleInput?next.queuedInputFrames-visibleInput:0;
    const U64 target=std::max<U64>(cadenceFrames,periodInput+next.paddingInputFrames);
    if(!target||target>std::numeric_limits<U32>::max())return s.Fail("Native output queue target exceeds the input frame range");
    next.periodNanoseconds=(U64(s.devicePeriodFrames)*1000000000ull+s.deviceRate-1)/s.deviceRate;
    next.targetInputFrames=U32(target);next.periodReady=next.availableOutputFrames>=next.periodOutputFrames;
    status=next;return true;
}
std::string Playback::Backend()const{auto& s=*m_State;std::lock_guard<std::mutex> guard(s.mutex);return s.backend;}
std::string Playback::Error()const{auto& s=*m_State;std::lock_guard<std::mutex> guard(s.mutex);return s.error;}
PlaybackDiagnostics Playback::Diagnostics()const{
    auto& s=*m_State;std::lock_guard<std::mutex> guard(s.mutex);
    return {s.diagnostics,s.deviceRate,s.deviceChannels,s.devicePeriodFrames,
        s.callbacks.load(std::memory_order_relaxed),s.shortfalls.load(std::memory_order_relaxed),
        s.shortfallBytes.load(std::memory_order_relaxed),s.maxAdditional.load(std::memory_order_relaxed),s.maxTotal.load(std::memory_order_relaxed)};
}
}

// [Blur2PC] No callback touches the application mutex; these atomic fields are
// approximate observations, never synchronization for playback decisions.
namespace Audio::Mac {
bool Playback::TraceEnabled()const{return bool(m_State->trace);}
void Playback::ObserveProducer(ProducerPhase phase,const PlaybackQueueStatus* queue,U64 submitted,U64 notReady,U64 backpressure,ProducerEvent event,U64 detail){
    auto& s=*m_State;if(!s.trace)return;
    s.phase.store(U64(phase),std::memory_order_relaxed);s.phaseNS.store(State::Now(),std::memory_order_relaxed);
    s.submitted.store(submitted,std::memory_order_relaxed);s.notReady.store(notReady,std::memory_order_relaxed);s.backpressure.store(backpressure,std::memory_order_relaxed);
    if(queue){s.queued=queue->queuedInputFrames;s.available=queue->availableOutputFrames;s.target=queue->targetInputFrames;s.period=queue->periodOutputFrames;s.queueNS=State::Now();}
    if(event!=ProducerEvent::None)s.Record(16+U64(event),detail);
}
}
