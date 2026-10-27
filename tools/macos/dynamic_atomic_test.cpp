// [Blur2PC] Original particle allocator contracts on authored native objects.
#include <core/Core.h>
#include <core/Maths.h>
#include <core/containers.h>
#include <btl/containers/vector.h>
#include <btl/allocators/pool.h>
#include <dynamic/Atomic.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
struct Element : DynamicEffect::AtomicPoolElement {
    U64 identity = 0;
};
template<class Function> static void workers(Function function) {
    std::array<std::thread, 8> threads;
    for (U32 i = 0; i != threads.size(); ++i) threads[i] = std::thread(function, i);
    for (auto& thread : threads) thread.join();
}
int main() { try {
    // The engine joins allocation jobs before retirement jobs, then drains the
    // deleted list on the owner thread. This does not claim mixed push/pop use.
    constexpr U32 count = 512;
    DynamicEffect::AtomicActiveCache<Element> cache;
    std::array<Element*, count> original{}, allocated{};
    for (U32 i = 0; i != count; ++i) {
        original[i] = new Element;
        original[i]->identity = 0x8000000000000000ull | i;
        cache.Cache(original[i]);
    }
    for (U32 round = 0; round != 32; ++round) {
        workers([&](U32 worker) {
            for (U32 i = worker; i < count; i += 8) allocated[i] = cache.AtomicAllocate();
        });
        auto sorted = allocated;
        std::sort(sorted.begin(), sorted.end(), std::less<Element*>());
        auto expected = original;
        std::sort(expected.begin(), expected.end(), std::less<Element*>());
        require(sorted == expected, "Original particle cache lost, duplicated or truncated a pointer");
        for (Element* element : allocated) {
            require(element->m_VectorPosition >= 0 && element->m_VectorPosition < count &&
                    cache.m_Active[element->m_VectorPosition] == element,
                    "Original particle cache published the wrong active index");
            require((element->identity & 0x8000000000000000ull) != 0,
                    "Original particle identity changed during allocation");
        }
        require(cache.AtomicAllocate() == nullptr, "Exhausted original particle cache returned an element");
        workers([&](U32 worker) {
            for (U32 i = worker; i < count; i += 8) cache.AtomicFree(allocated[i]);
        });
        require(cache.m_ActiveElements == count, "Particle retirement removed active elements before the owner drain");
        cache.Update();
        require(cache.m_ActiveElements == 0 && cache.m_CachedElements == count,
                "Original particle retirement did not restore the cached pool");
    }
    cache.Free();
    require(cache.m_ActiveElements == 0 && cache.m_CachedElements == 0,
            "Original particle cache cleanup left live entries");

    DynamicEffect::AtomicDynamicPool<Element> pool;
    pool.Update();
    const S32 capacity = pool.m_PoolElements;
    require(capacity > 0, "Original particle pool did not allocate a block");
    std::vector<Element*> poolEntries(capacity);
    workers([&](U32 worker) {
        for (U32 i = worker; i < poolEntries.size(); i += 8) poolEntries[i] = pool.AtomicAllocate();
    });
    auto sortedPool = poolEntries;
    std::sort(sortedPool.begin(), sortedPool.end(), std::less<Element*>());
    require(sortedPool.front() && std::adjacent_find(sortedPool.begin(), sortedPool.end()) == sortedPool.end(),
            "Original particle pool returned duplicate or null entries");
    workers([&](U32 worker) {
        for (U32 i = worker; i < poolEntries.size(); i += 8) pool.AtomicFree(poolEntries[i]);
    });
    require(pool.m_PoolElements == capacity, "Concurrent particle pool retirement lost entries");
    workers([&](U32 worker) {
        for (U32 i = worker; i < poolEntries.size(); i += 8) poolEntries[i] = pool.AtomicAllocate();
    });
    auto reusedPool = poolEntries;
    std::sort(reusedPool.begin(), reusedPool.end(), std::less<Element*>());
    require(reusedPool == sortedPool, "Concurrent particle pool retirement corrupted a pointer on reuse");
    // A failed allocation records demand as a negative count; Update repairs it.
    require(pool.AtomicAllocate() == nullptr, "Exhausted original particle pool returned an element");
    pool.Update();
    require(pool.m_PoolElements == capacity && pool.m_TotalElements == capacity * 2,
            "Original particle pool did not grow after exhausted demand");
    for (Element* element : poolEntries) pool.Free(element);
    require(pool.m_PoolElements == capacity * 2, "Original particle pool failed to reclaim its previous block");
    std::cout << "Original particle caches/pools concurrent allocation, pointer width, exhaustion, phased retirement and reuse PASS\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
} }
