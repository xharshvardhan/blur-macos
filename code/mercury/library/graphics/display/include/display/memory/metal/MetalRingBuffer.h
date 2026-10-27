// [Blur2PC][macOS] PC ring compatibility. Metal command buffers retain their
// buffers/textures until completion; allocations never come from a console
// command-memory ring. Like the original D3D11 implementation, these hooks do
// not allocate or retire GPU storage. Metal resource owners perform that work.
#pragma once
namespace Display {
class C_MemorySet { public: void Clear() {} };
class C_RingBuffer {
public:
    void Initialise(U32) {}
    void Shutdown() {}
    void EndFrame() {}
    U32 GetUsedSize()const{return 0;}
};
inline C_RingBuffer* RingBuffer(){static C_RingBuffer ring;return &ring;}
}
