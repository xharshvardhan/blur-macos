// [Blur2PC][macOS] Native disk/memory streams implementing the real Sys::I_File.
#pragma once
#include <core/Core.h>
#include <system/serialise/I_File.h>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace Sys {
class NativeFile final : public I_File {
public:
    static std::unique_ptr<NativeFile> Open(const std::filesystem::path& path,
        E_FileAccess access = E_FileAccess_Read, size_t bufferSize = 64 * 1024);
    static std::unique_ptr<NativeFile> FromMemory(std::vector<U8> bytes, std::string name);
    ~NativeFile() override;
    const C_StreamSize Length() override;
    const C_StreamPos GetPosition() const override;
    void SetPosition(C_StreamPos position) override;
    const bool IsWriting() const override { return writing_; }
    Btl::String Name() const override { return Btl::String(name_.c_str()); }
    bool Underflow(UnderflowContext& context) override;
    bool Overflow() override;
    const bool GetEndianSwapFlag() const override { return swap_; }
    void SetEndianSwapFlag(bool swap) override { swap_ = swap; }
    U32 GetVersionNumber() const override { return version_; }
    void SetVersionNumber(U32 version) override { version_ = version; }
    void Flush() override;
private:
    NativeFile(std::string name, size_t bufferSize);
    NativeFile(const NativeFile&) = delete;
    NativeFile& operator=(const NativeFile&) = delete;
    void ClearReadBuffer(C_StreamPos position);
    FILE* file_ = nullptr;
    std::string name_;
    std::vector<U8> buffer_, payload_;
    C_StreamPos offset_ = 0;
    bool writing_ = false, memory_ = false, swap_ = false, unbuffered_ = false;
    U32 version_ = 0;
};
}
