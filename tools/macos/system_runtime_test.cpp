// [Blur2PC][macOS] Original Time/Address/Socket APIs against real Darwin services.
#include <core/Core.h>
#include <core/threads/Thread.h>
#include <system/time/Time.h>
#include <system/time/SimpleTimer.h>
#include <system/network/Platform.h>
#include <system/network/Address.h>
#include <system/network/Socket.h>
#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#include <vector>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
static U16 portOf(Network::Socket& socket) {
    sockaddr_in address{};
    socklen_t size = sizeof(address);
    require(getsockname(socket.GetSocket(), reinterpret_cast<sockaddr*>(&address), &size) == 0,
            "getsockname failed");
    return ntohs(address.sin_port);
}
static void bindLoopback(Network::Socket& socket) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    require(bind(socket.GetSocket(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0,
            "loopback bind failed");
}
static void timeServices() {
    const auto wall = Time::GetCurrentTime();
    const auto expected = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    require(wall <= static_cast<U64>(expected) && static_cast<U64>(expected) - wall < 1000000,
            "wall clock epoch/unit incorrect");
    const auto start = Time::GetPerformanceCounter();
    Time::C_SimpleTimer timer;
    MT::Thread::Sleep(20);
    const auto elapsed = Time::GetPerformanceCounter() - start;
    require(Time::GetPerformanceFrequency() == 1000000 && elapsed >= 15000,
            "performance clock scale/monotonicity incorrect");
    require(timer.Stop() >= 15.f, "original simple timer did not advance in milliseconds");
    Time::C_DecomposedTime epoch(0, true);
    require(epoch.m_year == 70 && epoch.m_mon == 0 && epoch.m_mday == 1 && epoch.m_wday == 4,
            "UTC Unix epoch conversion failed");
    Time::C_DecomposedTime leap(1709210096000000ULL, true); // 2024-02-29 12:34:56 UTC
    require(leap.m_year == 124 && leap.m_mon == 1 && leap.m_mday == 29 && leap.m_hour == 12 &&
            leap.m_min == 34 && leap.m_sec == 56, "leap-day conversion failed");
    require(std::strcmp(Time::FormatDate_TimeString(leap), "29-02-24_12-34-56") == 0,
            "original date/time formatting failed");
    std::atomic<bool> correct{true};
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i) threads.emplace_back([&, i] {
        for (int j = 0; j < 200; ++j) {
            Time::C_DecomposedTime value(i ? 1709210096000000ULL : 0, true);
            if (value.m_year != (i ? 124 : 70)) correct = false;
        }
    });
    for (auto& thread : threads) thread.join();
    require(correct, "calendar conversion raced between threads");
}
static void tcp() {
    Network::Address loopback("127.0.0.1"), empty;
    require(loopback.GetAddress().s_addr == htonl(INADDR_LOOPBACK) && empty.GetAddress().s_addr == 0,
            "numeric/default address failed");
    Network::Address copy(loopback);
    require(std::strcmp(copy.GetHostname(), "127.0.0.1") == 0, "address copy lost hostname");
    Network::TCPSocket server(U16(0), 16);
    const U16 port = portOf(server);
    require(port != 0, "TCP ephemeral listener failed");
    // More than three simultaneous transport clients; this is not a game lobby test.
    std::vector<std::unique_ptr<Network::TCPSocket>> clients, peers;
    for (int i = 0; i < 8; ++i) {
        clients.emplace_back(new Network::TCPSocket);
        require(clients.back()->Connect(loopback, port), "engine TCP connect failed");
        require(server.IsInputPending(1000), "listener readiness failed");
        const int accepted = accept(server.GetSocket(), nullptr, nullptr);
        require(accepted >= 0, "TCP accept failed");
        peers.emplace_back(new Network::TCPSocket(accepted));
    }
    for (int i = 0; i < 8; ++i) {
        const U32 payload = 0x12340000U + i;
        require(clients[i]->Send(&payload, sizeof(payload)) == sizeof(payload), "TCP send failed");
        require(peers[i]->IsInputPending(1000), "TCP input readiness failed");
        U32 peeked = 0, received = 0;
        require(peers[i]->Peek(&peeked, sizeof(peeked)) == sizeof(peeked) && peeked == payload,
                "TCP peek failed");
        require(peers[i]->Receive(&received, sizeof(received)) == sizeof(received) && received == payload,
                "TCP receive/peek consumption failed");
        require(!peers[i]->IsInputPending(0), "TCP input unexpectedly remained after receive");
    }
    const int old = clients[0]->GetSocket();
    clients[0]->Close(); clients[0]->Close();
    require(clients[0]->GetSocket() == -1 && fcntl(old, F_GETFD) == -1 && errno == EBADF,
            "idempotent close leaked descriptor");
    require(peers[0]->IsInputPending(1000), "TCP EOF not reported ready");
    char byte;
    require(peers[0]->Receive(&byte, 1) == 0, "TCP EOF not returned as zero bytes");
    require(!clients[0]->IsInputPending(0) && clients[0]->Send(&byte, 1) == U32(-1),
            "closed socket error contract failed");
    int released;
    { Network::Socket scoped(AF_INET, SOCK_STREAM, 0); released = scoped.GetSocket(); }
    require(fcntl(released, F_GETFD) == -1 && errno == EBADF, "plain Socket destructor leaked descriptor");
}
static void udp() {
    Network::Socket server(AF_INET, SOCK_DGRAM, 0), client(AF_INET, SOCK_DGRAM, 0);
    bindLoopback(server); bindLoopback(client);
    const Network::Address loopback("127.0.0.1");
    require(client.Connect(loopback, portOf(server)) && server.Connect(loopback, portOf(client)),
            "UDP connect failed");
    constexpr char payload[] = "Blur native datagram fixture";
    require(client.Send(payload, sizeof(payload)) == sizeof(payload), "UDP send failed");
    require(server.IsInputPending(1000), "UDP readiness failed");
    char buffer[128]{};
    require(server.Receive(buffer, sizeof(buffer)) == sizeof(payload) && !std::strcmp(buffer, payload),
            "UDP datagram payload changed");
    const auto before = std::chrono::steady_clock::now();
    require(!server.IsInputPending(20), "empty UDP socket became readable");
    require(std::chrono::steady_clock::now() - before >= std::chrono::milliseconds(15),
            "socket timeout did not wait");
    const int flags = fcntl(server.GetSocket(), F_GETFL);
    require(flags >= 0 && fcntl(server.GetSocket(), F_SETFL, flags | O_NONBLOCK) == 0, "nonblocking setup failed");
    require(server.Receive(buffer, sizeof(buffer)) == U32(-1) && (errno == EAGAIN || errno == EWOULDBLOCK),
            "would-block error sentinel failed");
}
int main() {
    try {
        timeServices(); tcp(); udp();
        std::cout << "Engine Time/Address/Socket: clocks, UTC, TCP x8, UDP, readiness and ownership passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
