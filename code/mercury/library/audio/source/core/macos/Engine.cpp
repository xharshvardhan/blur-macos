// [Blur2PC] SDL owns CoreAudio, conversion and device timing; the game owns its mixer.
#include "AudioPch.h"
#include "Engine.h"
#include <audio/macos/SDLPlayback.h>
#include <atomic>
#include <mutex>
#include <stdexcept>
#include <vector>

struct Audio::Core::Engine::State {
    std::mutex lifecycle;
    mutable std::mutex errors;
    Audio::Mac::Playback output;
    std::thread writer;
    std::atomic<bool> stop{true},running{false};
    std::atomic<U64> submitted{0};
    Renderer* renderer=nullptr;
    U32 rate=48000;
    std::string error;
    bool Fail(const std::string& value){std::lock_guard<std::mutex> guard(errors);error=value;return false;}
};
Audio::Core::Engine::Engine():m_State(new State){}
Audio::Core::Engine::~Engine(){Shutdown();}
Audio::Core::Engine& Audio::Core::Engine::Instance(){static Engine engine;return engine;}
bool Audio::Core::Engine::Start(Renderer* renderer){
    auto& s=*m_State;
    if(!renderer)return s.Fail("Native audio requires an initialized renderer");
    if(!s.output.Open(s.rate,m_uChannels))return s.Fail(s.output.Error());
    s.renderer=renderer;s.stop=false;s.submitted=0;
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
    auto& s=*m_State;s.stop=true;
    if(s.writer.joinable())s.writer.join();
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
        // Native platform buffering policy: at most one 60-Hz tick plus one PCM
        // block queued; the renderer's own fixed-size ring remains unchanged.
        while(!s.stop){
            const S64 queued=s.output.QueuedFrames();
            if(queued<0)throw std::runtime_error(s.output.Error());
            if(queued>=s.rate/60){MT::Thread::Sleep(1);continue;}
            F32 *surroundLFE=nullptr,*surround=nullptr,*stereo=nullptr,*mono=nullptr;
            if(!s.renderer->GetNextFrame(&surroundLFE,&surround,&stereo,&mono)){MT::Thread::Sleep(1);continue;}
            std::fill(frame.begin(),frame.end(),0.f);
            if(!s.renderer->IsOutputMuted()){
                if(mono)MixMono(mono,frame.data(),m_uChannels);
                if(stereo)MixStereo(stereo,frame.data(),m_uChannels);
                if(surround)MixStereo(surround,frame.data(),m_uChannels);
                if(surroundLFE)MixSurroundLFE(surroundLFE,frame.data(),m_uChannels);
                for(auto& value:frame)value=(value<1.f)?((value< -1.f)?-1.f:value):1.f;
            }
            if(!s.output.Queue(frame.data(),uFrameLength))throw std::runtime_error(s.output.Error());
            s.submitted+=uFrameLength;
        }
    }catch(const std::exception& error){s.Fail(error.what());s.stop=true;}
    s.running=false;
}
U64 Audio::Core::Engine::SubmittedFrames()const{return m_State->submitted;}
S64 Audio::Core::Engine::QueuedFrames()const{return m_State->output.QueuedFrames();}
bool Audio::Core::Engine::IsRunning()const{return m_State->running;}
std::string Audio::Core::Engine::Error()const{std::lock_guard<std::mutex> guard(m_State->errors);return m_State->error;}
#include "../OutputMix.inl"
