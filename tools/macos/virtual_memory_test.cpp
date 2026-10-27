// [Blur2PC][macOS] Actual native mappings and the original small-block allocator.
#include <core/Core.h>
#include <system/memory/macos/NativeVirtualMemory.h>
#include <system/memory/BlockAllocator.h>
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#include <vector>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
struct Reservation {
    void* pointer;
    ~Reservation() { Memory::FreeVirtual(pointer); }
};
static void mappings() {
    using namespace Memory;
    require(nativeVirtualStats().regions == 0, "native mappings leaked before test");
    const size_t page = getpagesize();
    Reservation region{AllocVirtual(3 * 65536)};
    require(region.pointer && reinterpret_cast<INTPTR>(region.pointer) % 65536 == 0,
            "reserve failed to honor the engine's 64 KiB page mask");
    auto* base = static_cast<U8*>(region.pointer);
    require(!AddressReadAccess(base) && nativeVirtualStats().committedBytes == 0,
            "reservation was committed or readable before commit");
    require(AllocPhysical(base, page) == base, "first-page commit failed");
    require(AllocPhysical(base + page, page) == base + page, "adjacent page commit failed");
    require(AddressReadAccess(base) && AddressReadAccess(base + page) && !AddressReadAccess(base + 2 * page),
            "actual Mach page protections disagree with commit tracking");
    for (size_t i = 0; i < page; ++i) require(base[i] == 0, "new committed page is not zero-filled");
    std::memset(base, 0xa5, page);
    std::memset(base + page, 0x4c, page);
    require(AllocPhysical(base, page) == base && base[0] == 0xa5, "idempotent commit destroyed existing contents");
    FreePhysical(base);
    require(!AddressReadAccess(base) && AddressReadAccess(base + page) && base[page] == 0x4c,
            "decommit affected an adjacent independent commit");
    require(AllocPhysical(base, page) == base, "recommit failed");
    for (size_t i = 0; i < page; ++i) require(base[i] == 0, "decommit retained old contents on recommit");
    FreePhysical(base); FreePhysical(base + page);
    require(nativeVirtualStats().committedBytes == 0, "decommit retained committed-byte accounting");
    require(AllocPhysical(base + 2 * page + 7, 32) == base + 2 * page,
            "unaligned commit did not round within its owned reservation");
    FreePhysical(base + 2 * page + 7);
    require(!AddressReadAccess(base + 2 * page), "interior decommit did not remove its tracked span");
    errno = 0;
    require(!AllocPhysical(base + 3 * 65536 - page, 2 * page) && errno == EINVAL,
            "commit escaped the owned virtual reservation");
    require(!AllocPhysical(base, 0), "zero-sized commit succeeded");
    int stack = 3;
    require(AddressReadAccess(&stack) && !AddressReadAccess(nullptr), "Mach read-access query failed on stack/null");
    errno = 0;
    FreeVirtual(&stack);
    require(errno == EINVAL && stack == 3, "foreign virtual free was accepted");
    const auto reserved = nativeVirtualStats().reservedBytes;
    FreeVirtual(base + page);
    require(nativeVirtualStats().reservedBytes == reserved, "interior virtual free released a whole reservation");
    FreeVirtual(region.pointer); region.pointer = nullptr;
    require(nativeVirtualStats().regions == 0, "released virtual region remains tracked");
    Reservation direct{AllocPhysical(nullptr, 123)};
    require(direct.pointer && AddressReadAccess(direct.pointer), "combined reserve/commit failed");
    std::memset(direct.pointer, 5, 123);
    FreePhysical(direct.pointer);
    require(!AddressReadAccess(direct.pointer), "combined region decommit failed");
    FreeVirtual(direct.pointer); direct.pointer = nullptr;
    require(!AllocVirtual(0) && !AllocVirtual(std::numeric_limits<U64>::max()), "invalid reservation succeeded");
}
static void blocks() {
    using namespace Memory;
    // The actual engine defaults in AllocatorFunctions.inl, unchanged on Mac.
    int sizes[] = {16,32,48,64,80,96,112,128,144,160,176,192,208,224,240,256,0};
    BlockAllocator allocator(sizes);
    require(allocator.Initialized(), "original block allocator failed to initialize");
    std::vector<std::pair<void*, U32>> blocks;
    for (U32 size = 1; size <= 256; ++size) {
        void* memory = allocator.Allocate(size);
        require(memory && allocator.MemoryOwnedByAllocator(memory) &&
                allocator.AlocationSize(memory) >= int(size) && reinterpret_cast<INTPTR>(memory) % 16 == 0,
                "block allocation pointer width, area, ownership or alignment failed");
        std::memset(memory, int(size), size);
        blocks.emplace_back(memory, size);
    }
    require(!allocator.Allocate(0) && !allocator.Allocate(257), "unsupported block size succeeded");
    int foreign = 1;
    require(!allocator.Free(&foreign), "block allocator freed a foreign stack pointer");
    for (const auto& item : blocks) {
        const auto* bytes = static_cast<U8*>(item.first);
        require(bytes[0] == U8(item.second) && bytes[item.second - 1] == U8(item.second), "block contents corrupted");
        require(allocator.Free(item.first), "block free failed");
    }
    require(nativeVirtualStats().regions == 0, "empty block pages remain mapped");
    blocks.clear();
    // Exhaust a page to exercise the original full-page/unlink/relink paths.
    for (int i = 0; i < 9000; ++i) blocks.emplace_back(allocator.Allocate(16), 16);
    require(nativeVirtualStats().regions >= 3, "block page-full path was not exercised");
    {
        std::unique_ptr<BlockAllocator::Stats> active(allocator.GetStats());
        require(active->m_nAllocs == 9000 && active->m_iUsedMem == 9000 * 16 &&
                active->m_iTotalMem >= active->m_iUsedMem,
                "original block allocator active allocation totals are incorrect");
    }
    std::reverse(blocks.begin(), blocks.end());
    for (const auto& item : blocks) require(item.first && allocator.Free(item.first), "full-page block free failed");
    std::atomic<bool> good{true};
    std::vector<std::thread> threads;
    for (int thread = 0; thread < 6; ++thread) threads.emplace_back([&, thread] {
        for (int i = 0; i < 1000; ++i) {
            const U32 size = (i * 7 + thread * 19) % 256 + 1;
            auto* memory = static_cast<U8*>(allocator.Allocate(size));
            if (!memory) { good = false; return; }
            std::memset(memory, thread + 1, size);
            if (!allocator.MemoryOwnedByAllocator(memory) || memory[size - 1] != thread + 1 || !allocator.Free(memory))
                good = false;
        }
    });
    for (auto& thread : threads) thread.join();
    require(good, "threaded block allocator lost ownership/data");
    std::unique_ptr<BlockAllocator::Stats> stats(allocator.GetStats());
    require(stats->m_nAllocs == 0 && stats->m_iUsedMem == 0 && stats->m_iTotalMem == 0,
            "original block allocator stats retained freed pages");
    require(nativeVirtualStats().regions == 0 && nativeVirtualStats().committedBytes == 0,
            "block allocator leaked virtual/physical regions");
}
int main() {
    try {
        mappings(); blocks();
        std::cout << "Native VM/original BlockAllocator: protections, zero recommit, 64-bit ownership, full pages and threads passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
