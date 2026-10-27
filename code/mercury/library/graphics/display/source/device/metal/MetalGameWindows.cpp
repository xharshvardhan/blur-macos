// [Blur2PC][macOS] Original PC window sizing/resources/mask/restore mechanism over actual Metal commands and original Quad.
#include "NativeDisplayIncludes.h"
#include <display/device/metal/MetalGameDevice.h>
#include <display/render/Render.h>
#include <cmath>
#include <stdexcept>
namespace Display {
C_Window::C_Window(const Maths::C_Rect& coords):m_Coords(coords){
    const F32 left=coords.Min().X(),top=coords.Min().Y(),right=coords.Max().X(),bottom=coords.Max().Y();
    if(!std::isfinite(left)||!std::isfinite(top)||!std::isfinite(right)||!std::isfinite(bottom)||left<0||top<0||right>1||bottom>1||right<=left||bottom<=top)throw std::invalid_argument("Native window coordinates must be a nonempty normalized rectangle");
}
void C_Window::Resize(U32 width,U32 height,E_Multisample samples,U32 screenWidth,U32 screenHeight,E_Format colour,E_Format depth){
    if(!width||!height||!screenWidth||!screenHeight||width>16384||height>16384||screenWidth>16384||screenHeight>16384)throw std::invalid_argument("Invalid native window buffer/screen dimensions");
    // Exact original PC float/truncation arithmetic in D3D11Window.cpp::Resize.
    m_Width=U32(F32(width)*m_Coords.Max().X()-F32(width)*m_Coords.Min().X());m_Height=U32(F32(height)*m_Coords.Max().Y()-F32(height)*m_Coords.Min().Y());
    m_ScreenWidth=U32(F32(screenWidth)*m_Coords.Max().X()-F32(screenWidth)*m_Coords.Min().X());m_ScreenHeight=U32(F32(screenHeight)*m_Coords.Max().Y()-F32(screenHeight)*m_Coords.Min().Y());
    m_Multisample=samples;m_ColourFormat=colour;m_DepthStencilFormat=depth;
    if(!m_Width||!m_Height||!m_ScreenWidth||!m_ScreenHeight)throw std::invalid_argument("Native window rectangle rounds to no pixels");
}
}
namespace Display::Metal {
S32 C_GameDevice::AddWindow(const Maths::C_Rect& coords){auto next=std::make_unique<C_Window>(coords);m_Windows.push_back(std::move(next));return GetWindowCount()-1;}
C_Window* C_GameDevice::GetWindow(S32 index){if(index<0||index>=GetWindowCount())throw std::out_of_range("Native window index is invalid");return m_Windows[index].get();}
const C_Window* C_GameDevice::GetWindow(S32 index)const{if(index<0||index>=GetWindowCount())throw std::out_of_range("Native window index is invalid");return m_Windows[index].get();}
void C_GameDevice::ClearWindows(){DestroyWindows();m_Windows.clear();}
void C_GameDevice::ResizeWindows(){
    DestroyWindows();const auto& p=m_DeviceParams;
    for(auto& window:m_Windows)window->Resize(p.m_WindowBufferWidth?p.m_WindowBufferWidth:p.m_Width,p.m_WindowBufferHeight?p.m_WindowBufferHeight:p.m_Height,p.m_WindowBufferMultisample,p.m_ScreenWidth?p.m_ScreenWidth:p.m_Width,p.m_ScreenHeight?p.m_ScreenHeight:p.m_Height,p.m_WindowBufferColourFormat,p.m_WindowBufferDepthStencilFormat);
    if(!CreateWindows())throw std::runtime_error("Native window colour/depth/mask resources are unsupported or could not be created");
}
bool C_GameDevice::CreateWindows(){
    if(!m_Display2.IsCreated())return false;
    for(auto& window:m_Windows){
        if(window->m_DepthStencilFormat!=E_Format_D24S8||!CreateRenderTexture2D(window->m_Width,window->m_Height,1,E_Usage_Colour|E_Usage_Video,window->m_ColourFormat,window->m_Multisample,nullptr,&window->m_ColourRenderTexture)||
           !CreateSurface(window->m_Width,window->m_Height,E_Usage_DepthStencil|E_Usage_Video,window->m_DepthStencilFormat,window->m_Multisample,nullptr,&window->m_DepthStencilSurface)||
           !CreateRenderTexture2D(window->m_Width,window->m_Height,1,E_Usage_Colour|E_Usage_Video,E_Format_A8R8G8B8,window->m_Multisample,nullptr,&window->m_MaskRenderTexture)){
            DestroyWindows();return false;
        }
        window->m_ClearDepthStencil=true;window->m_MaskEnable=true;
    }
    return true;
}
void C_GameDevice::DestroyWindows(){
    if(m_Display2.IsCreated()){
        // Retire upper attachment identities before wrapper storage can disappear.
        SetColourDepthStencilSurface(nullptr,nullptr);
        for(auto& window:m_Windows){DestroyRenderTexture2D(&window->m_ColourRenderTexture);DestroyRenderTexture2D(&window->m_MaskRenderTexture);DestroySurface(&window->m_DepthStencilSurface);window->m_pDepthTexture=nullptr;}
    }
    m_CurrentWindow=-1;
}
void C_GameDevice::SetWindow(S32 index,bool){
    if(index<0){m_CurrentWindow=index;return;}auto* window=GetWindow(index);
    if(!m_InScene||!window->m_ColourRenderTexture.IsCreated()||!window->m_DepthStencilSurface.IsCreated())throw std::logic_error("Native window rendering requires a scene and created resources");
    SetColourDepthStencilSurface(window->GetColourSurface(),window->GetDepthStencilSurface());
    Clear(window->m_ClearDepthStencil?E_Clear_Colour|E_Clear_Depth|E_Clear_Stencil:E_Clear_Colour,Maths::RGBA(0,0,0,255));m_CurrentWindow=index;
}
void C_GameDevice::RestoreWindow(S32 index,U32 colourWrite){
    auto* window=GetWindow(index);if(!m_InScene||!window->m_ColourRenderTexture.IsCreated()||!m_RenderTarget.GetColourSurface())throw std::logic_error("Native window restore requires created source and current attachments");
    m_CurrentWindow=index;window->m_ColourRenderTexture.Resolve();SetBlendMode(E_BlendMode_Solid);SetTextureGamma(0,E_Bool_False);SetColourWriteEnable(colourWrite);
    const auto* destination=m_RenderTarget.GetColourSurface();const auto* source=window->GetColourSurface();if(destination==source)return;
    // Original PC uses a quad onto the back buffer and a same-format copy onto matching offscreen storage.
    if(destination->GetMultisample()==E_Multisample_None&&destination!=m_BackBuffer.GetColourSurface()&&colourWrite==E_ColourWriteEnable_All&&destination->GetWidth()==source->GetWidth()&&destination->GetHeight()==source->GetHeight()&&destination->GetFormat()==source->GetFormat())m_Display2.GetContext().CopySurface(destination,source);
    else Quad::DrawFullScreen(window->GetColourTexture());
}
void C_GameDevice::RenderWindowMask(){
    if(m_CurrentWindow<0||m_CurrentWindow>=GetWindowCount())return;auto* window=GetWindow(m_CurrentWindow);window->m_ColourRenderTexture.Resolve();if(!window->m_MaskEnable)return;
    SetColourSurface(&window->m_MaskRenderTexture.GetSurface());Clear(E_Clear_Colour,Maths::RGBA(0,0,0,0));SetBlendMode(E_BlendMode_Solid);
    C_StencilState stencil;stencil.m_StencilEnable=E_Bool_True;stencil.m_StencilFunc=E_CmpFunc_Equal;stencil.m_StencilRef=0xff;stencil.m_StencilMask=0xff;stencil.m_StencilWriteMask=0;
    SetStencilState(stencil);SetDepthEnable(E_Bool_False);SetDepthWriteEnable(E_Bool_False);SetCullMode(E_CullMode_None);Quad::DrawVertexColour(Maths::RGBA(255,255,255,255));
    stencil.m_StencilEnable=E_Bool_False;SetStencilState(stencil);SetCullMode(E_CullMode_Default);SetDepthWriteEnable(E_Bool_True);SetDepthEnable(E_Bool_True);window->m_MaskRenderTexture.Resolve();SetColourSurface(window->GetColourSurface());
}
void C_GameDevice::SetStencilState(const C_StencilState& value){
    auto& context=m_Display2.GetContext();Display2::C_StencilState state;state.SetStencilFunc(ToCmpFunc2(value.m_StencilFunc));state.SetStencilFail(static_cast<Display2::E_StencilOp>(value.m_StencilFail));state.SetStencilDepthFail(static_cast<Display2::E_StencilOp>(value.m_StencilDepthFail));state.SetStencilDepthPass(static_cast<Display2::E_StencilOp>(value.m_StencilDepthPass));state.SetStencilRef(value.m_StencilRef);state.SetStencilMask(value.m_StencilMask);state.SetStencilWriteMask(value.m_StencilWriteMask);context.SetStencilState(state);context.SetStencilEnable(ToBool2(value.m_StencilEnable));m_StencilEnable=value.m_StencilEnable;
}
void C_GameDevice::SetTextureGamma(U32 slot,E_Bool value){if(value==E_Bool_Default)value=C_SamplerBase().m_GammaDefault;auto& context=m_Display2.GetContext();auto state=context.GetSamplerState(slot);state.SetTextureGamma(ToBool2(value)==Display2::E_Bool_True);context.SetSamplerState(slot,state);}
}
