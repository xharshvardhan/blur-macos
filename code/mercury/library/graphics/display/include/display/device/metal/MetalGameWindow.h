// [Blur2PC][macOS] Original PC per-view window resources; native resources retain explicit device ownership.
#pragma once
#include <display/device/metal/MetalResources.h>
#include <display/window/WindowBase.h>
namespace Display {
class C_Window {
public:
    explicit C_Window(const Maths::C_Rect& coords);
    void Resize(U32 width,U32 height,E_Multisample samples,U32 screenWidth,U32 screenHeight,E_Format colour,E_Format depth);
    E_Multisample GetMultisample()const{return m_Multisample;}
    C_Surface* GetColourSurface(U32=0){return &m_ColourRenderTexture.GetSurface();}
    C_Surface* GetDepthStencilSurface(){return &m_DepthStencilSurface;}
    C_Texture2D* GetColourTexture(U32=0){return &m_ColourRenderTexture.GetTexture();}
    const C_Texture2D* GetDepthStencilTexture()const{return m_pDepthTexture;}
    C_Texture2D* GetMaskTexture(){return &m_MaskRenderTexture.GetTexture();}
    Maths::C_Rect m_Coords;U32 m_Width=0,m_Height=0;E_Multisample m_Multisample=E_Multisample_None;
    U32 m_ScreenWidth=0,m_ScreenHeight=0;C_TileSetup m_TileSetup{0,0,E_Multisample_None};
    E_Format m_ColourFormat=E_Format_A8R8G8B8,m_DepthStencilFormat=E_Format_D24S8;
    C_RenderTexture2D m_ColourRenderTexture;C_Surface m_DepthStencilSurface;C_RenderTexture2D m_MaskRenderTexture;
    const C_Texture2D* m_pDepthTexture=nullptr;bool m_ClearDepthStencil=false,m_MaskEnable=false;
};
}
