// [Blur2PC][macOS] Existing two-device service API with real queued operations.
#include "../IoImpl.h"
#include <limits>
#include <stdexcept>

namespace {
IoRequest rejected(IoRequestImpl& reason, IoCallback* callback) {
    IoRequest result(&reason);
    if (callback) {
        try { callback->Event(kIoEvent_Failed, result); }
        catch (...) { callback->Release(); throw; }
        callback->Release();
    }
    return result;
}
}
IoService::IoService() : _initialised(false), _dvd(nullptr), _hdd(nullptr) {}
IoService::~IoService() = default; // CoreSystems explicitly shuts I/O down before allocators.
bool IoService::Initialise() {
    std::unique_lock<std::mutex> lock(_nativeSubmit);
    if (_nativeStopping) return false;
    if (_initialised) return true;
    try {
        _dvd = CreateDevice("dvd"); _hdd = CreateDevice("hdd");
        if (!_dvd || !_hdd || !InitialiseDevices()) throw std::runtime_error("native I/O worker creation failed");
        _initialised = true; return true;
    } catch (...) {
        for (XtUt::List<IoDevice>::Iterator it(&_devices); it; ++it) it->Shutdown();
        XtUt::Free(&_devices); _dvd = _hdd = nullptr;
        return false;
    }
}
void IoService::Shutdown() {
    if (IoDevice::NativeOnWorkerThread()) throw std::logic_error("I/O callback cannot shut down its worker service");
    {
        std::unique_lock<std::mutex> lock(_nativeSubmit);
        if (_nativeStopping) {
            _nativeStopped.wait(lock, [&] { return !_nativeStopping; });
            return;
        }
        if (!_initialised) return;
        _initialised = false; _nativeStopping = true;
    }
    // Never hold admission locks while joining: completing callbacks may submit
    // another request and must receive failure rather than deadlock on shutdown.
    for (XtUt::List<IoDevice>::Iterator it(&_devices); it; ++it) it->Shutdown();
    XtUt::Free(&_devices);
    {
        std::lock_guard<std::mutex> lock(_nativeSubmit);
        _dvd = _hdd = nullptr; _nativeStopping = false;
    }
    _nativeStopped.notify_all();
}
IoDevice* IoService::GetDeviceByPath(const char*) { return _hdd; }
IoDevice* IoService::CreateDevice(const char* name) { return XtUt::AppendEx(&_devices, new IoDevice(name)); }
bool IoService::InitialiseDevices() {
    for (XtUt::List<IoDevice>::Iterator it(&_devices); it; ++it) if (!it->Initialise()) return false;
    return true;
}
void IoService::CachePrioritise(const char**, int) {
    // Same desktop policy as PC: no title-managed disk cache. Darwin page cache
    // services actual pread calls; the PS3 FIOS cache API is not emulated.
}
void IoSuspendFileCaching() {}
void IoResumeFileCaching() {}
void IoGetCacheStats(IoCacheStats* result) { if (result) *result = {}; }

IoRequest IoService::Open(IoFile& file, const char* filename, IoCallback* callback) {
    std::unique_lock<std::mutex> lock(_nativeSubmit);
    file = IoFile();
    if (!_initialised) { lock.unlock(); return rejected(gIoRequest_NoDeviceMapping, callback); }
    if (!filename || !*filename) { lock.unlock(); return rejected(gIoRequest_InvalidParameters, callback); }
    IoFileImpl* value = nullptr;
    IoRequestImpl* request = nullptr;
    try {
        value = new IoFileImpl;
        value->filename = filename; value->device = GetDeviceByPath(filename);
        request = new IoRequestImpl(kIoAction_Open, value->self, callback);
    } catch (...) {
        if (value) value->self = IoFile();
        lock.unlock(); return rejected(gIoRequest_OutOfMemory, callback);
    }
    IoRequest result(request); file = value->self;
    try { value->device->Queue(request); }
    catch (...) {
        value->state = IoFileImpl::kState_Closed; value->self = IoFile();
        request->error = kIoError_OutOfMemory; lock.unlock(); request->NativeFinish(kIoEvent_Failed);
    }
    // A fast completion may already have dropped request->self. Return the
    // independent reference, not the mutable queue/self field.
    return result;
}
IoRequest IoService::Read(const IoFile& file, void* buffer, U64 offset, U32 size,
                          IoPriority priority, IoCallback* callback) {
    std::unique_lock<std::mutex> lock(_nativeSubmit);
    if (!_initialised) { lock.unlock(); return rejected(gIoRequest_NoDeviceMapping, callback); }
    if (!file._impl || !buffer || !size || priority < kIoPriority_High || priority > kIoPriority_Low ||
        offset > static_cast<U64>(std::numeric_limits<S64>::max()) ||
        size > static_cast<U64>(std::numeric_limits<S64>::max()) - offset) {
        lock.unlock(); return rejected(gIoRequest_InvalidParameters, callback);
    }
    auto* value = file._impl;
    if (value->state != IoFileImpl::kState_Open) {
        lock.unlock(); return rejected(gIoRequest_FileNotYetOpen, callback);
    }
    IoRequestImpl* request;
    try { request = new IoRequestImpl(kIoAction_Read, file, callback); }
    catch (...) { lock.unlock(); return rejected(gIoRequest_OutOfMemory, callback); }
    IoRequest result(request);
    request->buffer = buffer; request->offset = offset; request->requestSize = size; request->priority = priority;
    try { value->device->Queue(request); }
    catch (...) { request->error = kIoError_OutOfMemory; lock.unlock(); request->NativeFinish(kIoEvent_Failed); }
    return result;
}
IoRequest IoService::Close(const IoFile& file, IoCallback* callback) {
    std::unique_lock<std::mutex> lock(_nativeSubmit);
    if (!_initialised) { lock.unlock(); return rejected(gIoRequest_NoDeviceMapping, callback); }
    auto* value = file._impl;
    if (!value) { lock.unlock(); return rejected(gIoRequest_InvalidParameters, callback); }
    if (value->state != IoFileImpl::kState_Open) {
        lock.unlock(); return rejected(gIoRequest_FileNotYetOpen, callback);
    }
    IoRequestImpl* request;
    try { request = new IoRequestImpl(kIoAction_Close, file, callback); }
    catch (...) { lock.unlock(); return rejected(gIoRequest_OutOfMemory, callback); }
    IoRequest result(request);
    value->state = IoFileImpl::kState_Closing;
    try { value->device->Queue(request); }
    catch (...) {
        value->state = IoFileImpl::kState_Open;
        request->error = kIoError_OutOfMemory; lock.unlock(); request->NativeFinish(kIoEvent_Failed);
    }
    return result;
}

bool IoService::NativeIsInitialised() {
    std::lock_guard<std::mutex> lock(_nativeSubmit); return _initialised;
}
IoService gIoService;
namespace Sys {
bool IsNativeIoInitialised() { return gIoService.NativeIsInitialised(); }
bool IsNativeIoWorkerThread() { return IoDevice::NativeOnWorkerThread(); }
}
bool IoInitialise() { return gIoService.Initialise(); }
void IoShutdown() { gIoService.Shutdown(); }
void IoCachePrioritise(const char** files, int count) { gIoService.CachePrioritise(files, count); }
IoRequest IoFileOpen(IoFile& file, const char* name, IoCallback* callback) {
    return gIoService.Open(file, name, callback);
}
IoRequest IoFileRead(const IoFile& file, void* buffer, U64 offset, U32 size, IoPriority priority, IoCallback* callback) {
    return gIoService.Read(file, buffer, offset, size, priority, callback);
}
IoRequest IoFileClose(const IoFile& file, IoCallback* callback) { return gIoService.Close(file, callback); }
