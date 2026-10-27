// [Blur2PC][macOS] Actual upper game-device lifecycle/resources over the original CPU base and Metal Display2.
#pragma once
#include <core/Maths.h>
#include <display/device/DeviceBase.h>
#include <display/device/metal/MetalResources.h>
#include <display/device/metal/MetalGameBuffers.h>
#include <display/device/metal/MetalGameWindow.h>
#include <vector>
#include <display/device/metal/NativeDeviceAccess.h>
#include <display/memory/MemoryTracker.h>
#include <display/memory/metal/MetalRingBuffer.h>
#include <display/buffer/DynamicVertexBuffer.h>
#include <display/DisplayToDisplay2.h>
#include <display2/device/Device.h>
#include <string>
#include <mutex>
#include <thread>
namespace Display::Metal {
struct C_DeviceParams {
    U32 m_Width=0,m_Height=0,m_ScreenWidth=0,m_ScreenHeight=0;
    E_Multisample m_Multisample=E_Multisample_None;
    E_Format m_ColourFormat=E_Format_A8R8G8B8,m_DepthStencilFormat=E_Format_D24S8;
    U32 m_RefreshRate=0;bool m_Windowed=true;
    U32 m_WindowBufferWidth=0,m_WindowBufferHeight=0;
    E_Format m_WindowBufferColourFormat=E_Format_A8R8G8B8,m_WindowBufferDepthStencilFormat=E_Format_D24S8;
    E_Multisample m_WindowBufferMultisample=E_Multisample_None;
    U32 m_BeginVerticesSize=1024*256+1024*1024*2,m_FreeBeginVerticesSize=1024*256; // Original PC defaults.
    E_PresentInterval m_PresentInterval=E_PresentInterval_One;
    void* m_PresentationLayer=nullptr;
    void NormalizeWindowDimensions(){ // PC Setup is empty; native dimensions originate from the actual host/settings.
        if(!m_ScreenWidth)m_ScreenWidth=m_Width;if(!m_ScreenHeight)m_ScreenHeight=m_Height;
        if(!m_WindowBufferWidth)m_WindowBufferWidth=m_Width;if(!m_WindowBufferHeight)m_WindowBufferHeight=m_Height;
    }
};
class C_GameDevice:public C_DeviceBase,public C_MemoryTracker {
public:
    C_GameDevice();~C_GameDevice();
    bool BeginScene();void EndScene();bool Present();
    bool AcquireThreadOwnership(U32 timeMs=WAIT_TIME_INFINITE);void ReleaseThreadOwnership();bool HaveThreadOwnership()const;
    void BlockUntilIdle();void SetPresentInterval(E_PresentInterval interval);
    void SetupView(C_View* view,const C_ViewSetup& setup);void SetupView(C_View* view,const C_ViewSetup& setup,U32 width,U32 height);
    void Clear(U32 flags);void Clear(U32 flags,Maths::RGBA colour);void Clear(U32 flags,Maths::RGBA colour,F32 depth,U8 stencil);
    void DefaultStates();void Reset();void Unbind();
    void SetBlendMode(E_BlendMode mode);
    void SetAlphaTestEnable(E_Bool);void SetAlphaTestFunc(E_CmpFunc);void SetAlphaTestRef(U32);
    void SetBlendEnable(E_Bool);void SetBlendSrc(E_Blend);void SetBlendDst(E_Blend);void SetBlendOp(E_BlendOp);
    void SetBlend(E_Blend src,E_Blend dst,E_BlendOp op,E_Blend srcAlpha,E_Blend dstAlpha,E_BlendOp opAlpha){SetBlendSrc(src);SetBlendDst(dst);SetBlendOp(op);SetBlendSrcAlpha(srcAlpha);SetBlendDstAlpha(dstAlpha);SetBlendOpAlpha(opAlpha);}
    void SetBlendSrcAlpha(E_Blend);void SetBlendDstAlpha(E_Blend);void SetBlendOpAlpha(E_BlendOp);
    void SetColourWriteEnable(U32);
    void SetColourWriteEnable(U32 target,U32 mask);U32 GetColourWriteEnable(U32 target)const;
    S32 AddWindow(const Maths::C_Rect& coords);void ClearWindows();void ResizeWindows();bool CreateWindows();void DestroyWindows();
    S32 GetWindowCount()const{return static_cast<S32>(m_Windows.size());}
    C_Window* GetWindow(S32 index);const C_Window* GetWindow(S32 index)const;
    void SetWindow(S32 index,bool aa=true);void EndWindow(){m_CurrentWindow=-1;}
    void RestoreWindow(S32 index,U32 colourWrite=E_ColourWriteEnable_All);void RenderWindowMask();
    void SetStencilState(const C_StencilState& state);
    void SetStencilEnable(E_Bool v){m_Display2.GetContext().SetStencilEnable(ToBool2(v));m_StencilEnable=v;}
    void SetStencilFunc(E_CmpFunc v){auto state=m_Display2.GetContext().GetStencilState();state.SetStencilFunc(ToCmpFunc2(v));m_Display2.GetContext().SetStencilState(state);}
    void SetStencilFail(E_StencilOp v){auto state=m_Display2.GetContext().GetStencilState();state.SetStencilFail(static_cast<Display2::E_StencilOp>(v));m_Display2.GetContext().SetStencilState(state);}
    void SetStencilDepthFail(E_StencilOp v){auto state=m_Display2.GetContext().GetStencilState();state.SetStencilDepthFail(static_cast<Display2::E_StencilOp>(v));m_Display2.GetContext().SetStencilState(state);}
    void SetStencilPass(E_StencilOp v){auto state=m_Display2.GetContext().GetStencilState();state.SetStencilDepthPass(static_cast<Display2::E_StencilOp>(v));m_Display2.GetContext().SetStencilState(state);}
    void SetStencilRef(S32 v){auto state=m_Display2.GetContext().GetStencilState();state.SetStencilRef(U32(v));m_Display2.GetContext().SetStencilState(state);}
    void SetStencilMask(U32 v){auto state=m_Display2.GetContext().GetStencilState();state.SetStencilMask(v);m_Display2.GetContext().SetStencilState(state);}
    void SetStencilWriteMask(U32 v){auto state=m_Display2.GetContext().GetStencilState();state.SetStencilWriteMask(v);m_Display2.GetContext().SetStencilState(state);}
void SetTextureGamma(U32 sampler,E_Bool gamma);
    void SetDepthEnable(E_Bool);void SetDepthWriteEnable(E_Bool);void SetDepthFunc(E_CmpFunc);void SetCullMode(E_CullMode);
    void SetViewport(const C_Viewport& viewport);
    void SetScissorRect(U32 x,U32 y,U32 width,U32 height){if(U64(x)+width>0x7fffffff||U64(y)+height>0x7fffffff)throw std::out_of_range("Native scissor extent overflows");m_Display2.GetContext().SetScissorRect({S32(x),S32(y),S32(x+width),S32(y+height)});}
    void SetScissorTest(E_Bool enabled){m_Display2.GetContext().SetScissorEnable(ToBool2(enabled));}

    void SetCullModeFlip(){SetCullModeFlip(!m_CullModeFlip);}
    void SetCullModeFlip(bool flip){m_CullModeFlip=flip;SetCullMode(flip?m_CullMode:FlippedCullMode(m_CullMode));}
    void SetDepthBias(F32 bias){m_Display2.GetContext().SetDepthBias(bias);m_DepthBias=bias;}
    const C_BackBuffer& GetBackBuffer()const{return m_BackBuffer;}
    const C_RenderTarget& GetRenderTarget()const{return m_RenderTarget;}
    void SetColourDepthStencilSurface(const C_Surface* colour,const C_Surface* depth);
    void SetColourDepthStencilSurface(){SetColourDepthStencilSurface(&m_BackBuffer.m_ColourSurface,m_BackBuffer.m_DepthStencilSurface.IsCreated()?&m_BackBuffer.m_DepthStencilSurface:nullptr);}
    void SetColourSurface(){SetColourSurface(m_BackBuffer.GetColourSurface());}
    void SetDepthStencilSurface(){SetDepthStencilSurface(m_BackBuffer.GetDepthStencilSurface());}
    void SetColourSurface(const C_Surface* colour){SetColourDepthStencilSurface(colour,m_RenderTarget.GetDepthStencilSurface());}
    void SetVertexShaderParameterF(C_VertexShaderParameter parameter,const F32* data,U32 count=0){if(count&&parameter.m_RegisterCount>count)parameter.m_RegisterCount=count;m_Display2.GetContext().SetVertexShaderParamF(parameter,data);}
    void SetPixelShaderParameterF(C_PixelShaderParameter parameter,const F32* data){m_Display2.GetContext().SetPixelShaderParamF(parameter,data);}
    void SetPixelShaderParameterArrayF(const C_PixelShaderParameterArray& parameter,const F32* data){m_Display2.GetContext().SetPixelShaderParamArrayF(parameter,data);}
    void SetPixelShaderParameters(const C_PixelShader*){} // Original PC behavior: current constants are captured/uploaded by each actual draw.
    void SetFillMode(E_FillMode mode){const auto resolved=mode==E_FillMode_Default?m_FillModeDefault:mode;m_Display2.GetContext().SetFillMode(static_cast<Display2::E_FillMode>(resolved));m_FillMode=mode;}
    void SetTextureFilter(U32 slot,E_TextureFilter filter){SetTextureFilterMagMinMip(slot,filter,filter,filter);}
    void SetTextureAddress(U32 slot,E_TextureAddress address){SetTextureAddressUVW(slot,address,address,address);}
    void SetView(const C_View* view,bool setStates=true);
    void SetViewShaderConstants();void SetViewShaderConstants2D();void SetViewShaderConstants3D();
    bool BeginVerticesLocked()const{return m_BeginVerticesLocked;}
    void* BeginVertices(E_PrimitiveType type,U32 count,U32 stride,C_VertexDeclaration* declaration,bool checkUsage=false);
    void EndVertices(void* end=nullptr);void FakeEndVertices(void* end=nullptr);
    void SetMaxBeginVerticesUsage(U32 bytes);void SetUserMaxBeginVerticesUsage(F32 scale);
    const std::string& GetNativeError()const{return m_LastError;}
    Display2::C_Device& GetDisplay2(){return m_Display2;}
    C_Surface* GetNativeColourSurface(){return &m_BackBuffer.m_ColourSurface;}
    C_Surface* GetNativeDepthSurface(){return &m_BackBuffer.m_DepthStencilSurface;}
    const C_Surface* GetNativeColourSurface()const{return &m_BackBuffer.m_ColourSurface;}
    const C_Surface* GetNativeDepthSurface()const{return &m_BackBuffer.m_DepthStencilSurface;}
    bool CanRender()const{return m_Created&&m_Volatile&&m_Display2.IsCreated()&&m_BackBuffer.m_ColourSurface.IsCreated()&&(m_DeviceParams.m_DepthStencilFormat==E_Format_Unknown||m_BackBuffer.m_DepthStencilSurface.IsCreated());}
    bool CanReset()const{return m_Created&&m_Display2.IsCreated()&&!m_InScene;}
    bool GetValid()const{return CanRender();}
    bool GetLost()const{return m_Created&&!m_Display2.IsCreated();} // Native command errors are reported by operations/WaitIdle, separately from missing GPU ownership.
    void DrawPrimitive(E_PrimitiveType type,U32 start,U32 count){m_Display2.GetContext().DrawPrimitive(ToPrimitiveType2(type),start,count);}
    void DrawIndexedPrimitive(E_PrimitiveType type,U32 base,U32 minimum,U32 vertices,U32 start,U32 count){m_Display2.GetContext().DrawIndexedPrimitive(ToPrimitiveType2(type),base,minimum,vertices,start,count);}
    void SetVertexDeclaration(const C_VertexDeclaration* declaration){m_Display2.GetContext().SetVertexDeclaration(declaration);}
    void SetVertexBuffer(U32 stream,const C_VertexBuffer* buffer,U32 offset,U32 stride){m_Display2.GetContext().SetVertexBuffer(stream,buffer,offset,stride);}
    void SetIndexBuffer(const C_IndexBuffer* buffer){m_Display2.GetContext().SetIndexBuffer(buffer);}
    void SetVertexShader(const C_VertexShader* shader){m_Display2.GetContext().SetVertexShader(shader);}
    void SetPixelShader(const C_PixelShader* shader){m_Display2.GetContext().SetPixelShader(shader);}
    void SetVertexShaderConstantF(U32 start,const F32* values,U32 count){m_Display2.GetContext().SetVertexShaderConstantF(start,values,count);}
    void SetPixelShaderConstantF(U32 start,const F32* values,U32 count){m_Display2.GetContext().SetPixelShaderConstantF(start,values,count);}
    void SetTexture(U32 sampler,const C_Texture* texture){m_Display2.GetContext().SetTextureWithColourSnapshot(sampler,texture);}
    void SetTextureBorderColour(U32 index,U32 colour){auto state=m_Display2.GetContext().GetSamplerState(index);state.SetTextureBorderColour(colour);m_Display2.GetContext().SetSamplerState(index,state);}
    void SetTextureFilterMag(U32 i,E_TextureFilter v){auto state=m_Display2.GetContext().GetSamplerState(i);state.m_TextureFilterMag=ToTextureFilter2(v);m_Display2.GetContext().SetSamplerState(i,state);}
    void SetTextureFilterMin(U32 i,E_TextureFilter v){auto state=m_Display2.GetContext().GetSamplerState(i);state.m_TextureFilterMin=ToTextureFilter2(v);m_Display2.GetContext().SetSamplerState(i,state);}
    void SetTextureFilterMip(U32 i,E_TextureFilter v){auto state=m_Display2.GetContext().GetSamplerState(i);state.m_TextureFilterMip=ToTextureFilter2(v);m_Display2.GetContext().SetSamplerState(i,state);}
    void SetTextureAddressU(U32 i,E_TextureAddress v){auto state=m_Display2.GetContext().GetSamplerState(i);state.SetTextureAddressU(ToTextureAddress2(v));m_Display2.GetContext().SetSamplerState(i,state);}
    void SetTextureAddressV(U32 i,E_TextureAddress v){auto state=m_Display2.GetContext().GetSamplerState(i);state.SetTextureAddressV(ToTextureAddress2(v));m_Display2.GetContext().SetSamplerState(i,state);}
    void SetTextureLodBias(U32 i,S8 v){auto state=m_Display2.GetContext().GetSamplerState(i);state.SetTextureLodBias(v);m_Display2.GetContext().SetSamplerState(i,state);}
    void SetTextureAddressUVW(U32 sampler,E_TextureAddress u,E_TextureAddress v,E_TextureAddress w);
    void SetTextureFilterMagMinMip(U32 sampler,E_TextureFilter mag,E_TextureFilter min,E_TextureFilter mip);
    void SetColourSurface(U32 index,const C_Surface* surface);
    void SetDepthStencilSurface(const C_Surface* surface){m_Display2.GetContext().SetDepthStencilSurface(surface);m_RenderTarget.m_pDepthStencilSurface=surface;}
    bool CreateVertexBuffer(U32 size,U32 flags,C_VertexBuffer* buffer,bool=false){return m_Display2.CreateVertexBuffer(size,flags,buffer);}
    bool CreateIndexBuffer(U32 size,U32 flags,E_Format format,C_IndexBuffer* buffer){return m_Display2.CreateIndexBuffer(size,flags,static_cast<Display2::E_IndexFormat>(format),buffer);}
    void DestroyVertexBuffer(C_VertexBuffer* buffer){m_Display2.DestroyVertexBuffer(buffer);}
    void DestroyIndexBuffer(C_IndexBuffer* buffer){m_Display2.DestroyIndexBuffer(buffer);}
    bool CreateVertexShader(U32 size,const S32* data,U32 flags,C_VertexShader* shader){return m_Display2.CreateVertexShader(size,reinterpret_cast<const U8*>(data),flags,shader,nullptr,&m_LastError);}
    bool CreatePixelShader(U32 size,const S32* data,U32 flags,C_PixelShader* shader){return m_Display2.CreatePixelShader(size,reinterpret_cast<const U8*>(data),flags,shader,nullptr,&m_LastError);}
    void DestroyVertexShader(C_VertexShader* shader){m_Display2.DestroyVertexShader(shader);}
    void DestroyPixelShader(C_PixelShader* shader){m_Display2.DestroyPixelShader(shader);}
    bool CreateSurface(U32 width,U32 height,U32 flags,E_Format format,E_Multisample samples,const C_SurfaceParams* params,C_Surface* surface){return m_Display2.CreateSurface(width,height,flags,static_cast<Display2::E_SurfaceFormat>(format),static_cast<Display2::E_Multisample>(samples),params,surface);}
    bool CreateDepthStencilSurface(U32 w,U32 h,U32 flags,E_Format format,E_Multisample samples,const C_SurfaceParams* params,C_Surface* surface,void* memory=nullptr,U32 bytes=0){return m_Display2.CreateDepthStencilSurface(w,h,flags,static_cast<Display2::E_SurfaceFormat>(format),static_cast<Display2::E_Multisample>(samples),params,surface,memory,bytes);}
    void DestroySurface(C_Surface* surface){m_Display2.DestroySurface(surface);}
    bool CreateTexture2D(U32 w,U32 h,U32 levels,U32 flags,E_Format format,C_Texture2D* texture){return m_Display2.CreateTexture2D(w,h,levels,flags,static_cast<Display2::E_TextureFormat>(format),texture);}
    bool CreateTexture3D(U32 w,U32 h,U32 depth,U32 levels,U32 flags,E_Format format,C_Texture3D* texture){return m_Display2.CreateTexture3D(w,h,depth,levels,flags,static_cast<Display2::E_TextureFormat>(format),texture);}
    bool CreateTextureCube(U32 w,U32 levels,U32 flags,E_Format format,C_TextureCube* texture){return m_Display2.CreateTextureCube(w,levels,flags,static_cast<Display2::E_TextureFormat>(format),texture);}
    void DestroyTexture2D(C_Texture2D* texture){m_Display2.DestroyTexture(texture);}
    void DestroyTexture3D(C_Texture3D* texture){m_Display2.DestroyTexture(texture);}
    void DestroyTextureCube(C_TextureCube* texture){m_Display2.DestroyTexture(texture);}
    bool CreateRenderTexture2D(U32 w,U32 h,U32 levels,U32 flags,E_Format format,E_Multisample samples,const C_SurfaceParams* params,C_RenderTexture2D* target,void* memory=nullptr,U32 bytes=0){return m_Display2.CreateRenderTexture2D(w,h,levels,flags,static_cast<Display2::E_TextureFormat>(format),static_cast<Display2::E_SurfaceFormat>(format),static_cast<Display2::E_Multisample>(samples),params,target,memory,bytes);}
    bool CreateRenderTextureCube(U32 w,U32 levels,U32 flags,E_Format format,E_Multisample samples,const C_SurfaceParams* params,C_RenderTextureCube* target){return m_Display2.CreateRenderTextureCube(w,levels,flags,static_cast<Display2::E_TextureFormat>(format),static_cast<Display2::E_SurfaceFormat>(format),static_cast<Display2::E_Multisample>(samples),params,target);}
    void DestroyRenderTexture2D(C_RenderTexture2D* target){m_Display2.DestroyRenderTexture2D(target);}
    void DestroyRenderTextureCube(C_RenderTextureCube* target){m_Display2.DestroyRenderTextureCube(target);}
    C_DeviceParams m_DeviceParams; // Original PC application/view code shares the configured dimensions.
protected:
    bool CreateDevice();bool CreateDeviceVolatile();void DestroyDeviceVolatile();void DestroyDevice();
    C_VertexDeclaration* InitVertexDeclaration(S32 count,const C_VertexElement* elements);
    Display2::C_Device m_Display2;
private:
    MT::Mutex m_ThreadOwnershipMutex;
    mutable std::mutex m_OwnershipMetadataMutex;std::thread::id m_OwnerThread;U32 m_OwnerDepth=0;
    Display2::C_DeviceManager m_Manager;
    C_NativeDeviceBinding m_Binding;
    C_BackBuffer m_BackBuffer;C_RenderTarget m_RenderTarget;bool m_Volatile=false;
    C_DynamicVertexBuffer m_DynamicVertexBuffer;
    U32 m_BeginVerticesUsage=0,m_MaxBeginVerticesUsage=0,m_UserBeginVerticesUsage=0,m_UserMaxBeginVerticesUsage=0;
    bool m_BeginVerticesLocked=false;
    std::vector<std::unique_ptr<C_Window>> m_Windows;S32 m_CurrentWindow=-1;
    std::string m_LastError;
};
}
namespace Display {using C_DeviceParams=Metal::C_DeviceParams;}
