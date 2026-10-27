// [Blur2PC][macOS] Host-memory backing for the original Display2 suballocator.
#pragma once
#include <core/Core.h>
namespace Display2 {
enum E_MemoryBuffer {E_MemoryBuffer_Unknown,E_MemoryBuffer_Main,E_MemoryBuffer_Local,E_MemoryBuffer_Indirect};
class C_MemoryBuffer {
public:
    C_MemoryBuffer()=default;
    ~C_MemoryBuffer(){Destroy();}
    C_MemoryBuffer(const C_MemoryBuffer&)=delete;
    C_MemoryBuffer& operator=(const C_MemoryBuffer&)=delete;
    U32 GetSize()const{return m_Size;} U32 GetAlign()const{return m_Align;}
    U32 GetFlags()const{return m_Flags;} U8* GetAddress()const{return m_Address;}
    E_MemoryBuffer GetType()const{return m_Type;}
    bool Create(U32 size,U32 align,U32 flags,E_MemoryBuffer type,U8* address=nullptr);
    void Destroy();
    bool Inside(U32 size,const U8* address)const;
private:
    U32 m_Size=0,m_Align=0,m_Flags=0;
    U8* m_Address=nullptr;
    E_MemoryBuffer m_Type=E_MemoryBuffer_Unknown;
};
}
