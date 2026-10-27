// [Blur2PC][macOS] SDL owns hardware output, buffering, rate conversion and channel conversion.
#include <audio/macos/SDLPlayback.h>
#include <SDL3/SDL.h>
#include <limits>
#include <mutex>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <algorithm>
namespace Audio::Mac {
struct Playback::State {
    static_assert(std::atomic<U64>::is_always_lock_free,"Audio callback diagnostics require lock-free counters");
    mutable std::mutex mutex;SDL_AudioStream* stream=nullptr;bool ownsAudioRef=false;
    U32 rate=0,channels=0;std::string backend,error;
    bool diagnostics=false;
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
    void Close(){if(stream)SDL_DestroyAudioStream(stream);stream=nullptr;rate=channels=0;backend.clear();if(ownsAudioRef)SDL_QuitSubSystem(SDL_INIT_AUDIO);ownsAudioRef=false;}
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
    s.deviceRate=0;s.deviceChannels=0;s.devicePeriodFrames=0;
    if(!s.UpdateDevicePeriod()){s.Close();return false;}
    if(s.diagnostics){
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
bool Playback::Resume(){auto& s=*m_State;std::lock_guard<std::mutex> guard(s.mutex);if(!s.stream)return s.Fail("Native PCM output is closed");return SDL_ResumeAudioStreamDevice(s.stream)||s.Fail(SDL_GetError());}
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
