// [Blur2PC][macOS] Original file/shader/texture helpers through the real native device.
#include <display2/utils/Utils.h>
#include <display/DisplayToDisplay2.h>
#include <display/texture/Texture.h>
#include <display/buffer/VertexBuffer.h>
#include <display/surface/Surface.h>
#include <display2/memory/Memory.h>
#include <system/serialise.h>
#include <system/serialise/macos/NativeFile.h>
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <atomic>
#include <vector>
using namespace Display2;
static void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
static void arena(){
    C_MemoryManager memory;
    require(memory.Create(1024*1024,256,0,E_MemoryBuffer_Main,"native display arena"),"Display arena creation failed");
    require(memory.GetSize()==1024*1024&&memory.GetUsed()==0&&memory.GetFree()==1024*1024,"Initial display arena accounting failed");
    std::vector<std::pair<U8*,U32>> allocations;
    for(U32 i=0;i<257;++i){U8* p=nullptr;U32 n=1+(i*37)%1024;require(memory.Alloc(256,n,&p,E_MemoryType_Misc),"Display arena suballocation failed");require(reinterpret_cast<uintptr_t>(p)%256==0&&memory.Inside(n,p),"Display arena alignment/range failed");std::memset(p,U8(i),n);allocations.emplace_back(p,n);}
    for(U32 i=0;i<257;++i){auto& a=allocations[(i*73)%257];for(U32 j=0;j<a.second;++j)require(a.first[j]==U8((i*73)%257),"Display arena allocations overlap");memory.Free(a.second,a.first,E_MemoryType_Misc);}
    U8* p=nullptr;require(memory.Alloc(256,1024*1024,&p,E_MemoryType_Misc),"Freed display blocks did not merge");U8* exhausted=nullptr;require(!memory.Alloc(256,1,&exhausted,E_MemoryType_Misc)&&!exhausted,"Exhausted display arena returned storage");memory.Free(1024*1024,p,E_MemoryType_Misc);
    require(memory.GetUsed()==0&&memory.GetFree()==1024*1024,"Display arena accounting leaked");memory.Destroy();
    require(!memory.IsCreated()&&memory.GetSize()==0,"Display arena shutdown left stale state");
    require(memory.Create(65536,256,0,E_MemoryBuffer_Main,"native display arena restart"),"Display arena recreation failed");
    std::atomic<bool> valid=true;std::vector<std::thread> workers;
    for(U32 t=0;t<8;++t)workers.emplace_back([&,t]{for(U32 i=0;i<1000;++i){U8* q=nullptr;const U32 n=1+(i*13+t*97)%1024;if(!memory.Alloc(256,n,&q,E_MemoryType_Misc)){valid=false;return;}std::memset(q,U8(t),n);for(U32 j=0;j<n;++j)if(q[j]!=U8(t))valid=false;memory.Free(n,q,E_MemoryType_Misc);}});
    for(auto& worker:workers)worker.join();require(valid&&memory.GetUsed()==0&&memory.GetTracker()->m_Info[E_MemoryType_Misc].m_Count==0,"Concurrent display arena allocations/accounting failed");memory.Destroy();
    alignas(256) U8 external[1024];C_MemoryBuffer indirect;
    require(indirect.Create(sizeof(external),256,0,E_MemoryBuffer_Indirect,external),"Borrowed host arena failed");
    require(indirect.Inside(1024,external)&&!indirect.Inside(1025,external)&&!indirect.Inside(1,external+1024)&&!indirect.Inside(0,reinterpret_cast<const U8*>(UINTPTR_MAX)),"Host arena range/overflow checks failed");indirect.Destroy();external[0]=42;
    require(!indirect.Create(1024,256,0,E_MemoryBuffer_Local),"Unsupported PS3 local memory accepted");
}
template<class T>struct Resource {C_Device& device;T value;explicit Resource(C_Device& d):device(d){}~Resource(){device.Destroy(&value);}};
static void put(std::vector<U8>& b,U32 p,U32 v){for(U32 i=0;i<4;++i)b[p+i]=U8(v>>(8*i));}
static std::vector<U8> shader(bool vertex,bool green=false){
    const std::string metadata=R"({"parameters":{},"input_semantics":[],"reflection":{"ubos":[]}})";
    const std::string source=vertex?
        "#include <metal_stdlib>\nusing namespace metal;struct Output{float4 p[[position]];};vertex Output blur_main(uint i[[vertex_id]]){const float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};return {float4(p[i],.5,1)};}\n":
        green?"#include <metal_stdlib>\nusing namespace metal;fragment float4 blur_main(){return float4(0,1,0,1);}// revision two\n":
              "#include <metal_stdlib>\nusing namespace metal;fragment float4 blur_main(){return float4(1,0,0,1);}\n";
    const std::string entry="blur_main";std::vector<U8> bytes(40);std::memcpy(bytes.data(),"BMSL",4);put(bytes,4,1);put(bytes,8,vertex?0:1);
    put(bytes,16,40);put(bytes,20,metadata.size());put(bytes,24,40+metadata.size());put(bytes,28,source.size());put(bytes,32,40+metadata.size()+source.size());put(bytes,36,entry.size());
    bytes.insert(bytes.end(),metadata.begin(),metadata.end());bytes.insert(bytes.end(),source.begin(),source.end());bytes.insert(bytes.end(),entry.begin(),entry.end());put(bytes,12,bytes.size());return bytes;
}
static void write(const std::filesystem::path& p,const std::vector<U8>& b){std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),b.size());require(bool(f),"Shader fixture write failed");}
static void images(C_Device& device){
    const U32 palette[]={0xff0000ff,0xff00ff00,0xffff0000,0x80808080,0xff123456,0};
    for(const char* name:{"bottom.tga","top-right.tga","bottom.bmp","top.bmp","rgba.dds","bc.dds","bc3.dds","bc5.dds"}){
        Resource<C_Texture> ownedTexture(device),ownedClone(device);auto& texture=ownedTexture.value;auto& clone=ownedClone.value;require(CreateTexture2DFromFile(name,&texture),"Original texture helper failed to load an authored image");
        require(CloneTexture2D(&texture,&clone),"Original texture helper failed its native GPU clone");
        const bool bc=!std::strncmp(name,"bc",2),rgba=!std::strcmp(name,"rgba.dds"),bmp=std::strstr(name,".bmp");
        for(auto* image:{&texture,&clone})for(U32 l=0;l<image->GetLevels();++l){
            auto r=image->LockRect2D(l,16);require(r.pBits,"Loaded/cloned image GPU readback failed");
            if(bc){const U16 colours[]={0xf800,0x07e0,0x001f,0xffff};const U32 blockSize=image->GetFormat()==E_TextureFormat_DXT1?8:16;U8 block[16]={};
                if(image->GetFormat()==E_TextureFormat_DXT3)std::memset(block,255,8);if(image->GetFormat()==E_TextureFormat_DXT5)block[0]=255;
                block[blockSize-8]=U8(colours[l]);block[blockSize-7]=U8(colours[l]>>8);
                for(U32 y=0;y<(image->GetHeight(l)+3)/4;++y)for(U32 x=0;x<(image->GetWidth(l)+3)/4;++x)require(!std::memcmp(r.pBits+y*r.pitch+x*blockSize,block,blockSize),"BC mip/tail data changed during file upload/clone");}
            else for(U32 y=0;y<image->GetHeight(l);++y)for(U32 x=0;x<image->GetWidth(l);++x){U32 expected;
                if(rgba)expected=(7+l*11+x*3+y)|(((y+1)*29+l*5)<<8)|(((x+1)*19+l*3)<<16)|((128+l*16+(x+y)%7)<<24);
                else expected=palette[y*3+x]|(bmp?0xff000000U:0);
                U32 actual;std::memcpy(&actual,r.pBits+y*r.pitch+x*4,4);require(actual==expected,"Image origin, BMP offset/stride, Metal row pitch or mip bytes changed");}
            image->UnlockRect2D(l);
        }
#if TRACK_DISPLAY2_RESOURCES
        require(device.GetTextureCount()==2&&device.GetTextureResourceInfo(&texture)->m_Name==name,"File helpers did not use native resource metadata");
#endif
        device.Destroy(&clone);device.Destroy(&texture);
    }
    for(const char* name:{"short.dds","truncated.dds","bad-levels.dds","short.tga","short.bmp","absent.dds"}){C_Texture t;require(!CreateTexture2DFromFile(name,&t)&&!t.IsCreated(),"Malformed/missing image produced a GPU texture");}
}
static void programs(C_Device& device,const std::filesystem::path& root){
    auto immediate=device.GetContextNoCheckNoTrack();
    write(root/"vertex.bmsl",shader(true));write(root/"pixel.bmsl",shader(false));
    Resource<Display::C_VertexShader> ownedVertex(device);Resource<Display::C_PixelShader> ownedPixel(device);auto& vs=ownedVertex.value;auto& ps=ownedPixel.value;
    require(CreateVertexShaderFromFile("vertex",0,&vs)&&CreatePixelShaderFromFile("pixel",0,&ps),"Original shader file helpers failed to create native programs");
    Display::C_VertexElement elements[]={Display::C_VertexElement(0,0,Display::E_VertexElementType_F32_3,Display::E_VertexElementUsage_Position,0),VERTEXELEMENT_END()};Resource<Display::C_VertexDeclaration> ownedDeclaration(device);auto& declaration=ownedDeclaration.value;require(device.CreateVertexDeclaration(elements,&declaration),"Upper native vertex terminator/resource declaration failed");
    Resource<Display::C_Surface> ownedTarget(device);auto& target=ownedTarget.value;require(device.CreateColourSurface(16,16,0,E_SurfaceFormat_A8R8G8B8,E_Multisample_None,nullptr,&target),"Utility render target failed");
    C_DepthState depth;depth.SetDepthEnable(ToBool2(Display::E_Bool_False));immediate.SetDepthState(depth);immediate.SetCullMode(ToCullMode2(Display::E_CullMode_None));immediate.SetColourSurface(0,&target);immediate.SetVertexDeclaration(&declaration);immediate.SetVertexShader(&vs);
    auto pixels=[&](U32 expected){immediate.SetPixelShader(&ps);immediate.BeginScene();immediate.Clear(E_Clear_Colour,0xff000000);immediate.DrawPrimitive(ToPrimitiveType2(Display::E_PrimitiveType_TriangleList),0,3);immediate.EndScene();
        auto r=target.LockRect(16);require(r.pBits,"Utility shader readback failed");for(U32 y=0;y<16;++y)for(U32 x=0;x<16;++x){U32 value;std::memcpy(&value,r.pBits+y*r.pitch+x*4,4);require(value==expected,"Reloaded helper shader produced stale/different pixels");}target.UnlockRect();};
    pixels(0xffff0000);
#if TRACK_DISPLAY2_RESOURCES
    require(device.GetVertexShaderCount()==1&&device.GetPixelShaderCount()==1,"Shader helper metadata was not registered");
    device.GetVertexShaderResourceInfo(&vs)->m_Time=GetFileTime("vertex.bmsl");device.GetPixelShaderResourceInfo(&ps)->m_Time=GetFileTime("pixel.bmsl");
#endif
    const auto oldTime=std::filesystem::last_write_time(root/"pixel.bmsl");write(root/"pixel.bmsl",shader(false,true));std::filesystem::last_write_time(root/"pixel.bmsl",oldTime+std::chrono::seconds(2));
#if TRACK_DISPLAY2_RESOURCES
    RefreshShaders(false,true);
#else
    require(CreatePixelShaderFromFile("pixel",0,&ps),"Speed explicit shader reload failed");
#endif
    pixels(0xff00ff00);
    require(!CreatePixelShaderFromFile("absent",0,&ps)&&ps.IsCreated(),"Missing source file destroyed the current program");
    C_PixelShader ignored;const auto bytes=shader(false);require(CreatePixelShaderFromFunc("untracked",E_Flags_DoNotTrack,&ignored,bytes.size(),bytes.data()),"DoNotTrack shader helper failed");device.Destroy(&ignored);
    device.Destroy(&target);device.Destroy(&declaration);device.Destroy(&ps);device.Destroy(&vs);
}
int main(int argc,char** argv){try{
    Memory::Tracker::Instance().Initialize("native display helper test",0,0,true);
    arena();
    require(argc==2,"Image fixture directory missing");const auto root=std::filesystem::path(argv[1]);auto& fs=Sys::C_FileSystem::Instance();Sys::SetDefaultFileSystem(&fs);require(fs.SetWorkingDirectory(root.string().c_str()),"Native filesystem setup failed");
    C_DeviceManager manager;C_DeviceManagerParams mp;require(CreateDeviceManager(&manager,mp),"Native device manager failed");C_DeviceParams params;SetupDefaultDeviceParams(&manager,params);C_Device device;require(CreateDevice(&manager,&device,params),"Native engine device failed");Device(&device);
    C_Surface unsupportedSurface;C_Texture unsupportedTexture;
    require(device.CreateSurface(16,16,E_Flags_DepthStencil,E_SurfaceFormat_D24S8,E_Multisample_None,nullptr,&unsupportedSurface)&&!unsupportedSurface.LockRect(16).pBits,"Native depth/stencil surface failed or exposed an invented packed CPU layout");device.Destroy(&unsupportedSurface);
    require(device.CreateTexture2D(16,16,1,E_Flags_DepthStencil,E_TextureFormat_D24S8,&unsupportedTexture)&&!unsupportedTexture.LockRect2D(0).pBits,"Native opaque depth/stencil texture failed or exposed fabricated CPU bytes");device.Destroy(&unsupportedTexture);
    images(device);programs(device,root);require(device.WaitIdle(),"Helper GPU completion failed");
#if TRACK_DISPLAY2_RESOURCES
    require(device.GetTextureCount()==0&&device.GetVertexShaderCount()==0&&device.GetPixelShaderCount()==0&&device.GetSurfaceCount()==0,"Resource helpers leaked tracked allocations");
#endif
    DestroyDevice(&manager,&device);DestroyDeviceManager(&manager);Sys::SetDefaultFileSystem(nullptr);
    std::cout<<"Original Display2 helpers: DDS/BC mip tails, BMP/TGA orientation/pitch, GPU clone, shader files/reload and resource lifecycle PASS\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
