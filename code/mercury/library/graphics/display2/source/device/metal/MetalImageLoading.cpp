// [Blur2PC][macOS] PC DDS, uncompressed 32-bit TGA and 24-bit BI_RGB BMP.
#include "MetalImageLoading.h"
#include <display2/utils/Utils.h>
#include <algorithm>
#include <cstring>
#include <memory>
namespace Display2::Metal {
namespace {
U16 word(const U8* p){return U16(p[0])|U16(p[1])<<8;}
U32 dword(const U8* p){return U32(word(p))|U32(word(p+2))<<16;}
bool dimensions(U32 w,U32 h){return w&&h&&w<=16384&&h<=16384;}
std::unique_ptr<U8[]> read(const Util::C_String& file,U32& size){U8* data=nullptr;if(!GetFileData(file,size,&data))return {};return std::unique_ptr<U8[]>(data);}
void image(C_TextureParams& p,U32 w,U32 h,U32 flags,std::unique_ptr<U8[]> pixels){p.m_Width=w;p.m_Height=h;p.m_Depth=1;p.m_Levels=1;p.m_Format=E_TextureFormat_A8R8G8B8;p.m_Flags=flags;p.m_FileSize=w*h*4;p.m_pFileData=pixels.release();}
}
bool LoadDDS(const Util::C_String& file,C_TextureParams* params){
    if(!params)return false;C_TextureParams next;if(!LoadDDS(file,&next,3))return false;
    // Original DDS helpers return validated metadata and stream tight mip rows from the file.
    delete[] next.m_pFileData;next.m_pFileData=nullptr;*params=next;return true;
}
bool LoadDDS(const Util::C_String& file,C_TextureParams* params,U32 textureType){
    if(!params)return false;U32 size=0;auto bytes=read(file,size);if(!bytes||size<128||std::memcmp(bytes.get(),"DDS ",4)||dword(bytes.get()+4)!=124||dword(bytes.get()+76)!=32)return false;
    const auto* p=bytes.get();const U32 w=dword(p+16),h=dword(p+12),depth=dword(p+24),levels=std::max(1U,dword(p+28));
    const U32 caps=dword(p+112);const bool volume=textureType==4,cube=textureType==5;
    if(!dimensions(w,h)||(textureType!=3&&!volume&&!cube))return false;
    if(volume){if(!(caps&0x200000)||(caps&0xfe00)||!depth||depth>2048||w>2048||h>2048)return false;}
    else if(cube){if((caps&0xfe00)!=0xfe00||(caps&0x200000)||w!=h||depth>1)return false;}
    else if(depth>1||(caps&0x20fe00))return false;
    U32 maxLevels=1;for(U32 n=std::max({w,h,volume?depth:1U});n>1;n>>=1)++maxLevels;if(levels>maxLevels)return false;
    E_TextureFormat format=E_TextureFormat_Unknown;
    if(dword(p+80)&4){switch(dword(p+84)){case 0x31545844:format=E_TextureFormat_DXT1;break;case 0x33545844:format=E_TextureFormat_DXT3;break;case 0x35545844:format=E_TextureFormat_DXT5;break;
    case 21:case 22:case 28:case 32:case 33:case 50:case 51:case 60:case 81:case 111:case 112:case 113:case 114:case 115:case 116:format=E_TextureFormat(dword(p+84));break;default:return false;}}
    else if(dword(p+80)==0x41&&dword(p+88)==32&&dword(p+92)==0x00ff0000&&dword(p+96)==0x0000ff00&&dword(p+100)==0x000000ff&&dword(p+104)==0xff000000)format=E_TextureFormat_A8R8G8B8;
    // [Blur2PC] Original PC bloom's ghost_mask.dds is an ordinary DDS L8
    // luminance image. Preserve its exact bytes and existing native LLL1 view.
    else if(dword(p+80)==0x20000&&dword(p+88)==8&&dword(p+92)==0xff&&!dword(p+96)&&!dword(p+100)&&!dword(p+104))format=E_TextureFormat_L8;
    else return false;
    if(textureType==3&&format!=E_TextureFormat_A8R8G8B8&&format!=E_TextureFormat_L8&&format!=E_TextureFormat_DXT1&&format!=E_TextureFormat_DXT3&&format!=E_TextureFormat_DXT5)return false;
    const bool compressed=format==E_TextureFormat_DXT1||format==E_TextureFormat_DXT3||format==E_TextureFormat_DXT5;
    if(volume&&compressed)return false; // Native Metal volumes have no BC upload path.
    U64 payload=0;try{payload=volume?GetTexture3DSize(w,h,depth,levels,0,format):U64(GetTexture2DSize(w,h,levels,0,format))*(cube?6:1);}catch(...){return false;}
    if(!compressed&&(dword(p+8)&8)&&dword(p+20)&&dword(p+20)!=GetTexturePitchFromWidth(w,0,format)){
        // [Blur2PC] The original PC DDS reader derives rows from dimensions and
        // format, ignoring dwPitchOrLinearSize. Retail ghost_mask.dds has a
        // stale pitch512 but exactly64x64 L8 bytes. Only an exact complete tight
        // 2D L8 mip chain proves this legacy layout without guessing padding.
        if(textureType!=3||format!=E_TextureFormat_L8||payload+128!=size)return false;
    }
    if(payload+128>size)return false;
    C_TextureParams next;next.m_Width=w;next.m_Height=h;next.m_Depth=volume?depth:1;next.m_Levels=levels;next.m_Format=format;next.m_Flags=E_Flags_Linear;
    next.m_MipOffset=128;next.m_MipEnd=levels-1;next.m_FileSize=size;next.m_pFileData=bytes.release();*params=next;return true;
}
bool LoadTGA(const Util::C_String& file,C_TextureParams* params){
    if(!params)return false;U32 size=0;auto bytes=read(file,size);if(!bytes||size<18)return false;const auto* p=bytes.get();
    const U32 w=word(p+12),h=word(p+14),offset=18+U32(p[0]);
    if(p[1]||p[2]!=2||p[16]!=32||(p[17]&0xc0)||(p[17]&15)>8||!dimensions(w,h)||U64(offset)+U64(w)*h*4>size)return false;
    auto pixels=std::make_unique<U8[]>(U64(w)*h*4);
    for(U32 y=0;y<h;++y)for(U32 x=0;x<w;++x){const U32 sourceY=p[17]&32?y:h-1-y,sourceX=p[17]&16?w-1-x:x;
        std::memcpy(pixels.get()+(U64(y)*w+x)*4,p+offset+(U64(sourceY)*w+sourceX)*4,4);}
    C_TextureParams next;image(next,w,h,0,std::move(pixels));*params=next;return true;
}
bool LoadBMP(const Util::C_String& file,C_TextureParams* params){
    if(!params)return false;U32 size=0;auto bytes=read(file,size);if(!bytes||size<54)return false;const auto* p=bytes.get();
    const U32 header=dword(p+14),offset=dword(p+10);const S32 width=S32(dword(p+18)),height=S32(dword(p+22));
    const U64 h=height<0?-S64(height):S64(height);
    if(word(p)!=0x4d42||header<40||U64(header)+14>size||offset<U64(header)+14||width<=0||h>16384||!dimensions(U32(width),U32(h))||word(p+26)!=1||word(p+28)!=24||dword(p+30)!=0)return false;
    const U64 stride=(U64(width)*3+3)&~U64(3);if(U64(offset)+stride*h>size)return false;
    auto pixels=std::make_unique<U8[]>(U64(width)*h*4);
    for(U32 y=0;y<h;++y)for(U32 x=0;x<U32(width);++x){const auto* source=p+offset+(height<0?y:h-1-y)*stride+x*3;
        auto* destination=pixels.get()+(U64(y)*width+x)*4;std::memcpy(destination,source,3);destination[3]=255;}
    C_TextureParams next;image(next,U32(width),U32(h),E_Flags_Linear,std::move(pixels));*params=next;return true;
}
}
