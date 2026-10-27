// [Blur2PC][macOS] Validate actual I_BinaryStream/I_File behavior over disk/memory.
#include <system/serialise/macos/NativeFile.h>
#include <algorithm>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class F> static void rejects(F operation, const char* message) {
    bool failed = false;
    try { operation(); } catch (const std::exception&) { failed = true; }
    require(failed, message);
}
struct Scratch {
    std::filesystem::path path;
    explicit Scratch(const char* parent) {
        std::string pattern = (std::filesystem::path(parent) / "file-fixture-XXXXXX").string();
        require(mkdtemp(pattern.data()) != nullptr, "cannot create fixture directory");
        path = pattern;
    }
    ~Scratch() { std::error_code error; std::filesystem::remove_all(path, error); }
};
int main(int argc, char** argv) {
    try {
        require(argc == 2, "test requires an ignored build directory");
        Scratch scratch(argv[1]);
        const auto path = scratch.path / "buffered.bin";
        std::vector<U8> payload(200003);
        for (size_t i = 0; i < payload.size(); ++i) payload[i] = U8(i * 13 + i / 97);
        {
            auto file = Sys::NativeFile::Open(path, Sys::E_FileAccess_Write, 37);
            Sys::I_File& stream = *file;
            require(stream.Write(payload.data(), payload.size()), "buffered write failed");
            require(stream.GetPosition() == payload.size() && stream.Length() == payload.size(),
                    "write position/length excludes pending bytes");
            stream.SetPosition(123);
            const U8 patch[] = {91, 92, 93};
            require(stream.Write(patch, sizeof(patch)), "write after seek failed");
            std::copy(std::begin(patch), std::end(patch), payload.begin() + 123);
            stream.Flush();
            require(stream.Length() == payload.size(), "overwrite truncated file");
        }
        for (size_t bufferSize : {0, 1, 37, 4096, 65536}) {
            auto file = Sys::NativeFile::Open(path, Sys::E_FileAccess_Read, bufferSize);
            std::vector<U8> actual(payload.size());
            require(file->Read(actual.data(), 17), "initial buffered read failed");
            require(file->Read(actual.data() + 17, actual.size() - 17) && actual == payload,
                    "direct/buffered read changed payload");
            if (!bufferSize) require(file->m_pCurBuffer == file->m_pBufferEnd,
                                     "zero-buffer read performed unwanted read-ahead");
            require(file->GetPosition() == payload.size(), "read position incorrect");
            U8 byte = 0;
            require(!file->Read(&byte, 1), "EOF falsely succeeded");
            file->SetPosition(0);
            require(file->Read(nullptr, 99999) && file->GetPosition() == 99999,
                    "null-target skip failed across buffer boundaries");
            require(file->Read(&byte, 1) && byte == payload[99999], "skip consumed incorrect bytes");
            file->SetPosition(payload.size() - 3);
            U8 tail[4]{};
            require(!file->Read(tail, 4) && file->GetPosition() == payload.size(),
                    "partial EOF position/result failed");
            require(!std::memcmp(tail, payload.data() + payload.size() - 3, 3), "partial EOF data lost");
            rejects([&] { file->SetPosition(payload.size() + 1); }, "seek beyond EOF accepted");
            rejects([&] { file->Overflow(); }, "read-only stream accepted overflow");
        }
        {
            auto append = Sys::NativeFile::Open(path, Sys::E_FileAccess_Append, 7);
            require(append->GetPosition() == payload.size(), "append did not start at EOF");
            require(append->Write(nullptr, 11), "null-source zero write failed");
            append->Flush();
        }
        {
            auto check = Sys::NativeFile::Open(path);
            require(check->Length() == payload.size() + 11, "append length wrong");
            check->SetPosition(payload.size());
            U8 zeros[11]; std::memset(zeros, 1, sizeof(zeros));
            require(check->Read(zeros, sizeof(zeros)), "zero padding read failed");
            for (U8 value : zeros) require(value == 0, "null write did not emit zero bytes");
        }
        {
            const std::string longName(200, 'x');
            auto memory = Sys::NativeFile::FromMemory(payload, longName);
            Sys::I_File& stream = *memory;
            require(stream.Length() == payload.size() && !stream.IsWriting(), "memory length/mode failed");
            require(std::strcmp(stream.Name().Chars(), longName.c_str()) == 0, "engine heap string name failed");
            stream.SetVersionNumber(71); stream.SetEndianSwapFlag(true);
            require(stream.GetVersionNumber() == 71 && stream.GetEndianSwapFlag(), "stream metadata lost");
            require(stream.Read(nullptr, 100001), "memory skip failed");
            U8 byte;
            require(stream.Read(&byte, 1) && byte == payload[100001], "memory seek/read failed");
            stream.SetPosition(0);
            std::vector<U8> actual(payload.size());
            require(stream.Read(actual.data(), actual.size()) && actual == payload, "memory stream payload mismatch");
            require(!stream.Read(&byte, 1), "memory EOF failed");
            auto empty = Sys::NativeFile::FromMemory({}, "empty");
            require(empty->Read(nullptr, 0) && !empty->Read(&byte, 1) && empty->GetPosition() == 0,
                    "empty memory stream failed");
            empty->SetPosition(0);
        }
        // A sparse fixture checks 64-bit offsets without allocating 4 GiB.
        const auto sparsePath = scratch.path / "sparse.bin";
        const U64 last = (U64(1) << 32) + 123;
        const int descriptor = open(sparsePath.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
        require(descriptor >= 0, "cannot create sparse fixture");
        const U8 marker = 0xa7;
        const bool sparseWritten = ftruncate(descriptor, last + 1) == 0 &&
            pwrite(descriptor, &marker, 1, last) == 1;
        close(descriptor);
        require(sparseWritten, "cannot write sparse fixture");
        {
            auto sparse = Sys::NativeFile::Open(sparsePath);
            require(sparse->Length() == last + 1, "file length truncated to 32 bits");
            sparse->SetPosition(last);
            U8 actual = 0;
            require(sparse->Read(&actual, 1) && actual == marker && sparse->GetPosition() == last + 1,
                    "large seek/read truncated to 32 bits");
        }
        rejects([&] { Sys::NativeFile::Open(scratch.path / "missing"); }, "missing file succeeded");
        rejects([&] { Sys::NativeFile::Open(scratch.path); }, "directory accepted as file");
        rejects([&] { Sys::NativeFile::Open(path, Sys::E_FileAccess_Read, 16 * 1024 * 1024 + 1); },
                "oversized native buffer accepted");
        std::cout << "Engine I_File: buffered/direct IO, skip, append, EOF, memory and >4 GiB offsets passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
