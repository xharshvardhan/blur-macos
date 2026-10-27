// [Blur2PC][macOS] Original upper texture upload/container paths plus reuse of the validated native image decoder.
#include <core/Maths.h>
#include <system/debuglog.h>
#include <system/serialise.h>
#include <system/serialise/C_FileBlock.h>
#include <display/device/Device.h>
#include <display/texture/Texture2DUtils.h>
#include <display/texture/TextureParams.h>
#include <array>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
static void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
static void verify(const Display::C_Texture2D& texture){
    for(U32 level=0;level<texture.GetLevels();++level){auto read=texture.LockRect2D(level,0x10);require(read.pBits,"Original upper upload readback failed");bool good=true;for(U32 y=0;y<texture.GetHeight(level);++y)for(U32 x=0;x<texture.GetWidth(level);++x){U32 actual;std::memcpy(&actual,read.pBits+y*read.pitch+x*4,4);const U32 expected=(7+level*11+x*3+y)|(((y+1)*29+level*5)<<8)|(((x+1)*19+level*3)<<16)|((128+level*16+(x+y)%7)<<24);good&=actual==expected;}texture.UnlockRect2D(level);require(good,"Original texture params upload changed PC rows/mips in Metal staging");}
}
int main(int argc,char** argv){try{
    require(argc==2,"Texture params fixture directory missing");auto& fs=Sys::C_FileSystem::Instance();Sys::SetDefaultFileSystem(&fs);require(fs.SetWorkingDirectory(argv[1]),"Native texture params filesystem setup failed");
    Display::C_Device device;Display::C_DeviceParams config;config.m_Width=config.m_Height=16;require(device.Create(config),"Original upper texture GPU setup failed");
    std::vector<U8> pixels;for(U32 level=0;level<3;++level){const U32 w=level==0?5:level==1?2:1,h=level==0?3:1;for(U32 y=0;y<h;++y)for(U32 x=0;x<w;++x){pixels.push_back(7+level*11+x*3+y);pixels.push_back((y+1)*29+level*5);pixels.push_back((x+1)*19+level*3);pixels.push_back(128+level*16+(x+y)%7);}}
    require(pixels.size()==72,"Independent PC mip fixture size changed");Display::C_TextureParams params;params.m_Width=5;params.m_Height=3;params.m_Levels=3;params.m_Format=Display::E_Format_A8R8G8B8;params.m_MipTail=2;params.m_pMipData=pixels.data();params.m_MipSize=pixels.size();
    Display::C_Texture2D texture;require(device.CreateTexture2D(5,3,3,0,Display::E_Format_A8R8G8B8,&texture)&&Display::UploadTexture2D(&texture,&params),"Original upper texture params upload failed");verify(texture);device.DestroyTexture2D(&texture);
    {
        std::unique_ptr<Sys::I_File> file(fs.Open("native.texture",Sys::E_FileAccess_Write));require(bool(file),"Original texture container writer could not open fixture");
        Sys::C_FileBlock root(file.get(),"Texture");{Sys::C_FileBlock header(&root,"Header");const std::array<U32,2> fields={0,0};require(header.Write(fields.data(),sizeof(fields)),"Original texture header block write failed");}
        {Sys::C_FileBlock data(&root,"Data");const std::array<U32,7> fields={5,3,1,3,21,0,3};require(data.Write(fields.data(),sizeof(fields))&&data.Write(pixels.data(),pixels.size()),"Original PC texture data block write failed");}
    }
    require(Display::CreateTexture2DFromFile("native.texture",&texture),"Original upper texture container could not load native PC payload");verify(texture);device.DestroyTexture2D(&texture);
    require(Display::CreateTexture2DFromFile("rgba.dds",&texture),"Original upper DDS file API failed to use the validated native decoder");verify(texture);device.DestroyTexture2D(&texture);
    const U32 palette[]={0xff0000ff,0xff00ff00,0xffff0000,0x80808080,0xff123456,0};
    for(const char* name:{"bottom.tga","top-right.tga","bottom.bmp","top.bmp"}){require(Display::CreateTexture2DFromFile(name,&texture),"Upper image helper failed");auto read=texture.LockRect2D(0,0x10);require(read.pBits,"Upper image readback failed");bool good=true;for(U32 y=0;y<2;++y)for(U32 x=0;x<3;++x){U32 actual;std::memcpy(&actual,read.pBits+y*read.pitch+x*4,4);good&=actual==(palette[y*3+x]|(std::strstr(name,".bmp")?0xff000000U:0));}texture.UnlockRect2D(0);device.DestroyTexture2D(&texture);require(good,"Upper image helper lost origin/offset/stride/channel bytes");}
    for(const char* name:{"short.dds","truncated.dds","bad-levels.dds","short.tga","short.bmp"})require(!Display::CreateTexture2DFromFile(name,&texture)&&!texture.IsCreated(),"Upper native image decoder accepted a truncated or invalid image");
    device.Destroy();Sys::SetDefaultFileSystem(nullptr);std::cout<<"Original upper texture params/PC container/GPU rows and mip payloads/shared DDS-BMP-TGA decoder/truncation PASS (authored data only)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
