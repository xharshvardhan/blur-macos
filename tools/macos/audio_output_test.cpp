// [Blur2PC][macOS] Real SDL/CoreAudio output consumption, silence only. No game mixer/assets or audible tone.
#include <audio/macos/SDLPlayback.h>
#include <SDL3/SDL.h>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
static void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
static void output(U32 rate,U32 channels){
    Audio::Mac::Playback device;require(!device.IsOpen()&&device.QueuedFrames()==-1&&!device.Queue(nullptr,1),"Closed native output accepted PCM");
    require(!device.Open(0,channels)&&!device.Open(rate,3)&&!device.IsOpen(),"Invalid PCM configuration acquired output ownership");
    if(!device.Open(rate,channels))throw std::runtime_error("SDL native output open failed: "+device.Error());
    require(device.Backend()=="coreaudio"&&device.SampleRate()==rate&&device.Channels()==channels,"Native audio fixture did not use actual CoreAudio with the requested PCM format");
    const U32 frames=rate/10;std::vector<F32> silence(size_t(frames)*channels,0);
    require(device.QueuedFrames()==0&&device.Queue(silence.data(),frames)&&device.QueuedFrames()==frames,"Paused native stream did not retain all queued PCM frames");
    require(!device.Open(rate,channels)&&device.IsOpen()&&device.QueuedFrames()==frames,"Duplicate output initialization damaged the live stream");
    require(!device.Queue(nullptr,1)&&!device.Queue(silence.data(),~0U)&&device.QueuedFrames()==frames,"Invalid PCM block changed the queued frame count");
    require(device.Resume(),"CoreAudio playback did not resume");const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(device.QueuedFrames()==frames&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(10));
    require(device.QueuedFrames()>=0&&device.QueuedFrames()<frames,"Actual CoreAudio output never consumed queued silence");
    require(device.Pause()&&device.Clear()&&device.QueuedFrames()==0,"Native output pause/clear failed");
    require(device.Queue(silence.data(),frames)&&device.QueuedFrames()==frames,"Paused native output did not accept a second PCM block");
    device.Close();device.Close();require(!device.IsOpen()&&device.QueuedFrames()==-1&&device.SampleRate()==0&&device.Channels()==0,"Native output cleanup retained a live device or format");
}
int main(){try{
    require(!SDL_WasInit(SDL_INIT_AUDIO),"Audio fixture process already owns an SDL audio subsystem");
    output(48000,2);require(!SDL_WasInit(SDL_INIT_AUDIO),"Owned native output retained its SDL subsystem");
    require(SDL_InitSubSystem(SDL_INIT_AUDIO),"External SDL audio owner could not initialize");output(24000,6);require(SDL_WasInit(SDL_INIT_AUDIO),"Native output stopped an externally owned SDL subsystem");SDL_QuitSubSystem(SDL_INIT_AUDIO);
    output(36000,8);require(!SDL_WasInit(SDL_INIT_AUDIO),"Native output restart retained SDL audio ownership");
    std::cout<<"Upstream SDL3/CoreAudio output/actual PCM consumption/stereo and surround input/rates/pause/clear/restart/borrowed subsystem ownership PASS (silence only; game mixer not integrated)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
