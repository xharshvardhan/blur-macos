// [Blur2PC][macOS] Opt-in CPU spans only; these are not GPU execution timestamps.
#pragma once
#include <atomic>
#include <cstdint>
namespace Display2::Metal::Trace {
inline std::atomic<bool> enabled{false};
enum class Phase : unsigned { BufferWait, BufferRetireWait, SurfaceWait, SurfaceRetireWait,
    DownloadWait, DeviceIdleWait, PresentResourceWait, NextDrawable, PresentSubmit, Present, Encoding };
enum class Encoding : unsigned { Draw, Pipeline, Constants, Textures, Count };
void BeginSession() noexcept;
void EndSession() noexcept;
std::uint64_t Now() noexcept;
class Span {
public:
    Span(Phase phase, const void* resource=nullptr, const void* command=nullptr, int status=-1) noexcept;
    ~Span();
    void Status(int status) noexcept { m_Status=status; }
    Span(const Span&)=delete; Span& operator=(const Span&)=delete;
private:
    friend void PresentEncodingSummary(std::uint64_t) noexcept;
    unsigned m_Index=~0u;
    int m_Status=-1;
};
class EncodingScope {
public:
    explicit EncodingScope(Encoding kind) noexcept : m_Start(enabled.load(std::memory_order_relaxed)?Now():0), m_Kind(kind) {}
    ~EncodingScope();
private: std::uint64_t m_Start; Encoding m_Kind;
};
void PresentEncodingSummary(std::uint64_t present) noexcept;
}
