// [Blur2PC][macOS] Original service, handshake and framed messages in Speed mode.
#include <core/Core.h>
#include <system/artery/Artery.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

static void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}
struct State {
    std::atomic<int> clients{0}, servers{0}, echoes{0};
    std::atomic<bool> release{false}, good{true};
};
static void echo(ArteryConnection connection, void* context) {
    auto& state = *static_cast<State*>(context);
    ++state.servers;
    while (connection.IsConnected()) {
        ArteryMessage message;
        if (connection.Receive(message, 100) && message) {
            if (!connection.Send(message)) state.good = false;
            ++state.echoes;
        }
    }
}
static void client(ArteryConnection connection, void* context) {
    auto& state = *static_cast<State*>(context);
    ++state.clients;
    while (connection.IsConnected() && !state.release)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    connection.Close();
}
template<class F> static void await(F done) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    while (!done()) {
        require(std::chrono::steady_clock::now() < deadline, "Artery service progress timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
static void messageFormat() {
    ArteryMessageBuilder builder;
    require(builder.Write(ArteryU8(0xa5)) && builder.Write(ArteryU16(0x1234)) &&
            builder.Write(ArteryU32(0x89abcdef)) && builder.Write(ArteryU64(0x0102030405060708ULL)) &&
            builder.Write(ArteryF32(1.0f)) && builder.Write("Mac"), "message build failed");
    ArteryMessage message;
    require(builder.Build(message), "message allocation failed");
    // Source authority: 16/32/F32 are network-endian, but 64-bit fields are
    // explicitly little-endian. Preserve that asymmetric historical format.
    const unsigned char expected[] = {
        0xa5, 0x12, 0x34, 0x89, 0xab, 0xcd, 0xef,
        8, 7, 6, 5, 4, 3, 2, 1, 0x3f, 0x80, 0, 0,
        0, 0, 0, 4, 'M', 'a', 'c', 0
    };
    require(message.DataLength() == sizeof(expected) &&
            std::memcmp(message.Data(), expected, sizeof(expected)) == 0,
            "message bytes differ from original PC serializers");
    ArteryMessageReader reader(message);
    ArteryU8 a; ArteryU16 b; ArteryU32 c; ArteryU64 d; ArteryF32 e; const char* text;
    require(reader.Read(a) && a == 0xa5 && reader.Read(b) && b == 0x1234 &&
            reader.Read(c) && c == 0x89abcdef && reader.Read(d) && d == 0x0102030405060708ULL &&
            reader.Read(e) && e == 1.0f && reader.Read(text) && std::strcmp(text, "Mac") == 0,
            "message reader changed scalar values");
    require(!reader.Read(a), "message reader read past its payload");
    require(!builder.Write(nullptr, 1) && !builder.Write(expected, -1) &&
            !builder.Write(expected, 0x7fffffff), "message builder accepted invalid lengths");
    ArteryMessageReader invalid(message);
    require(!invalid.Read(nullptr, -1), "message reader accepted a negative length");
    const unsigned char badString[] = {0, 0, 0, 2, 'x', 'y'};
    ArteryMessage malformed(badString, sizeof(badString));
    ArteryMessageReader malformedReader(malformed);
    require(!malformedReader.Read(text), "unterminated message string accepted");
    std::vector<std::thread> copies;
    std::atomic<bool> good{true};
    for (int i = 0; i < 8; ++i) copies.emplace_back([&] {
        for (int n = 0; n < 10000; ++n) {
            ArteryMessage copy(message), assigned;
            assigned = copy; copy.Clear();
            if (assigned.DataLength() != sizeof(expected) ||
                std::memcmp(assigned.Data(), expected, sizeof(expected))) good = false;
        }
    });
    for (auto& thread : copies) thread.join();
    require(good, "shared Artery message ownership corrupted payloads");
}
static void service() {
    for (int cycle = 0; cycle < 3; ++cycle) {
        State state;
        require(ArteryStartupEx(true, "Native % Mac", 16), "original Artery service failed startup");
        struct Cleanup { ~Cleanup() { ArteryCleanup(); } } cleanup;
        require(ArteryRegister("native-test", echo, &state), "service registration failed");
        require(!ArteryRegister("native-test", echo, &state), "duplicate service accepted");
        ArteryConnection connections[8];
        for (auto& connection : connections)
            require(ArteryConnect(connection, ArterySockAddr(INADDR_LOOPBACK, 6317),
                                  "native-test", client, &state), "service client failed connect");
        await([&] { return state.clients == 8 && state.servers == 8; });
        for (int i = 0; i < 8; ++i) {
            require(connections[i].IsConnected(), "simultaneous service peer disconnected");
            std::vector<unsigned char> payload(65536 + i);
            for (std::size_t n = 0; n < payload.size(); ++n) payload[n] = (n * 17 + i) & 255;
            ArteryMessage message(payload.data(), int(payload.size()));
            require(connections[i].Send(message), "framed send failed");
        }
        for (int i = 0; i < 8; ++i) {
            ArteryMessage reply;
            await([&] { return connections[i].TryReceive(reply); });
            require(reply.DataLength() == 65536 + i, "framed message length changed");
            const auto* bytes = static_cast<const unsigned char*>(reply.Data());
            for (int n = 0; n < reply.DataLength(); ++n)
                require(bytes[n] == ((n * 17 + i) & 255), "framed echo payload changed");
        }
        require(state.good && state.echoes == 8, "original service lost/duplicated messages");
        state.release = true;
        for (auto& connection : connections) connection.Close();
    }
}
int main() {
    try { messageFormat(); service(); std::cout << "Artery original service/framing/eight peers/restart PASS\n"; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
