// [Blur2PC][macOS] Context registration and immediate native device binding; no Direct3D bridge.
#include "NativeDisplayIncludes.h"
#include <display/device/metal/MetalGameContext.h>
#include <display/device/Device.h>
#include <stdexcept>
namespace Display {
DEFINE_CONTEXT(Context,C_Context);
void C_SamplerState::SetMaxAnisotropy(S8 value){if(value<1||value>16)throw std::out_of_range("Native sampler anisotropy must be 1..16");m_MaxAnisotropy=value;}
void C_Context::Initialise(){m_Flags=m_Id;CloneState(nullptr);}
void C_Context::CloneState(const I_Context* source){
    if(!source){
        if(!DeviceExists()||!Device()->IsCreated())throw std::logic_error("Native context requires a created upper device");
        m_Context.Bind(Device()->GetDisplay2());
        m_CullMode=Device()->GetCullModeDefault();
        m_Multisample=static_cast<E_Multisample>(Device()->GetNativeColourSurface()->GetMultisample());
        m_pVertexDeclaration=nullptr;m_pIndexBuffer=nullptr;
        m_pColourTexture=m_pDepthStencilTexture=m_pStencilTexture=nullptr;
    }else{
        auto* other=dynamic_cast<const C_Context*>(source);if(!other)throw std::invalid_argument("Cannot clone a different native context type");
        m_CullMode=other->m_CullMode;m_Multisample=other->m_Multisample;
        m_pColourTexture=other->m_pColourTexture;m_pDepthStencilTexture=other->m_pDepthStencilTexture;m_pStencilTexture=other->m_pStencilTexture;
        m_pVertexDeclaration=other->m_pVertexDeclaration;m_pIndexBuffer=other->m_pIndexBuffer;
        *m_pView=*other->m_pView;m_Context=other->m_Context;
    }
}
// [Blur2PC] Match Context_D3D11::SetTexture -> SetNativeTexture, including the existing pixel colour snapshot policy.
void C_Context::SetTexture(U32 slot,const void* texture){Device()->SetTexture(slot,static_cast<const C_Texture*>(texture));}
void C_Context::SetColourWriteEnable(U32 target,U32 mask){Device()->SetColourWriteEnable(target,mask);}
void C_Context::InvalidateState(){} // Original immediate PC context has no redundant-state cache either.
void C_Context::BeginCommandBuffer(C_CommandBuffer* buffer){if(buffer)throw std::logic_error("Native deferred command buffers are not implemented");}
void C_Context::EndCommandBuffer(){}
void* C_Context::BeginVertices(E_PrimitiveType type,U32 count,U32 stride,const C_VertexDeclaration* declaration){if(!m_Context.IsBound())throw std::logic_error("Native dynamic context is unbound");void* data=Device()->BeginVertices(type,count,stride,const_cast<C_VertexDeclaration*>(declaration));if(data)m_pVertexDeclaration=declaration;return data;}
void C_Context::EndVertices(){if(!m_Context.IsBound())throw std::logic_error("Native dynamic context is unbound");Device()->EndVertices();}
void C_Context::SetBlendHighPrecisionEnable(E_Bool enabled){if(enabled!=E_Bool_False&&enabled!=E_Bool_True)throw std::invalid_argument("Invalid native blend precision boolean");}
}
