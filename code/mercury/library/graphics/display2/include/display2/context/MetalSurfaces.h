// [Blur2PC][macOS] Native render surfaces with the original PC format identifiers.
#pragma once
#include <core/Core.h>
#include <memory>
namespace Display2 {
namespace Metal { struct Surface; }
class C_MetalDevice; class C_Context;
enum E_SurfaceFormat {
    // L8 is metadata-only; D24S8 uses supported opaque native depth/stencil storage.
    E_SurfaceFormat_L8=50,E_SurfaceFormat_D24S8=75,
    E_SurfaceFormat_A8R8G8B8=21, E_SurfaceFormat_A8B8G8R8=32, E_SurfaceFormat_D16=80,
    E_SurfaceFormat_A16B16G16R16F=113, E_SurfaceFormat_R32F=114,
    E_SurfaceFormat_A32B32G32R32F=116, E_SurfaceFormat_Unknown=~0U
};
enum E_Multisample { E_Multisample_None=0, E_Multisample_2=2, E_Multisample_4=4, E_Multisample_Unknown=~0U };
struct C_SurfaceParams {};
struct C_LockedRect {
    U8* pBits=nullptr; U32 pitch=0;
    S32 GetPitch() const { return S32(pitch); }
    U8* GetBits() const { return pBits; }
};
struct C_Surface {
    bool IsCreated() const { return bool(m_Surface); }
    U32 GetSize() const;
    U32 GetFlags() const { return m_Flags; }
    U8* GetAddress() const { return nullptr; } // Private GPU storage has no CPU mapping.
    U32 GetWidth() const;
    U32 GetHeight() const;
    E_SurfaceFormat GetFormat() const;
    E_Multisample GetMultisample() const;
    void SetFlags(U32 flags) { m_Flags=flags; }
    Metal::Surface* GetGPUSurface() { return m_Surface.get(); }
    const Metal::Surface* GetGPUSurface() const { return m_Surface.get(); }
    C_LockedRect LockRect(U32 flags=0);
    void UnlockRect();
private:
    friend class C_MetalDevice; friend class C_Context;friend struct C_Texture;friend struct C_RenderTexture2D;
    std::shared_ptr<Metal::Surface> m_Surface;
    U32 m_Flags=0;
};
}
