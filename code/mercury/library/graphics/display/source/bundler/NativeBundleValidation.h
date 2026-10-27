// [Blur2PC][macOS] Validate original PC bundle offset chains before legacy readers dereference them.
#pragma once
#include <cstring>
namespace Display::ResourceBundler {
inline bool ValidateNativeBundleHeaders(const C_BundleHeader& header,const U8* bytes,U32 size,U32 version){
    if(!bytes||header.platformCode!=E_Platform_PC||!size||header.stringTableOffset>=size)return false;
    U32 offset=0;
    for(;;){
        const U32 minimum=version==3?20:version==2?12:10;
        if(U64(offset)+minimum>header.stringTableOffset)return false;
        U16 next=0,string=0,type=0;std::memcpy(&next,bytes+offset,2);std::memcpy(&string,bytes+offset+2,2);std::memcpy(&type,bytes+offset+8,2);
        U32 record=0;
        if(version==1){if(type==1)record=20;else if(type==20)record=20;else if(type==30)record=16;else return false;}
        else {if(type==1||type==4)record=version==3?28:20;else if(type==2||type==3)record=version==3?32:24;else return false;}
        if(U64(offset)+record>header.stringTableOffset||U64(header.stringTableOffset)+string>=size)return false;
        const U32 name=header.stringTableOffset+string;if(!std::memchr(bytes+name,0,size-name))return false;
        if(!next)return true;
        if(next<record||next%(version==1?2:4)||U64(offset)+next>=header.stringTableOffset)return false;
        offset+=next;
    }
}
}
