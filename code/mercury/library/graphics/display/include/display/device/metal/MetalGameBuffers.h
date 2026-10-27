// [Blur2PC][macOS] Native ownership for the original upper framebuffer/current-target interfaces.
#pragma once
#include <display/device/metal/MetalResources.h>
#include <array>
namespace Display {namespace Metal {class C_GameDevice;}
class C_BackBuffer {
    friend class Metal::C_GameDevice;
public:
    U32 GetWidth()const{return m_Width;}U32 GetHeight()const{return m_Height;}
    E_Format GetColourFormat()const{return m_ColourFormat;}E_Format GetDepthStencilFormat()const{return m_DepthStencilFormat;}
    E_Multisample GetMultisample()const{return m_Multisample;}
    const C_Surface* GetColourSurface()const{return &m_ColourSurface;}const C_Surface* GetDepthStencilSurface()const{return &m_DepthStencilSurface;}
private:
    U32 m_Width=0,m_Height=0;E_Format m_ColourFormat=E_Format_Unknown,m_DepthStencilFormat=E_Format_Unknown;
    E_Multisample m_Multisample=E_Multisample_None;C_Surface m_ColourSurface,m_DepthStencilSurface;
};
class C_RenderTarget {
    friend class Metal::C_GameDevice;
public:
    U32 GetColourWidth(U32 target=0)const{return Colour(target).GetWidth();}U32 GetColourHeight(U32 target=0)const{return Colour(target).GetHeight();}
    E_Format GetColourFormat(U32 target=0)const{return GetFormatFromSurfaceFormat(Colour(target).GetFormat());}
    E_Multisample GetColourMultisample(U32 target=0)const{return static_cast<E_Multisample>(Colour(target).GetMultisample());}
    const C_Surface* GetColourSurface(U32 target=0)const{if(target>=4)throw std::out_of_range("Native colour attachment index exceeds four targets");return m_pColourSurfaces[target];}
    U32 GetDepthStencilWidth()const{return Depth().GetWidth();}U32 GetDepthStencilHeight()const{return Depth().GetHeight();}
    E_Format GetDepthStencilFormat()const{return GetFormatFromSurfaceFormat(Depth().GetFormat());}
    E_Multisample GetDepthStencilMultisample()const{return static_cast<E_Multisample>(Depth().GetMultisample());}
    const C_Surface* GetDepthStencilSurface()const{return m_pDepthStencilSurface;}
private:
    const C_Surface& Colour(U32 target=0)const{const auto* surface=GetColourSurface(target);if(!surface)throw std::logic_error("Native colour attachment is not bound");return *surface;}
    const C_Surface& Depth()const{if(!m_pDepthStencilSurface)throw std::logic_error("Native depth attachment is not bound");return *m_pDepthStencilSurface;}
    std::array<const C_Surface*,4> m_pColourSurfaces={};const C_Surface* m_pDepthStencilSurface=nullptr;
};
}
