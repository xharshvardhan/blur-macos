// [Blur2PC][macOS] Real host storage; PS3 local video memory is deliberately unsupported.
#include <display2/memory/MetalMemory.h>
#include <core/allocators/CoreMemoryFunctions.h>
#include <cstdint>
namespace Display2 {
bool C_MemoryBuffer::Create(U32 size,U32 align,U32 flags,E_MemoryBuffer type,U8* address){
    if(m_Address||!size||align<sizeof(void*)||(align&(align-1))||size%align)return false;
    if(type!=E_MemoryBuffer_Main&&type!=E_MemoryBuffer_Indirect)return false;
    if(type==E_MemoryBuffer_Indirect){if(!address||reinterpret_cast<uintptr_t>(address)%align)return false;}
    else {if(address)return false;address=static_cast<U8*>(BizAlignedMalloc(size,align));if(!address)return false;}
    m_Address=address;m_Size=size;m_Align=align;m_Flags=flags;m_Type=type;return true;
}
void C_MemoryBuffer::Destroy(){
    if(m_Type==E_MemoryBuffer_Main&&m_Address)BizAlignedFree(m_Address);
    m_Address=nullptr;m_Size=m_Align=m_Flags=0;m_Type=E_MemoryBuffer_Unknown;
}
bool C_MemoryBuffer::Inside(U32 size,const U8* address)const{
    if(!m_Address||!address)return false;
    const auto begin=reinterpret_cast<uintptr_t>(m_Address),candidate=reinterpret_cast<uintptr_t>(address);
    return candidate>=begin&&candidate-begin<=m_Size&&size<=m_Size-(candidate-begin);
}
}
