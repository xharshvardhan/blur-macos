// [Blur2PC] Native opt-in observations of the original ADPCM decoder decisions.
// Storage has process lifetime; its renderer owner drains only after producer join.
#pragma once
#include <atomic>
#include <cstdint>

namespace Audio { namespace Mac {
extern std::atomic<bool> g_DecoderTraceEnabled;
bool BeginDecoderTrace(const void* owner);
void EndDecoderTrace(const void* owner);
enum DecoderTraceReason : std::uint32_t { DecoderUnprimed=1, DecoderNoData=2, DecoderInsufficientBytes=4 };
struct DecoderZeroFill {
    std::uint32_t channels,streamIndex,streamHandle,sampleCounter,sampleIndex,loopEnd;
    std::uint32_t sourcePosition,bytesConsumed,bytesAvailable,samplesRequired,bytesRequired,reason;
    float pitch;
    bool primed,playing,finished,loop;
};
void CountDecoderAttempt(std::uint32_t channels,bool streamed);
void CountDecoderSuccess(std::uint32_t channels);
void RecordDecoderZeroFill(const DecoderZeroFill& event);
inline bool DecoderTraceEnabled(){return g_DecoderTraceEnabled.load(std::memory_order_relaxed);}
} }
