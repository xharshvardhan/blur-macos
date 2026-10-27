// [Blur2PC] Original renderer buses and PC mixing feed SDL's native PCM output.
#pragma once
#include <core/Core.h>
#include <memory>
#include <string>
namespace Audio::Core {
class Renderer;
class Engine {
public:
    Engine();~Engine();
    static Engine& Instance();
    bool Init(Renderer* renderer);
    bool Shutdown(Renderer* expected=nullptr);
    bool SetOutputSampleRate(U32 rate);
    bool ConfigureChannels(U32 channels); // While stopped; SDL converts to the physical layout.
    U64 SubmittedFrames()const;
    S64 QueuedFrames()const;
    bool IsRunning()const;
    std::string Error()const;
protected:
    // One physical implementation is shared with the original PC engine.
    void MixMono(const F32* input,F32* output,U32 channels);
    void MixStereo(const F32* input,F32* output,U32 channels);
    void MixSurroundLFE(const F32* input,F32* output,U32 channels);
    U32 m_uChannels=2;
private:
    struct State;std::unique_ptr<State> m_State;
    void WritePCM();
    bool Start(Renderer* renderer);
    void Stop();
};
}
