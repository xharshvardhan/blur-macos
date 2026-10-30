// [Blur2PC][macOS] Native upper device owns actual tracked Metal resources and a host-supplied presentation layer.
#include "NativeDisplayIncludes.h"
#include <display/device/metal/MetalGameDevice.h>
#include <stdexcept>
#include <cmath>
#include <cstdlib>
#include <display/device/metal/MetalWindowMSAAPolicy.h>
#include <display/shaders/Constants.h>
namespace Display::Metal {
C_GameDevice::C_GameDevice():m_Binding(*this){
    C_DeviceBase::InvalidateState(); // [Blur2PC] Initialize caches before selecting the actual PC device policy.
    // Original PCD3D11GameDevice selects SetDepthFlip(true,false): reverse the
    // projection/clear/default comparison together, without reversing viewport Z.
    // No GPU state exists yet; CreateVolatile/DefaultStates publishes this default.
    m_DepthFuncDefault=E_CmpFunc_GreaterEqual;m_DepthCmpFlip=true;m_DepthViewportFlip=false;
}
C_GameDevice::~C_GameDevice(){try{DestroyDevice();}catch(const std::exception& error){LOGERROR(g_Log,"Native upper device cleanup failed: %s",error.what());}}
bool C_GameDevice::AcquireThreadOwnership(U32 timeMs){
    if(!m_ThreadOwnershipMutex.Wait(timeMs))return false;
    std::lock_guard<std::mutex> guard(m_OwnershipMetadataMutex);m_OwnerThread=std::this_thread::get_id();++m_OwnerDepth;return true;
}
bool C_GameDevice::HaveThreadOwnership()const{
    std::lock_guard<std::mutex> guard(m_OwnershipMetadataMutex);return m_OwnerDepth&&m_OwnerThread==std::this_thread::get_id();
}
void C_GameDevice::ReleaseThreadOwnership(){
    {std::lock_guard<std::mutex> guard(m_OwnershipMetadataMutex);
        if(!m_OwnerDepth||m_OwnerThread!=std::this_thread::get_id())throw std::logic_error("Native device ownership release from a non-owner thread");
        if(!--m_OwnerDepth)m_OwnerThread={};
    }
    m_ThreadOwnershipMutex.Release();
}
void C_GameDevice::BlockUntilIdle(){
    if(m_InScene||m_BeginVerticesLocked)throw std::logic_error("Cannot wait for native GPU completion while a scene is recording");
    if(!m_Display2.IsCreated()||!m_Display2.WaitIdle())throw std::runtime_error("Native GPU completion failed");
}
void C_GameDevice::SetPresentInterval(E_PresentInterval interval){
    // CommonRender updates this inside BeginScene/EndScene. It changes future drawable presentation, not the recording render pass.
    if(!m_Display2.SetPresentationInterval(static_cast<Display2::E_PresentInterval>(interval)))throw std::runtime_error("Native presentation interval could not be applied: "+std::to_string(U32(interval)));
    m_DeviceParams.m_PresentInterval=interval;
}
void C_GameDevice::SetupView(C_View* view,const C_ViewSetup& setup){SetupView(view,setup,m_RenderTarget.GetColourWidth(),m_RenderTarget.GetColourHeight());}
void C_GameDevice::SetupView(C_View* view,const C_ViewSetup& setup,U32 width,U32 height){
    if(!view||!width||!height)throw std::invalid_argument("Native view setup requires a view and nonzero target dimensions");view->Setup(setup,width,height);
}
bool C_GameDevice::CreateDevice(){
    // [Blur2PC] Shared C_Device::Create publishes these selected params back to
    // the application. Read only at creation, never during static construction.
    unsigned windowSamples=U32(m_DeviceParams.m_WindowBufferMultisample);
    if(!SelectWindowMultisample(std::getenv("BLUR_MAC_WINDOW_MSAA"),windowSamples,windowSamples)){
        m_LastError=WindowMSAAConfigurationError;return false;
    }
    m_DeviceParams.m_WindowBufferMultisample=static_cast<E_Multisample>(windowSamples);
    if(!m_DeviceParams.m_Width||!m_DeviceParams.m_Height||m_DeviceParams.m_Width>16384||m_DeviceParams.m_Height>16384){m_LastError="Invalid native framebuffer dimensions";return false;}
    if(!m_DeviceParams.m_Windowed||m_DeviceParams.m_RefreshRate){m_LastError="Native fullscreen/mode selection is not implemented";return false;}
    if(m_DeviceParams.m_PresentInterval!=E_PresentInterval_Default&&m_DeviceParams.m_PresentInterval!=E_PresentInterval_One&&m_DeviceParams.m_PresentInterval!=E_PresentInterval_Immediate){m_LastError="Native presentation interval is unsupported";return false;}
    if(Display2::DeviceExists()){m_LastError="Another native Display2 device is active";return false;}
    Display2::C_DeviceManagerParams mp;if(!Display2::CreateDeviceManager(&m_Manager,mp)){m_LastError="Native Metal adapter creation failed";return false;}
    Display2::C_DeviceParams params;Display2::SetupDefaultDeviceParams(&m_Manager,params);
    if(!Display2::CreateDevice(&m_Manager,&m_Display2,params)){Display2::DestroyDeviceManager(&m_Manager);m_LastError="Native Metal device creation failed";return false;}
    if(!m_Display2.SupportsMultisample(static_cast<Display2::E_Multisample>(windowSamples))){
        m_Display2.Destroy();Display2::DestroyDeviceManager(&m_Manager);
        m_LastError="Native display-window MSAA is unsupported: "+std::to_string(windowSamples)+" (no quality fallback)";return false;
    }
    Display2::Device(&m_Display2);m_ShaderProfile=E_ShaderProfile_3_0;m_VertexShaderProfile=E_VertexShaderProfile_3_0;m_PixelShaderProfile=E_PixelShaderProfile_3_0;
    SetAspectRatio(F32(m_DeviceParams.m_Width)/m_DeviceParams.m_Height);m_LastError.clear();return true;
}
bool C_GameDevice::CreateDeviceVolatile(){
    if(!m_Display2.IsCreated()||m_InScene){m_LastError="Native device is not ready for framebuffer creation";return false;}
    // [Blur2PC] Reject malformed/unsupported requests before destroying an old
    // window or allocating any new colour/depth/mask framebuffer resources.
    unsigned windowSamples=U32(m_DeviceParams.m_WindowBufferMultisample);
    if(!SelectWindowMultisample(std::getenv("BLUR_MAC_WINDOW_MSAA"),windowSamples,windowSamples)){
        m_LastError=WindowMSAAConfigurationError;return false;
    }
    if(!m_Display2.SupportsMultisample(static_cast<Display2::E_Multisample>(windowSamples))){
        m_LastError="Native display-window MSAA is unsupported: "+std::to_string(windowSamples)+" (no quality fallback)";return false;
    }
    m_DeviceParams.m_WindowBufferMultisample=static_cast<E_Multisample>(windowSamples);
    DestroyDeviceVolatile();
    if(m_DeviceParams.m_PresentationLayer&&m_DeviceParams.m_ColourFormat!=E_Format_A8R8G8B8){m_LastError="Native drawable presentation requires BGRA8";return false;}
    if(!m_DeviceParams.m_BeginVerticesSize||m_DeviceParams.m_FreeBeginVerticesSize>m_DeviceParams.m_BeginVerticesSize){m_LastError="Invalid original dynamic vertex budget";return false;}
    const auto samples=static_cast<Display2::E_Multisample>(m_DeviceParams.m_Multisample);
    if(!m_Display2.CreateSurface(m_DeviceParams.m_Width,m_DeviceParams.m_Height,Display2::E_Flags_Colour,static_cast<Display2::E_SurfaceFormat>(m_DeviceParams.m_ColourFormat),samples,nullptr,&m_BackBuffer.m_ColourSurface)){m_LastError="Native colour framebuffer format/samples are unsupported";return false;}
    if(m_DeviceParams.m_DepthStencilFormat!=E_Format_Unknown&&!m_Display2.CreateSurface(m_DeviceParams.m_Width,m_DeviceParams.m_Height,Display2::E_Flags_DepthStencil,static_cast<Display2::E_SurfaceFormat>(m_DeviceParams.m_DepthStencilFormat),samples,nullptr,&m_BackBuffer.m_DepthStencilSurface)){m_Display2.DestroySurface(&m_BackBuffer.m_ColourSurface);m_LastError="Native depth/stencil framebuffer format/samples are unsupported";return false;}
    if((m_DeviceParams.m_PresentationLayer&&!m_Display2.SetPresentationLayer(m_DeviceParams.m_PresentationLayer,m_DeviceParams.m_Width,m_DeviceParams.m_Height,m_DeviceParams.m_PresentInterval!=E_PresentInterval_Immediate))||!m_Display2.SetPresentationInterval(static_cast<Display2::E_PresentInterval>(m_DeviceParams.m_PresentInterval))){DestroyDeviceVolatile();m_LastError="Native presentation layer/interval attachment failed";return false;}
#if TRACK_DISPLAY2_RESOURCES
    m_Display2.SetSurfaceName(&m_BackBuffer.m_ColourSurface,"Native upper colour framebuffer");if(m_BackBuffer.m_DepthStencilSurface.IsCreated())m_Display2.SetSurfaceName(&m_BackBuffer.m_DepthStencilSurface,"Native upper depth framebuffer");
#endif
    if(!m_DynamicVertexBuffer.Create(m_DeviceParams.m_BeginVerticesSize)){DestroyDeviceVolatile();m_LastError="Native dynamic vertex buffers could not be created";return false;}
    m_MaxBeginVerticesUsage=m_UserMaxBeginVerticesUsage=m_DeviceParams.m_BeginVerticesSize-m_DeviceParams.m_FreeBeginVerticesSize;
    m_BeginVerticesUsage=m_UserBeginVerticesUsage=0;
    m_Viewport.m_X=m_Viewport.m_Y=0;m_Viewport.m_W=m_DeviceParams.m_Width;m_Viewport.m_H=m_DeviceParams.m_Height;m_Viewport.m_MinZ=0;m_Viewport.m_MaxZ=1;SetViewport(m_Viewport);
    m_BackBuffer.m_Width=m_DeviceParams.m_Width;m_BackBuffer.m_Height=m_DeviceParams.m_Height;m_BackBuffer.m_ColourFormat=m_DeviceParams.m_ColourFormat;m_BackBuffer.m_DepthStencilFormat=m_DeviceParams.m_DepthStencilFormat;m_BackBuffer.m_Multisample=m_DeviceParams.m_Multisample;
    m_Volatile=true;
    try{ResizeWindows();}catch(const std::exception& error){m_LastError=error.what();DestroyDeviceVolatile();return false;}
    SetColourDepthStencilSurface();SetAspectRatio(F32(m_DeviceParams.m_Width)/m_DeviceParams.m_Height);m_LastError.clear();return true;
}
void C_GameDevice::DestroyDeviceVolatile(){
    if(!m_Display2.IsCreated()){m_Volatile=false;return;}if(m_BeginVerticesLocked)FakeEndVertices();if(m_InScene)EndScene();
    DestroyWindows();m_DynamicVertexBuffer.Destroy();
    SetColourDepthStencilSurface(nullptr,nullptr);
    if(!m_Display2.SetPresentationLayer(nullptr,0,0))throw std::runtime_error("Native upper presentation detachment failed");
    m_Display2.DestroySurface(&m_BackBuffer.m_ColourSurface);m_Display2.DestroySurface(&m_BackBuffer.m_DepthStencilSurface);m_BackBuffer.m_Width=m_BackBuffer.m_Height=0;m_BackBuffer.m_ColourFormat=m_BackBuffer.m_DepthStencilFormat=E_Format_Unknown;m_Volatile=false;
}
void C_GameDevice::DestroyDevice(){
    DestroyDeviceVolatile();ClearWindows();if(m_Display2.IsCreated()){if(!m_Display2.WaitIdle())throw std::runtime_error("Native upper GPU completion failed");Display2::DestroyDevice(&m_Manager,&m_Display2);}
    if(m_Manager.IsCreated())Display2::DestroyDeviceManager(&m_Manager);
}
bool C_GameDevice::BeginScene(){if(!CanRender()||m_InScene)return false;m_pView=nullptr;SetColourDepthStencilSurface();Reset();m_Display2.BeginScene();m_InScene=true;return true;}
void C_GameDevice::EndScene(){if(m_BeginVerticesLocked)throw std::logic_error("Cannot end a native scene while vertices are mapped");if(!m_InScene)throw std::logic_error("Native upper scene is not active");m_Display2.EndScene();m_pView=nullptr;Reset();m_InScene=false;}
bool C_GameDevice::Present(){
    if(!CanRender()||m_InScene||m_BeginVerticesLocked)return false;
    const bool presented=m_DeviceParams.m_PresentationLayer&&m_Display2.PresentSurface(&m_BackBuffer.m_ColourSurface);
    // Original PC rotates the dynamic buffers and resets both budgets after each presentation attempt.
    m_DynamicVertexBuffer.Swap();m_BeginVerticesUsage=m_UserBeginVerticesUsage=0;return presented;
}
void C_GameDevice::Clear(U32 flags){Clear(flags,m_ColourClear,GetDepthClear(),m_StencilClear);}
void C_GameDevice::Clear(U32 flags,Maths::RGBA colour){Clear(flags,colour,GetDepthClear(),m_StencilClear);}
void C_GameDevice::Clear(U32 flags,Maths::RGBA colour,F32 depth,U8 stencil){m_Display2.Clear(flags,(U32(colour.A())<<24)|(U32(colour.R())<<16)|(U32(colour.G())<<8)|U32(colour.B()),depth,stencil);}
void C_GameDevice::DefaultStates(){
    C_DeviceBase::InvalidateState(); // [Blur2PC][macOS] Original PC reset/default ordering, including cull-flip initialization.
    auto& context=m_Display2.GetContext();SetAlphaToMaskEnable(m_AlphaToMaskEnableDefault);Display2::C_AlphaTestState alpha;alpha.SetAlphaTestCmpFunc(ToCmpFunc2(m_AlphaTestFuncDefault));alpha.SetAlphaTestRef(m_AlphaTestRefDefault);context.SetAlphaTestState(alpha);context.SetAlphaTestEnable(ToBool2(m_AlphaTestEnableDefault));
    Display2::C_BlendState blend;blend.SetBlendColourSrc(ToBlend2(m_BlendSrcDefault));blend.SetBlendColourDst(ToBlend2(m_BlendDstDefault));blend.SetBlendColourOp(ToBlendOp2(m_BlendOpDefault));blend.SetBlendAlphaSrc(ToBlend2(m_BlendSrcAlphaDefault));blend.SetBlendAlphaDst(ToBlend2(m_BlendDstAlphaDefault));blend.SetBlendAlphaOp(ToBlendOp2(m_BlendOpAlphaDefault));context.SetBlendState(blend);context.SetBlendEnable(ToBool2(m_BlendEnableDefault));
    Display2::C_DepthState depth;depth.SetDepthEnable(m_BackBuffer.m_DepthStencilSurface.IsCreated()?ToBool2(m_DepthEnableDefault):Display2::E_Bool_False);depth.SetDepthWriteEnable(ToBool2(m_DepthWriteEnableDefault));depth.SetDepthFunc(ToCmpFunc2(m_DepthFuncDefault));context.SetDepthState(depth);
    context.SetCullMode(ToCullMode2(m_CullModeDefault));context.SetColourWriteEnable(m_ColourWriteEnableDefault);context.SetFillMode(ToFillMode2(m_FillModeDefault));context.SetScissorEnable(Display2::E_Bool_False);context.SetStencilEnable(Display2::E_Bool_False);
    m_BlendMode=m_BlendModeDefault;m_AlphaTestEnable=m_AlphaTestEnableDefault;m_AlphaTestFunc=m_AlphaTestFuncDefault;m_AlphaTestRef=m_AlphaTestRefDefault;
    m_BlendEnable=m_BlendEnableDefault;m_BlendSrc=m_BlendSrcDefault;m_BlendDst=m_BlendDstDefault;m_BlendOp=m_BlendOpDefault;m_BlendSrcAlpha=m_BlendSrcAlphaDefault;m_BlendDstAlpha=m_BlendDstAlphaDefault;m_BlendOpAlpha=m_BlendOpAlphaDefault;
    m_DepthEnable=m_BackBuffer.m_DepthStencilSurface.IsCreated()?m_DepthEnableDefault:E_Bool_False;m_DepthWriteEnable=m_DepthWriteEnableDefault;m_DepthFunc=m_DepthFuncDefault;m_DepthBias=0;m_CullMode=m_CullModeDefault;m_ColourWriteEnable=m_ColourWriteEnableDefault;m_FillMode=m_FillModeDefault;m_StencilEnable=E_Bool_False;
    C_SamplerBase defaults;Display2::C_SamplerState sampler;sampler.SetTextureAddress(ToTextureAddress2(defaults.m_AddressUDefault),ToTextureAddress2(defaults.m_AddressVDefault),ToTextureAddress2(defaults.m_AddressWDefault));sampler.SetTextureFilter(ToTextureFilter2(defaults.m_FilterMagDefault),ToTextureFilter2(defaults.m_FilterMinDefault),ToTextureFilter2(defaults.m_FilterMipDefault));sampler.SetTextureGamma(defaults.m_GammaDefault==E_Bool_True);
    for(U32 i=0;i<E_Sampler_Count;++i)context.SetSamplerState(i,sampler);
    for(U32 i=0;i<E_VertexSampler_Count;++i)context.SetVertexSamplerState(i,sampler);
}
void C_GameDevice::SetTextureAddressUVW(U32 index,E_TextureAddress u,E_TextureAddress v,E_TextureAddress w){
    // [Blur2PC] Default restores the engine sampler defaults; it is not a native
    // address enumerator. Resolve all axes before publishing any state change.
    const C_SamplerBase defaults;
    if(u==E_TextureAddress_Default)u=defaults.m_AddressUDefault;
    if(v==E_TextureAddress_Default)v=defaults.m_AddressVDefault;
    if(w==E_TextureAddress_Default)w=defaults.m_AddressWDefault;
    auto address=[](E_TextureAddress value){if(U32(value)<U32(E_TextureAddress_Wrap)||U32(value)>U32(E_TextureAddress_Border))throw std::invalid_argument("Unsupported native texture address "+std::to_string(U32(value)));return ToTextureAddress2(value);};
    const auto nativeU=address(u),nativeV=address(v),nativeW=address(w);
    auto& context=m_Display2.GetContext();auto state=context.GetSamplerState(index);state.SetTextureAddress(nativeU,nativeV,nativeW);context.SetSamplerState(index,state);
}
void C_GameDevice::SetTextureFilterMagMinMip(U32 index,E_TextureFilter mag,E_TextureFilter min,E_TextureFilter mip){
    // [Blur2PC] The original sampler has distinct mag/min/mip defaults:
    // linear magnification/minification, with mip filtering disabled.
    const C_SamplerBase defaults;
    if(mag==E_TextureFilter_Default)mag=defaults.m_FilterMagDefault;
    if(min==E_TextureFilter_Default)min=defaults.m_FilterMinDefault;
    if(mip==E_TextureFilter_Default)mip=defaults.m_FilterMipDefault;
    auto filter=[](E_TextureFilter value){if(U32(value)>U32(E_TextureFilter_Anisotropic))throw std::invalid_argument("Unsupported native texture filter "+std::to_string(U32(value)));return ToTextureFilter2(value);};
    const auto nativeMag=filter(mag),nativeMin=filter(min),nativeMip=filter(mip);
    auto& context=m_Display2.GetContext();auto state=context.GetSamplerState(index);state.SetTextureFilter(nativeMag,nativeMin,nativeMip);context.SetSamplerState(index,state);
}
C_VertexDeclaration* C_GameDevice::InitVertexDeclaration(S32 count,const C_VertexElement* elements){if(count<=0||count>=Display2::C_VertexFormat::m_MaxElementCount||!elements)return nullptr;Display2::C_VertexFormat format;format.m_ElementCount=count;for(S32 i=0;i<count;++i)format.m_Elements[i]=elements[i];format.m_Elements[count]=VERTEXELEMENT_END();return m_Display2.CreateVertexDeclaration(format);}
}

// [Blur2PC][macOS] Original PC Begin/EndVertices mechanism over the shared dynamic allocator.
namespace Display::Metal {
void* C_GameDevice::BeginVertices(E_PrimitiveType type,U32 count,U32 stride,C_VertexDeclaration* declaration,bool checkUsage){
    if(!CanRender()||!m_InScene||m_BeginVerticesLocked||!count||!stride||U64(count)*stride>m_DynamicVertexBuffer.GetSize())return nullptr;
    const U32 size=count*stride;
    if(checkUsage&&(U64(m_BeginVerticesUsage)+size>m_MaxBeginVerticesUsage||U64(m_UserBeginVerticesUsage)+size>m_UserMaxBeginVerticesUsage))return nullptr;
    void* result=m_DynamicVertexBuffer.Begin(type,count,stride,declaration);
    if(result){m_BeginVerticesLocked=true;m_BeginVerticesUsage+=size;m_UserBeginVerticesUsage+=size;}return result;
}
void C_GameDevice::EndVertices(void*){
    if(!m_BeginVerticesLocked)return;
    const U32 offset=m_DynamicVertexBuffer.GetOffset();m_DynamicVertexBuffer.End();m_BeginVerticesLocked=false;
    SetVertexBuffer(0,m_DynamicVertexBuffer.GetVertexBuffer(),offset,m_DynamicVertexBuffer.GetVertexStride());SetVertexDeclaration(m_DynamicVertexBuffer.GetVertexDeclaration());
    DrawPrimitive(m_DynamicVertexBuffer.GetPrimitiveType(),0,m_DynamicVertexBuffer.GetVertexCount());
}
void C_GameDevice::FakeEndVertices(void*){if(m_BeginVerticesLocked){m_DynamicVertexBuffer.End();m_BeginVerticesLocked=false;}}
void C_GameDevice::SetMaxBeginVerticesUsage(U32 bytes){if(bytes>m_DynamicVertexBuffer.GetSize())throw std::out_of_range("Native dynamic vertex budget exceeds allocation");m_MaxBeginVerticesUsage=bytes;}
void C_GameDevice::SetUserMaxBeginVerticesUsage(F32 scale){if(!std::isfinite(scale)||scale<0||scale>1)throw std::out_of_range("Native user vertex budget scale must be 0..1");m_UserMaxBeginVerticesUsage=U32(F64(m_MaxBeginVerticesUsage)*scale);}
}

// [Blur2PC][macOS] Original shared blend conversions and view constant registers.
namespace Display::Metal {
void C_GameDevice::SetView(const C_View* view,bool setStates){
    m_pView=view;if(!setStates)return;
    if(view){SetViewport(view->GetViewport());SetDepthFunc(E_CmpFunc_Default);}
    SetViewShaderConstants();
}
void C_GameDevice::SetViewShaderConstants(){SetViewShaderConstants2D();SetViewShaderConstants3D();}
void C_GameDevice::SetViewShaderConstants2D(){
    if(m_pView){const auto& value=m_pView->GetViewSettings2D().GetClipTransform();SetVertexShaderConstantF(CV_DISPLAY_CLIP_VECTOR,reinterpret_cast<const F32*>(&value),1);}
    else {
        // Original PC fallback for the default 1024x768 rendering coordinates.
        const F32 value[]={0.001953125f,-0.00260416674f,-1,1};
        SetVertexShaderConstantF(CV_DISPLAY_CLIP_VECTOR,value,1);
    }
}
void C_GameDevice::SetViewShaderConstants3D(){
    if(!m_pView)return;const auto& view=m_pView->GetViewSettings3D();
    SetVertexShaderConstantF(CV_DISPLAY_VIEW_MATRIX,reinterpret_cast<const F32*>(&view.GetViewMatrix()),4);SetVertexShaderConstantF(CV_DISPLAY_PROJ_MATRIX,reinterpret_cast<const F32*>(&view.GetProjMatrix()),4);
    SetVertexShaderConstantF(CV_DISPLAY_VIEWPROJ_MATRIX,reinterpret_cast<const F32*>(&view.GetViewProjMatrix()),4);SetVertexShaderConstantF(CV_DISPLAY_VIEW_POSITION,reinterpret_cast<const F32*>(&view.GetViewPosition()),1);SetPixelShaderConstantF(CP_DISPLAY_VIEW_POSITION,reinterpret_cast<const F32*>(&view.GetViewPosition()),1);
}
}

// [Blur2PC][macOS] Track original upper attachment identity while binding actual native resources.
namespace Display::Metal {
void C_GameDevice::SetColourSurface(U32 index,const C_Surface* surface){
    m_Display2.GetContext().SetColourSurface(index,surface);m_RenderTarget.m_pColourSurfaces[index]=surface;
}
void C_GameDevice::SetColourWriteEnable(U32 target,U32 value){
    if(value==E_ColourWriteEnable_Default)value=m_ColourWriteEnableDefault;
    m_Display2.GetContext().SetColourWriteEnable(target,value);if(!target)m_ColourWriteEnable=value;m_BlendMode=E_BlendMode_Unknown;
}
U32 C_GameDevice::GetColourWriteEnable(U32 target)const{return m_Display2.GetContext().GetColourWriteEnable(target);}
void C_GameDevice::SetColourDepthStencilSurface(const C_Surface* colour,const C_Surface* depth){
    SetColourSurface(0,colour);for(U32 slot=1;slot<4;++slot)SetColourSurface(slot,nullptr);SetDepthStencilSurface(depth);
    if(colour){C_Viewport viewport;viewport.m_W=colour->GetWidth();viewport.m_H=colour->GetHeight();SetViewport(viewport);}
}
}

// [Blur2PC][macOS] Original upper PC Reset/Unbind contract: transient bindings must be selected again each scene.
namespace Display::Metal {
void C_GameDevice::Reset(){DefaultStates();Unbind();SetViewShaderConstants();}
void C_GameDevice::Unbind(){
    auto& context=m_Display2.GetContext();context.SetVertexShader(nullptr);context.SetPixelShader(nullptr);context.SetVertexDeclaration(nullptr);context.SetIndexBuffer(nullptr);
    for(U32 slot=0;slot<8;++slot)context.SetVertexBuffer(slot,nullptr,0,0);
    for(U32 slot=0;slot<16;++slot)context.SetTexture(slot,nullptr);
    for(U32 slot=0;slot<4;++slot)context.SetVertexTexture(slot,nullptr);
}
}
