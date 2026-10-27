// [Blur2PC][macOS] Completion includes callback release; queues retain requests.
#include "../IoImpl.h"
#include <stdexcept>

IoRequestImpl gIoRequest_Null(kIoError_NullRequest);
IoRequestImpl gIoRequest_InvalidParameters(kIoError_InvalidParameters);
IoRequestImpl gIoRequest_NoDeviceMapping(kIoError_NoDeviceMapping);
IoRequestImpl gIoRequest_OutOfMemory(kIoError_OutOfMemory);
IoRequestImpl gIoRequest_FileNotYetOpen(kIoError_FileNotYetOpen);

IoCallback::IoCallback() = default;
IoCallback::~IoCallback() = default;
IoRequest::IoRequest() : _impl(&gIoRequest_Null) { _impl->Acquire(); }
IoRequest::IoRequest(const IoRequest& other) : _impl(other._impl) { _impl->Acquire(); }
IoRequest::IoRequest(IoRequestImpl* value) : _impl(value) { _impl->Acquire(); }
IoRequest::~IoRequest() { _impl->Release(); }
void IoRequest::operator=(const IoRequest& other) {
    auto* previous = _impl;
    other._impl->Acquire(); _impl = other._impl; previous->Release();
}
void IoRequest::SetPriority(IoPriority value) {
    if (value < kIoPriority_High || value > kIoPriority_Low)
        throw std::invalid_argument("invalid I/O priority");
    _impl->priority = value;
}
void IoRequest::Cancel() {
    std::lock_guard<std::mutex> lock(_impl->nativeMutex);
    if (!_impl->nativeDone && !_impl->nativeStarted) _impl->nativeCancel = true;
}
void IoRequest::Wait() {
    std::unique_lock<std::mutex> lock(_impl->nativeMutex);
    if (_impl->nativeCallbackThread == std::this_thread::get_id() && !_impl->nativeDone)
        throw std::logic_error("an I/O callback cannot wait for its own release");
    _impl->nativeCondition.wait(lock, [&] { return _impl->nativeDone; });
    if (_impl->nativeFailure) std::rethrow_exception(_impl->nativeFailure);
}
IoAction IoRequest::Action() const { return _impl->action; }
const IoFile& IoRequest::File() const { return _impl->file; }
void* IoRequest::Buffer() const { return _impl->buffer; }
U64 IoRequest::Offset() const { return _impl->offset; }
U32 IoRequest::RequestSize() const { return _impl->requestSize; }
U32 IoRequest::ActualSize() const { return _impl->actualSize.load(); }
IoPriority IoRequest::Priority() const { return _impl->priority.load(); }
IoError IoRequest::Error() const { return _impl->error.load(); }

IoRequestImpl::IoRequestImpl(IoAction action_, const IoFile& file_, IoCallback* callback_)
    : references(0), action(action_), file(file_), buffer(nullptr), offset(0), requestSize(0),
      actualSize(0), priority(kIoPriority_Medium), callback(callback_), creation(0), error(kIoError_Okay), link(this) {
    ++Sys::NativeIoCounters().requests;
    self = IoRequest(this);
}
IoRequestImpl::IoRequestImpl(IoError value)
    : references(1), action(kIoAction_None), buffer(nullptr), offset(0), requestSize(0),
      actualSize(0), priority(kIoPriority_Medium), callback(nullptr), creation(0), error(value), link(this) {
    nativeDone = true;
    self = IoRequest(this);
}
IoRequestImpl::~IoRequestImpl() {
    DoUserRelease();
    if (action != kIoAction_None) --Sys::NativeIoCounters().requests;
}
void IoRequestImpl::Acquire() { __atomic_add_fetch(&references, 1, __ATOMIC_RELAXED); }
void IoRequestImpl::Release() {
    if (__atomic_sub_fetch(&references, 1, __ATOMIC_ACQ_REL) == 0) delete this;
}
bool IoRequestImpl::NativeBegin() {
    std::lock_guard<std::mutex> lock(nativeMutex);
    nativeStarted = true; return !nativeCancel;
}
void IoRequestImpl::DoUserCallback(IoEvent value) { if (callback) callback->Event(value, IoRequest(this)); }
void IoRequestImpl::DoUserRelease() {
    auto* value = callback; callback = nullptr;
    if (value) value->Release();
}
void IoRequestImpl::NativeFinish(IoEvent value) {
    IoRequest keepAlive(this);
    {
        std::lock_guard<std::mutex> lock(nativeMutex);
        nativeCallbackThread = std::this_thread::get_id();
    }
    try { DoUserCallback(value); } catch (...) { nativeFailure = std::current_exception(); }
    try { DoUserRelease(); } catch (...) { if (!nativeFailure) nativeFailure = std::current_exception(); }
    // Drop the queue/self reference before notifying Wait; keepAlive pins the
    // completion mutex until this method returns even if the caller releases.
    self = IoRequest();
    {
        std::lock_guard<std::mutex> lock(nativeMutex);
        nativeDone = true;
    }
    nativeCondition.notify_all();
}
