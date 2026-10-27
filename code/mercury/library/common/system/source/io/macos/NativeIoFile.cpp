// [Blur2PC][macOS] Preserve IoFile's shared handle and explicit close API.
#include "../IoImpl.h"
#include <unistd.h>

IoFile::IoFile() : _impl(nullptr) {}
IoFile::IoFile(const IoFile& other) : _impl(other._impl) { IoFileImpl::Acquire(_impl); }
IoFile::IoFile(IoFileImpl* value) : _impl(value) { IoFileImpl::Acquire(_impl); }
IoFile::~IoFile() { IoFileImpl::Release(_impl); }
void IoFile::operator=(const IoFile& other) {
    auto* previous = _impl;
    IoFileImpl::Acquire(other._impl); _impl = other._impl; IoFileImpl::Release(previous);
}
const Btl::String& IoFile::Filename() const {
    static const Btl::String empty; return _impl ? _impl->filename : empty;
}
U64 IoFile::Size() const { return _impl ? _impl->fileSize.load() : 0; }
IoFileImpl::IoFileImpl() : references(0), state(kState_Opening), handle(-1), fileSize(0), device(nullptr) {
    ++Sys::NativeIoCounters().files;
    self = IoFile(this);
}
IoFileImpl::~IoFileImpl() {
    if (handle >= 0) { ::close(handle); --Sys::NativeIoCounters().handles; }
    --Sys::NativeIoCounters().files;
}
void IoFileImpl::Acquire(IoFileImpl* value) {
    if (value) __atomic_add_fetch(&value->references, 1, __ATOMIC_RELAXED);
}
void IoFileImpl::Release(IoFileImpl* value) {
    if (value && __atomic_sub_fetch(&value->references, 1, __ATOMIC_ACQ_REL) == 0) delete value;
}
