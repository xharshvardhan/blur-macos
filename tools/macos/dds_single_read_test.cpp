// [Blur2PC] Original DDS validator/file facade -> actual Metal mip storage, one read.
#include <display2/device/Device.h>
#include <display2/utils/Utils.h>
#include <system/serialise.h>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>
namespace {
void Require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
void Word(std::vector<U8>& bytes,U32 at,U32 value){for(U32 i=0;i<4;++i)bytes[at+i]=U8(value>>(8*i));}
struct Mip {U32 width,height,row,rows;std::vector<U8> bytes;};
struct Image {std::vector<U8> bytes;std::vector<Mip> mips;Display2::E_TextureFormat format;};
Image Authored(U32 width,U32 height,U32 levels,Display2::E_TextureFormat format,bool numericL8=false){
    Image result;result.format=format;auto& bytes=result.bytes;bytes.resize(128);
    std::memcpy(bytes.data(),"DDS ",4);Word(bytes,4,124);Word(bytes,8,0x2100f);
    Word(bytes,12,height);Word(bytes,16,width);Word(bytes,28,levels);Word(bytes,76,32);Word(bytes,108,0x401008);
    const bool blocks=format==Display2::E_TextureFormat_DXT1||format==Display2::E_TextureFormat_DXT3||format==Display2::E_TextureFormat_DXT5;
    if(blocks){Word(bytes,80,4);Word(bytes,84,U32(format));}
    else if(format==Display2::E_TextureFormat_L8){Word(bytes,80,numericL8?4:0x20000);Word(bytes,84,numericL8?50:0);Word(bytes,88,8);Word(bytes,92,255);Word(bytes,20,width);}
    else{Word(bytes,80,0x41);Word(bytes,88,32);Word(bytes,92,0x00ff0000);Word(bytes,96,0x0000ff00);Word(bytes,100,0xff);Word(bytes,104,0xff000000);Word(bytes,20,width*4);}
    for(U32 level=0;level<levels;++level){
        const U32 block=blocks?4:1,pixel=blocks?(format==Display2::E_TextureFormat_DXT1?8:16):(format==Display2::E_TextureFormat_L8?1:4);
        Mip mip{width,height,((width+block-1)/block)*pixel,(height+block-1)/block,{}};
        mip.bytes.resize(mip.row*mip.rows);
        for(U32 i=0;i<mip.bytes.size();++i)mip.bytes[i]=U8(i*19+level*47+3);
        bytes.insert(bytes.end(),mip.bytes.begin(),mip.bytes.end());result.mips.push_back(std::move(mip));
        width=std::max(1U,width>>1);height=std::max(1U,height>>1);
    }
    return result;
}
struct Counts {U32 opens=0,closes=0,underflows=0,liveFiles=0,peakFiles=0;U64 bytes=0,maxRequest=0;bool critical=true,readAccess=true;};
struct CountingFile final:Sys::I_File {
    const std::vector<U8>& bytes;Counts& counts;U64 advertised,available,position=0;bool throwRead,swapped=false;U32 version=0;
    CountingFile(const std::vector<U8>& data,Counts& c,U64 length,U64 availableBytes,bool throws):bytes(data),counts(c),advertised(length),available(availableBytes),throwRead(throws){++counts.liveFiles;counts.peakFiles=std::max(counts.peakFiles,counts.liveFiles);}
    ~CountingFile()override{++counts.closes;--counts.liveFiles;}
    const bool IsWriting()const override{return false;}
    Btl::String Name()const override{return Btl::String("authored-counted.dds");}
    const Sys::C_StreamSize Length()override{return advertised;}
    const Sys::C_StreamPos GetPosition()const override{return position;}
    void SetPosition(const Sys::C_StreamPos next)override{Require(next<=advertised,"Authored DDS seek out of range");position=next;m_pCurBuffer=m_pBufferEnd=nullptr;}
    const bool GetEndianSwapFlag()const override{return swapped;}
    void SetEndianSwapFlag(const bool value)override{swapped=value;}
    U32 GetVersionNumber()const override{return version;}
    void SetVersionNumber(U32 value)override{version=value;}
    bool Overflow()override{return false;}
    bool Underflow(UnderflowContext& context)override{
        ++counts.underflows;counts.maxRequest=std::max(counts.maxRequest,U64(context.readSize));
        if(throwRead)throw std::runtime_error("Authored critical DDS read exception");
        const U64 end=std::min<U64>(bytes.size(),available);
        const U32 size=position<end?U32(std::min<U64>(context.readSize,end-position)):0;
        if(size&&context.readTarget)std::memcpy(context.readTarget,bytes.data()+position,size);
        context.bytesRead=size;position+=size;counts.bytes+=size;
        return size==context.readSize;
    }
};
struct CountingFs final:Sys::I_FileSystem {
    const std::vector<U8>* data=nullptr;Counts counts;U64 length=0,available=0;bool throwRead=false,missing=false;
    void Reset(const std::vector<U8>& bytes){Require(counts.liveFiles==0,"Previous DDS file owner still live");data=&bytes;length=available=bytes.size();counts={};throwRead=missing=false;}
    Sys::I_File* Open(const Sys::C_FilePath&,const Sys::E_FileAccess access=Sys::E_FileAccess_Read,const Sys::C_OpenParams& params=Sys::C_OpenParams())override{
        ++counts.opens;counts.critical&=params.m_IsCritical;counts.readAccess&=access==Sys::E_FileAccess_Read;
        return missing?nullptr:new CountingFile(*data,counts,length,available,throwRead);
    }
    Sys::C_FilePath GetWorkingDirectory()const override{return Sys::C_FilePath("/");}
};
U32 TextureCount(Display2::C_Device& device){
#if TRACK_DISPLAY2_RESOURCES
    return device.GetTextureCount();
#else
    (void)device;return 0;
#endif
}
void Verify(Display2::C_Texture& texture,const Image& image){
    Require(texture.IsCreated()&&texture.GetFormat()==image.format&&texture.GetLevels()==image.mips.size(),"DDS metadata changed");
    for(U32 level=0;level<image.mips.size();++level){
        const auto& mip=image.mips[level];Require(texture.GetWidth(level)==mip.width&&texture.GetHeight(level)==mip.height,"DDS logical mip dimensions changed");
        const auto lock=texture.LockRect2D(level,0x10);Require(lock.GetBits()&&lock.GetPitch()>=S32(mip.row),"Actual DDS GPU readback unavailable");
        bool same=true;for(U32 y=0;y<mip.rows;++y)same&=!std::memcmp(lock.GetBits()+U64(y)*lock.GetPitch(),mip.bytes.data()+U64(y)*mip.row,mip.row);
        texture.UnlockRect2D(level);Require(same,"DDS tight rows/BC tail blocks/mip offsets changed in GPU storage");
    }
}
void OneRead(const CountingFs& fs,U64 bytes){
    Require(fs.counts.opens==1&&fs.counts.closes==1&&fs.counts.liveFiles==0&&fs.counts.peakFiles==1,"DDS2D did not retire exactly one original filesystem owner");
    Require(fs.counts.critical&&fs.counts.readAccess,"DDS2D changed original critical/read Open policy");
    Require(fs.counts.underflows==1&&fs.counts.bytes==bytes&&fs.counts.maxRequest==bytes,"DDS2D validated then reread original bytes instead of retaining one complete buffer");
}
}
int main(){try{
    auto& original=Sys::C_FileSystem::Instance();Sys::SetDefaultFileSystem(&original);
    CountingFs fs;struct RestoreFs {Sys::I_FileSystem* old;~RestoreFs(){Sys::SetDefaultFileSystem(old);}} restore{&original};
    Sys::SetDefaultFileSystem(&fs);Display2::SetNativeShaderResourceRoot("");
    Display2::C_Device device;Require(device.Create(),"Native DDS fixture device unavailable");
    Display2::Device(&device);struct RestoreDevice {~RestoreDevice(){Display2::Device(nullptr);}} restoreDevice;
    const auto baseline=TextureCount(device);bool reportedCounts=false;
    auto valid=[&](const Image& image){
        fs.Reset(image.bytes);Display2::C_Texture texture;
        struct Retire {Display2::C_Device& d;Display2::C_Texture& t;~Retire(){d.Destroy(&t);}} retire{device,texture};
        Require(Display2::CreateTexture2DFromFile("authored.dds",&texture),"Actual native DDS loader rejected authored valid bytes");
        OneRead(fs,image.bytes.size());Verify(texture,image);
        if(!reportedCounts){std::cout<<"DDS2D actual counted FS: opens="<<fs.counts.opens<<" readCalls="<<fs.counts.underflows<<" bytes="<<fs.counts.bytes<<" fileSize="<<image.bytes.size()<<" peakOpenFiles="<<fs.counts.peakFiles<<'\n';reportedCounts=true;}
        // Rejected replacement must preserve the caller's existing owner/data.
        fs.Reset(image.bytes);Require(!Display2::CreateTexture2DFromFile("authored.dds",&texture)&&fs.counts.opens==0,"DDS2D replaced/read a live destination owner");Verify(texture,image);
    };
    valid(Authored(5,3,3,Display2::E_TextureFormat_A8R8G8B8));
    valid(Authored(5,3,3,Display2::E_TextureFormat_L8));
    valid(Authored(5,3,3,Display2::E_TextureFormat_L8,true));
    auto ghost=Authored(64,64,1,Display2::E_TextureFormat_L8);Word(ghost.bytes,20,512);valid(ghost);
    for(auto format:{Display2::E_TextureFormat_DXT1,Display2::E_TextureFormat_DXT3,Display2::E_TextureFormat_DXT5})valid(Authored(8,12,4,format));
    Require(TextureCount(device)==baseline,"Valid DDS fixtures retained GPU texture owners");
    auto rejects=[&](const std::vector<U8>& bytes){fs.Reset(bytes);Display2::C_Texture failed;Require(!Display2::CreateTexture2DFromFile("bad.dds",&failed)&&!failed.IsCreated(),"Malformed/unsupported DDS2D published a GPU owner");Require(fs.counts.opens==1&&fs.counts.closes==1&&fs.counts.liveFiles==0&&TextureCount(device)==baseline,"Failed DDS2D retained file/GPU ownership");};
    const auto good=Authored(5,3,3,Display2::E_TextureFormat_A8R8G8B8).bytes;
    for(const auto field:{0U,4U,76U,80U,92U,96U,100U,104U}){auto bad=good;Word(bad,field,0);rejects(bad);}
    for(const auto value:{0U,16385U}){auto bad=good;Word(bad,16,value);rejects(bad);}
    for(const auto field:{24U,28U,112U}){auto bad=good;Word(bad,field,field==24?2:field==28?32:0xfe00);rejects(bad);}
    {auto bad=good;Word(bad,20,512);rejects(bad);}
    {auto bad=ghost.bytes;bad.push_back(1);rejects(bad);}
    {auto bad=good;bad.pop_back();rejects(bad);}
    {auto bad=good;bad.resize(127);rejects(bad);}
    for(U32 fourcc:{0x32495441U,0x30315844U}){auto bad=good;Word(bad,80,4);Word(bad,84,fourcc);rejects(bad);} // ATI2/BC5 and DX10 remain unsupported.
    rejects(Authored(5,3,3,Display2::E_TextureFormat_DXT1).bytes); // Original native BC root-block limitation.
    // Facade short read and thrown critical-reader cleanup are genuine I/O paths.
    for(bool throws:{false,true}){
        fs.Reset(good);fs.available=good.size()-1;fs.throwRead=throws;Display2::C_Texture failed;bool rejected=false;
        try{rejected=!Display2::CreateTexture2DFromFile("short.dds",&failed);}catch(const std::runtime_error& error){rejected=throws&&std::string(error.what())=="Authored critical DDS read exception";}
        Require(rejected&&!failed.IsCreated()&&fs.counts.opens==1&&fs.counts.closes==1&&!fs.counts.liveFiles&&TextureCount(device)==baseline,"DDS read failure lost original file/texture ownership");
    }
    fs.Reset(good);fs.length=U64(std::numeric_limits<U32>::max())+1;
    {Display2::C_Texture failed;Require(!Display2::CreateTexture2DFromFile("huge.dds",&failed)&&!failed.IsCreated()&&fs.counts.underflows==0&&!fs.counts.liveFiles,"DDS read length truncated U64 or allocated/read an oversized file");}
    fs.Reset(good);fs.missing=true;
    {Display2::C_Texture failed;Require(!Display2::CreateTexture2DFromFile("missing.dds",&failed)&&!failed.IsCreated()&&fs.counts.opens==1&&fs.counts.closes==0&&!fs.counts.liveFiles,"Missing DDS file created/leaked an owner");}
    fs.Reset(good);valid(Authored(5,3,3,Display2::E_TextureFormat_A8R8G8B8));
    Require(TextureCount(device)==baseline&&device.WaitIdle(),"DDS failure/retry left a live resource or failed command");
    device.Destroy();std::cout<<"Native DDS2D one complete critical FS read, retained validated buffer, exact BGRA/L8/stale-pitch/BC1/BC2/BC3 mip GPU bytes and malformed/short/throw/oversize ownership rejection/retry PASS; authored data only\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
