// [Blur2PC][macOS] Original PC TextureDesc and face-major mip payloads -> actual tracked Metal textures.
#include "../../device/metal/NativeDisplayIncludes.h"
#include <display/device/Device.h>
#include <display/texture/TextureDesc.h>
#include <display/texture/d3d11/D3D11TextureLoad.h>
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

namespace Display {
S32 CreateTexture2D(C_Texture2D* texture,Sys::I_BinaryStream* stream){
    if(!texture||texture->IsCreated()||!stream||stream->IsWriting()||!DeviceExists()||!Device()->IsCreated())return 0;
    TextureDesc desc{};static_assert(sizeof(desc)==28,"Original PC texture header is seven 32-bit fields");
    // TextureDesc::Deserialise uses these seven fields with NoEndianSwap on PC.
    if(!stream->Read(&desc,sizeof(desc)))return 0;
    if(desc.Width<=0||desc.Height<=0||desc.Width>16384||desc.Height>16384||desc.Levels<=0||
       (desc.Type!=3&&desc.Type!=5)||(desc.Type==5&&desc.Width!=desc.Height))return 0;
    U32 maxLevels=1;for(U32 n=std::max(desc.Width,desc.Height);n>1;n>>=1)++maxLevels;
    if(U32(desc.Levels)>maxLevels)return 0;
    const auto format=static_cast<Display2::E_TextureFormat>(desc.Format);
    if(format==Display2::E_TextureFormat_D16||format==Display2::E_TextureFormat_D24S8)return 0; // Native opaque/depth streaming has no PC colour-byte upload path.
    U64 total=0;
    try{total=Display2::GetTexture2DSize(desc.Width,desc.Height,desc.Levels,0,format);if(desc.Type==5)total*=6;}
    catch(const std::exception&){return 0;}
    if(!total||total>U64(std::numeric_limits<S32>::max()))return 0;
    const bool created=desc.Type==5?Device()->CreateTextureCube(desc.Width,desc.Levels,0,static_cast<E_Format>(desc.Format),texture):
        Device()->CreateTexture2D(desc.Width,desc.Height,desc.Levels,0,static_cast<E_Format>(desc.Format),texture);
    return created?S32(total):0;
}

void LoadTextureData2D(C_Texture2D* texture,Sys::I_BinaryStream* stream,int size){
    if(!texture||!texture->IsCreated()||!stream||stream->IsWriting()||size<=0)throw std::invalid_argument("Invalid native PC texture payload upload");
    const auto type=texture->GetType();if(type!=Display2::E_TextureType_2D&&type!=Display2::E_TextureType_Cube)throw std::invalid_argument("PC texture upload requires a 2D or cube texture");
    struct Mip {U32 face,level,rowBytes,rows,bytes,offset;};std::vector<Mip> plan;U64 consumed=0;
    // The original PC loader accepts complete newly streamed top mips. Never
    // invent padding in the file or consume part of a mip as a valid payload.
    for(U32 face=0;face<(type==Display2::E_TextureType_Cube?6U:1U)&&consumed<U32(size);++face){
        for(U32 level=0;level<texture->GetLevels()&&consumed<U32(size);++level){
            const auto width=texture->GetWidth(level),height=texture->GetHeight(level);
            const U32 row=Display2::GetTexturePitchFromWidth(width,0,texture->GetFormat());
            const U32 bytes=Display2::GetTexture2DSize(width,height,0,texture->GetFormat());
            if(!row||consumed+bytes>U32(size))throw std::invalid_argument("PC texture payload ends inside a mip");
            plan.push_back({face,level,row,bytes/row,bytes,U32(consumed)});consumed+=bytes;
        }
    }
    if(consumed!=U32(size))throw std::invalid_argument("PC texture payload exceeds its mip layout");
    std::vector<U8> payload(size);
    if(!stream->Read(payload.data(),U32(size)))throw std::runtime_error("Truncated PC texture payload");
    for(const auto& mip:plan){
        auto lock=type==Display2::E_TextureType_Cube?texture->LockRectCube(mip.face,mip.level):texture->LockRect2D(mip.level);
        if(!lock.pBits)throw std::runtime_error("Native PC texture mip could not be mapped for upload");
        if(lock.pitch<mip.rowBytes){if(type==Display2::E_TextureType_Cube)texture->UnlockRectCube(mip.face,mip.level);else texture->UnlockRect2D(mip.level);throw std::runtime_error("Native staging row is smaller than the PC texture row");}
        for(U32 row=0;row<mip.rows;++row)std::memcpy(lock.pBits+row*lock.pitch,payload.data()+mip.offset+row*mip.rowBytes,mip.rowBytes);
        if(type==Display2::E_TextureType_Cube)texture->UnlockRectCube(mip.face,mip.level);else texture->UnlockRect2D(mip.level);
    }
}
void LoadTexture2D(C_Texture2D* texture,Sys::I_BinaryStream* stream){
    const S32 size=Display::CreateTexture2D(texture,stream);if(!size)throw std::runtime_error("Unsupported or invalid native PC texture header");
    try{Display::LoadTextureData2D(texture,stream,size);}catch(...){Device()->DestroyTexture2D(texture);throw;}
}
void PCLoadTexture(C_Texture2D* texture,Sys::I_BinaryStream* stream){Display::LoadTexture2D(texture,stream);}
// [Blur2PC] Streaming clones allocate actual native resources; migration is a separate GPU blit.
void CloneTexture2D(C_Texture2D* destination,const C_Texture2D* source,int width,int height,int levels,int size){
    if(!destination||!source||!source->IsCreated()||width<=0||height<=0||levels<=0||size<=0)return;
    if(destination->IsCreated())throw std::invalid_argument("Native streaming clone requires an uncreated destination");
    if(source->GetType()!=Display2::E_TextureType_2D&&source->GetType()!=Display2::E_TextureType_Cube)throw std::invalid_argument("Native streaming clone requires a 2D or cube source");
    U64 expected=Display2::GetTexture2DSize(width,height,levels,0,source->GetFormat());
    if(source->GetType()==Display2::E_TextureType_Cube)expected*=6;
    if(expected!=U32(size))throw std::invalid_argument("Native streaming clone byte size disagrees with PC mip layout");
    if(source->GetType()==Display2::E_TextureType_Cube){if(width!=height)throw std::invalid_argument("Native cube clone must remain square");Device()->CreateTextureCube(width,levels,source->GetFlags(),static_cast<E_Format>(source->GetFormat()),destination);}
    else Device()->CreateTexture2D(width,height,levels,source->GetFlags(),static_cast<E_Format>(source->GetFormat()),destination);
}
void CopyTextureData2D(C_Texture2D* destination,const C_Texture2D* source){
    if(!destination||!source||!destination->IsCreated()||!source->IsCreated()||destination==source)return;
    if(!Device()->GetDisplay2().CopyTexture(destination,source,true))throw std::runtime_error("Native PC common mip-tail GPU copy failed");
}

}
