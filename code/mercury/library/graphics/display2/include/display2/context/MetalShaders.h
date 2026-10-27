// [Blur2PC][macOS] Original shader resource interface over compiled Metal functions.
#pragma once
#include <core/Core.h>
#include <btl/containers/Vector.h>
#include <memory>

namespace Display2 {
namespace Metal { struct Shader; }
class C_MetalDevice;
struct C_ShaderParam {
    C_ShaderParam(U32 index = ~0U, U32 count = 0) : m_RegisterIndex(index), m_RegisterCount(count) {}
    U32 m_RegisterIndex, m_RegisterCount;
};
using C_VertexShaderParam = C_ShaderParam;
using C_PixelShaderParam = C_ShaderParam;
using C_PixelShaderParamArray = C_ShaderParam;
struct C_ShaderSemantic {
    U8 m_Usage = 0xff, m_UsageIndex = 0xff;
};
struct C_VertexShader {
    bool IsCreated() const { return bool(m_Shader); }
    U32 GetSize() const;
    U32 GetFlags() const { return m_Flags; }
    void* GetAddress() const;
    void SetFlags(U32 flags) { m_Flags = flags; }
    U8 GetRegisterCount() const { return m_RegisterCount; }
    void SetRegisterCount(U8 count) { m_RegisterCount = count; }
    Metal::Shader* GetGPUVertexShader() { return m_Shader.get(); }
    const Metal::Shader* GetGPUVertexShader() const { return m_Shader.get(); }
    // [Blur2PC] Original PC resource type (2D/3D/cube), or ~0U for an undeclared sampler.
    U32 GetTextureType(U32 sampler) const;
    C_ShaderParam GetParamFromName(const char* name) const;
    C_ShaderParam GetParameter(S32, const char* name) const { return GetParamFromName(name); }
    C_ShaderParam GetParameterArray(S32, const char* name, U32 = 0) const { return GetParamFromName(name); } // [Blur2PC] Reflection already includes the complete array's register count.
    bool GetSemantics(Btl::Vector<C_ShaderSemantic>& semantics);
    S32 GetInputLocation(U8 usage, U8 usageIndex) const;
private:
    friend class C_MetalDevice;
    friend class C_Context;
    std::shared_ptr<Metal::Shader> m_Shader;
    U32 m_Flags = 0;
    U8 m_RegisterCount = 0;
};
struct C_PixelShader {
    bool IsCreated() const { return bool(m_Shader); }
    U32 GetSize() const;
    void SetRegisterCount(U8 count){m_RegisterCount=count;} // [Blur2PC] Retain original shader metadata.
    U32 GetFlags() const { return m_Flags; }
    void* GetAddress() const;
    void SetFlags(U32 flags) { m_Flags = flags; }
    Metal::Shader* GetGPUPixelShader() { return m_Shader.get(); }
    const Metal::Shader* GetGPUPixelShader() const { return m_Shader.get(); }
    // [Blur2PC] Original reflected resource dimension, matching the vertex-stage query.
    U32 GetTextureType(U32 sampler) const;
    C_ShaderParam GetParamFromName(const char* name) const;
    C_ShaderParam GetParameter(S32, const char* name) const { return GetParamFromName(name); }
    C_ShaderParam GetParameterArray(S32, const char* name, U32 = 0) const { return GetParamFromName(name); }
private:
    friend class C_MetalDevice;
    friend class C_Context;
    std::shared_ptr<Metal::Shader> m_Shader;
    U32 m_Flags = 0;U8 m_RegisterCount=0;
};
}
