// [Blur2PC] Original renderer buses and PC mixing feed SDL's native PCM output.
#pragma once
#include <core/Core.h>
#include <memory>
#include <string>
#include <audio/macos/SDLPlayback.h>
namespace Audio::Core {
class Renderer;
// [Blur2PC] Last/current opt-in output window, reset on each device start.
struct EngineDiagnostics {
    bool enabled=false;
    U32 sampleRate=0,channels=0;
    U64 submittedFrames=0,producerNotReady=0,emptyQueueObservations=0,backpressurePolls=0;
    U64 clippedSamples=0,nonfiniteSamples=0;
    U64 maximumQueueTargetFrames=0,maximumObservedPaddingFrames=0;
    F64 elapsedSeconds=0,maximumSubmissionGapMilliseconds=0;
    F32 peakAbsolute=0;
    Audio::Mac::PlaybackDiagnostics output;
};
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
    EngineDiagnostics Diagnostics()const;
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
