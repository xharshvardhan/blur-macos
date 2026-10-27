// [Blur2PC][macOS] Diagnostics for actual queued I/O resource ownership.
#pragma once
#include <atomic>
#include <cstddef>
namespace Sys {
bool IsNativeIoInitialised();
bool IsNativeIoWorkerThread();
struct NativeIoStats { std::size_t files, requests, handles, workers; };
struct NativeIoCounts {
    std::atomic<std::size_t> files{0}, requests{0}, handles{0}, workers{0};
};
inline NativeIoCounts& NativeIoCounters() { static NativeIoCounts value; return value; }
inline NativeIoStats GetNativeIoStats() {
    auto& c = NativeIoCounters(); return {c.files.load(), c.requests.load(), c.handles.load(), c.workers.load()};
}
// A pending cancel completes with Failed/Unknown: the historical IoError enum
// has no cancellation value. Wait includes callback Release and buffer access.
}
