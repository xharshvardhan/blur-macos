// [Blur2PC][macOS] Desktop commit/reserve fidelity through real MT/TL/JQ workers.
// The queued zero-pair Gravity task tests its real frame, not collision results.
#include <core/Core.h>
#include <core/Maths.h>
#include <core/Maths/HVec/New/IncludePhysics.h>
#include <core/Maths/HVec/New/IncludePhysics.inl>
#include <core/containers.h>
#include <core/threads.h>
#include <system/scheduler/jobqueue.h>
#include <gravity/sdk/Scene.h>
#include <gravity/narrowphase/EPA.h>
#include <gravity/narrowphase/CollisionResult.h>
#include <gravity/narrowphase/ResultSize.h>
#include <atomic>
#include <cstring>
#include <iostream>
#include <new>
#include <pthread.h>
#include <stdexcept>

namespace {
constexpr size_t desktopReserve = 1024 * 1024;
// Exact original CollideMesh local storage families, with native pointer widths.
constexpr size_t collisionStorage = sizeof(Collision::EPA) +
    1024 * sizeof(void*) + sizeof(Collision::EPAFacet) * MaxFacets +
    sizeof(Collision::C_CollisionResult<Static_MaxPatches, Static_MaxContacts>);
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
size_t expectedReserve(U32 commit) {
    return ((size_t(commit) < desktopReserve ? desktopReserve : size_t(commit)) +
            desktopReserve - 1) / desktopReserve * desktopReserve;
}
// Keep storage and page accesses present in optimized builds too. No recursion,
// guessed physics capacity or heap replacement is introduced into the engine.
__attribute__((noinline)) void collisionStorageWorkload() {
    volatile U8* storage = static_cast<volatile U8*>(__builtin_alloca(collisionStorage));
    U64 expected = 0, observed = 0;
    for (size_t i = 0; i < collisionStorage; ++i) {
        storage[i] = U8(i * 13 + 7);
        expected += U8(i * 13 + 7);
    }
    for (size_t i = 0; i < collisionStorage; ++i) observed += storage[i];
    require(observed == expected, "worker stack storage was corrupted");
}
void verifyStack(U32 requested) {
    require(pthread_get_stacksize_np(pthread_self()) >= expectedReserve(requested),
            "PC initial commit incorrectly became a small total native stack");
    collisionStorageWorkload();
}
class StackThread final : public MT::Thread {
public:
    U32 requested = 0;
    void ThreadMain() override { verifyStack(requested); }
};
TL_THREAD_FUNCTION(tlStack) {
    verifyStack(*static_cast<U32*>(ThreadParam));
    TL_THREAD_RETURN(37);
}
struct JobResult { std::atomic<bool> ran{false}; U64 mainID; };
void gravityJob(Scheduler::Task* task) {
    auto& result = *static_cast<JobResult*>(task->Output);
    require(MT::GetThreadId() != result.mainID, "Gravity fixture ran on main instead of JQ worker");
    verifyStack(65536);
    // Construct the actual pointer-bearing parameter type in its original task
    // storage. Zero pairs is valid and still enters the real EPA/alloca frame.
    static_assert(sizeof(Gravity::C_CollisionQuery) <= sizeof(task->ParamData));
    auto* query = new(task->ParamData) Gravity::C_CollisionQuery;
    Gravity::CollideMesh(task);
    query->~C_CollisionQuery();
    result.ran = true;
}
}
int main() { try {
    require(collisionStorage > 65536 && collisionStorage < desktopReserve,
            "source-derived Gravity local storage no longer exposes the small-stack regression");
    for (U32 requested : {U32(0), U32(1), U32(32768), U32(65536),
                          U32(desktopReserve), U32(desktopReserve + 1)}) {
        StackThread worker;
        worker.requested = requested;
        worker.Start(requested, 0, "Blur MT stack contract");
        worker.Join();
        // A joined object must support a new lifecycle with the same policy.
        worker.Start(requested, 0, "Blur MT stack restart");
        worker.Join();
        const auto handle = tlCreateThread(tlStack, reinterpret_cast<INTPTR>(&requested),
            "Blur TL stack contract", 0, requested, TL_THREAD_CREATE_SUSPENDED);
        tlResumeThread(handle);
        require(tlWaitForThread(handle) == 37, "TL stack workload exit code changed");
    }
    using namespace Scheduler;
    const Policy onlyWorker[8] = {0, 1, 0, 0, 0, 0, 0, 0};
    const auto policy = CreatePolicy(onlyWorker);
    const jqModule module(gravityJob, "Original Gravity stack entry");
    for (int cycle = 0; cycle < 2; ++cycle) {
        Init(); SetGroupPoolSize(4); jqEnableWorkers(JQ_CORE_1); Start();
        JobResult result; result.mainID = MT::GetThreadId();
        auto group = CreateGroup(true);
        AddTask(module, nullptr, &result, group, policy);
        Flush(group, false, 5000);
        require(Poll(group) && result.ran, "real Gravity worker entry did not complete");
        DestroyGroup(group); Shutdown();
        require(!Active() && nativeQueueStats().pools == 0, "stack fixture left scheduler owners alive");
    }
    std::cout << "Native MT/TL/JQ PC reserve contract PASS; source-derived collision storage="
              << collisionStorage << "; actual zero-pair CollideMesh returned; not a race\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; } }
