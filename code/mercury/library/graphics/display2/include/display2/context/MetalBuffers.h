// [Blur2PC][macOS] Engine buffer interfaces backed by native Metal resources.
#pragma once
#include <core/Core.h>
#include <memory>

namespace Display2 {
namespace Metal { struct Buffer; }
class C_MetalDevice;
// Preserve the original PC serialized index format identifiers.
// https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dformat
enum E_IndexFormat { E_IndexFormat_16 = 101, E_IndexFormat_32 = 102, E_IndexFormat_Unknown = ~0U };
// Original engine flags (D3D11Context.h); these describe engine intent, not Metal options.
enum E_Flags {
    E_Flags_Video = 0x1, E_Flags_System = 0x2, E_Flags_Device = 0x4,
    E_Flags_Indirect = 0x10, E_Flags_AlignSizeToBlockSize = 0x20,
    E_Flags_Dynamic = 0x100, E_Flags_Colour = 0x1000, E_Flags_DepthStencil = 0x2000,
    E_Flags_Raw = 0x10000, E_Flags_Tiled = 0x20000, E_Flags_Linear = 0x40000,
    E_Flags_SRGB = 0x100000, E_Flags_DoNotTrack = 0x1000000,
    E_Flags_DoNotReload = 0x2000000, E_Flags_ReadWrite = 0x10000000
};
struct C_VertexBuffer {
    bool IsCreated() const;
    U32 GetSize() const;
    U32 GetFlags() const { return m_Flags; }
    void* GetAddress() const;
    void SetSize(U32 size);
    void SetFlags(U32 flags) { m_Flags = flags; }
    void SetAddress(void* address);
    Metal::Buffer* GetGPUVertexBuffer() { return m_Buffer.get(); }
    const Metal::Buffer* GetGPUVertexBuffer() const { return m_Buffer.get(); }
    void* Lock(U32 offset, U32 size, U32 flags = 0);
    void* Lock() { return Lock(0, 0); }
    void Unlock();
private:
    friend class C_MetalDevice;
    friend class C_Context;
    std::shared_ptr<Metal::Buffer> m_Buffer;
    U32 m_Flags = 0;
};
struct C_IndexBuffer {
    bool IsCreated() const;
    U32 GetSize() const;
    U32 GetFlags() const { return m_Flags; }
    void* GetAddress() const;
    E_IndexFormat GetFormat() const { return m_Format; }
    void SetSize(U32 size);
    void SetFlags(U32 flags) { m_Flags = flags; }
    void SetAddress(void* address);
    Metal::Buffer* GetGPUIndexBuffer() { return m_Buffer.get(); }
    const Metal::Buffer* GetGPUIndexBuffer() const { return m_Buffer.get(); }
    void* Lock(U32 offset, U32 size, U32 flags = 0);
    void* Lock() { return Lock(0, 0); }
    void Unlock();
private:
    friend class C_MetalDevice;
    friend class C_Context;
    std::shared_ptr<Metal::Buffer> m_Buffer;
    U32 m_Flags = 0;
    E_IndexFormat m_Format = E_IndexFormat_Unknown;
};
}
