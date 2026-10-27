// [Blur2PC][macOS] Real engine allocator and native named-handle static lifetime.
#include "../../code/mercury/library/common/system/SystemPch.h"
#include <core/threads.h>
#include <system/memory/MemoryTrackerDefines.h>
#include <system/memory/MemoryTracker.h>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>

// Textually precede the allocator definition, as the real ray-group/global
// containers do in the game's link order. CPU capacity is retained until exit.
struct NativeGameStaticOwner {
    U32* retained = nullptr;
    ~NativeGameStaticOwner();
};
static NativeGameStaticOwner nativeGameStaticOwner;

// Actual libc++ teardown releases a malloc-backed string after Main's SBA.
// Keep a system allocation until that same ordering boundary without retaining
// or leaking a game-allocator block.
struct EarlySystemStaticOwner {
    void* retained;
    EarlySystemStaticOwner() : retained(std::malloc(32)) { if(!retained)std::abort(); }
    ~EarlySystemStaticOwner();
};
static EarlySystemStaticOwner earlySystemStaticOwner __attribute__((init_priority(101)));

// The actual graph constructs platform registries before the default-priority
// global allocator constructor registers its destructor. Reproduce that order.
struct EarlyNamedRegistries {
    EarlyNamedRegistries() {
        MT::Mutex mutex("early-registry");
        MT::Event event("early-registry");
        MT::Semaphore semaphore("early-registry", 0, 1);
    }
};
static EarlyNamedRegistries earlyNamedRegistries __attribute__((init_priority(101)));
#define ENABLE_SMALL_BLOCK_ALLOCATOR
#include <system/memory/AllocatorFunctions.inl>

EarlySystemStaticOwner::~EarlySystemStaticOwner() {
    if(g_SmallBlockAllocator.Initialized() || malloc_size(retained)==0)std::abort();
    ::operator delete(retained);
    std::puts("Late genuine system allocation released through real global delete PASS");
}

NativeGameStaticOwner::~NativeGameStaticOwner() {
    if(!g_SmallBlockAllocator.Initialized()) {
        std::fputs("Game static owner outlived the original allocator\n",stderr);
        std::abort();
    }
    delete[] retained;
}

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
static unsigned liveBlocks() {
    std::unique_ptr<Memory::BlockAllocator::Stats> stats(g_SmallBlockAllocator.GetStats());
    return stats->m_nAllocs;
}
int main() {
    try {
        require(g_SmallBlockAllocator.Initialized(), "real game allocator is not initialized");
        nativeGameStaticOwner.retained = new U32[16];
        require(g_SmallBlockAllocator.MemoryOwnedByAllocator(nativeGameStaticOwner.retained),
                "Static-owner regression did not retain an actual game small-block allocation");
        require(malloc_size(nativeGameStaticOwner.retained)==0,
                "Original SBA virtual pages overlap the Darwin malloc allocation domain");
        const auto before = liveBlocks();
        const char* name = "native-registry-name-long-enough-to-allocate-key-storage-beyond-small-string-capacity";
        for (unsigned lifetime=0; lifetime<3; ++lifetime) {
            MT::Mutex first(name), same(name);
            require(first.Wait(0) && same.Wait(0), "named recursive mutex sharing failed");
            require(first.Release() && same.Release(), "named mutex ownership failed");
            MT::Event event(name), sameEvent(name);
            require(!event.Wait(0), "new named event reused expired state");
            event.Signal();
            require(sameEvent.Wait(0) && !event.Wait(0), "named auto-reset event sharing failed");
            MT::Semaphore semaphore(name, 1, 2), sameSemaphore(name, 0, 99);
            require(sameSemaphore.Wait(0) && !semaphore.Wait(0), "named semaphore sharing failed");
        }
        require(liveBlocks() == before, "platform registries retained game allocator blocks after handles closed");
        std::puts("Real game allocator/named registry ownership PASS; static destructor exit must also succeed");
        return 0;
    } catch(const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
