// [Blur2PC][macOS] Exercise the real engine async API, callbacks and stream layer.
#include <core/Core.h>
#include <core/threads/Misc.h>
#include <system/serialise/I_File.h>
#include <system/serialise/C_FileSystem.h>
#include <system/serialise/macos/NativeFile.h>
#include <memory>
#include <system/io/ASync.h>
#include <system/io/macos/NativeIo.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#include <vector>

static void require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
template<class F> static void await(F condition) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    while (!condition()) {
        require(std::chrono::steady_clock::now() < deadline, "native async progress timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
struct Callback : IoCallback {
    std::atomic<int> events{0}, releases{0};
    std::atomic<IoEvent> last{kIoEvent_Failed};
    std::atomic<U64> thread{0};
    void Event(IoEvent value, const IoRequest&) override {
        last = value; thread = MT::GetThreadId(); ++events;
    }
    void Release() override { ++releases; }
    void check(IoEvent expected) {
        require(events == 1 && releases == 1 && last == expected, "callback event/release count changed");
    }
};
struct Gate : Callback {
    std::mutex mutex;
    std::condition_variable condition;
    bool entered = false, opened = false, gateRelease = false;
    explicit Gate(bool release = false) : gateRelease(release) {}
    void Block() {
        std::unique_lock<std::mutex> lock(mutex); entered = true; condition.notify_all();
        require(condition.wait_for(lock, std::chrono::seconds(8), [&] { return opened; }), "callback gate timed out");
    }
    void Event(IoEvent value, const IoRequest& request) override {
        Callback::Event(value, request); if (!gateRelease) Block();
    }
    void Release() override { if (gateRelease) Block(); Callback::Release(); }
    void WaitEntered() {
        std::unique_lock<std::mutex> lock(mutex);
        require(condition.wait_for(lock, std::chrono::seconds(8), [&] { return entered; }), "worker never entered callback");
    }
    void Open() { std::lock_guard<std::mutex> lock(mutex); opened = true; condition.notify_all(); }
    ~Gate() { Open(); }
};
static void resourcesEmpty() {
    const auto counts = Sys::GetNativeIoStats();
    require(counts.files == 0 && counts.requests == 0 && counts.handles == 0 && counts.workers == 0,
            "native async resources survived shutdown");
}
static void requests(const std::filesystem::path& path, const std::vector<U8>& expected) {
    IoFile file;
    Callback stopped;
    IoFileOpen(file, path.c_str(), &stopped).Wait();
    stopped.check(kIoEvent_Failed);
    require(IoInitialise(), "I/O service initialization failed");
    struct Shutdown { ~Shutdown() { IoShutdown(); } } shutdown;
    Callback opened;
    auto open = IoFileOpen(file, path.c_str(), &opened); open.Wait();
    opened.check(kIoEvent_Complete);
    require(open.Error() == kIoError_Okay && file.Size() == expected.size() &&
            opened.thread != MT::GetThreadId(), "open did not complete on a native worker");
    // Exercise the actual framework's filesystem entry point, including the
    // original-case resolver and an unbuffered read hint.
    Sys::C_OpenParams params;
    params.m_BufferSizeHint = 0; params.m_NumBuffers = 1;
    std::string folded = path.string();
    for (auto& c : folded) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    {
        std::unique_ptr<Sys::I_File> stream(Sys::C_FileSystem::Instance().Open(folded.c_str(), Sys::E_FileAccess_Read, params));
        U8 one = 0;
        require(dynamic_cast<AsyncFile*>(stream.get()) && stream->Read(&one, 1) && one == expected[0],
                "C_FileSystem bypassed live async service/original-case resolution");
    }
    {
        struct ReentrantFs : Callback {
            const std::filesystem::path* path = nullptr; bool good = false;
            void Event(IoEvent event, const IoRequest& request) override {
                Callback::Event(event, request);
                std::unique_ptr<Sys::I_File> stream(Sys::C_FileSystem::Instance().Open(path->c_str()));
                U8 byte = 0;
                good = dynamic_cast<Sys::NativeFile*>(stream.get()) && stream->Read(&byte, 1) && byte == 7;
            }
        } callback;
        callback.path = &path; U8 one = 0;
        IoFileRead(file, &one, 0, 1, kIoPriority_Medium, &callback).Wait();
        require(callback.good, "filesystem callback reentry queued onto its own worker");
    }
    std::atomic<bool> good{true}; std::vector<std::thread> callers;
    for (int t = 0; t < 8; ++t) callers.emplace_back([&, t] {
        try {
            for (int n = 0; n < 60; ++n) {
                std::vector<U8> bytes(513 + t); Callback callback;
                const auto offset = (n * 1013 + t * 61) % (expected.size() - bytes.size());
                auto request = IoFileRead(file, bytes.data(), offset, U32(bytes.size()), kIoPriority_Low, &callback);
                request.SetPriority(kIoPriority_High); request.Wait(); callback.check(kIoEvent_Complete);
                if (request.ActualSize() != bytes.size() ||
                    !std::equal(bytes.begin(), bytes.end(), expected.begin() + offset)) good = false;
            }
        } catch (...) { good = false; }
    });
    for (auto& caller : callers) caller.join();
    require(good, "concurrent offset reads lost bytes/callback ownership");
    U8 byte = 0;
    {
        Gate gate;
        auto active = IoFileRead(file, &byte, 0, 1, kIoPriority_Medium, &gate);
        gate.WaitEntered();
        std::vector<U8> cancelledBytes(64, 0xcc); Callback cancelled;
        auto request = IoFileRead(file, cancelledBytes.data(), 111, U32(cancelledBytes.size()), kIoPriority_Low, &cancelled);
        request.Cancel(); gate.Open(); active.Wait(); request.Wait();
        cancelled.check(kIoEvent_Failed);
        require(request.Error() == kIoError_Unknown && request.ActualSize() == 0 &&
                std::all_of(cancelledBytes.begin(), cancelledBytes.end(), [](U8 v) { return v == 0xcc; }),
                "pending cancellation accessed the caller buffer");
    }
    {
        Gate release(true);
        auto request = IoFileRead(file, &byte, 1, 1, kIoPriority_Medium, &release);
        release.WaitEntered(); std::atomic<bool> returned{false};
        std::thread waiter([&] { request.Wait(); returned = true; });
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
        const bool returnedEarly = returned;
        release.Open(); waiter.join();
        require(!returnedEarly && returned, "Wait returned before callback Release completed");
        release.check(kIoEvent_Complete);
    }
    {
        struct SelfWait : Callback {
            bool rejected = false;
            void Event(IoEvent event, const IoRequest& request) override {
                Callback::Event(event, request);
                try { IoRequest copy(request); copy.Wait(); }
                catch (const std::logic_error&) { rejected = true; }
            }
        } callback;
        IoFileRead(file, &byte, 2, 1, kIoPriority_Medium, &callback).Wait();
        require(callback.rejected, "callback self-wait was allowed to deadlock");
    }
    {
        struct Throwing : Callback {
            void Event(IoEvent event, const IoRequest& request) override {
                Callback::Event(event, request); throw std::runtime_error("owned test callback failure");
            }
        } callback;
        auto request = IoFileRead(file, &byte, 3, 1, kIoPriority_Medium, &callback);
        bool threw = false; try { request.Wait(); } catch (const std::runtime_error&) { threw = true; }
        require(threw && callback.releases == 1, "callback failure lost release/exception");
        IoFileRead(file, &byte, 4, 1, kIoPriority_Medium, nullptr).Wait();
    }
    {
        Gate gate;
        auto active = IoFileRead(file, &byte, 0, 1, kIoPriority_Medium, &gate);
        gate.WaitEntered();
        std::vector<U8> bytes(65539); Callback readCallback, closeCallback;
        auto read = IoFileRead(file, bytes.data(), 7, U32(bytes.size()), kIoPriority_Low, &readCallback);
        auto close = IoFileClose(file, &closeCallback);
        gate.Open(); active.Wait(); read.Wait(); close.Wait();
        readCallback.check(kIoEvent_Complete); closeCallback.check(kIoEvent_Complete);
        require(read.Error() == kIoError_Okay && std::equal(bytes.begin(), bytes.end(), expected.begin() + 7) &&
                file.Size() == 0, "close overtook an earlier read");
    }
    open = IoRequest(); file = IoFile();
}
static void sparse(const std::filesystem::path& path) {
    const int fd = ::open(path.c_str(), O_CREAT | O_TRUNC | O_RDWR | O_CLOEXEC, 0600);
    require(fd >= 0, "sparse fixture open failed");
    const U64 offset = 0x100000000ULL + 123;
    const char marker[] = "Native large offset";
    const auto written = pwrite(fd, marker, sizeof(marker), static_cast<off_t>(offset)); ::close(fd);
    require(written == sizeof(marker), "sparse fixture write failed");
    require(IoInitialise(), "sparse service init failed");
    {
        IoFile file; auto opened = IoFileOpen(file, path.c_str(), nullptr); opened.Wait();
        require(file.Size() == offset + sizeof(marker), "large file size narrowed");
        char bytes[sizeof(marker)]{};
        auto read = IoFileRead(file, bytes, offset, sizeof(bytes), kIoPriority_High, nullptr); read.Wait();
        require(read.ActualSize() == sizeof(bytes) && !std::memcmp(bytes, marker, sizeof(bytes)), "large offset read narrowed");
        auto eof = IoFileRead(file, bytes, file.Size(), 1, kIoPriority_Medium, nullptr); eof.Wait();
        require(eof.Error() == kIoError_EndOfFile && !eof.ActualSize(), "zero-byte EOF was reported as a completed read");
        IoFileClose(file, nullptr).Wait();
    }
    IoShutdown(); resourcesEmpty();
}
static void stream(const std::filesystem::path& path, const std::vector<U8>& expected) {
    require(IoInitialise(), "stream service init failed");
    for (int buffer : {0, 1, 4096}) for (U32 buffers : {1, 2, 10}) {
        AsyncFile file;
        require(file.Open(path.c_str(), buffer, false, buffers), "real AsyncFile failed open");
        require(file.Length() == expected.size(), "async stream length changed");
        std::vector<U8> bytes(33333);
        require(file.Read(bytes.data(), U32(bytes.size())) &&
                std::equal(bytes.begin(), bytes.end(), expected.begin()) && file.GetPosition() == bytes.size(),
                "buffered/read-through AsyncFile lost bytes or logical position");
        require(file.Read(nullptr, 71) && file.GetPosition() == bytes.size() + 71, "async null-target skip failed");
        file.SetPosition(expected.size() - 5);
        U8 tail[8]{};
        require(!file.Read(tail, sizeof(tail)) && file.GetPosition() == expected.size() &&
                std::equal(tail, tail + 5, expected.end() - 5), "async partial EOF advanced past fulfilled bytes");
        file.SetPosition(19); U8 one = 0;
        require(file.Read(&one, 1) && one == expected[19], "async seek after EOF failed");
        file.SetVersionNumber(0x12345678); file.SetEndianSwapFlag(true);
        require(file.GetVersionNumber() == 0x12345678 && file.GetEndianSwapFlag(), "async stream metadata changed");
    }
    {
        AsyncFile missing;
        require(!missing.Open((path.string() + ".missing").c_str(), 2048, false), "missing async stream opened");
    }
    IoShutdown(); resourcesEmpty();
}
static void shutdownPending(const std::filesystem::path& path) {
    require(IoInitialise(), "shutdown service init failed");
    {
        IoFile file; auto opened = IoFileOpen(file, path.c_str(), nullptr); opened.Wait();
        U8 byte = 0; Gate gate;
        auto active = IoFileRead(file, &byte, 0, 1, kIoPriority_Medium, &gate); gate.WaitEntered();
        std::vector<U8> bytes(16, 0xaa); std::vector<IoRequest> queued;
        struct Reentrant : Callback {
            IoFile* file = nullptr; U8 byte = 0; bool rejected = false;
            void Event(IoEvent event, const IoRequest& request) override {
                Callback::Event(event, request);
                auto next = IoFileRead(*file, &byte, 0, 1, kIoPriority_Medium, nullptr);
                next.Wait(); rejected = next.Error() == kIoError_NoDeviceMapping;
            }
        } callback;
        callback.file = &file;
        queued.push_back(IoFileRead(file, &bytes[0], 10, 1, kIoPriority_Low, &callback));
        for (int i = 1; i < 16; ++i) queued.push_back(IoFileRead(file, &bytes[i], i, 1, kIoPriority_Low, nullptr));
        std::thread stopping([] { IoShutdown(); });
        await([&] { return !IoInitialise(); }); // Gate keeps shutdown live, so false proves admission closed.
        std::atomic<bool> secondReturned{false};
        std::thread secondStop([&] { IoShutdown(); secondReturned = true; });
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
        const bool returnedBeforeJoin = secondReturned;
        gate.Open(); stopping.join(); secondStop.join(); active.Wait();
        require(!returnedBeforeJoin && secondReturned, "concurrent shutdown returned before workers joined");
        for (auto& request : queued) { request.Wait(); require(request.Error() == kIoError_Unknown, "shutdown executed pending read"); }
        callback.check(kIoEvent_Failed);
        require(callback.rejected && file.Size() == 0 &&
                std::all_of(bytes.begin(), bytes.end(), [](U8 v) { return v == 0xaa; }),
                "shutdown callback reentry/handle cleanup changed");
        require(IoInitialise(), "service could not restart after cancellation drain");
        auto stale = IoFileRead(file, &byte, 0, 1, kIoPriority_Medium, nullptr); stale.Wait();
        require(stale.Error() == kIoError_FileNotYetOpen, "old file reached a freed/reused device");
        IoShutdown();
    }
    resourcesEmpty();
}
int main(int argc, char** argv) {
    try {
        require(argc == 2, "fixture root argument missing"); resourcesEmpty();
        const auto root = std::filesystem::path(argv[1]) / "async-fixture";
        std::filesystem::create_directories(root);
        const auto path = root / "MixedCaseData.bin";
        std::vector<U8> expected(512 * 1024 + 19);
        for (std::size_t i = 0; i < expected.size(); ++i) expected[i] = U8(i * 31 + 7);
        { std::ofstream output(path, std::ios::binary); output.write(reinterpret_cast<const char*>(expected.data()), expected.size()); }
        requests(path, expected); resourcesEmpty();
        sparse(root / "SparseData.bin"); stream(path, expected); shutdownPending(path);
        std::filesystem::remove_all(root);
        std::cout << "Native queued I/O, completion/cancel/shutdown and original AsyncFile PASS\n";
    } catch (const std::exception& error) { IoShutdown(); std::cerr << error.what() << '\n'; return 1; }
}
