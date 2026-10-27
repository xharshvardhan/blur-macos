// [Blur2PC][macOS] SDL owns hardware output, buffering, rate conversion and channel conversion.
#include <audio/macos/SDLPlayback.h>
#include <SDL3/SDL.h>
#include <limits>
#include <mutex>
namespace Audio::Mac {
struct Playback::State {
    mutable std::mutex mutex;SDL_AudioStream* stream=nullptr;bool ownsAudioRef=false;
    U32 rate=0,channels=0;std::string backend,error;
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
    s.rate=rate;s.channels=channels;const char* backend=SDL_GetCurrentAudioDriver();s.backend=backend?backend:"";s.error.clear();return true;
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
std::string Playback::Backend()const{auto& s=*m_State;std::lock_guard<std::mutex> guard(s.mutex);return s.backend;}
std::string Playback::Error()const{auto& s=*m_State;std::lock_guard<std::mutex> guard(s.mutex);return s.error;}
}
