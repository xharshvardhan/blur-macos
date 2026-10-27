// [Blur2PC][macOS] Behavioral tests of the engine's public MT interfaces.
#include <core/Core.h>
#include <core/threads.h>
#include <core/threads/Atomic.h>
#include <core/threads/AtomicMutex.h>
#include <core/threads/ThreadLocalStorage.h>
#include <core/threads/misc.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class F> static void rejects(F operation, const char* message) {
    bool threw = false;
    try { operation(); } catch (const std::exception&) { threw = true; }
    require(threw, message);
}
static void atomics() {
    volatile U32 word = 3;
    require(MT::AtomicAdd(&word, 4) == 3 && MT::AtomicGet(&word) == 7, "atomic fetch-add contract");
    require(MT::AtomicAnd(&word, 6) == 7 && MT::AtomicOr(&word, 8) == 6, "atomic bitwise contract");
    require(!MT::AtomicCompareAndSwap(&word, 99, 3) && MT::AtomicGet(&word) == 14, "failed CAS changed storage");
    require(MT::AtomicCompareAndSwap(&word, 99, 14), "successful CAS failed");
    MT::AtomicSet(&word, 0);
    volatile int count = 8;
    require(MT::AtomicIncrement(&count) == 8 && MT::AtomicDecrement(&count) == 9, "increment must fetch old value");
    volatile U64 wide = 0x100000000ULL;
    require(MT::AtomicAdd(&wide, U64(2)) == 0x100000000ULL, "64-bit add truncated");
    require(MT::AtomicCompareAndSwap(&wide, U64(8), U64(0x100000002ULL)), "64-bit CAS truncated");
    require(MT::AtomicOr(&wide, U64(3)) == 8 && MT::AtomicAnd(&wide, U64(7)) == 11, "64-bit bitwise contract");
    MT::AtomicSet(&wide, 0);
    volatile S64 signedWide = -5;
    require(MT::AtomicIncrement(&signedWide) == -5 && MT::AtomicDecrement(&signedWide) == -4, "signed 64-bit increment");
    volatile INTPTR pointerWord=INTPTR(0x100000000ULL);
    require(MT::AtomicAdd(&pointerWord,INTPTR(17))==INTPTR(0x100000000ULL)&&MT::AtomicGet(&pointerWord)==INTPTR(0x100000011ULL),"LP64 native fetch-add truncated a pointer-sized offset");
    constexpr int workers = 8, iterations = 5000;
    std::vector<std::vector<U32>> observed(workers);
    std::vector<std::thread> threads;
    for (auto& values : observed) threads.emplace_back([&values, &word] {
        values.resize(iterations);
        for (auto& value : values) value = MT::AtomicAdd(&word, U32(1));
        MT::MemoryFence();
    });
    for (auto& thread : threads) thread.join();
    std::vector<U32> all;
    for (auto& values : observed) all.insert(all.end(), values.begin(), values.end());
    std::sort(all.begin(), all.end());
    require(MT::AtomicGet(&word) == workers * iterations, "atomic updates lost under contention");
    for (size_t i = 0; i < all.size(); ++i) require(all[i] == i, "fetch-add values are not unique");
}
static void synchronization() {
    MT::Mutex first("mac-test-mutex"), second("mac-test-mutex");
    require(first.Wait(0) && second.Wait(0), "named mutex must share recursive ownership");
    std::atomic<bool> denied{false}, wrongOwnerRejected{false};
    std::thread contender([&] {
        denied = !second.Wait(20);
        wrongOwnerRejected = !second.Release();
    });
    contender.join();
    require(denied && wrongOwnerRejected, "mutex contention or ownership failed");
    require(first.Release() && second.Release() && !second.Release(), "recursive release depth failed");
    MT::Lock critical;
    critical.Enter();
    require(critical.TryEnter(), "critical section must be recursive");
    critical.Exit(); critical.Exit();

    MT::Event event("mac-test-event"), sameEvent("mac-test-event");
    require(!event.Wait(0), "event must start clear");
    event.Signal(); event.Signal();
    require(sameEvent.Wait(0) && !event.Wait(0), "event signal must persist and coalesce");
    MT::Semaphore ready(nullptr, 0, 2);
    std::atomic<int> released{0};
    std::thread a([&] { ready.Release(); if (event.Wait()) ++released; });
    std::thread b([&] { ready.Release(); if (event.Wait()) ++released; });
    require(ready.Wait(1000) && ready.Wait(1000), "event waiters failed to start");
    event.Signal();
    // Wait for exactly one consumption before posting the second signal.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!released.load() && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    const int afterOne = released.load();
    event.Signal(); a.join(); b.join();
    require(afterOne == 1 && released == 2, "auto-reset event released wrong number of waiters");

    MT::Semaphore tokens("mac-test-semaphore", 1, 2), sameTokens("mac-test-semaphore", 0, 99);
    require(sameTokens.Wait(0) && !tokens.Wait(10), "named semaphore initial count or timeout failed");
    tokens.Release(2);
    rejects([&] { tokens.Release(); }, "semaphore overflow was accepted");
    rejects([&] { tokens.Release(0); }, "zero semaphore release was accepted");
    require(sameTokens.Wait(0) && tokens.Wait(0) && !tokens.Wait(0), "semaphore count not shared");
    rejects([] { MT::Semaphore invalid(nullptr, 3, 2); }, "invalid initial count accepted");

    MT::RwLock rw;
    require(rw.ReadLock(0), "read lock failed");
    std::atomic<bool> concurrentRead{false}, writerBlocked{false};
    std::thread reader([&] {
        concurrentRead = rw.ReadLock(50);
        if (concurrentRead) rw.ReleaseReadLock();
        writerBlocked = !rw.WriteLock(20);
    });
    reader.join(); rw.ReleaseReadLock();
    require(concurrentRead && writerBlocked, "reader/writer exclusion failed");
    require(rw.WriteLock(0), "write lock failed after readers exited");
    std::atomic<bool> readerBlocked{false};
    std::thread blockedReader([&] {
        const bool acquired = rw.ReadLock(20);
        readerBlocked = !acquired;
        if (acquired) rw.ReleaseReadLock();
    });
    blockedReader.join(); rw.ReleaseWriteLock();
    require(readerBlocked, "reader acquired during write");
}
class Worker : public MT::Thread {
public:
    MT::Event entered, finish;
    MT::ThreadLocalStorage& tls;
    std::atomic<bool> correct{false};
    explicit Worker(MT::ThreadLocalStorage& tls) : tls(tls) {}
    ~Worker() override { finish.Signal(); Join(); }
    void ThreadMain() override {
        int local = 19;
        tls.SetData(&local);
        correct = GetCurrentThreadHandle() == GetHandle() && tls.GetData() == &local;
        entered.Signal();
        finish.Wait();
    }
};
class FailingWorker : public MT::Thread {
    void ThreadMain() override { throw std::runtime_error("worker failure"); }
};
static void lifecycle() {
    MT::ThreadLocalStorage tls;
    int mainValue = 41;
    tls.SetData(&mainValue);
    require(tls.GetData() == &mainValue && MT::GetProcessID() > 0, "main TLS/process ID failed");
    Worker worker(tls);
    const auto mainIdentifier = MT::GetThreadId();
    require(mainIdentifier != 0, "native thread ID is zero");
    MT::ThreadId otherIdentifier = 0;
    std::thread identify([&] { otherIdentifier = MT::GetThreadId(); MT::YieldThread(); });
    identify.join();
    require(otherIdentifier && otherIdentifier != mainIdentifier, "native thread IDs collided");
    for (int restart = 0; restart < 3; ++restart) {
        worker.Start(256 * 1024, restart == 0 ? 0 : (restart == 1 ? 1 : -1), "Blur native MT test");
        require(worker.entered.Wait(2000) && worker.IsRunning() && worker.correct,
                "worker did not start with valid native handle/TLS");
        rejects([&] { worker.Start(); }, "running thread started twice");
        require(tls.GetData() == &mainValue, "worker TLS leaked into main thread");
        worker.finish.Signal(); worker.Join();
        require(!worker.IsRunning(), "joined worker still running");
        worker.Join(); worker.Close(); worker.Close();
    }
    FailingWorker failure;
    failure.Start();
    rejects([&] { failure.Join(); }, "worker exception was swallowed");
    failure.Close();
    std::thread external([&] { require(tls.GetData() == nullptr, "new thread TLS not empty"); });
    external.join();
}
static void resourceAtomicMutex() {
    MT::AtomicLock lock;
    require(lock.Wait() && lock.Wait(0), "resource atomic lock is not recursive");
    std::atomic<bool> timedOut{false};
    std::thread contender([&] {
        const auto start = std::chrono::steady_clock::now();
        timedOut = !lock.Wait(15) && std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(10);
        rejects([&] { lock.Release(); }, "non-owner released resource atomic lock");
    });
    contender.join(); lock.Release(); lock.Release();
    require(timedOut, "resource mutex timeout did not use elapsed milliseconds");
    unsigned count = 0; std::vector<std::thread> workers;
    for (int i = 0; i < 6; ++i) workers.emplace_back([&] {
        for (int n = 0; n < 3000; ++n) { lock.Wait(); ++count; lock.Release(); }
    });
    for (auto& worker : workers) worker.join();
    require(count == 18000 && lock.Wait(0), "resource atomic ownership lost protected writes");
    lock.Release();
}
int main() {
    try {
        atomics(); synchronization(); lifecycle(); resourceAtomicMutex();
        std::cout << "Native engine MT: atomics, synchronization, TLS and thread lifecycle passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
