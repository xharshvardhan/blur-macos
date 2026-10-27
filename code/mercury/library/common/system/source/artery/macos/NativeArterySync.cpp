// [Blur2PC][macOS] Artery's manual/auto-reset signals and recursive locks.
#include <core/Core.h>
#include <system/artery/ArterySync.h>
#include <system/scheduler/macos/NativeTLThread.h>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>

struct NativeArteryMutex { std::recursive_timed_mutex mutex; };
struct NativeArteryCond {
    std::condition_variable_any cond;
    NativeArteryMutex* mutex;
    explicit NativeArteryCond(NativeArteryMutex* m) : mutex(m) {}
};
struct NativeArterySignal {
    std::mutex mutex;
    std::condition_variable cond;
    bool raised = false;
    void Set(bool value, bool broadcast) {
        std::lock_guard<std::mutex> lock(mutex);
        raised = value;
        if (value) { if (broadcast) cond.notify_all(); else cond.notify_one(); }
    }
    bool Wait(int milliseconds, bool consume) {
        std::unique_lock<std::mutex> lock(mutex);
        if (milliseconds < 0) cond.wait(lock, [&] { return raised; });
        else if (!cond.wait_for(lock, std::chrono::milliseconds(milliseconds),
                               [&] { return raised; })) return false;
        if (consume) raised = false;
        return true;
    }
};
namespace {
template<class T> T& Live(T* state) {
    if (!state) throw std::logic_error("Artery object has not been created");
    return *state;
}
}

// The callback fields are published by tlCreateThread before the worker starts.
void CallArteryThreadProc(ArteryThread* thread) { thread->_proc(thread->_context); }
static U32 NativeArteryEntry(void* p) {
    CallArteryThreadProc(static_cast<ArteryThread*>(p)); return 0;
}
ArteryThread::ArteryThread() : _thread(0), _proc(nullptr), _context(nullptr) {}
ArteryThread::~ArteryThread() { Close(); }
bool ArteryThread::Create(ArteryThreadProc proc, void* context, int stackSize) {
    if (_thread || !proc || stackSize < 0) return false;
    _proc = proc; _context = context;
    try {
        _thread = tlCreateThread(NativeArteryEntry, reinterpret_cast<INTPTR>(this),
            "Artery", TL_THREAD_PRIORITY_NORMAL,
            stackSize ? static_cast<U32>(stackSize) : TL_THREAD_DEFAULT_STACK_SIZE);
    } catch (const std::exception&) {
        _proc = nullptr; _context = nullptr; return false;
    }
    if (!_thread) { _proc = nullptr; _context = nullptr; }
    return _thread != 0;
}
void ArteryThread::Close() {
    if (!_thread) return;
    const auto handle = _thread;
    // Preserve the join-before-release contract, including callback exceptions.
    try { tlWaitForThread(handle); }
    catch (...) { _thread = 0; _proc = nullptr; _context = nullptr; throw; }
    _thread = 0; _proc = nullptr; _context = nullptr;
}

ArteryLock::ArteryLock() : _native(new NativeArteryMutex) {}
ArteryLock::~ArteryLock() { delete _native; }
bool ArteryLock::Try() { return _native->mutex.try_lock(); }
void ArteryLock::Wait() { _native->mutex.lock(); }
void ArteryLock::Release() { _native->mutex.unlock(); }

ArteryMutex::ArteryMutex() : _native(nullptr) {}
ArteryMutex::~ArteryMutex() { Destroy(); }
bool ArteryMutex::Create() {
    if (_native) return false;
    _native = new NativeArteryMutex; return true;
}
void ArteryMutex::Destroy() { delete _native; _native = nullptr; }
bool ArteryMutex::Try() { return Live(_native).mutex.try_lock(); }
void ArteryMutex::Wait() { Live(_native).mutex.lock(); }
bool ArteryMutex::Wait(int milliseconds) {
    if (milliseconds < 0) { Wait(); return true; }
    return Live(_native).mutex.try_lock_for(std::chrono::milliseconds(milliseconds));
}
void ArteryMutex::Release() { Live(_native).mutex.unlock(); }

ArteryCondVar::ArteryCondVar() : _native(nullptr) {}
ArteryCondVar::~ArteryCondVar() { Destroy(); }
bool ArteryCondVar::Create(ArteryMutex* mutex) {
    if (_native || !mutex || !mutex->_native) return false;
    _native = new NativeArteryCond(mutex->_native); return true;
}
void ArteryCondVar::Destroy() { delete _native; _native = nullptr; }
void ArteryCondVar::Signal() { Live(_native).cond.notify_one(); }
void ArteryCondVar::Broadcast() { Live(_native).cond.notify_all(); }
void ArteryCondVar::Wait() {
    auto& state = Live(_native);
    state.cond.wait(state.mutex->mutex);
}
bool ArteryCondVar::Wait(int milliseconds) {
    if (milliseconds < 0) { Wait(); return true; }
    auto& state = Live(_native);
    return state.cond.wait_for(state.mutex->mutex, std::chrono::milliseconds(milliseconds))
        != std::cv_status::timeout;
}

// A flag is manual-reset: it releases all waiters and remains raised until Lower.
ArteryFlag::ArteryFlag() : _native(nullptr) {}
ArteryFlag::~ArteryFlag() { Destroy(); }
bool ArteryFlag::Create() {
    if (_native) return false;
    _native = new NativeArterySignal; return true;
}
void ArteryFlag::Destroy() { delete _native; _native = nullptr; }
void ArteryFlag::Lower() { Live(_native).Set(false, true); }
void ArteryFlag::Raise() { Live(_native).Set(true, true); }
bool ArteryFlag::Try() { return Live(_native).Wait(0, false); }
void ArteryFlag::Wait() { Live(_native).Wait(-1, false); }
bool ArteryFlag::Wait(int milliseconds) { return Live(_native).Wait(milliseconds, false); }

// An event is auto-reset: exactly one waiter consumes a retained raise.
ArteryEvent::ArteryEvent() : _native(nullptr) {}
ArteryEvent::~ArteryEvent() { Destroy(); }
bool ArteryEvent::Create() {
    if (_native) return false;
    _native = new NativeArterySignal; return true;
}
void ArteryEvent::Destroy() { delete _native; _native = nullptr; }
void ArteryEvent::Lower() { Live(_native).Set(false, false); }
void ArteryEvent::Raise() { Live(_native).Set(true, false); }
bool ArteryEvent::Try() { return Live(_native).Wait(0, true); }
void ArteryEvent::Wait() { Live(_native).Wait(-1, true); }
bool ArteryEvent::Wait(int milliseconds) { return Live(_native).Wait(milliseconds, true); }
