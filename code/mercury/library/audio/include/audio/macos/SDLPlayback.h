// [Blur2PC][macOS] Native PCM output through SDL's maintained CoreAudio backend. The game retains its mixer.
#pragma once
#include <core/Core.h>
#include <memory>
#include <string>
namespace Audio::Mac {
class Playback {
public:
    Playback();~Playback();
    Playback(const Playback&)=delete;Playback& operator=(const Playback&)=delete;
    bool Open(U32 sampleRate,U32 channels); // Opens paused; interleaved float32 engine samples.
    bool Queue(const F32* samples,U32 frames);
    bool Resume();bool Pause();bool Clear();void Close();
    bool IsOpen()const;S64 QueuedFrames()const;
    U32 SampleRate()const;U32 Channels()const;
    std::string Backend()const;std::string Error()const;
private:
    struct State;std::unique_ptr<State> m_State;
};
}
