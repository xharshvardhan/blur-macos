// [Blur2PC][macOS] Real pthread workers through the already ported MT::Thread.
#include <system/scheduler/macos/NativeTLThread.h>
#include <core/threads.h>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace {
struct ExitCode { U32 value; };
thread_local tlThreadHandle current = 0;
class NativeTLThread final : public MT::Thread {
public:
    tlThreadFunction entry;
    INTPTR parameter;
    U32 result = 0;
    tlThreadHandle handle = 0;
    MT::Event startGate;
    bool suspended;
    bool joining = false; // protected by the registry lock
    NativeTLThread(tlThreadFunction entry, INTPTR parameter, bool suspended)
        : entry(entry), parameter(parameter), suspended(suspended) {}
    ~NativeTLThread() override { Join(); }
    void ThreadMain() override {
        current = handle;
        struct Reset { ~Reset() { current = 0; } } reset;
        if (suspended) startGate.Wait();
        try { result = entry(reinterpret_cast<void*>(parameter)); }
        catch (const ExitCode& exit) { result = exit.value; }
    }
};
struct Registry {
    std::mutex mutex;
    std::unordered_map<tlThreadHandle, std::shared_ptr<NativeTLThread>> threads;
    tlThreadHandle next = 1;
};
Registry& registry() { static Registry value; return value; }
std::shared_ptr<NativeTLThread> find(tlThreadHandle handle) {
    auto& state = registry();
    std::lock_guard<std::mutex> guard(state.mutex);
    const auto found = state.threads.find(handle);
    if (found == state.threads.end()) throw std::invalid_argument("invalid native TL thread handle");
    return found->second;
}
}

tlThreadHandle tlCreateThread(tlThreadFunction entry, INTPTR param, const char* name,
        int priority, U32 stacksize, U32 flags) {
    if (!entry || (flags & ~U32(TL_THREAD_CREATE_SUSPENDED)))
        throw std::invalid_argument("invalid native TL entry or creation flags");
    auto thread = std::make_shared<NativeTLThread>(entry, param, flags & TL_THREAD_CREATE_SUSPENDED);
    auto& state = registry();
    tlThreadHandle handle;
    {
        std::lock_guard<std::mutex> guard(state.mutex);
        if (state.next >= (U64(1) << 63)) throw std::overflow_error("native TL handle space exhausted");
        thread->handle = handle = state.next++;
        state.threads.emplace(handle, thread);
    }
    try { thread->Start(stacksize, priority, name); }
    catch (...) {
        std::lock_guard<std::mutex> guard(state.mutex);
        state.threads.erase(handle);
        throw;
    }
    return handle;
}
tlThreadHandle tlGetCurrentThread() {
    // External/native main threads receive a pseudo-handle. Only handles from
    // tlCreateThread can be resumed or joined through the TL registry.
    return current ? current : ((U64(1) << 63) | MT::GetThreadId());
}
void tlSleep(U32 milliseconds) { MT::Thread::Sleep(milliseconds); }
[[noreturn]] void tlExitThread(U32 exitcode) {
    // Preserve C++ unwinding and MT running/TLS cleanup for TL worker exits.
    if (!current) throw std::logic_error("tlExitThread requires a TL-created worker");
    throw ExitCode{exitcode};
}
U32 tlWaitForThread(tlThreadHandle handle) {
    auto thread = find(handle);
    if (current == handle) throw std::logic_error("native TL thread cannot join itself");
    auto& state = registry();
    {
        std::lock_guard<std::mutex> guard(state.mutex);
        if (thread->joining) throw std::logic_error("native TL thread already being joined");
        thread->joining = true;
    }
    try { thread->Join(); }
    catch (...) {
        std::lock_guard<std::mutex> guard(state.mutex);
        state.threads.erase(handle);
        throw;
    }
    const U32 result = thread->result;
    {
        std::lock_guard<std::mutex> guard(state.mutex);
        state.threads.erase(handle);
    }
    return result;
}
void tlResumeThread(tlThreadHandle handle) { find(handle)->startGate.Signal(); }
bool tlIsThreadRunning(tlThreadHandle handle) { return find(handle)->IsRunning(); }
void tlSetCurrentThreadName(const char* name) {
    if (!name) throw std::invalid_argument("null native thread name");
    const auto bounded = std::string(name).substr(0, 63);
    const int error = pthread_setname_np(bounded.c_str());
    if (error) throw std::system_error(error, std::generic_category(), "pthread_setname_np");
}
