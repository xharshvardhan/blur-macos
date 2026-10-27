// [Blur2PC][macOS] Buffered file IO with 64-bit seek and explicit failure paths.
#include <system/serialise/macos/NativeFile.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <sys/stat.h>
#include <unistd.h>

namespace Sys {
namespace {
[[noreturn]] void ioError(const char* operation, const std::string& name) {
    throw std::system_error(errno ? errno : EIO, std::generic_category(),
                            std::string(operation) + ": " + name);
}
void seek(FILE* file, C_StreamPos position, const std::string& name) {
    if (position > static_cast<U64>(std::numeric_limits<off_t>::max()))
        throw std::out_of_range("native file position exceeds off_t");
    if (fseeko(file, static_cast<off_t>(position), SEEK_SET)) ioError("fseeko", name);
}
}
NativeFile::NativeFile(std::string name, size_t bufferSize) : name_(std::move(name)) {
    if (bufferSize > 16 * 1024 * 1024)
        throw std::invalid_argument("native file buffer exceeds 16 MiB");
    // Audio::Core::FileStreamFileWrapper, StreamData and rampak loading request
    // a zero read-buffer hint. Keep a valid pointer anchor while routing every
    // positive read straight to its caller's buffer, without read-ahead.
    unbuffered_ = bufferSize == 0;
    buffer_.resize(std::max<size_t>(bufferSize, 1));
    m_pCurBuffer = m_pBufferEnd = buffer_.data();
}
std::unique_ptr<NativeFile> NativeFile::Open(const std::filesystem::path& path,
        E_FileAccess access, size_t bufferSize) {
    auto result = std::unique_ptr<NativeFile>(new NativeFile(path.string(), bufferSize));
    if (access != E_FileAccess_Read && access != E_FileAccess_Write && access != E_FileAccess_Append)
        throw std::invalid_argument("invalid native file access mode");
    const char* mode = access == E_FileAccess_Write ? "w+b" : (access == E_FileAccess_Read ? "rb" : "r+b");
    result->file_ = fopen(path.c_str(), mode);
    if (!result->file_ && access == E_FileAccess_Append && errno == ENOENT)
        result->file_ = fopen(path.c_str(), "w+b");
    if (!result->file_) ioError("fopen", result->name_);
    if (fcntl(fileno(result->file_), F_SETFD, FD_CLOEXEC) == -1) ioError("fcntl", result->name_);
    struct stat info;
    if (fstat(fileno(result->file_), &info)) ioError("fstat", result->name_);
    if (!S_ISREG(info.st_mode)) throw std::invalid_argument("native stream requires a regular file");
    // This class owns the buffer; avoid a second hidden stdio buffering layer.
    if (setvbuf(result->file_, nullptr, _IONBF, 0)) ioError("setvbuf", result->name_);
    result->writing_ = access != E_FileAccess_Read;
    if (result->writing_) {
        result->m_pBufferEnd = result->buffer_.data() + result->buffer_.size();
        if (access == E_FileAccess_Append) result->offset_ = static_cast<C_StreamPos>(info.st_size);
        seek(result->file_, result->offset_, result->name_);
    }
    return result;
}
std::unique_ptr<NativeFile> NativeFile::FromMemory(std::vector<U8> bytes, std::string name) {
    if (bytes.size() > std::numeric_limits<U32>::max())
        throw std::length_error("memory stream exceeds the engine's contiguous buffer width");
    auto result = std::unique_ptr<NativeFile>(new NativeFile(std::move(name), 1));
    result->memory_ = true;
    result->payload_ = std::move(bytes);
    auto* begin = result->payload_.empty() ? result->buffer_.data() : result->payload_.data();
    result->m_pCurBuffer = begin;
    result->m_pBufferEnd = begin + result->payload_.size();
    return result;
}
NativeFile::~NativeFile() {
    if (!file_) return;
    if (writing_) {
        // Destruction cannot throw. Call Flush explicitly to observe save errors.
        try { Flush(); }
        catch (const std::exception& error) { fprintf(stderr, "native file flush failed: %s\n", error.what()); }
    }
    fclose(file_);
}
const C_StreamSize NativeFile::Length() {
    if (memory_) return payload_.size();
    struct stat info;
    if (fstat(fileno(file_), &info)) ioError("fstat", name_);
    const auto disk = static_cast<C_StreamSize>(info.st_size);
    return writing_ ? std::max(disk, GetPosition()) : disk;
}
const C_StreamPos NativeFile::GetPosition() const {
    const U8* begin = memory_ && !payload_.empty() ? payload_.data() : buffer_.data();
    return offset_ + static_cast<C_StreamPos>(m_pCurBuffer - begin);
}
void NativeFile::ClearReadBuffer(C_StreamPos position) {
    offset_ = position;
    m_pCurBuffer = m_pBufferEnd = buffer_.data();
}
void NativeFile::SetPosition(C_StreamPos position) {
    if (position > Length()) throw std::out_of_range("native seek past end of stream");
    if (memory_) {
        U8* begin = payload_.empty() ? buffer_.data() : payload_.data();
        m_pCurBuffer = begin + position;
    } else if (writing_) {
        if (!Overflow()) ioError("write before seek", name_);
        offset_ = position;
        seek(file_, position, name_);
    } else {
        seek(file_, position, name_);
        ClearReadBuffer(position);
    }
}
bool NativeFile::Underflow(UnderflowContext& context) {
    if (writing_) throw std::logic_error("read from a write stream");
    context.bytesRead = 0;
    if (memory_) return false;
    const auto position = GetPosition();
    seek(file_, position, name_);
    ClearReadBuffer(position);
    if (unbuffered_ && !context.readTarget && context.readSize) {
        const auto length = Length();
        if (position >= length) return false;
        context.bytesRead = static_cast<U32>(std::min<U64>(context.readSize, length - position));
        offset_ += context.bytesRead;
        seek(file_, offset_, name_);
        return true;
    }
    if (context.readTarget && context.readSize >= buffer_.size()) {
        context.bytesRead = static_cast<U32>(fread(context.readTarget, 1, context.readSize, file_));
        offset_ += context.bytesRead;
        if (ferror(file_)) ioError("fread", name_);
        return context.bytesRead != 0;
    }
    const auto received = fread(buffer_.data(), 1, buffer_.size(), file_);
    if (ferror(file_)) ioError("fread", name_);
    m_pBufferEnd = buffer_.data() + received;
    return received != 0;
}
bool NativeFile::Overflow() {
    if (!writing_) throw std::logic_error("write to a read stream");
    const size_t pending = static_cast<size_t>(m_pCurBuffer - buffer_.data());
    if (pending) {
        seek(file_, offset_, name_);
        const size_t written = fwrite(buffer_.data(), 1, pending, file_);
        offset_ += written;
        const size_t remaining = pending - written;
        if (remaining) memmove(buffer_.data(), buffer_.data() + written, remaining);
        m_pCurBuffer = buffer_.data() + remaining;
        if (remaining || ferror(file_)) return false;
    }
    m_pCurBuffer = buffer_.data();
    m_pBufferEnd = buffer_.data() + buffer_.size();
    return true;
}
void NativeFile::Flush() {
    if (!writing_) return;
    if (!Overflow() || fflush(file_) || fsync(fileno(file_))) ioError("flush", name_);
}
}
