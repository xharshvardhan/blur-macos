// [Blur2PC][macOS] Implement the existing MT API, without Win32 or console SDKs.
#include <core/Core.h>
#include <core/threads.h>
#include <core/maths/hvec/FP.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <limits>
#include <exception>
#include <memory>
#include <mutex>
#include <pthread.h>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <unistd.h>

namespace MT {
namespace {
using Guard = std::unique_lock<std::mutex>;
template<class Predicate> bool WaitFor(std::condition_variable& changed,
        Guard& guard, U32 milliseconds, Predicate predicate) {
    if (milliseconds == WAIT_TIME_INFINITE) {
        changed.wait(guard, predicate);
        return true;
    }
    return changed.wait_for(guard, std::chrono::milliseconds(milliseconds), predicate);
}
// [Blur2PC] Platform named-object registries may be constructed before the
// game's global allocator and destroyed after it. Keep every registry allocation
// (including long names, map buckets/nodes and weak control blocks) in libc's
// lifetime domain. These allocations are released normally; the registry does
// not retain game-allocator pages past allocator shutdown.
template<class T> struct RegistryAllocator {
    using value_type = T;
    RegistryAllocator() noexcept = default;
    template<class U> RegistryAllocator(const RegistryAllocator<U>&) noexcept {}
    T* allocate(size_t count) {
        if (count > std::numeric_limits<size_t>::max() / sizeof(T)) throw std::bad_alloc();
        void* memory = nullptr;
        if constexpr (alignof(T) > alignof(std::max_align_t)) {
            if (posix_memalign(&memory, alignof(T), count * sizeof(T))) throw std::bad_alloc();
        } else memory = std::malloc(count * sizeof(T));
        if (!memory) throw std::bad_alloc();
        return static_cast<T*>(memory);
    }
    void deallocate(T* memory, size_t) noexcept { std::free(memory); }
    template<class U> bool operator==(const RegistryAllocator<U>&) const noexcept { return true; }
    template<class U> bool operator!=(const RegistryAllocator<U>&) const noexcept { return false; }
};
using RegistryName = std::basic_string<char, std::char_traits<char>, RegistryAllocator<char>>;
struct RegistryNameHash {
    size_t operator()(const RegistryName& name) const noexcept {
        return std::hash<std::string_view>{}(std::string_view(name.data(), name.size()));
    }
};
// Named handles share state within this process, matching engine worker use.
// Cross-process Windows named objects are NOT implemented by this registry.
template<class T> struct Registry {
    std::mutex mutex;
    using Entry = std::pair<const RegistryName, std::weak_ptr<T>>;
    std::unordered_map<RegistryName, std::weak_ptr<T>, RegistryNameHash,
                       std::equal_to<RegistryName>, RegistryAllocator<Entry>> objects;
};
template<class T> Registry<T>& GetRegistry() { static Registry<T> registry; return registry; }
template<class T, class Factory> std::shared_ptr<T> Named(const char* name, Factory make) {
    if (!name || !*name) return make();
    auto& registry = GetRegistry<T>();
    Guard guard(registry.mutex);
    for (auto it = registry.objects.begin(); it != registry.objects.end();) {
        if (it->second.expired()) it = registry.objects.erase(it);
        else ++it;
    }
    auto& entry = registry.objects[RegistryName(name)];
    auto object = entry.lock();
    if (!object) { object = make(); entry = object; }
    return object;
}
struct RecursiveState {
    std::mutex mutex;
    std::condition_variable changed;
    std::thread::id owner;
    size_t depth = 0;
    bool Enter(U32 milliseconds) {
        Guard guard(mutex);
        const auto self = std::this_thread::get_id();
        if (!WaitFor(changed, guard, milliseconds, [&] { return !depth || owner == self; }))
            return false;
        owner = self;
        ++depth;
        return true;
    }
    bool Exit() {
        Guard guard(mutex);
        if (!depth || owner != std::this_thread::get_id()) return false;
        if (!--depth) { owner = {}; changed.notify_one(); }
        return true;
    }
};
struct EventState {
    std::mutex mutex;
    std::condition_variable changed;
    bool signaled = false;
};
struct SemaphoreState {
    SemaphoreState(long initial, long maximum) : count(initial), maximum(maximum) {}
    std::mutex mutex;
    std::condition_variable changed;
    long count, maximum;
};
void Check(int error, const char* operation) {
    if (error) throw std::system_error(error, std::generic_category(), operation);
}
}

class PlatformLock { public: RecursiveState state; };
class PlatformMutex { public: std::shared_ptr<RecursiveState> state; };
class PlatformEvent { public: std::shared_ptr<EventState> state; };
class PlatformSemaphore { public: std::shared_ptr<SemaphoreState> state; };
class PlatformRwLock { public: std::shared_timed_mutex state; };

Lock::Lock() : m_lock(new PlatformLock) {}
Lock::Lock(const Lock&) : m_lock(nullptr) { throw std::logic_error("cannot copy MT::Lock"); }
Lock::~Lock() { delete m_lock; }
void Lock::operator=(const Lock&) { throw std::logic_error("cannot assign MT::Lock"); }
void Lock::Enter() { m_lock->state.Enter(WAIT_TIME_INFINITE); }
bool Lock::TryEnter() { return m_lock->state.Enter(0); }
void Lock::Exit() {
    if (!m_lock->state.Exit()) throw std::logic_error("MT::Lock released by non-owner");
}

Mutex::Mutex(const char* name) : CreatedOK(false), m_Handle(nullptr) {
    auto handle = std::make_unique<PlatformMutex>();
    handle->state = Named<RecursiveState>(name, [] { return std::allocate_shared<RecursiveState>(RegistryAllocator<RecursiveState>{}); });
    m_Handle = handle.release();
    CreatedOK = true;
}
Mutex::~Mutex() { delete m_Handle; }
bool Mutex::Wait(U32 milliseconds) { return m_Handle->state->Enter(milliseconds); }
bool Mutex::Release() { return m_Handle->state->Exit(); }

Event::Event(const char* name) : m_Handle(nullptr), m_pMutex(nullptr) {
    auto handle = std::make_unique<PlatformEvent>();
    handle->state = Named<EventState>(name, [] { return std::allocate_shared<EventState>(RegistryAllocator<EventState>{}); });
    m_Handle = handle.release();
}
Event::~Event() { delete m_Handle; }
void Event::Signal() {
    auto& state = *m_Handle->state;
    Guard guard(state.mutex);
    // Win32 CreateEvent(..., FALSE, FALSE, ...): auto-reset, initially clear.
    // A signal before Wait is retained; repeated signals coalesce.
    state.signaled = true;
    state.changed.notify_one();
}
bool Event::Wait(U32 milliseconds) {
    auto& state = *m_Handle->state;
    Guard guard(state.mutex);
    if (!WaitFor(state.changed, guard, milliseconds, [&] { return state.signaled; })) return false;
    state.signaled = false;
    return true;
}

Semaphore::Semaphore(const char* name, long initial, long maximum) : m_Handle(nullptr) {
    if (initial < 0 || maximum <= 0 || maximum > 0x7fffffffL || initial > maximum)
        throw std::invalid_argument("invalid MT::Semaphore count");
    auto handle = std::make_unique<PlatformSemaphore>();
    handle->state = Named<SemaphoreState>(name, [=] {
        return std::allocate_shared<SemaphoreState>(RegistryAllocator<SemaphoreState>{}, initial, maximum);
    });
    m_Handle = handle.release();
}
Semaphore::~Semaphore() { delete m_Handle; }
bool Semaphore::Wait(U32 milliseconds) {
    auto& state = *m_Handle->state;
    Guard guard(state.mutex);
    if (!WaitFor(state.changed, guard, milliseconds, [&] { return state.count > 0; })) return false;
    --state.count;
    return true;
}
void Semaphore::Release(long count) {
    auto& state = *m_Handle->state;
    Guard guard(state.mutex);
    if (count <= 0 || count > state.maximum - state.count)
        throw std::overflow_error("MT::Semaphore release exceeds maximum or is non-positive");
    state.count += count;
    state.changed.notify_all();
}

RwLock::RwLock() : m_Native(new PlatformRwLock) {}
RwLock::~RwLock() { delete m_Native; }
bool RwLock::ReadLock(U32 milliseconds) {
    if (milliseconds == WAIT_TIME_INFINITE) { m_Native->state.lock_shared(); return true; }
    return m_Native->state.try_lock_shared_for(std::chrono::milliseconds(milliseconds));
}
bool RwLock::WriteLock(U32 milliseconds) {
    if (milliseconds == WAIT_TIME_INFINITE) { m_Native->state.lock(); return true; }
    return m_Native->state.try_lock_for(std::chrono::milliseconds(milliseconds));
}
void RwLock::ReleaseReadLock() { m_Native->state.unlock_shared(); }
void RwLock::ReleaseWriteLock() { m_Native->state.unlock(); }

class ThreadHandle {
public:
    pthread_t thread{};
    std::atomic<bool> running{false};
    bool joined = false;
    std::string name;
    std::exception_ptr failure;
};
static thread_local ThreadHandle* currentHandle = nullptr;

int GetProcessID() { return static_cast<int>(getpid()); }
Thread::Thread() : m_Handle(nullptr), m_Priority(0), m_bIsRunning(false) {}
Thread::~Thread() { Close(); }
void Thread::Close() {
    if (!m_Handle) return;
    try { Join(); }
    catch (...) {
        // A self-join or failed pthread_join must not free a live worker's handle.
        if (m_Handle->joined) { delete m_Handle; m_Handle = nullptr; }
        throw;
    }
    delete m_Handle;
    m_Handle = nullptr;
}
void Thread::Start(U32 stackSize, S32 priority, const char* name) {
    if (IsRunning()) throw std::logic_error("MT::Thread already running");
    Close();
    auto handle = std::make_unique<ThreadHandle>();
    handle->name = name ? std::string(name).substr(0, 63) : "";
    pthread_attr_t attributes;
    Check(pthread_attr_init(&attributes), "pthread_attr_init");
    // [Blur2PC] The original PC MT and TL APIs pass this as CreateThread's
    // initial commit, without STACK_SIZE_PARAM_IS_A_RESERVATION. A small request
    // therefore retains the desktop linker's 1MiB reserve, not a 32/64KiB total
    // stack. Darwin's attribute is the total stack; preserve that reserve for
    // every native caller, including job workers and default-size threads.
    // Larger PC commit requests round the reserve up to a whole MiB.
    constexpr size_t desktopReserve = 1024 * 1024;
    const size_t requested = std::max<size_t>(stackSize, desktopReserve);
    const size_t reserve = (requested + desktopReserve - 1) / desktopReserve * desktopReserve;
    const size_t page = static_cast<size_t>(getpagesize());
    const size_t bytes = (std::max<size_t>(reserve, PTHREAD_STACK_MIN) + page - 1) / page * page;
    const int stackError = pthread_attr_setstacksize(&attributes, bytes);
    if (stackError) { pthread_attr_destroy(&attributes); Check(stackError, "pthread_attr_setstacksize"); }
    m_Priority = priority;
    m_Handle = handle.get();
    handle->running.store(true, std::memory_order_release);
    const int error = pthread_create(&handle->thread, &attributes, [](void* arg) -> void* {
        static_cast<Thread*>(arg)->_ThreadRunProc();
        return nullptr;
    }, this);
    pthread_attr_destroy(&attributes);
    if (error) { m_Handle = nullptr; Check(error, "pthread_create"); }
    handle.release();
}
U32 Thread::_ThreadRunProc() {
    ThreadHandle* handle = m_Handle;
    SetCurrentThreadHandle(handle);
    try {
        if (!handle->name.empty()) Check(pthread_setname_np(handle->name.c_str()), "pthread_setname_np");
        // Darwin QoS is a coarse priority mapping, not Windows scheduler parity.
        if (m_Priority) Check(pthread_set_qos_class_self_np(
            m_Priority > 0 ? QOS_CLASS_USER_INITIATED : QOS_CLASS_UTILITY, 0), "pthread_set_qos_class_self_np");
        Maths::FP_Init();
        ThreadMain();
    } catch (...) { handle->failure = std::current_exception(); }
    SetCurrentThreadHandle(nullptr);
    handle->running.store(false, std::memory_order_release);
    return 0;
}
void Thread::Join() {
    if (!m_Handle) return;
    if (pthread_equal(pthread_self(), m_Handle->thread))
        throw std::logic_error("MT::Thread cannot join itself");
    if (!m_Handle->joined) {
        Check(pthread_join(m_Handle->thread, nullptr), "pthread_join");
        m_Handle->joined = true;
    }
    // Consume once so Close/destruction can still clean up after a failed Join.
    auto failure = m_Handle->failure;
    m_Handle->failure = nullptr;
    if (failure) std::rethrow_exception(failure);
}
void Thread::Sleep(U32 milliseconds) {
    if (!milliseconds) std::this_thread::yield();
    else std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}
const bool Thread::IsRunning() {
    return m_Handle && m_Handle->running.load(std::memory_order_acquire);
}
ThreadHandle* Thread::GetCurrentThreadHandle() { return currentHandle; }
void Thread::SetCurrentThreadHandle(ThreadHandle* handle) { currentHandle = handle; }
}
