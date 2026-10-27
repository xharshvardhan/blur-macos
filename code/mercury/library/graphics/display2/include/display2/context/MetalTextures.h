// [Blur2PC][macOS] Native texture storage; original PC resource identifiers.
#pragma once
#include <display2/context/MetalSurfaces.h>
#include <btl/containers/Vector.h>
#include <stdexcept>
namespace Display2 {
namespace Metal { struct Texture; }
enum E_TextureType { E_TextureType_2D=3,E_TextureType_3D=4,E_TextureType_Cube=5,E_TextureType_Unknown=~0U };
enum E_CubeMapFace {E_CubeMapFace_PosX=0,E_CubeMapFace_NegX=1,E_CubeMapFace_PosY=2,E_CubeMapFace_NegY=3,E_CubeMapFace_PosZ=4,E_CubeMapFace_NegZ=5,E_CubeMapFace_Unknown=~0U};
enum E_TextureFormat {
    // Original D3DFORMAT IDs used by upper Display metadata; unsupported resources still fail creation.
    E_TextureFormat_G16R16=34,E_TextureFormat_A2R10G10B10=35,E_TextureFormat_A16B16G16R16=36,
    E_TextureFormat_D24S8=75,E_TextureFormat_D24FS8=83,
    E_TextureFormat_A8R8G8B8=21,E_TextureFormat_X8R8G8B8=22,E_TextureFormat_A8=28,
    E_TextureFormat_A8B8G8R8=32,E_TextureFormat_X8B8G8R8=33,E_TextureFormat_L8=50,E_TextureFormat_A8L8=51,
    E_TextureFormat_V8U8=60,E_TextureFormat_D16=80,E_TextureFormat_L16=81,
    E_TextureFormat_R16F=111,E_TextureFormat_G16R16F=112,E_TextureFormat_A16B16G16R16F=113,
    E_TextureFormat_R32F=114,E_TextureFormat_G32R32F=115,E_TextureFormat_A32B32G32R32F=116,
    E_TextureFormat_DXT1=0x31545844,E_TextureFormat_DXT2=0x32545844,E_TextureFormat_DXT3=0x33545844,
    E_TextureFormat_DXT4=0x34545844,E_TextureFormat_DXT5=0x35545844,E_TextureFormat_Unknown=~0U
};
struct C_ConstLockedRect {
    const U8* pBits=nullptr;U32 pitch=0;
    S32 GetPitch()const{return S32(pitch);} const U8* GetBits()const{return pBits;}
};
struct C_LockedBox {U8* pBits=nullptr;U32 rowPitch=0,slicePitch=0;S32 GetRowPitch()const{return S32(rowPitch);}S32 GetSlicePitch()const{return S32(slicePitch);}U8* GetBits()const{return pBits;}};
struct C_ConstLockedBox {const U8* pBits=nullptr;U32 rowPitch=0,slicePitch=0;S32 GetRowPitch()const{return S32(rowPitch);}S32 GetSlicePitch()const{return S32(slicePitch);}const U8* GetBits()const{return pBits;}};
struct C_Texture {
    bool IsCreated()const{return bool(m_Texture);}
    // [Blur2PC] Borrowed native binding handle. The owning wrapper stays live
    // through binding; C_Context then retains the actual GPU storage and flags.
    C_Texture* GetGPUTexture(){return IsCreated()?this:nullptr;}
    const C_Texture* GetGPUTexture()const{return IsCreated()?this:nullptr;}
    E_TextureType GetType()const; E_TextureFormat GetFormat()const;
    U32 GetSize()const;U32 GetFlags()const{return m_Flags;}
    U32 GetWidth(U32 level=0)const;U32 GetHeight(U32 level=0)const;U32 GetDepth(U32 level=0)const;U32 GetLevels()const;
    U8* GetAddress()const{return nullptr;}
    void SetFlags(U32 flags){m_Flags=flags;} void SetSRGB(bool enable=true);
    C_LockedRect LockRect2D(U32 level,U32 flags=0);C_ConstLockedRect LockRect2D(U32 level,U32 flags=0)const;
    void UnlockRect2D(U32 level)const;
    C_LockedRect LockRectCube(U32 face,U32 level,U32 flags=0);C_ConstLockedRect LockRectCube(U32 face,U32 level,U32 flags=0)const;
    void UnlockRectCube(U32 face,U32 level)const;
    C_LockedBox LockBox3D(U32 level,U32 flags=0);C_ConstLockedBox LockBox3D(U32 level,U32 flags=0)const;
    void UnlockBox3D(U32 level)const;
    C_LockedRect Lock(U32 level){return LockRect2D(level);} C_ConstLockedRect Lock(U32 level)const{return LockRect2D(level);}
    void Unlock(U32 level)const{UnlockRect2D(level);}
private:
    friend class C_MetalDevice;friend class C_Context;
    std::shared_ptr<Metal::Texture> m_Texture;U32 m_Flags=0;
};
enum E_TextureAddress { E_TextureAddress_Wrap=1,E_TextureAddress_Mirror=2,E_TextureAddress_Clamp=3,
    E_TextureAddress_ClampToEdge=3,E_TextureAddress_Border=4,E_TextureAddress_Unknown=~0U };
enum E_TextureFilter { E_TextureFilter_None=0,E_TextureFilter_Point=1,E_TextureFilter_Linear=2,E_TextureFilter_Anisotropic=3,E_TextureFilter_Unknown=~0U };
struct C_SamplerState {
    union {struct {U32 m_TextureAddressU:4,m_TextureAddressV:4,m_TextureAddressW:4;
        U32 m_TextureFilterMag:4,m_TextureFilterMin:4,m_TextureFilterMip:4,m_TextureGamma:1;};U32 m_TextureBits;};
    U32 m_TextureBorderColour;S32 m_TextureLodBias;
    C_SamplerState(){SetDefaultStates();}
    void SetDefaultStates(){m_TextureBits=0;m_TextureAddressU=1;m_TextureAddressV=1;m_TextureAddressW=1;
        m_TextureFilterMag=1;m_TextureFilterMin=1;m_TextureBorderColour=0;m_TextureLodBias=0;}
    bool IsSame(const C_SamplerState& o)const;
    void SetTextureAddressU(E_TextureAddress v){m_TextureAddressU=v;}void SetTextureAddressV(E_TextureAddress v){m_TextureAddressV=v;}void SetTextureAddressW(E_TextureAddress v){m_TextureAddressW=v;}
    void SetTextureAddress(E_TextureAddress u,E_TextureAddress v,E_TextureAddress w){m_TextureAddressU=u;m_TextureAddressV=v;m_TextureAddressW=w;}
    void SetTextureAddress(E_TextureAddress u,E_TextureAddress v){m_TextureAddressU=u;m_TextureAddressV=v;}
    void SetTextureAddress(E_TextureAddress v){SetTextureAddress(v,v,v);}
    void SetTextureFilter(E_TextureFilter mag,E_TextureFilter min,E_TextureFilter mip){m_TextureFilterMag=mag;m_TextureFilterMin=min;m_TextureFilterMip=mip;}
    void SetTextureFilter(E_TextureFilter v){SetTextureFilter(v,v,v);}
    void SetTextureBorderColour(U32 v){m_TextureBorderColour=v;}void SetTextureLodBias(S8 v){m_TextureLodBias=v;}
    void SetTextureGamma(U32 v){m_TextureGamma=v;}
};
static_assert(sizeof(C_SamplerState)==12,"PC sampler state layout changed");
struct C_RenderTexture2D {
    bool IsCreated()const{return m_Texture[0].IsCreated()&&m_Surfaces[0].Size()&&m_Surfaces[0][0].IsCreated();}U32 GetFlags()const{return m_Flags;}
    C_Texture& GetTexture(){if(m_TextureIndex>=2)throw std::logic_error("Invalid render-texture read slot");return m_Texture[m_TextureIndex];}
    const C_Texture& GetTexture()const{if(m_TextureIndex>=2)throw std::logic_error("Invalid render-texture read slot");return m_Texture[m_TextureIndex];}
    C_Surface& GetSurface(U32 level=0){if(m_SurfaceIndex>=2||level>=U32(m_Surfaces[m_SurfaceIndex].Size()))throw std::out_of_range("Invalid render-texture surface/mip");return m_Surfaces[m_SurfaceIndex][level];}
    const C_Surface& GetSurface(U32 level=0)const{if(m_SurfaceIndex>=2||level>=U32(m_Surfaces[m_SurfaceIndex].Size()))throw std::out_of_range("Invalid render-texture surface/mip");return m_Surfaces[m_SurfaceIndex][level];}
    void Resolve(U32 level=0)const;
    mutable U16 m_TextureIndex=0,m_SurfaceIndex=0;
    C_Texture m_Texture[2];Btl::Vector<C_Surface> m_Surfaces[2],m_ResolveSurfaces[2];U32 m_Flags=0,m_Multisample=0;
};
struct C_RenderTextureCube {
    bool IsCreated()const{return m_Texture.IsCreated();}U32 GetFlags()const{return m_Flags;}
    C_Texture& GetTexture(){return m_Texture;}const C_Texture& GetTexture()const{return m_Texture;}
    C_Surface& GetSurface(U32 face,U32 level=0){if(face>=6||level>=U32(m_Surfaces[face].Size()))throw std::out_of_range("Invalid render-cube face/mip");return m_Surfaces[face][level];}
    const C_Surface& GetSurface(U32 face,U32 level=0)const{if(face>=6||level>=U32(m_Surfaces[face].Size()))throw std::out_of_range("Invalid render-cube face/mip");return m_Surfaces[face][level];}
    C_Texture m_Texture;Btl::Vector<C_Surface> m_Surfaces[6];U32 m_Flags=0;
};
struct C_RenderBuffer {}; // Original PC resource wrapper has no fields.
}
