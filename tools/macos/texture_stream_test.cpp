// [Blur2PC][macOS] Independently authored PC texture headers/rows -> actual private Metal storage.
#include <core/Maths.h>
#include <system/debuglog.h>
#include <system/serialise.h>
#include <system/serialise/C_MemoryStream.h>
#include <display/device/Device.h>
#include <display/texture/d3d11/D3D11TextureLoad.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
static void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
template<class F>static void rejects(F action,const char* text){bool failed=false;try{action();}catch(const std::exception&){failed=true;}require(failed,text);}
static void word(std::vector<U8>& data,U32 value){for(U32 i=0;i<4;++i)data.push_back(U8(value>>(8*i)));}
static std::vector<U8> header(U32 width,U32 height,U32 levels,U32 format,U32 type=3){std::vector<U8> result;for(U32 value:{width,height,1U,levels,format,0U,type})word(result,value);return result;}
struct Mip {U32 width,height,rowBytes,rows;std::vector<U8> bytes;};
static void pattern(std::vector<Mip>& mips,U32 face=0){for(U32 level=0;level<mips.size();++level){auto& mip=mips[level];mip.bytes.resize(mip.rowBytes*mip.rows);for(U32 i=0;i<mip.bytes.size();++i)mip.bytes[i]=U8(face*29+level*43+i*7);}}
static void verify(const Display::C_Texture2D& texture,const std::vector<std::vector<Mip>>& faces){
    require(texture.GetLevels()==faces[0].size(),"PC mip count was lost");
    for(U32 face=0;face<faces.size();++face)for(U32 level=0;level<faces[face].size();++level){const auto& expected=faces[face][level];require(texture.GetWidth(level)==expected.width&&texture.GetHeight(level)==expected.height,"PC logical mip dimensions changed");auto lock=faces.size()==6?texture.LockRectCube(face,level,0x10):texture.LockRect2D(level,0x10);require(lock.pBits&&lock.pitch>=expected.rowBytes,"Private native mip readback failed");bool equal=true;for(U32 row=0;row<expected.rows;++row)equal&=std::memcmp(lock.pBits+row*lock.pitch,expected.bytes.data()+row*expected.rowBytes,expected.rowBytes)==0;if(faces.size()==6)texture.UnlockRectCube(face,level);else texture.UnlockRect2D(level);require(equal,"PC payload order/format/rows changed in actual GPU storage");}
}
int main(){try{
    Display::C_Device device;Display::C_DeviceParams params;params.m_Width=params.m_Height=16;require(device.Create(params),"Native texture stream GPU setup failed");
    auto exercise=[&](U32 format,std::vector<Mip> mips,U32 count){
        std::vector<std::vector<Mip>> faces;auto bytes=header(mips[0].width,mips[0].height,mips.size(),format,count==6?5:3);
        for(U32 face=0;face<count;++face){auto expected=mips;pattern(expected,face);for(const auto& mip:expected)bytes.insert(bytes.end(),mip.bytes.begin(),mip.bytes.end());faces.push_back(expected);}
        Display::C_Texture2D texture;Sys::C_MemoryStream stream(bytes.data(),bytes.size());Display::PCLoadTexture(&texture,&stream);require(texture.IsCreated()&&U32(texture.GetFormat())==format&&texture.GetSize()==bytes.size()-28&&stream.GetPosition()==bytes.size(),"Original PC descriptor/payload size or stream consumption changed");verify(texture,faces);
        Sys::C_MemoryStream replacement(bytes.data(),bytes.size());require(Display::CreateTexture2D(&texture,&replacement)==0&&replacement.GetPosition()==0,"An existing tracked native texture was silently replaced");verify(texture,faces);
        if(count==1){
            auto top=faces[0][0].bytes;for(auto& byte:top)byte^=0x5a;Sys::C_MemoryStream prefix(top.data(),top.size());Display::LoadTextureData2D(&texture,&prefix,top.size());faces[0][0].bytes=top;verify(texture,faces);
            Sys::C_MemoryStream shortRead(top.data(),top.size()-1);rejects([&]{Display::LoadTextureData2D(&texture,&shortRead,top.size());},"A short native texture stream succeeded");verify(texture,faces);
            Sys::C_MemoryStream partial(top.data(),top.size());rejects([&]{Display::LoadTextureData2D(&texture,&partial,top.size()-1);},"A partial PC mip was uploaded");require(partial.GetPosition()==0,"An invalid mip boundary consumed stream bytes");verify(texture,faces);
        }

        if(mips.size()>1){
            // Original PC streaming migrates a common mip tail, rather than resizing/cropping mip zero.
            Display::C_Texture2D smaller,grown;U32 smallSize=0;for(U32 level=1;level<mips.size();++level)smallSize+=mips[level].rowBytes*mips[level].rows;smallSize*=count;
            Display::CloneTexture2D(&smaller,&texture,mips[1].width,mips[1].height,mips.size()-1,smallSize);require(smaller.IsCreated(),"Native streaming clone did not allocate a real GPU resource");Display::CopyTextureData2D(&smaller,&texture);
            std::vector<std::vector<Mip>> tail;for(const auto& face:faces)tail.emplace_back(face.begin()+1,face.end());verify(smaller,tail);
            Display::CloneTexture2D(&grown,&smaller,mips[0].width,mips[0].height,mips.size(),texture.GetSize());require(grown.IsCreated(),"Native streaming growth clone failed");
            std::vector<std::vector<Mip>> expanded;std::vector<U8> sentinel;
            for(U32 face=0;face<count;++face){auto expected=mips;pattern(expected,face+73);for(const auto& mip:expected)sentinel.insert(sentinel.end(),mip.bytes.begin(),mip.bytes.end());for(U32 level=1;level<expected.size();++level)expected[level]=faces[face][level];expanded.push_back(expected);}
            Sys::C_MemoryStream initial(sentinel.data(),sentinel.size());Display::LoadTextureData2D(&grown,&initial,sentinel.size());Display::CopyTextureData2D(&grown,&smaller);verify(grown,expanded);
            rejects([&]{Display::CloneTexture2D(&grown,&smaller,1,1,1,4);},"A streaming clone replaced a live tracked resource");verify(grown,expanded);
            Display::C_Texture2D invalid;rejects([&]{Display::CloneTexture2D(&invalid,&texture,mips[0].width,mips[0].height,mips.size(),texture.GetSize()+1);},"Invalid streaming byte metadata created a clone");require(!invalid.IsCreated(),"Invalid clone retained GPU storage");
            device.DestroyTexture2D(&grown);device.DestroyTexture2D(&smaller);
        }
        device.DestroyTexture2D(&texture);
    };
    exercise(21,{{4,2,16,2,{}},{2,1,8,1,{}},{1,1,4,1,{}}},1);
    exercise(21,{{4,4,16,4,{}},{2,2,8,2,{}},{1,1,4,1,{}}},6);
    exercise(0x31545844,{{8,8,16,2,{}},{4,4,8,1,{}},{2,2,8,1,{}},{1,1,8,1,{}}},1);
    for(U32 format:{0x33545844U,0x35545844U})exercise(format,{{8,8,32,2,{}},{4,4,16,1,{}},{2,2,16,1,{}},{1,1,16,1,{}}},1);
    exercise(116,{{2,2,32,2,{}},{1,1,16,1,{}}},1);
    for(auto bytes:{header(0,4,1,21),header(4,4,20,21),header(4,2,1,21,5),header(4,4,1,75),header(4,4,1,21,4),header(4,4,1,0xdeadbeef),header(2,2,1,0x31545844)}){Display::C_Texture2D texture;Sys::C_MemoryStream stream(bytes.data(),bytes.size());require(Display::CreateTexture2D(&texture,&stream)==0&&!texture.IsCreated(),"Invalid/unsupported PC texture header created a resource");}
    {auto bytes=header(4,2,3,21);bytes.resize(27);Display::C_Texture2D texture;Sys::C_MemoryStream stream(bytes.data(),bytes.size());rejects([&]{Display::LoadTexture2D(&texture,&stream);},"Truncated PC texture header succeeded");require(!texture.IsCreated(),"Truncated header retained GPU ownership");}
    {auto bytes=header(4,2,3,21);bytes.resize(28+43);Display::C_Texture2D texture;Sys::C_MemoryStream stream(bytes.data(),bytes.size());rejects([&]{Display::LoadTexture2D(&texture,&stream);},"Truncated PC texture payload succeeded");require(!texture.IsCreated(),"Failed full PC texture load retained a tracked GPU resource");}
    device.Destroy();std::cout<<"Original PC texture descriptor/2D/cube/mips/BC1/BC2/BC3/float GPU upload/prefix streaming/clone/common mip-tail growth/shrink/truncation/rollback PASS (authored data only)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
