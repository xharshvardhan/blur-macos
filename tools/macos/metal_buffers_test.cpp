// [Blur2PC][macOS] Actual engine buffer API and native GPU transfer verification.
#include <display2/device/MetalDevice.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

static void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main() {
    using namespace Display2;
    try {
        C_MetalDevice device;
        require(device.Create() && device.IsCreated() && !device.Create(), "Metal device lifecycle failed");
        C_ContextNoCheckNoTrack unbound;bool contextRejected=false;try{unbound.GetDepthState();}catch(const std::logic_error&){contextRejected=true;}
        require(contextRejected&&!unbound.IsBound(),"Unbound immediate context accepted a state access");
        auto immediate=device.GetContextNoCheckNoTrack();unbound.Bind(device);
        require(immediate.IsBound()&&&immediate.GetDepthState()==&device.GetContext().GetDepthState()&&&unbound.GetDepthState()==&device.GetContextNoCheck().GetDepthState(),"Native context views copied/diverged from actual device state");
        constexpr U32 bytes = 4 * 1024 * 1024;
        C_VertexBuffer vertices, destination;
        require(!device.CreateVertexBuffer(0, 0, &vertices) && !vertices.IsCreated(), "zero allocation reported a native buffer");
        require(device.CreateVertexBuffer(bytes, E_Flags_Dynamic, &vertices) &&
                device.CreateVertexBuffer(bytes + 64, 0, &destination), "Metal engine buffer allocation failed");
        require(vertices.GetSize() == bytes && vertices.GetFlags() == E_Flags_Dynamic && vertices.GetAddress(),
                "engine buffer metadata differs from native resource");
        std::vector<U32> expected(bytes / 4);
        for (U32 i = 0; i < expected.size(); ++i) expected[i] = i * 0x9e3779b1U + 0x12345678U;
        void* data = vertices.Lock();
        require(data && !vertices.Lock(), "nested CPU buffer lock accepted");
        std::memcpy(data, expected.data(), bytes);
        require(!device.CopyBuffer(destination.GetGPUVertexBuffer(), 0, vertices.GetGPUVertexBuffer(), 0, bytes),
                "GPU submission accessed a CPU-locked buffer");
        std::atomic<bool> rejected{false};
        std::thread other([&] { try { vertices.Unlock(); } catch (const std::logic_error&) { rejected = true; } });
        other.join(); require(rejected, "foreign thread released the CPU buffer lock");
        vertices.Unlock();
        require(!vertices.Lock(bytes - 1, 2) && !vertices.Lock(~0U, 4), "buffer lock range overflow accepted");
        require(!device.CopyBuffer(destination.GetGPUVertexBuffer(), 1, vertices.GetGPUVertexBuffer(), 0, 4) &&
                !device.CopyBuffer(destination.GetGPUVertexBuffer(), bytes + 64, vertices.GetGPUVertexBuffer(), 0, 4),
                "Metal copy alignment/range violation accepted");
        require(device.CopyBuffer(destination.GetGPUVertexBuffer(), 32, vertices.GetGPUVertexBuffer(), 0, bytes), "GPU blit submission failed");
        data = destination.Lock(32, bytes);
        require(data && std::memcmp(data, expected.data(), bytes) == 0, "GPU blit/CPU completion lost bytes");
        destination.Unlock();
        // Aliased engine wrappers retain the GPU resource through owner release.
        C_VertexBuffer alias = vertices;
        device.Destroy(&vertices);
        require(!vertices.IsCreated() && alias.IsCreated() && alias.GetSize() == bytes, "engine buffer alias lost native ownership");
        C_IndexBuffer indices;
        require(!device.CreateIndexBuffer(3, 0, E_IndexFormat_16, &indices) &&
                !device.CreateIndexBuffer(8, 0, E_IndexFormat_Unknown, &indices), "invalid index layout was accepted");
        require(device.CreateIndexBuffer(bytes, E_Flags_ReadWrite, E_IndexFormat_32, &indices) &&
                indices.GetFormat() == E_IndexFormat_32, "engine index format changed");
        require(device.CopyBuffer(indices.GetGPUIndexBuffer(), 0, alias.GetGPUVertexBuffer(), 0, bytes), "index buffer GPU transfer failed");
        device.Destroy(&alias); // Metal commands retain GPU resources until completion.
        data = indices.Lock();
        require(data && std::memcmp(data, expected.data(), bytes) == 0, "index GPU upload lost bytes on source destruction");
        indices.Unlock();
        device.Destroy(&indices); device.Destroy(&destination);
        // Mesh/resource loading may allocate and upload from background workers.
        std::atomic<int> failures{0};
        std::vector<std::thread> workers;
        for (U32 worker = 0; worker < 8; ++worker) workers.emplace_back([&, worker] {
            try {
                C_VertexBuffer source, target;
                const U32 length = 512 * 1024;
                require(device.CreateVertexBuffer(length, E_Flags_Dynamic, &source) &&
                        device.CreateVertexBuffer(length, 0, &target), "worker allocation failed");
                auto* words = static_cast<U32*>(source.Lock());
                require(words, "worker CPU upload lock failed");
                for (U32 i = 0; i < length / 4; ++i) words[i] = i * 73U + worker;
                source.Unlock();
                require(device.CopyBuffer(target.GetGPUVertexBuffer(), 0, source.GetGPUVertexBuffer(), 0, length),
                        "worker GPU upload failed");
                words = static_cast<U32*>(target.Lock());
                require(words, "worker GPU completion lock failed");
                for (U32 i = 0; i < length / 4; ++i) require(words[i] == i * 73U + worker, "concurrent GPU transfer changed bytes");
                target.Unlock(); device.Destroy(&source); device.Destroy(&target);
            } catch (...) { ++failures; }
        });
        for (auto& worker : workers) worker.join();
        require(failures == 0, "concurrent native buffer uploads failed");
        require(device.WaitIdle(), "Metal command reported a GPU failure");
        std::cout << "Engine Metal vertex/index buffers and 4 MiB GPU readback PASS: " << device.GetDeviceName() << '\n';
        device.Destroy(); device.Destroy();
        require(!immediate.IsBound()&&!unbound.IsBound(),"Destroying the native device retained exported context views");
        contextRejected=false;try{immediate.Clear(E_Clear_Colour,0,1,0);}catch(const std::logic_error&){contextRejected=true;}
        require(contextRejected,"Destroyed device accepted work through an exported immediate context");
        require(!device.IsCreated() && device.Create(), "native Metal device did not restart");
        require(!immediate.IsBound(),"Old native context rebound implicitly to a recreated device");unbound.Bind(device);require(unbound.IsBound(),"Explicit native context rebind failed");
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
