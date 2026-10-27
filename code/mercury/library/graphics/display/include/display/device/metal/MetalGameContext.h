// [Blur2PC][macOS] Actual upper context using original state conversions and native lifetime-checked bindings.
#pragma once
#include <display/context/Context_Base.h>
namespace Display {
DECLARE_CONTEXT(Context,C_Context);
class C_CommandBuffer;
class C_Context final:public C_ContextBase {
public:
    I_Context* New() override{return new C_Context;}
    void Initialise() override;
    void CloneState(const I_Context*) override;
    void InvalidateState() override; // Immediate native bindings retain no separate redundant-state cache.
    void BeginCommandBuffer(C_CommandBuffer*);
    void EndCommandBuffer();
    void* BeginVertices(E_PrimitiveType type,U32 count,U32 stride,const C_VertexDeclaration* declaration);
    void EndVertices();
    void SetTexture(U32 slot,const void* texture); // Original PC context uses the device binding-time colour-feedback adapter.
    void SetVertexTexture(U32 slot,const void* texture){m_Context.SetVertexTexture(slot,static_cast<const C_Texture*>(texture));}
    void SetAlphaTestFunc(E_CmpFunc func){m_Context.SetAlphaTestFunc(static_cast<Display2::E_CmpFunc>(func));}
    using C_ContextBase::SetColourWriteEnable; // [Blur2PC] Preserve legacy global masks beside explicit native MRT masks.
    void SetColourWriteEnable(U32 target,U32 mask); // Resolve upper defaults through the same device policy as the scalar form.
    U32 GetColourWriteEnable(U32 target)const{return m_Context.GetColourWriteEnable(target);}
    void SetBlendHighPrecisionEnable(E_Bool); // Native floating point render-target formats carry their own precision.
};
}
