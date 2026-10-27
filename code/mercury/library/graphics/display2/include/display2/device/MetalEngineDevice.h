// [Blur2PC][macOS] Engine-facing device/manager lifecycle over actual Metal resources.
#pragma once
#include <display2/device/MetalDevice.h>
#include <display2/device/BaseDevice.h>
namespace Display2 {
namespace Metal {struct Adapter;}
class C_DeviceParams {
public:
    void* m_PresentationLayer=nullptr; // Host-owned CAMetalLayer, valid through creation.
    U32 m_Width=0,m_Height=0;bool m_VSync=true;
private:
    friend class C_Device;friend class C_DeviceManager;
    std::shared_ptr<Metal::Adapter> m_Adapter;
};
struct C_DeviceManagerParams {};
class C_Device:public C_BaseDevice,public C_MetalDevice {
public:
    ~C_Device();
    using C_MetalDevice::Destroy;
    void GetParams(C_DeviceParams& params)const;void SetParams(const C_DeviceParams& params);
    U32 IsCreated()const{return m_Stage;}
    bool Create();bool CreateVolatile();void DestroyVolatile();void Destroy();
    bool CreateVertexBuffer(U32 size,U32 flags,C_VertexBuffer* buffer,void* memory=nullptr);void DestroyVertexBuffer(C_VertexBuffer* buffer);void Destroy(C_VertexBuffer* b){DestroyVertexBuffer(b);}
    bool CreateIndexBuffer(U32 size,U32 flags,E_IndexFormat format,C_IndexBuffer* buffer,void* memory=nullptr);void DestroyIndexBuffer(C_IndexBuffer* buffer);void Destroy(C_IndexBuffer* b){DestroyIndexBuffer(b);}
    bool CreateVertexShader(U32 size,const U8* data,U32 flags,C_VertexShader* shader,void* memory=nullptr,std::string* error=nullptr);void DestroyVertexShader(C_VertexShader* shader);void Destroy(C_VertexShader* s){DestroyVertexShader(s);}
    bool CreatePixelShader(U32 size,const U8* data,U32 flags,C_PixelShader* shader,void* memory=nullptr,std::string* error=nullptr);void DestroyPixelShader(C_PixelShader* shader);void Destroy(C_PixelShader* s){DestroyPixelShader(s);}
    bool CreateSurface(U32 w,U32 h,U32 flags,E_SurfaceFormat format,E_Multisample samples,const C_SurfaceParams* params,C_Surface* surface,void* memory=nullptr,U32 size=0);void DestroySurface(C_Surface* surface);void Destroy(C_Surface* s){DestroySurface(s);}
    bool CreateTexture2D(U32 w,U32 h,U32 levels,U32 flags,E_TextureFormat format,C_Texture* texture,void* memory=nullptr,U32 size=0);
    bool CreateTexture3D(U32 w,U32 h,U32 depth,U32 levels,U32 flags,E_TextureFormat format,C_Texture* texture,void* memory=nullptr,U32 size=0);
    bool CreateTextureCube(U32 w,U32 levels,U32 flags,E_TextureFormat format,C_Texture* texture,void* memory=nullptr,U32 size=0);
    void DestroyTexture(C_Texture* texture);void Destroy(C_Texture* t){DestroyTexture(t);}
    bool GetTextureSurface(const C_Texture* texture,U32 face,U32 level,C_Surface* surface);
    bool CreateRenderTexture2D(U32 w,U32 h,U32 levels,U32 flags,E_TextureFormat tf,E_SurfaceFormat sf,E_Multisample samples,const C_SurfaceParams* params,C_RenderTexture2D* target,void* memory=nullptr,U32 size=0);
    void DestroyRenderTexture2D(C_RenderTexture2D* target);void Destroy(C_RenderTexture2D* t){DestroyRenderTexture2D(t);}
    bool CreateRenderTextureCube(U32 w,U32 levels,U32 flags,E_TextureFormat tf,E_SurfaceFormat sf,E_Multisample samples,const C_SurfaceParams* params,C_RenderTextureCube* target,void* memory=nullptr,U32 size=0);
    void DestroyRenderTextureCube(C_RenderTextureCube* target);void Destroy(C_RenderTextureCube* t){DestroyRenderTextureCube(t);}
#define METAL_DEVICE_STATE(Type,Name) const Type& Get##Name##State()const{return GetContext().Get##Name##State();}void Set##Name##State(const Type& s){GetContext().Set##Name##State(s);}
    METAL_DEVICE_STATE(C_AlphaTestState,AlphaTest) METAL_DEVICE_STATE(C_BlendState,Blend) METAL_DEVICE_STATE(C_ColourState,Colour)
    METAL_DEVICE_STATE(C_CullState,Cull) METAL_DEVICE_STATE(C_DepthState,Depth) METAL_DEVICE_STATE(C_FillState,Fill)
    METAL_DEVICE_STATE(C_ScissorState,Scissor) METAL_DEVICE_STATE(C_StencilState,Stencil)
#undef METAL_DEVICE_STATE
    const C_SamplerState& GetSamplerState(U32 i)const{return GetContext().GetSamplerState(i);}void SetSamplerState(U32 i,const C_SamplerState& s){GetContext().SetSamplerState(i,s);}
    const C_VertexSamplerState& GetVertexSamplerState(U32 i)const{return GetContext().GetVertexSamplerState(i);}void SetVertexSamplerState(U32 i,const C_VertexSamplerState& s){GetContext().SetVertexSamplerState(i,s);}
    void SetCullMode(E_CullMode v){GetContext().SetCullMode(v);}void SetFillMode(E_FillMode v){GetContext().SetFillMode(v);}
    void SetMaxAnisotropy(U32 v){GetContext().SetMaxAnisotropy(v);}void SetDepthBias(F32 v){GetContext().SetDepthBias(v);}void SetSlopeScaledBias(F32 v){GetContext().SetSlopeScaledBias(v);}
    void SetAlphaTestEnable(E_Bool v){GetContext().SetAlphaTestEnable(v);}void SetBlendEnable(E_Bool v){GetContext().SetBlendEnable(v);}void SetStencilEnable(E_Bool v){GetContext().SetStencilEnable(v);}
    void SetIndexBuffer(const C_IndexBuffer* b){GetContext().SetIndexBuffer(b);}void SetVertexDeclaration(const C_VertexDeclaration* d){GetContext().SetVertexDeclaration(d);}
    void SetVertexBuffer(U32 stream,const C_VertexBuffer* b,U32 offset,U32 stride){GetContext().SetVertexBuffer(stream,b,offset,stride);}
    void SetVertexShader(const C_VertexShader* s){GetContext().SetVertexShader(s);}void SetPixelShader(const C_PixelShader* s){GetContext().SetPixelShader(s);}
    void SetVertexShaderConstantF(U32 start,const F32* data,U32 count){GetContext().SetVertexShaderConstantF(start,data,count);}
    void SetVertexShaderParamF(const C_VertexShaderParam& p,const F32* data){GetContext().SetVertexShaderParamF(p,data);}
    void SetPixelShaderParamF(const C_PixelShaderParam& p,const F32* data,U32 count=1){GetContext().SetPixelShaderParamF(p,data,count);}
    void SetPixelShaderConstantF(U32 start,const F32* data,U32 count){GetContext().SetPixelShaderConstantF(start,data,count);}
    void SetTexture(U32 i,const C_Texture* t){GetContext().SetTexture(i,t);}void SetVertexTexture(U32 i,const C_Texture* t){GetContext().SetVertexTexture(i,t);}
    void SetViewport(const C_Viewport& v){GetContext().SetViewport(v);}void SetColourSurface(U32 i,const C_Surface* s){GetContext().SetColourSurface(i,s);}void SetDepthStencilSurface(const C_Surface* s){GetContext().SetDepthStencilSurface(s);}
    void BeginScene(){GetContext().BeginScene();}void EndScene(){GetContext().EndScene();}void Clear(U32 flags,U32 colour,F32 depth=1,U8 stencil=0){GetContext().Clear(flags,colour,depth,stencil);}
    void DrawPrimitive(E_PrimitiveType type,U32 start,U32 count){GetContext().DrawPrimitive(type,start,count);}
    void DrawIndexedPrimitive(E_PrimitiveType type,U32 base,U32 minimum,U32 vertices,U32 start,U32 count){GetContext().DrawIndexedPrimitive(type,base,minimum,vertices,start,count);}
    bool CreateColourSurface(U32 w,U32 h,U32 flags,E_SurfaceFormat format,E_Multisample samples,const C_SurfaceParams* p,C_Surface* s,void* memory=nullptr,U32 size=0){return CreateSurface(w,h,flags|E_Flags_Colour,format,samples,p,s,memory,size);}
    bool CreateDepthStencilSurface(U32 w,U32 h,U32 flags,E_SurfaceFormat format,E_Multisample samples,const C_SurfaceParams* p,C_Surface* s,void* memory=nullptr,U32 size=0){return CreateSurface(w,h,flags|E_Flags_DepthStencil,format,samples,p,s,memory,size);}
private:
    C_DeviceParams m_Params;U32 m_Stage=0;
};
class C_DeviceManager {
public:
    bool IsCreated()const{return bool(m_Adapter);}
    bool Create(const C_DeviceManagerParams& params);void Destroy();
    void GetParams(C_DeviceManagerParams& params)const;void SetParams(const C_DeviceManagerParams& params);
    void SetupDefaultDeviceParams(C_DeviceParams& params);
    std::string GetDeviceName()const;
private:
    C_DeviceManagerParams m_Params;
    std::shared_ptr<Metal::Adapter> m_Adapter;
};
}
