// [Blur2PC][macOS] Exercise the real engine allocator and endian object files.
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
#include <core/Core.h>
#include <core/allocators/CoreMemoryFunctions.h>
#include <core/utility/endian.h>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// This is the assertion callback required by the original endian object. A
// thrown failure is observable in tests; assertions are never silently ignored.
static bool testCoreAssert(const char*, const char*, const char*, int) {
    throw std::runtime_error("original engine debug assertion");
}
// The linked original CoreAssert object owns the callback.

using Allocate = void* (*)(size_t, size_t);
using Free = void (*)(void*);
static void testAllocator(Allocate allocate, Free release, bool querySize) {
    for (std::size_t alignment : {1, 2, 4, 8, 16, 32, 64, 128, 4096}) {
        for (std::size_t size : {1, 15, 127, 8193}) {
            void* pointer = allocate(size, alignment);
            require(pointer != nullptr, "valid aligned allocation failed");
            require(reinterpret_cast<std::uintptr_t>(pointer) % alignment == 0,
                    "allocation alignment mismatch");
            std::memset(pointer, 0xa7, size);
            const auto* bytes = static_cast<unsigned char*>(pointer);
            require(bytes[0] == 0xa7 && bytes[size - 1] == 0xa7, "allocated memory is not writable");
            if (querySize) require(BizNonTrackedAlignedSize(pointer) >= size, "wrong usable size");
            release(pointer);
        }
    }
    for (std::size_t alignment : {0, 3, 6, 24}) {
        errno = 0;
        require(allocate(64, alignment) == nullptr && errno == EINVAL,
                "invalid alignment must fail with EINVAL");
    }
    // POSIX permits either NULL or a freeable pointer for a zero-byte request.
    release(allocate(0, 16));
    release(nullptr);
}

static void testEndian() {
    U16 shortValue = 0x1234;
    U32 wordValue = 0x12345678;
    U64 longValue = 0x0123456789abcdefULL;
    Ut::DoEndianSwap::EndianSwap(shortValue);
    Ut::DoEndianSwap::EndianSwap(wordValue);
    Ut::DoEndianSwap::EndianSwap(longValue);
    require(shortValue == 0x3412 && wordValue == 0x78563412 &&
            longValue == 0xefcdab8967452301ULL, "original in-place byte swap failed");
    alignas(8) const std::array<unsigned char, 8> source{{0, 1, 2, 3, 4, 5, 6, 7}};
    alignas(8) std::array<unsigned char, 8> dest{};
    Ut::DoEndianSwap::EndianSwapQ(dest.data(), source.data());
    require(dest == std::array<unsigned char, 8>{{7, 6, 5, 4, 3, 2, 1, 0}}, "64-bit copy swap failed");
    Ut::DoEndianSwap::EndianSwapD(dest.data(), source.data());
    require(dest[0] == 3 && dest[3] == 0, "32-bit copy swap failed");
    Ut::DoEndianSwap::EndianSwapW(dest.data(), source.data());
    require(dest[0] == 1 && dest[1] == 0, "16-bit copy swap failed");
    std::array<unsigned char, 14> records{{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}};
    Ut::DoEndianSwap::EndianSwap(records.data(), "BSL", 2);
    require(records == std::array<unsigned char, 14>{{0, 2, 1, 6, 5, 4, 3, 7, 9, 8, 13, 12, 11, 10}},
            "original format-driven record swap failed");
    U32 unchanged = 0x12345678;
    Ut::LittleEndianToNative::EndianSwap(unchanged);
    require(unchanged == 0x12345678, "little-endian PC assets must not be byte-swapped on arm64 Mac");
#if !defined(CFG_SPEED) && !defined(CFG_GOLD)
    bool caught = false;
    try { Ut::DoEndianSwap::EndianSwapQ(dest.data(), dest.data()); }
    catch (const std::runtime_error&) { caught = true; }
    require(caught, "original debug assertion must remain active");
#endif
}

int main() {
    g_pCoreAssertFunction = testCoreAssert;
    try {
        testAllocator(BizAlignedMalloc, BizAlignedFree, false);
        testAllocator(BizHeapAlignedMalloc, BizHeapAlignedFree, false);
        testAllocator(BizNonTrackedAlignedMalloc, BizNonTrackedAlignedFree, true);
        require(BizNonTrackedSize(nullptr) == 0 && BizNonTrackedAlignedSize(nullptr) == 0,
                "null allocation size must be zero");
        void* plain = BizNonTrackedMalloc(117);
        require(plain && BizNonTrackedSize(plain) >= 117, "plain allocation/size query failed");
        std::memset(plain, 0x52, 117);
        BizNonTrackedFree(plain);
        BizNonTrackedFree(nullptr);
        testEndian();
        std::atomic<bool> good{true};
        std::vector<std::thread> threads;
        for (int worker = 0; worker < 8; ++worker) threads.emplace_back([&good, worker] {
            for (int i = 0; i < 1000; ++i) {
                void* pointer = BizNonTrackedAlignedMalloc(257, 64);
                if (!pointer || reinterpret_cast<std::uintptr_t>(pointer) % 64 != 0) {
                    good = false;
                    BizNonTrackedAlignedFree(pointer);
                    return;
                }
                std::memset(pointer, worker, 257);
                if (static_cast<unsigned char*>(pointer)[256] != worker) good = false;
                BizNonTrackedAlignedFree(pointer);
            }
        });
        for (auto& thread : threads) thread.join();
        require(good, "concurrent engine allocator check failed");
        std::cout << "Original engine allocator/endian code: PASS (arm64, 8,000 threaded allocations)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Engine core runtime failure: " << error.what() << '\n';
        return 1;
    }
}
