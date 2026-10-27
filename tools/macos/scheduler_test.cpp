// [Blur2PC][macOS] Real TL lifetimes, original queue and scheduler job execution.
#include <core/Core.h>
#include <core/threads.h>
#include <core/containers.h>
#include <system/scheduler/jobqueue.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class F> static void rejects(F operation, const char* message) {
    bool threw = false;
    try { operation(); } catch (...) { threw = true; }
    require(threw, message);
}
struct ThreadData {
    std::atomic<bool> called{false};
    tlThreadHandle own = 0;
    U64 identifier = 0;
};
static TL_THREAD_FUNCTION(entry) {
    auto& data = *static_cast<ThreadData*>(ThreadParam);
    data.own = tlGetCurrentThread();
    data.identifier = tlGetCurrentThreadId();
    data.called = true;
    TL_THREAD_RETURN(73);
}
static TL_THREAD_FUNCTION(exitEntry) {
    auto& unwound = *static_cast<std::atomic<bool>*>(ThreadParam);
    struct Unwind { std::atomic<bool>& flag; ~Unwind() { flag = true; } } guard{unwound};
    tlExitThread(91);
}
static TL_THREAD_FUNCTION(failingEntry) { throw std::runtime_error("TL worker failure"); }
struct TLGuard {
    tlThreadHandle handle;
    ~TLGuard() {
        if (!handle) return;
        try { tlResumeThread(handle); tlWaitForThread(handle); } catch (...) {}
    }
    U32 join() { const auto result = tlWaitForThread(handle); handle = 0; return result; }
};
static void lifecycle() {
    ThreadData data;
    TLGuard worker{tlCreateThread(entry, reinterpret_cast<INTPTR>(&data), "Blur TL suspend test",
                                 0, 128 * 1024, TL_THREAD_CREATE_SUSPENDED)};
    const auto handle = worker.handle;
    require(tlIsThreadRunning(handle), "suspended worker lost its running state");
    tlSleep(20);
    require(!data.called, "suspended worker executed before resume");
    tlResumeThread(handle);
    require(worker.join() == 73 && data.called && data.own == handle && data.identifier != tlGetCurrentThreadId(),
            "TL worker handle, parameter, ID or exit code changed");
    rejects([&] { tlWaitForThread(handle); }, "closed TL handle remained joinable");
    std::atomic<bool> unwound{false};
    TLGuard explicitExit{tlCreateThread(exitEntry, reinterpret_cast<INTPTR>(&unwound), 0,
                                      128 * 1024, 0, "Blur TL exit test")};
    require(explicitExit.handle != handle && explicitExit.join() == 91 && unwound,
            "explicit TL exit lost its code or stack cleanup");
    const auto failure = tlCreateThread(failingEntry, 0, "Blur TL failure test");
    rejects([&] { tlWaitForThread(failure); }, "TL worker exception was swallowed");
    rejects([&] { tlResumeThread(handle); }, "stale TL handle resumed another thread");
    rejects([] { tlCreateThread(nullptr, 0, "invalid"); }, "null TL entry accepted");
    rejects([] { tlWaitForThread(tlGetCurrentThread()); }, "main pseudo-handle accepted for join");
}
static void queueStress() {
    using namespace Scheduler;
    require(nativeQueueStats().pools == 0, "queue pools already leaked before test");
    jqAtomicQueue queue, sharing;
    queue.Init(); sharing.Init(&queue);
    constexpr int producers = 4, each = 2000, total = producers * each;
    std::vector<std::atomic<int>> seen(total);
    for (auto& count : seen) count = 0;
    std::atomic<int> consumed{0};
    std::atomic<bool> good{true};
    std::vector<std::thread> threads;
    for (int p = 0; p < producers; ++p) threads.emplace_back([&, p] {
        for (int i = 0; i < each; ++i) {
            Task task;
            task.ParamData[0] = U32(p * each + i);
            task.Input = &queue;
            queue.Push(task);
        }
    });
    for (int c = 0; c < 4; ++c) threads.emplace_back([&] {
        while (consumed.load() < total) {
            Task task;
            if (queue.Pop(&task)) {
                const auto id = task.ParamData[0];
                if (id >= total || task.Input != &queue) good = false;
                else ++seen[id];
                ++consumed;
            } else std::this_thread::yield();
        }
    });
    for (auto& thread : threads) thread.join();
    require(good && consumed == total, "concurrent queue corrupted task pointers/data");
    for (const auto& count : seen) require(count == 1, "concurrent queue duplicated or lost a task");
    Task task;
    require(!queue.Pop(&task), "drained queue is not empty");
    for (int i = 0; i < 100; ++i) { task.ParamData[0] = i; sharing.Push(task); }
    for (int i = 0; i < 100; ++i)
        require(sharing.Pop(&task) && task.ParamData[0] == U32(i), "shared-pool FIFO order changed");
    const auto active = nativeQueueStats();
    require(active.pools == 1 && active.queues == 2 && active.blocks != 0, "native queue accounting failed");
    queue.Shutdown();
    require(nativeQueueStats().pools == 1, "owner shutdown freed a live shared pool");
    sharing.Shutdown();
    require(nativeQueueStats().pools == 0 && nativeQueueStats().blocks == 0,
            "native queue shutdown leaked node blocks");
}
struct Parameters {
    U64 magic;
    void* pointers[8];
    U32 index, value;
};
static_assert(sizeof(Parameters) > 48 && sizeof(Parameters) <= sizeof(Scheduler::Task::ParamData));
struct JobState {
    std::vector<std::atomic<int>> seen;
    std::atomic<int> stage{0}, ran{0};
    std::atomic<bool> good{true};
    U64 mainID;
    int expectedStage = 0;
    explicit JobState(int size) : seen(size), mainID(tlGetCurrentThreadId()) {
        for (auto& count : seen) count = 0;
    }
};
static void firstStage(Scheduler::Task* task) {
    auto& state = *static_cast<JobState*>(task->Output);
    ++state.stage;
}
static void actualJob(Scheduler::Task* task) {
    auto& state = *static_cast<JobState*>(task->Output);
    Parameters params;
    std::memcpy(&params, task->ParamData, sizeof(params));
    if (params.magic != 0x123456789abcdef0ULL || params.pointers[7] != &state ||
        state.stage != state.expectedStage || params.index >= state.seen.size() ||
        Scheduler::GetWorkerID() != 3 || tlGetCurrentThreadId() == state.mainID ||
        Scheduler::jqGetBatchForCurrentThread() != task) state.good = false;
    else ++state.seen[params.index];
    ++state.ran;
}
static void ungroupedJob(Scheduler::Task* task) { ++*static_cast<std::atomic<int>*>(task->Output); }
static void engineJobs() {
    using namespace Scheduler;
    static const Policy workerThree[8] = {0, 0, 0, 1, 0, 0, 0, 0};
    static const Policy anyCPU[8] = {1, 1, 1, 1, 1, 1, 1, 1};
    const auto workerPolicy = CreatePolicy(workerThree);
    const auto anyPolicy = CreatePolicy(anyCPU);
    const jqModule phase1(firstStage, "Native fixture dependency");
    const jqModule phase2(actualJob, "Native fixture task");
    const jqModule standalone(ungroupedJob, "Native ungrouped fixture");
    for (int cycle = 0; cycle < 3; ++cycle) {
        Init();
        SetGroupPoolSize(16);
        jqEnableWorkers(JQ_CORE_1 | JQ_CORE_3);
        Start();
        require(GetNumWorkers() == 2, "native scheduler worker mask/count changed");
        constexpr int jobs = 2000;
        JobState state(jobs);
        state.expectedStage = 300;
        auto before = CreateGroup(true), after = CreateGroup(true);
        for (int i = 0; i < state.expectedStage; ++i) AddTask(phase1, nullptr, &state, before, workerPolicy);
        AddGroupDependency(after, before);
        for (int i = 0; i < jobs; ++i) {
            Parameters params{};
            params.magic = 0x123456789abcdef0ULL;
            params.pointers[7] = &state;
            params.index = i;
            AddTask(phase2, nullptr, &state, after, workerPolicy, &params, sizeof(params));
        }
        Flush(after, false, 5000);
        require(Poll(after) && state.ran == jobs && state.good, "native jobs/dependencies/64-bit parameters failed");
        for (const auto& count : state.seen) require(count == 1, "job executed more or less than once");
        std::atomic<int> ungrouped{0};
        for (int i = 0; i < 100; ++i) AddTask(standalone, nullptr, &ungrouped, JQ_GROUP_NONE, anyPolicy);
        Flush();
        require(ungrouped == 100 && Poll(), "ungrouped task completion failed");
        DestroyGroup(after); DestroyGroup(before);
        Shutdown();
        require(!Active() && nativeQueueStats().pools == 0, "scheduler shutdown left live queue pools");
    }
}
int main() {
    try {
        lifecycle(); queueStress(); engineJobs();
        std::cout << "Original scheduler: TL lifecycle, queue x8000, workers, dependencies, payloads and shutdown passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
