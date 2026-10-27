// [Blur2PC][macOS] Engine vertex layout metadata, independent of shader compilation.
#include <display2/context/MetalVertices.h>
#include "MetalVertexData.h"
#include <algorithm>
#include <stdexcept>
namespace Display2 {
C_VertexElement::C_VertexElement(U16 stream,U32 offset,U32 type,U8 usage,U8 index) : Stream(stream),Usage(usage),UsageIndex(index) {
    SetOffset(offset); SetType(type);
}
void C_VertexElement::SetOffset(U32 offset) { if(offset>0xffff) throw std::out_of_range("Vertex offset exceeds PC layout"); Offset=U16(offset); }
void C_VertexElement::SetType(U32 type) { if(type>E_VertexElementType_Unknown) throw std::out_of_range("Unknown vertex element type"); Type=U8(type); }
U32 C_VertexElement::GetSize() const {
    static const U8 sizes[]={4,8,12,16,4,4,4,8,4,4,8,4,8,4,4,4,8,0};
    if(Type>E_VertexElementType_Unknown) throw std::out_of_range("Corrupt vertex element type");
    return sizes[Type];
}
U32 C_VertexElement::GetCount() const {
    static const U8 counts[]={1,2,3,4,1,4,2,4,4,2,4,2,4,3,3,2,4,0};
    if(Type>E_VertexElementType_Unknown) throw std::out_of_range("Corrupt vertex element type");
    return counts[Type];
}
bool C_VertexElement::operator==(const C_VertexElement& o) const {
    return Stream==o.Stream&&Offset==o.Offset&&Type==o.Type&&Method==o.Method&&Usage==o.Usage&&UsageIndex==o.UsageIndex;
}
U32 C_VertexFormat::GetStride(U32 stream) const {
    U32 sizes[8]={};
    for(S32 i=0;i<m_ElementCount;++i) {
        const auto& e=m_Elements[i];
        if(e.Stream>=8) throw std::out_of_range("Vertex stream exceeds engine limit");
        sizes[e.Stream]=std::max(sizes[e.Stream],e.GetOffset()+e.GetSize());
    }
    if(stream!=~0U) { if(stream>=8) throw std::out_of_range("Vertex stream exceeds engine limit"); return sizes[stream]; }
    U32 total=0;for(U32 size:sizes)total+=size;return total;
}
void C_VertexFormat::AddElement(U16 stream,U32 type,U8 usage,U8 index) { AddElement(stream,GetStride(stream),type,usage,index); }
void C_VertexFormat::AddElement(U16 stream,U32 offset,U32 type,U8 usage,U8 index) {
    if(m_ElementCount>=m_MaxElementCount-1||stream>=8||usage>=E_VertexElementUsage_Unknown||type>=E_VertexElementType_Unknown)
        throw std::invalid_argument("Invalid vertex format element");
    m_Elements[m_ElementCount++]=C_VertexElement(stream,offset,type,usage,index);m_Elements[m_ElementCount]=GPU_VERTEX_ELEMENT_END();
}
S32 C_VertexDeclaration::GetElementCount() const { return m_Declaration?S32(m_Declaration->elements.size()):0; }
bool C_VertexDeclaration::GetElements(U32 count,C_VertexElement* elements) const {
    if(!elements||!m_Declaration||count!=m_Declaration->elements.size())return false;
    std::copy(m_Declaration->elements.begin(),m_Declaration->elements.end(),elements);return true;
}
U32 C_VertexDeclaration::GetStride(U32 stream) const {
    U32 size=0;if(m_Declaration)for(const auto& e:m_Declaration->elements)if(e.Stream==stream)size=std::max(size,e.GetOffset()+e.GetSize());return size;
}
}
