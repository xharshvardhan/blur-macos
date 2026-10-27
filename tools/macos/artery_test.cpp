// [Blur2PC][macOS] Actual Artery locks/signals and original BSD transport API.
#include <core/Core.h>
#include <system/artery/ArterySync.h>
#include <system/artery/BkSocket.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <poll.h>
#include <stdexcept>
#include <thread>
#include <vector>

static void require(bool value, const char* reason) {
    if (!value) throw std::runtime_error(reason);
}
static void entry(void* p) { ++*static_cast<std::atomic<int>*>(p); }
static void synchronization() {
    std::atomic<int> calls{0};
    ArteryThread worker;
    require(!worker.Create(nullptr, nullptr), "accepted null callback");
    require(worker.Create(entry, &calls), "Artery thread creation failed");
    require(!worker.Create(entry, &calls), "live Artery thread replaced");
    worker.Close(); worker.Close();
    require(calls == 1 && worker.Create(entry, &calls), "Artery join/reuse failed");
    worker.Close(); require(calls == 2, "reused callback was lost");

    ArteryLock lock;
    lock.Wait(); require(lock.Try(), "critical lock is not recursive");
    lock.Release(); lock.Release();
    ArteryMutex mutex; ArteryCondVar cond;
    require(!cond.Create(&mutex) && mutex.Create() && !mutex.Create() && cond.Create(&mutex),
            "mutex/condition creation state changed");
    bool ready = false;
    std::thread producer([&] { mutex.Wait(); ready = true; cond.Signal(); mutex.Release(); });
    mutex.Wait(); while (!ready) cond.Wait(); mutex.Release(); producer.join();
    mutex.Wait(); require(!cond.Wait(10), "condition timeout reported success");
    require(mutex.Try(), "condition wait did not reacquire mutex");
    mutex.Release(); mutex.Release();
    mutex.Wait();
    bool timedOut = false;
    std::thread contender([&] { timedOut = !mutex.Wait(10); });
    contender.join(); mutex.Release(); require(timedOut, "contended timed mutex did not time out");
    cond.Destroy(); mutex.Destroy(); require(mutex.Create(), "mutex recreation failed");

    ArteryEvent event; ArteryFlag flag;
    require(event.Create() && flag.Create(), "signal creation failed");
    require(!event.Try() && !flag.Try(), "new signal already raised");
    event.Raise(); event.Raise();
    require(event.Try() && !event.Try(), "auto-reset raises did not coalesce");
    flag.Raise(); require(flag.Try() && flag.Try(), "manual flag was consumed");
    flag.Lower(); require(!flag.Wait(5), "lowered manual flag remained raised");
    std::atomic<int> waiting{0}, released{0};
    std::vector<std::thread> waiters;
    for (int i = 0; i < 6; ++i) waiters.emplace_back([&] {
        ++waiting; flag.Wait(); ++released;
    });
    while (waiting != 6) std::this_thread::yield();
    flag.Raise(); for (auto& t : waiters) t.join();
    require(released == 6 && flag.Try(), "manual flag failed broadcast/retention");
    released = 0; waiting = 0; waiters.clear();
    for (int i = 0; i < 6; ++i) waiters.emplace_back([&] {
        ++waiting; if (event.Wait(150)) ++released;
    });
    while (waiting != 6) std::this_thread::yield();
    event.Raise(); for (auto& t : waiters) t.join();
    require(released == 1 && !event.Try(), "one auto-reset raise released multiple waiters");
}
static BkSockAddr localAddress(const BkSocket& socket) {
    BkSockAddr address; socklen_t length = address.Length();
    require(getsockname(socket.Handle(), address, &length) == 0, "socket address query failed");
    return address;
}
static void waitFor(const BkSocket& socket, short events) {
    pollfd fd{socket.Handle(), events, 0};
    require(poll(&fd, 1, 1000) > 0, "socket readiness timed out");
}
static void transport() {
    BkSocket listener;
    require(listener.Listen(BkSockAddr(INADDR_LOOPBACK, 0)), "BSD listen failed");
    BkSocket empty; require(!listener.Accept(empty), "empty nonblocking accept succeeded");
    const auto address = localAddress(listener);
    require(address.Port() != 0, "ephemeral listener has no port");
    BkSocket clients[8], accepted[8];
    for (int i = 0; i < 8; ++i) {
        require(clients[i].Connect(address), "BSD client connect failed");
        waitFor(listener, POLLIN);
        require(listener.Accept(accepted[i]), "BSD accept failed");
        BkSockAddr remote;
        require(clients[i].TestConnection(remote) == 1 && remote.Port() == address.Port(),
                "connected endpoint status changed");
        require((fcntl(accepted[i].Handle(), F_GETFL) & O_NONBLOCK) &&
                (fcntl(accepted[i].Handle(), F_GETFD) & FD_CLOEXEC), "accepted descriptor flags missing");
        int noSignal = 0; socklen_t optionLength = sizeof(noSignal);
        require(getsockopt(accepted[i].Handle(), SOL_SOCKET, SO_NOSIGPIPE,
                           &noSignal, &optionLength) == 0 && noSignal == 1,
                "accepted stream may deliver SIGPIPE");
        char byte = char(i + 1), received = 0;
        require(accepted[i].Receive(&received, 1) == 0, "empty receive blocked/failed");
        require(clients[i].Send(&byte, 1) == 1, "stream send failed");
        waitFor(accepted[i], POLLIN);
        require(accepted[i].Receive(&received, 1) == 1 && received == byte,
                "stream payload changed");
    }
    // Transfer substantially more than the small kernel buffers can hold;
    // partial sends and would-block must make progress without losing bytes.
    BkSocket a, b;
    require(CrossConnect(a, b), "wake socket cross-connect failed");
    int small = 4096;
    setsockopt(a.Handle(), SOL_SOCKET, SO_SNDBUF, &small, sizeof(small));
    setsockopt(b.Handle(), SOL_SOCKET, SO_RCVBUF, &small, sizeof(small));
    std::vector<unsigned char> source(512 * 1024), received(source.size());
    for (std::size_t i = 0; i < source.size(); ++i) source[i] = (i * 31 + 19) & 255;
    std::size_t sent = 0, read = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (read < source.size()) {
        require(std::chrono::steady_clock::now() < deadline, "partial stream transfer stalled");
        if (sent < source.size()) {
            const int n = a.Send(source.data() + sent, int(source.size() - sent));
            require(n >= 0, "partial send failed"); sent += n;
        }
        const int n = b.Receive(received.data() + read, int(received.size() - read));
        require(n >= 0, "partial receive failed"); read += n;
        std::this_thread::yield();
    }
    require(received == source && sent == source.size(), "partial stream lost data");
    BkSocket moved; moved.Attach(b);
    require(!b.IsOpen() && moved.IsOpen(), "Attach duplicated descriptor ownership");
    a.Close(); waitFor(moved, POLLIN);
    char byte; require(moved.Receive(&byte, 1) == -1, "EOF mistaken for would-block");
    // A closed peer must return failure rather than terminate this process.
    moved.Send("x", 1); moved.Send("x", 1);
    BkSocket udpA, udpB;
    require(udpA.Broadcast(0) && udpB.Broadcast(0), "BSD datagram setup failed");
    BkSockAddr from;
    require(udpB.ReceiveFrom(&byte, 1, from) == 0, "empty datagram receive failed");
    const char message[] = "Artery native UDP";
    const auto destination = BkSockAddr(INADDR_LOOPBACK, localAddress(udpB).Port());
    require(udpA.SendTo(message, sizeof(message), destination) == sizeof(message), "datagram send failed");
    waitFor(udpB, POLLIN); char buffer[64]{};
    require(udpB.ReceiveFrom(buffer, sizeof(buffer), from) == sizeof(message) &&
            std::memcmp(buffer, message, sizeof(message)) == 0 && from.Port() == localAddress(udpA).Port(),
            "datagram payload/source changed");
}
int main() {
    try { synchronization(); transport(); std::cout << "Artery native synchronization/BSD transport PASS\n"; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
