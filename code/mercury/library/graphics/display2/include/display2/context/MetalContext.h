// [Blur2PC][macOS] Engine rendering entry points implemented by native Metal.
#pragma once
#include <display2/context/MetalBuffers.h>
#include <display2/context/MetalShaders.h>
#include <display2/context/MetalVertices.h>
#include <display2/context/MetalSurfaces.h>
#include <display2/context/MetalTextures.h>
#include <display2/context/MetalOcclusion.h>
#include <core/containers/String.h>
#include <stdexcept>
#include <utility>
// [Blur2PC][macOS] Same PC packed vertex-colour convention as D3D11Context.h.
#define GPU_U8_4_TO_U32(r,g,b,a) (((U32(a)&255U)<<24)|((U32(r)&255U)<<16)|((U32(g)&255U)<<8)|(U32(b)&255U))
namespace Display2 {
enum E_ResourceType {E_ResourceType_Surface=1,E_ResourceType_Texture2D=3,E_ResourceType_Texture3D=4,E_ResourceType_TextureCube=5,
    E_ResourceType_VertexBuffer=6,E_ResourceType_IndexBuffer=7,E_ResourceType_VertexShader=8,E_ResourceType_PixelShader=9,E_ResourceType_Unknown=~0U};
enum E_Bool { E_Bool_False=0, E_Bool_True=1, E_Bool_Unknown=~0U };
enum E_CmpFunc { E_CmpFunc_Never=1,E_CmpFunc_Less=2,E_CmpFunc_Equal=3,E_CmpFunc_LessEqual=4,
    E_CmpFunc_Greater=5,E_CmpFunc_NotEqual=6,E_CmpFunc_GreaterEqual=7,E_CmpFunc_Always=8,E_CmpFunc_Unknown=~0U };
enum E_PrimitiveType { E_PrimitiveType_PointList=1,E_PrimitiveType_LineList=2,E_PrimitiveType_LineStrip=3,
    E_PrimitiveType_TriangleList=4,E_PrimitiveType_TriangleStrip=5,E_PrimitiveType_TriangleFan=6,E_PrimitiveType_Unknown=~0U };
enum E_Clear { E_Clear_Colour=1,E_Clear_Depth=2,E_Clear_Stencil=4 };
enum E_CullMode { E_CullMode_None=1,E_CullMode_Cw=2,E_CullMode_Ccw=3,E_CullMode_Unknown=~0U,E_CullMode_Default=~1U,E_CullMode_Reversed=~2U };
enum E_ColourWriteEnable { E_ColourWriteEnable_Red=1,E_ColourWriteEnable_Green=2,E_ColourWriteEnable_Blue=4,
    E_ColourWriteEnable_Alpha=8,E_ColourWriteEnable_All=15 };
enum E_Blend { E_Blend_Zero=1,E_Blend_One=2,E_Blend_SrcColour=3,E_Blend_InvSrcColour=4,
    E_Blend_SrcAlpha=5,E_Blend_InvSrcAlpha=6,E_Blend_DstAlpha=7,E_Blend_InvDstAlpha=8,
    E_Blend_DstColour=9,E_Blend_InvDstColour=10,E_Blend_SrcAlphaSat=11,E_Blend_Unknown=~0U };
enum E_BlendOp { E_BlendOp_Add=1,E_BlendOp_Subtract=2,E_BlendOp_RevSubtract=3,E_BlendOp_Min=4,E_BlendOp_Max=5,E_BlendOp_Unknown=~0U };
// Original PC serialized identifiers; declaration does not enable interval/multisurface resolve support.
enum E_PresentInterval {E_PresentInterval_Default=0,E_PresentInterval_One=1,E_PresentInterval_Two=2,E_PresentInterval_Three=4,E_PresentInterval_Four=8,E_PresentInterval_Immediate=0x80000000U,E_PresentInterval_Unknown=~0U};
enum E_Resolve {E_Resolve_RenderTarget0,E_Resolve_RenderTarget1,E_Resolve_RenderTarget2,E_Resolve_RenderTarget3,E_Resolve_DepthStencil,E_Resolve_AllFragments};
enum E_FillMode {E_FillMode_Point=1,E_FillMode_Wireframe=2,E_FillMode_Solid=3,E_FillMode_Unknown=~0U};
enum E_StencilOp {E_StencilOp_Keep=1,E_StencilOp_Zero=2,E_StencilOp_Replace=3,E_StencilOp_IncrSat=4,E_StencilOp_DecrSat=5,E_StencilOp_Invert=6,E_StencilOp_Incr=7,E_StencilOp_Decr=8,E_StencilOp_Unknown=~0U};
struct C_AlphaTestState {
    union {struct {U32 m_AlphaTestCmpFunc:4,m_AlphaTestRef:8;};U32 m_AlphaTestBits;};
    C_AlphaTestState(){SetDefaultStates();}void SetDefaultStates(){m_AlphaTestBits=0;m_AlphaTestCmpFunc=E_CmpFunc_Always;}
    void SetAlphaTestCmpFunc(E_CmpFunc f){m_AlphaTestCmpFunc=f;}void SetAlphaTestRef(U8 v){m_AlphaTestRef=v;}
};
struct C_BlendState {
    union {struct {U32 m_BlendColorSrc:4,m_BlendColorDst:4,m_BlendColorOp:4,m_BlendAlphaSrc:4,m_BlendAlphaDst:4,m_BlendAlphaOp:4;};U32 m_BlendBits;};
    C_BlendState(){SetDefaultStates();}void SetDefaultStates(){m_BlendBits=0;m_BlendColorSrc=E_Blend_One;m_BlendColorDst=E_Blend_Zero;m_BlendColorOp=E_BlendOp_Add;m_BlendAlphaSrc=E_Blend_One;m_BlendAlphaDst=E_Blend_One;m_BlendAlphaOp=E_BlendOp_Add;}
    void SetBlendColourSrc(E_Blend v){m_BlendColorSrc=v;}void SetBlendColourDst(E_Blend v){m_BlendColorDst=v;}void SetBlendColourOp(E_BlendOp v){m_BlendColorOp=v;}
    void SetBlendAlphaSrc(E_Blend v){m_BlendAlphaSrc=v;}void SetBlendAlphaDst(E_Blend v){m_BlendAlphaDst=v;}void SetBlendAlphaOp(E_BlendOp v){m_BlendAlphaOp=v;}
};
struct C_ColourState {U32 m_ColourWriteEnable=15;void SetDefaultStates(){m_ColourWriteEnable=15;}void SetColourWriteEnable(U32 v){m_ColourWriteEnable=v;}};
struct C_CullState {U32 m_CullMode=E_CullMode_Ccw;void SetDefaultStates(){m_CullMode=E_CullMode_Ccw;}void SetCullMode(E_CullMode v){m_CullMode=v;}};
struct C_DepthState {
    union {struct {U32 m_DepthFunc:4,m_DepthEnable:1,m_DepthWriteEnable:1;};U32 m_DepthBits;};F32 m_DepthBias,m_SlopeScaledBias;
    C_DepthState(){SetDefaultStates();}void SetDefaultStates(){m_DepthBits=0;m_DepthFunc=E_CmpFunc_LessEqual;m_DepthEnable=1;m_DepthWriteEnable=1;m_DepthBias=0;m_SlopeScaledBias=0;}
    void SetDepthFunc(E_CmpFunc v){m_DepthFunc=v;}void SetDepthEnable(E_Bool v){m_DepthEnable=v;}void SetDepthWriteEnable(E_Bool v){m_DepthWriteEnable=v;}
    void SetDepthBias(F32 v){m_DepthBias=v;}void SetSlopeScaledBias(F32 v){m_SlopeScaledBias=v;}
};
struct C_FillState {U32 m_FillMode=E_FillMode_Solid;void SetDefaultStates(){m_FillMode=E_FillMode_Solid;}void SetFillMode(U32 v){m_FillMode=v;}};
struct C_ScissorRect {S32 left=0,top=0,right=0,bottom=0;};
struct C_ScissorState {U32 m_ScissorEnable=0;C_ScissorRect m_ScissorRect;
    void SetDefaultStates(){m_ScissorEnable=0;}void SetScissorEnable(E_Bool v){m_ScissorEnable=v;}void SetScissorRect(const C_ScissorRect& r){m_ScissorRect=r;}};
struct C_StencilState {
    union {struct {U32 m_StencilFunc:4,m_StencilFail:4,m_StencilDepthFail:4,m_StencilDepthPass:4;};U32 m_StencilBits;};U32 m_StencilRef,m_StencilMask,m_StencilWriteMask;
    C_StencilState(){SetDefaultStates();}void SetDefaultStates(){m_StencilBits=0;m_StencilFunc=E_CmpFunc_Always;m_StencilFail=E_StencilOp_Keep;m_StencilDepthFail=E_StencilOp_Keep;m_StencilDepthPass=E_StencilOp_Keep;m_StencilRef=0;m_StencilMask=~0U;m_StencilWriteMask=~0U;}
    void SetStencilFunc(E_CmpFunc v){m_StencilFunc=v;}void SetStencilFail(E_StencilOp v){m_StencilFail=v;}void SetStencilDepthFail(E_StencilOp v){m_StencilDepthFail=v;}void SetStencilDepthPass(E_StencilOp v){m_StencilDepthPass=v;}
    void SetStencilRef(U32 v){m_StencilRef=v;}void SetStencilMask(U32 v){m_StencilMask=v;}void SetStencilWriteMask(U32 v){m_StencilWriteMask=v;}
};
using C_VertexSamplerState=C_SamplerState;
static_assert(sizeof(C_AlphaTestState)==4&&sizeof(C_BlendState)==4&&sizeof(C_DepthState)==12&&sizeof(C_ScissorState)==20&&sizeof(C_StencilState)==16,"PC render state layout changed");
const Util::C_String GetFlagsAsString(U32 flags);
const Util::C_String GetIndexFormatAsString(E_IndexFormat format);
const Util::C_String GetSurfaceFormatAsString(E_SurfaceFormat format);
const Util::C_String GetTextureFormatAsString(E_TextureFormat format);
const Util::C_String GetTextureTypeAsString(E_TextureType type);
const Util::C_String GetMultisampleAsString(E_Multisample samples);
struct C_Viewport {
    U32 x=0,y=0,width=0,height=0; F32 minZ=0,maxZ=1;
    C_Viewport()=default;
    C_Viewport(U32 x_,U32 y_,U32 w,U32 h,F32 near_=0,F32 far_=1):x(x_),y(y_),width(w),height(h),minZ(near_),maxZ(far_){}
    U32 GetX()const{return x;} U32 GetY()const{return y;} U32 GetWidth()const{return width;} U32 GetHeight()const{return height;}
    F32 GetMinZ()const{return minZ;} F32 GetMaxZ()const{return maxZ;}
    void SetX(U32 v){x=v;} void SetY(U32 v){y=v;} void SetWidth(U32 v){width=v;} void SetHeight(U32 v){height=v;}
    void SetMinZ(F32 v){minZ=v;} void SetMaxZ(F32 v){maxZ=v;}
};
class C_Context {
public:
    ~C_Context();
    C_Context(const C_Context&)=delete; C_Context& operator=(const C_Context&)=delete;
    void BeginScene(); void EndScene();
    void SetViewport(const C_Viewport& viewport);
    void SetColourSurface(U32 index,const C_Surface* surface);
    void SetDepthStencilSurface(const C_Surface* surface);
    void Clear(U32 flags,U32 colour,F32 depth=1,U8 stencil=0);
    // [Blur2PC] Finish the owning render pass so native multisample resolves are visible to later commands.
    void ResolveSurface(const C_Surface* surface);
    void CopySurface(const C_Surface* destination,const C_Surface* source);
    void CopySurfaceRegion(const C_Surface* destination,const C_Surface* source,U32 sourceX,U32 sourceY,U32 width,U32 height,U32 destinationX=0,U32 destinationY=0);
    void SetVertexDeclaration(const C_VertexDeclaration* declaration);
    void SetVertexBuffer(U32 stream,const C_VertexBuffer* buffer,U32 offset,U32 stride);
    void SetVertexBuffer(U32 stream,const C_VertexBuffer* buffer,U32 offset,U32 stride,const C_VertexDeclaration* declaration);
    void SetIndexBuffer(const C_IndexBuffer* buffer);
    void SetVertexShader(const C_VertexShader* shader);
    void SetPixelShader(const C_PixelShader* shader);
    void SetTexture(U32 index,const C_Texture* texture);
    // [Blur2PC] Original PC game bindings snapshot active colour targets before sampling their prior contents.
    void SetTextureWithColourSnapshot(U32 index,const C_Texture* texture);
    void SetVertexTexture(U32 index,const C_Texture* texture);
    void SetSamplerState(U32 index,const C_SamplerState& state);
    void SetSamplerState(U32 index,const C_SamplerState& state,U32 anisotropy);
    void SetVertexSamplerState(U32 index,const C_SamplerState& state);
    void SetVertexSamplerState(U32 index,const C_SamplerState& state,U32 anisotropy);
    void SetMaxAnisotropy(U32 value);
#define NATIVE_STATE_API(Type,Name) const Type& Get##Name##State()const;void Set##Name##State(const Type& state);
    NATIVE_STATE_API(C_AlphaTestState,AlphaTest) NATIVE_STATE_API(C_BlendState,Blend) NATIVE_STATE_API(C_ColourState,Colour)
    NATIVE_STATE_API(C_CullState,Cull) NATIVE_STATE_API(C_DepthState,Depth) NATIVE_STATE_API(C_FillState,Fill)
    NATIVE_STATE_API(C_ScissorState,Scissor) NATIVE_STATE_API(C_StencilState,Stencil)
#undef NATIVE_STATE_API
    const C_SamplerState& GetSamplerState(U32 index)const;const C_VertexSamplerState& GetVertexSamplerState(U32 index)const;
    void SetScissorEnable(E_Bool enable);void SetScissorRect(const C_ScissorRect& rect);
    void SetFillMode(E_FillMode mode);void SetDepthBias(F32 value);void SetSlopeScaledBias(F32 value);void SetStencilEnable(E_Bool enable);
    void SetVertexShaderConstantF(U32 start,const F32* data,U32 count);
    void SetPixelShaderConstantF(U32 start,const F32* data,U32 count);
    void SetVertexShaderParamF(const C_VertexShaderParam& parameter,const F32* data);
    void SetPixelShaderParamF(const C_PixelShaderParam& parameter,const F32* data,U32 count=1);
    void SetPixelShaderParamArrayF(const C_PixelShaderParam& parameter,const F32* data,U32 count=1);
    void SetAlphaToMaskEnable(E_Bool enable);
    void SetAlphaTestEnable(E_Bool enable); void SetAlphaTestFunc(E_CmpFunc function); void SetAlphaTestRef(U32 reference);
    void SetDepthEnable(E_Bool enable); void SetDepthWriteEnable(E_Bool enable); void SetDepthFunc(E_CmpFunc function);
    void SetCullMode(E_CullMode mode); void SetColourWriteEnable(U32 mask);
    void SetColourWriteEnable(U32 target,U32 mask);U32 GetColourWriteEnable(U32 target)const;
    void SetBlendEnable(E_Bool enable); void SetBlendSrc(E_Blend factor); void SetBlendDst(E_Blend factor);
    void SetBlendSrcAlpha(E_Blend factor); void SetBlendDstAlpha(E_Blend factor);
    void SetBlendOp(E_BlendOp operation); void SetBlendOpAlpha(E_BlendOp operation);
    void DrawPrimitive(E_PrimitiveType type,U32 startVertex,U32 vertexCount);
    void DrawIndexedPrimitive(E_PrimitiveType type,U32 baseVertex,U32 minIndex,U32 vertexCount,U32 startIndex,U32 indexCount);
    void DrawIndexedPrimitive(E_PrimitiveType type,U32 baseVertex,U32 minIndex,U32 vertexCount,U32 startIndex,U32 indexCount,const C_IndexBuffer* indices);
private:
    friend class C_MetalDevice;
    friend class C_OcclusionQuery; // [Blur2PC] Query operations share the scene's real command stream.
    bool CreateOcclusionQuery(C_OcclusionQuery& query);
    void BeginOcclusionQuery(C_OcclusionQuery& query);
    void EndOcclusionQuery(C_OcclusionQuery& query);
    bool SubmitOcclusionQuery(const C_OcclusionQuery& query);
    explicit C_Context(C_MetalDevice& device);
    struct Impl; std::unique_ptr<Impl> m_Impl;
};
// [Blur2PC][macOS] Copyable, non-owning immediate-context views for upper Display.
// Destruction/recreation invalidates old views. Native validation and owner-thread checks remain active.
class C_ContextNoCheckNoTrack {
public:
    C_ContextNoCheckNoTrack()=default;
    explicit C_ContextNoCheckNoTrack(C_MetalDevice& device){Bind(device);}
    void Bind(C_MetalDevice& device);
    bool IsBound()const{return !m_Context.expired();}
#define NATIVE_CONTEXT_VIEW(Name) template<class... A> decltype(auto) Name(A&&... args)const {return Lock()->Name(std::forward<A>(args)...);}
    NATIVE_CONTEXT_VIEW(BeginScene)
    NATIVE_CONTEXT_VIEW(EndScene)
    NATIVE_CONTEXT_VIEW(SetViewport)
    NATIVE_CONTEXT_VIEW(SetColourSurface)
    NATIVE_CONTEXT_VIEW(SetDepthStencilSurface)
    NATIVE_CONTEXT_VIEW(Clear)
    NATIVE_CONTEXT_VIEW(CopySurface)
    NATIVE_CONTEXT_VIEW(CopySurfaceRegion)
    NATIVE_CONTEXT_VIEW(SetVertexDeclaration)
    NATIVE_CONTEXT_VIEW(SetVertexBuffer)
    NATIVE_CONTEXT_VIEW(SetIndexBuffer)
    NATIVE_CONTEXT_VIEW(SetVertexShader)
    NATIVE_CONTEXT_VIEW(SetPixelShader)
    NATIVE_CONTEXT_VIEW(SetTexture)
    NATIVE_CONTEXT_VIEW(SetVertexTexture)
    NATIVE_CONTEXT_VIEW(SetSamplerState)
    NATIVE_CONTEXT_VIEW(SetVertexSamplerState)
    NATIVE_CONTEXT_VIEW(SetMaxAnisotropy)
    NATIVE_CONTEXT_VIEW(GetSamplerState)
    NATIVE_CONTEXT_VIEW(GetVertexSamplerState)
    NATIVE_CONTEXT_VIEW(SetScissorEnable)
    NATIVE_CONTEXT_VIEW(SetScissorRect)
    NATIVE_CONTEXT_VIEW(SetFillMode)
    NATIVE_CONTEXT_VIEW(SetDepthBias)
    NATIVE_CONTEXT_VIEW(SetSlopeScaledBias)
    NATIVE_CONTEXT_VIEW(SetStencilEnable)
    NATIVE_CONTEXT_VIEW(SetVertexShaderConstantF)
    NATIVE_CONTEXT_VIEW(SetPixelShaderConstantF)
    NATIVE_CONTEXT_VIEW(SetVertexShaderParamF)
    NATIVE_CONTEXT_VIEW(SetPixelShaderParamF)
    NATIVE_CONTEXT_VIEW(SetPixelShaderParamArrayF)
    NATIVE_CONTEXT_VIEW(SetAlphaToMaskEnable)
    NATIVE_CONTEXT_VIEW(SetAlphaTestEnable)
    NATIVE_CONTEXT_VIEW(SetAlphaTestFunc)
    NATIVE_CONTEXT_VIEW(SetAlphaTestRef)
    NATIVE_CONTEXT_VIEW(SetDepthEnable)
    NATIVE_CONTEXT_VIEW(SetDepthWriteEnable)
    NATIVE_CONTEXT_VIEW(SetDepthFunc)
    NATIVE_CONTEXT_VIEW(SetCullMode)
    NATIVE_CONTEXT_VIEW(SetColourWriteEnable)
    NATIVE_CONTEXT_VIEW(GetColourWriteEnable)
    NATIVE_CONTEXT_VIEW(SetBlendEnable)
    NATIVE_CONTEXT_VIEW(SetBlendSrc)
    NATIVE_CONTEXT_VIEW(SetBlendDst)
    NATIVE_CONTEXT_VIEW(SetBlendSrcAlpha)
    NATIVE_CONTEXT_VIEW(SetBlendDstAlpha)
    NATIVE_CONTEXT_VIEW(SetBlendOp)
    NATIVE_CONTEXT_VIEW(SetBlendOpAlpha)
    NATIVE_CONTEXT_VIEW(DrawPrimitive)
    NATIVE_CONTEXT_VIEW(DrawIndexedPrimitive)
    NATIVE_CONTEXT_VIEW(GetAlphaTestState)
    NATIVE_CONTEXT_VIEW(SetAlphaTestState)
    NATIVE_CONTEXT_VIEW(GetBlendState)
    NATIVE_CONTEXT_VIEW(SetBlendState)
    NATIVE_CONTEXT_VIEW(GetColourState)
    NATIVE_CONTEXT_VIEW(SetColourState)
    NATIVE_CONTEXT_VIEW(GetCullState)
    NATIVE_CONTEXT_VIEW(SetCullState)
    NATIVE_CONTEXT_VIEW(GetDepthState)
    NATIVE_CONTEXT_VIEW(SetDepthState)
    NATIVE_CONTEXT_VIEW(GetFillState)
    NATIVE_CONTEXT_VIEW(SetFillState)
    NATIVE_CONTEXT_VIEW(GetScissorState)
    NATIVE_CONTEXT_VIEW(SetScissorState)
    NATIVE_CONTEXT_VIEW(GetStencilState)
    NATIVE_CONTEXT_VIEW(SetStencilState)
#undef NATIVE_CONTEXT_VIEW
private:
    friend class C_MetalDevice;
    explicit C_ContextNoCheckNoTrack(const std::shared_ptr<C_Context>& context):m_Context(context){}
    std::shared_ptr<C_Context> Lock()const{auto context=m_Context.lock();if(!context)throw std::logic_error("Native immediate context is unbound or its device was destroyed");return context;}
    std::weak_ptr<C_Context> m_Context;
};
using C_ContextNoCheck=C_ContextNoCheckNoTrack;
}
