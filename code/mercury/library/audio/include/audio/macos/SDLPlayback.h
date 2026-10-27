// [Blur2PC][macOS] Native PCM output through SDL's maintained CoreAudio backend. The game retains its mixer.
#pragma once
#include <core/Core.h>
#include <memory>
#include <string>
namespace Audio::Mac {
// [Blur2PC] Opt-in measurements; SDL's additional-byte demand includes resampler
// padding and is not an exact hardware-underrun counter.
struct PlaybackDiagnostics {
    bool enabled=false;
    U32 deviceRate=0,deviceChannels=0,devicePeriodFrames=0;
    U64 callbacks=0,demandShortfallCallbacks=0,demandShortfallBytes=0;
    U64 maximumAdditionalBytes=0,maximumTotalBytes=0;
};
struct PlaybackQueueStatus {
    U64 queuedInputFrames=0,availableOutputFrames=0,periodOutputFrames=0,paddingInputFrames=0;
    U32 targetInputFrames=0;
    U64 periodNanoseconds=0;
    bool periodReady=false;
};
// [Blur2PC] Sparse trace uses the same absolute steady-clock nanoseconds as frame diagnostics.
// Values are observational; callback snapshots of writer fields are approximate.
enum class ProducerPhase : U32 { Idle, QueueQuery, AcquireFrame, Mix, Submit, BackpressureSleep, ProducerSleep, Stopped };
enum class ProducerEvent : U32 { None, Start, FirstSubmission, SubmissionGap, ProducerStarved, ProducerRecovered, Stop, Primed };
class Playback {
public:
    Playback();~Playback();
    Playback(const Playback&)=delete;Playback& operator=(const Playback&)=delete;
    bool Open(U32 sampleRate,U32 channels); // Opens paused; interleaved float32 engine samples.
    bool Queue(const F32* samples,U32 frames);
    bool Resume();bool Pause();bool Clear();void Close();
    bool IsOpen()const;S64 QueuedFrames()const;
    U32 SampleRate()const;U32 Channels()const;
    U32 DevicePeriodInputFrames()const; // Current hardware period converted to input frames, rounded up; 0 on failure.
    bool GetQueueStatus(U32 cadenceFrames,PlaybackQueueStatus& status)const;
    std::string Backend()const;std::string Error()const;
    PlaybackDiagnostics Diagnostics()const;
    // One application writer; lock-free publication only when trace is opted in.
    void ObserveProducer(ProducerPhase phase,const PlaybackQueueStatus* queue,U64 submitted,U64 notReady,U64 backpressure,ProducerEvent event=ProducerEvent::None,U64 detailNanoseconds=0);
    bool TraceEnabled()const;
private:
    struct State;std::unique_ptr<State> m_State;
};
}
