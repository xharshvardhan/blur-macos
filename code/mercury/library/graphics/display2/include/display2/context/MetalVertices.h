// [Blur2PC][macOS] PC vertex data layouts retained by the native backend.
#pragma once
#include <core/Core.h>
#include <array>
#include <memory>
#include <vector>
namespace Display2 {
namespace Metal { struct VertexDeclaration; }
class C_MetalDevice;
class C_Context;
enum E_VertexElementType {
    E_VertexElementType_F32_1=0, E_VertexElementType_F32_2=1, E_VertexElementType_F32_3=2, E_VertexElementType_F32_4=3,
    E_VertexElementType_Colour=4, E_VertexElementType_U8_4=5, E_VertexElementType_S16_2=6, E_VertexElementType_S16_4=7,
    E_VertexElementType_U8_4N=8, E_VertexElementType_S16_2N=9, E_VertexElementType_S16_4N=10,
    E_VertexElementType_U16_2N=11, E_VertexElementType_U16_4N=12, E_VertexElementType_U10_3=13,
    E_VertexElementType_S10_3N=14, E_VertexElementType_F16_2=15, E_VertexElementType_F16_4=16, E_VertexElementType_Unknown=17
};
enum E_VertexElementUsage {
    E_VertexElementUsage_Position=0, E_VertexElementUsage_BlendWeight=1, E_VertexElementUsage_BlendIndices=2,
    E_VertexElementUsage_Normal=3, E_VertexElementUsage_PSize=4, E_VertexElementUsage_TexCoord=5,
    E_VertexElementUsage_Tangent=6, E_VertexElementUsage_Binormal=7, E_VertexElementUsage_TessFactor=8,
    E_VertexElementUsage_PositionT=9, E_VertexElementUsage_Colour=10, E_VertexElementUsage_Fog=11,
    E_VertexElementUsage_Depth=12, E_VertexElementUsage_Sample=13, E_VertexElementUsage_Unknown=14
};
struct C_VertexElement {
    U16 Stream=0xff, Offset=0;
    U8 Type=E_VertexElementType_Unknown, Method=0, Usage=0, UsageIndex=0;
    C_VertexElement() = default;
    C_VertexElement(U16 stream, U32 offset, U32 type, U8 usage, U8 index);
    U16 GetStream() const { return Stream; }
    U32 GetOffset() const { return Offset; }
    U32 GetType() const { return Type; }
    U8 GetUsage() const { return Usage; }
    U8 GetUsageIndex() const { return UsageIndex; }
    void SetStream(U16 stream) { Stream=stream; }
    void SetOffset(U32 offset);
    void SetType(U32 type);
    void SetUsage(U8 usage) { Usage=usage; }
    void SetUsageIndex(U8 index) { UsageIndex=index; }
    U32 GetSize() const;
    U32 GetCount() const;
    bool operator==(const C_VertexElement& other) const;
    bool operator!=(const C_VertexElement& other) const { return !(*this==other); }
};
static_assert(sizeof(C_VertexElement)==8, "PC vertex declaration wire layout changed");
#define GPU_VERTEX_ELEMENT_END() Display2::C_VertexElement()
struct C_VertexDeclaration {
    static constexpr U32 m_MaxElementCount=12;
    bool IsCreated() const { return bool(m_Declaration); }
    S32 GetElementCount() const;
    bool GetElements(U32 count, C_VertexElement* elements) const;
    U32 GetStride(U32 stream=0) const;
private:
    friend class C_MetalDevice;
    friend class C_Context;
    std::shared_ptr<Metal::VertexDeclaration> m_Declaration;
};
class C_VertexFormat {
public:
    U32 GetStride(U32 stream=~0U) const;
    void AddElement(U16 stream, U32 type, U8 usage, U8 index);
    void AddElement(U16 stream, U32 offset, U32 type, U8 usage, U8 index);
    S32 m_ElementCount=0;
    static constexpr S32 m_MaxElementCount=C_VertexDeclaration::m_MaxElementCount;
    C_VertexElement m_Elements[m_MaxElementCount];
};
}
