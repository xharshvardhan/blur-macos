// [Blur2PC][macOS] Actual queued device operations over Darwin open/pread/close.
#include "../IoImpl.h"
#include <system/serialise/macos/NativePath.h>
#include <system/time/Time.h>
#include <algorithm>
#include <cerrno>
#include <deque>
#include <fcntl.h>
#include <limits>
#include <set>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace {
thread_local NativeIoDevice* activeDevice = nullptr;
// Original GetTickCount deadlines use elapsed time, independently of wall-clock corrections.
U32 milliseconds() { return static_cast<U32>(Time::GetPerformanceCounter() / 1000); }
IoError osError(int value) {
    switch (value) {
        case ENOENT: return kIoError_FileNotFound;
        case ENOTDIR: return kIoError_PathNotFound;
        case EACCES: case EPERM: return kIoError_AccessDenied;
        case EMFILE: case ENFILE: return kIoError_TooManyFilesOpen;
        case ENOMEM: return kIoError_OutOfMemory;
        default: return kIoError_Unknown;
    }
}
}
struct NativeIoDevice {
    ArteryThread thread;
    std::mutex mutex;
    std::condition_variable condition;
    std::deque<IoRequestImpl*> pending;
    std::set<IoFileImpl*> openFiles;
    bool stopping = false, started = false;

    static void Entry(void* value) { static_cast<NativeIoDevice*>(value)->Run(); }
    void CloseHandle(IoFileImpl* file) {
        if (file->handle >= 0) {
            ::close(file->handle); file->handle = -1;
            --Sys::NativeIoCounters().handles;
        }
        file->state = IoFileImpl::kState_Closed;
        file->fileSize = 0;
        openFiles.erase(file);
    }
    void Failed(IoRequestImpl* request, IoError reason) {
        auto* file = request->file._impl;
        request->error = reason;
        if (request->action == kIoAction_Open) {
            CloseHandle(file);
            file->self = IoFile();
        } else if (request->action == kIoAction_Close && file->handle >= 0) {
            file->state = IoFileImpl::kState_Open;
        }
        request->NativeFinish(kIoEvent_Failed);
    }
    IoRequestImpl* Select() {
        // Retain the original desktop priority/deadline selection. A close is
        // additionally a barrier after earlier operations on the same file.
        auto chosen = pending.end();
        U32 chosenDeadline = 0;
        auto chosenPriority = kIoPriority_Low;
        const auto now = milliseconds();
        for (auto it = pending.begin(); it != pending.end(); ++it) {
            auto* request = *it;
            if (request->action == kIoAction_Close &&
                std::any_of(pending.begin(), it, [&](auto* earlier) {
                    return earlier->file._impl == request->file._impl;
                })) continue;
            auto deadline = request->Deadline();
            deadline -= request->requestSize / ((6 * 1024 * 1024) / 1000);
            const auto priority = request->priority.load();
            bool accept = chosen == pending.end();
            if (!accept && static_cast<S32>(deadline - now) < 0)
                accept = priority < chosenPriority || static_cast<S32>(deadline - chosenDeadline) < 0;
            else if (!accept)
                accept = static_cast<S32>(deadline - chosenDeadline) < 0 || priority < chosenPriority;
            if (accept) { chosen = it; chosenDeadline = deadline; chosenPriority = priority; }
        }
        if (chosen == pending.end()) return nullptr;
        auto* request = *chosen; pending.erase(chosen); return request;
    }
    void Execute(IoRequestImpl* request) {
        IoRequest keepAlive(request);
        if (!request->NativeBegin()) { Failed(request, kIoError_Unknown); return; }
        auto* file = request->file._impl;
        try {
            switch (request->action) {
                case kIoAction_Open: {
                    const auto path = Sys::NativeResolvePath(Sys::C_FilePath(file->filename));
                    int descriptor;
                    do { descriptor = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK); }
                    while (descriptor < 0 && errno == EINTR);
                    if (descriptor < 0) { Failed(request, osError(errno)); return; }
                    file->handle = descriptor; ++Sys::NativeIoCounters().handles;
                    struct stat info;
                    if (::fstat(descriptor, &info) != 0) { Failed(request, osError(errno)); return; }
                    if (!S_ISREG(info.st_mode) || info.st_size < 0) {
                        Failed(request, kIoError_InvalidParameters); return;
                    }
                    file->fileSize = static_cast<U64>(info.st_size);
                    openFiles.insert(file);
                    file->state = IoFileImpl::kState_Open;
                    break;
                }
                case kIoAction_Read: {
                    if (file->handle < 0) { Failed(request, kIoError_FileNotYetOpen); return; }
                    if (!request->buffer || !request->requestSize ||
                        request->offset > static_cast<U64>(std::numeric_limits<off_t>::max()) ||
                        request->requestSize > static_cast<U64>(std::numeric_limits<off_t>::max()) - request->offset) {
                        Failed(request, kIoError_InvalidParameters); return;
                    }
                    U32 actual = 0;
                    while (actual < request->requestSize) {
                        const auto amount = ::pread(file->handle, static_cast<U8*>(request->buffer) + actual,
                            request->requestSize - actual, static_cast<off_t>(request->offset + actual));
                        if (amount < 0 && errno == EINTR) continue;
                        if (amount < 0) { request->actualSize = actual; Failed(request, osError(errno)); return; }
                        if (!amount) break;
                        actual += static_cast<U32>(amount);
                    }
                    request->actualSize = actual;
                    if (!actual) { Failed(request, kIoError_EndOfFile); return; }
                    break;
                }
                case kIoAction_Close:
                    CloseHandle(file); file->self = IoFile(); break;
                default: Failed(request, kIoError_InvalidParameters); return;
            }
            request->error = kIoError_Okay;
            request->NativeFinish(kIoEvent_Complete);
        } catch (...) {
            // Filesystem/path errors are operation failures; callback errors
            // are retained by NativeFinish and rethrown to the request's Wait.
            Failed(request, kIoError_Unknown);
        }
    }
    void Run() {
        activeDevice = this;
        ++Sys::NativeIoCounters().workers;
        struct Exit {
            ~Exit() { activeDevice = nullptr; --Sys::NativeIoCounters().workers; }
        } exit;
        for (;;) {
            IoRequestImpl* request;
            {
                std::unique_lock<std::mutex> lock(mutex);
                condition.wait(lock, [&] { return stopping || !pending.empty(); });
                if (stopping) return;
                request = Select();
            }
            if (request) Execute(request);
        }
    }
};

IoDevice::IoDevice(const char* name) : _native(new NativeIoDevice), _name(name) {}
IoDevice::~IoDevice() { Shutdown(); delete _native; }
bool IoDevice::NativeOnWorkerThread() { return activeDevice != nullptr; }
bool IoDevice::Initialise() {
    if (_native->started) return true;
    _native->stopping = false;
    _native->started = _native->thread.Create(NativeIoDevice::Entry, _native, 128 * 1024);
    return _native->started;
}
void IoDevice::Queue(IoRequestImpl* request) {
    request->creation = milliseconds();
    {
        std::lock_guard<std::mutex> lock(_native->mutex);
        if (_native->stopping || !_native->started) throw std::logic_error("I/O device is stopped");
        _native->pending.push_back(request);
    }
    _native->condition.notify_one();
}
void IoDevice::Shutdown() {
    if (!_native->started) return;
    if (activeDevice == _native) throw std::logic_error("I/O worker cannot join itself");
    {
        std::lock_guard<std::mutex> lock(_native->mutex); _native->stopping = true;
    }
    _native->condition.notify_one(); _native->thread.Close(); _native->started = false;
    // Join the active callback before draining pending work or closing handles.
    for (auto* request : _native->pending) _native->Failed(request, kIoError_Unknown);
    _native->pending.clear();
    while (!_native->openFiles.empty()) {
        auto* file = *_native->openFiles.begin();
        IoFile keepAlive(file);
        _native->CloseHandle(file); file->self = IoFile();
    }
}
