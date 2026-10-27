// [Blur2PC][macOS] Real CPU/GPU texture coherence; mirrors never replace required GPU results.
#include "../../code/mercury/library/graphics/display2/source/device/metal/MetalNative.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unistd.h>
using namespace Display2;
static void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
static U32 pattern(U32 face,U32 level,U32 x,U32 y,U32 z=0){return 0xff000000|((face*31+level*17+x*7)<<16)|((y*19+z*29)<<8)|(x+y+z+level);}
static void rawCheck(Metal::Surface* s,const std::vector<U8>& expected){@autoreleasepool{
    std::lock_guard<std::mutex> lock(s->mutex);require(!s->locked&&!s->recording,"GPU check has a live CPU/recording owner");require(s->wait(),"GPU check previous command failed");
    const U32 rows=(s->height+s->block-1)/s->block;const U64 image=U64(s->pitch)*rows;
    auto output=[s->texture.device newBufferWithLength:image*s->depth options:MTLResourceStorageModeShared];
    auto command=[s->queue commandBuffer];auto blit=[command blitCommandEncoder];require(output&&command&&blit,"Independent GPU readback allocation failed");
    [blit copyFromTexture:s->texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(s->width,s->height,s->depth) toBuffer:output destinationOffset:0 destinationBytesPerRow:s->pitch destinationBytesPerImage:image];
    [blit endEncoding];[command commit];[command waitUntilCompleted];require(command.status==MTLCommandBufferStatusCompleted,"Independent GPU readback failed");
    const U32 rowBytes=((s->width+s->block-1)/s->block)*s->bytes;require(expected.size()==U64(rowBytes)*rows*s->depth,"GPU expectation dimensions disagree");
    for(U32 z=0;z<s->depth;++z)for(U32 y=0;y<rows;++y)require(!std::memcmp(static_cast<const U8*>(output.contents)+z*image+y*s->pitch,expected.data()+(z*rows+y)*rowBytes,rowBytes),"Actual GPU bytes disagree with independently authored texture rows");
}}
static std::vector<U8> bytes(U32 width,U32 height,U32 depth,U32 face,U32 level){
    std::vector<U8> result(U64(width)*height*depth*4);for(U32 z=0;z<depth;++z)for(U32 y=0;y<height;++y)for(U32 x=0;x<width;++x){U32 p=pattern(face,level,x,y,z);std::memcpy(result.data()+((z*height+y)*width+x)*4,&p,4);}return result;
}
static void write(C_Surface& surface,const std::vector<U8>& expected){auto map=surface.LockRect();require(map.pBits,"Writable texture map failed");auto* s=surface.GetGPUSurface();
    const U32 rows=(s->height+s->block-1)/s->block,rowBytes=((s->width+s->block-1)/s->block)*s->bytes;
    for(U32 z=0;z<s->depth;++z)for(U32 y=0;y<rows;++y)std::memcpy(map.pBits+(z*rows+y)*map.pitch,expected.data()+(z*rows+y)*rowBytes,rowBytes);surface.UnlockRect();
}
static void checkCpu(C_Surface& surface,const std::vector<U8>& expected){auto map=surface.LockRect(16);require(map.pBits,"Read-only texture map failed");auto* s=surface.GetGPUSurface();const U32 rows=(s->height+s->block-1)/s->block,rowBytes=((s->width+s->block-1)/s->block)*s->bytes;bool exact=true;
    for(U32 z=0;z<s->depth;++z)for(U32 y=0;y<rows;++y)exact&=!std::memcmp(map.pBits+(z*rows+y)*map.pitch,expected.data()+(z*rows+y)*rowBytes,rowBytes);surface.UnlockRect();require(exact,"Plain/read-only map did not preserve exact current bytes");
}
static void reuseProbe(){
    char directory[]="/tmp/blur-cpu-mirror-XXXXXX";require(mkdtemp(directory),"Mirror trace scratch directory failed");const std::string path=std::string(directory)+"/trace.jsonl";
    const char* old=std::getenv("BLUR_MAC_METAL_TRACE_FILE");const bool had=old;const std::string previous=old?old:"";setenv("BLUR_MAC_METAL_TRACE_FILE",path.c_str(),1);
    U32 expectedDownloads=0;
    {C_MetalDevice d;require(d.Create(),"Mirror probe device creation failed");C_Texture cube;
        require(d.CreateTextureCube(8,3,E_Flags_Dynamic,E_TextureFormat_A8R8G8B8,&cube),"Dynamic cube creation failed");
        for(U32 face=0;face<6;++face)for(U32 level=0;level<3;++level){C_Surface s;require(d.GetTextureSurface(&cube,face,level,&s),"Cube face/mip alias failed");const U32 w=cube.GetWidth(level),h=cube.GetHeight(level);auto expected=bytes(w,h,1,face,level);
            write(s,expected);++expectedDownloads;rawCheck(s.GetGPUSurface(),expected);
            auto map=cube.LockRectCube(face,level);require(map.pBits,"Repeated cube map failed");const U32 changed=0xff51a3e7;std::memcpy(map.pBits,&changed,4);std::memcpy(expected.data(),&changed,4);cube.UnlockRectCube(face,level);
            checkCpu(s,expected);rawCheck(s.GetGPUSurface(),expected);
        }
        C_Texture fixed;require(d.CreateTexture2D(5,3,1,0,E_TextureFormat_A8R8G8B8,&fixed),"Static texture creation failed");C_Surface s;require(d.GetTextureSurface(&fixed,0,0,&s),"Static alias failed");auto expected=bytes(5,3,1,0,0);write(s,expected);checkCpu(s,expected);expectedDownloads+=2;rawCheck(s.GetGPUSurface(),expected);
        d.Destroy(&fixed);d.Destroy(&cube);s=C_Surface{};require(d.WaitIdle(),"Mirror probe idle failed");d.Destroy();
    }
    std::ifstream file(path);std::string text{std::istreambuf_iterator<char>(file),{}};U32 downloads=0;std::size_t at=0;while((at=text.find("\"type\":\"download_wait\"",at))!=std::string::npos){++downloads;++at;}
    unlink(path.c_str());rmdir(directory);if(had)setenv("BLUR_MAC_METAL_TRACE_FILE",previous.c_str(),1);else unsetenv("BLUR_MAC_METAL_TRACE_FILE");
    std::cout<<"dynamic cube repeated maps plus uncached static: downloads="<<downloads<<" expected="<<expectedDownloads<<" actual_gpu_rows=1\n";
    require(downloads==expectedDownloads,"Valid dynamic CPU maps still issue repeated downloads, or static maps were incorrectly cached");
}
static void copiesAndVolume(){
    C_MetalDevice d;require(d.Create(),"Copy mirror device creation failed");auto& c=d.GetContext();
    C_Texture source,dest;require(d.CreateTextureCube(8,3,E_Flags_Dynamic,E_TextureFormat_A8R8G8B8,&source)&&d.CreateTextureCube(8,3,E_Flags_Dynamic,E_TextureFormat_A8R8G8B8,&dest),"Copy cubes failed");
    for(U32 f=0;f<6;++f)for(U32 l=0;l<3;++l){C_Surface a,b;require(d.GetTextureSurface(&source,f,l,&a)&&d.GetTextureSurface(&dest,f,l,&b),"Copy subresource aliases failed");write(a,bytes(a.GetWidth(),a.GetHeight(),1,f,l));write(b,bytes(b.GetWidth(),b.GetHeight(),1,0,0));}
    require(d.CopyTexture(&dest,&source,false),"Full cube GPU copy failed");
    for(U32 f=0;f<6;++f)for(U32 l=0;l<3;++l){C_Surface s;require(d.GetTextureSurface(&dest,f,l,&s),"Copied cube alias failed");auto expected=bytes(s.GetWidth(),s.GetHeight(),1,f,l);checkCpu(s,expected);rawCheck(s.GetGPUSurface(),expected);}
    C_Texture tail;require(d.CreateTextureCube(4,2,E_Flags_Dynamic,E_TextureFormat_A8R8G8B8,&tail),"Mip-tail cube creation failed");
    for(U32 f=0;f<6;++f)for(U32 l=0;l<2;++l){C_Surface s;require(d.GetTextureSurface(&tail,f,l,&s),"Mip-tail alias failed");write(s,bytes(s.GetWidth(),s.GetHeight(),1,0,0));}
    require(d.CopyTexture(&tail,&source,true),"Common mip-tail GPU copy failed");
    for(U32 f=0;f<6;++f)for(U32 l=0;l<2;++l){C_Surface s;require(d.GetTextureSurface(&tail,f,l,&s),"Copied mip-tail alias failed");checkCpu(s,bytes(s.GetWidth(),s.GetHeight(),1,f,l+1));}
    C_Surface a,b;require(d.GetTextureSurface(&source,3,0,&a)&&d.GetTextureSurface(&dest,1,0,&b),"Region aliases failed");auto expected=bytes(8,8,1,1,0),from=bytes(8,8,1,3,0);
    c.BeginScene();c.CopySurfaceRegion(&b,&a,2,1,3,4,1,2);c.EndScene();
    for(U32 y=0;y<4;++y)std::memcpy(expected.data()+((y+2)*8+1)*4,from.data()+((y+1)*8+2)*4,3*4);checkCpu(b,expected);rawCheck(b.GetGPUSurface(),expected);
    C_Texture volume;require(d.CreateTexture3D(5,3,2,1,E_Flags_Dynamic,E_TextureFormat_A8R8G8B8,&volume),"Dynamic volume creation failed");auto box=volume.LockBox3D(0);require(box.pBits,"Volume map failed");auto volumeBytes=bytes(5,3,2,2,0);
    for(U32 z=0;z<2;++z)for(U32 y=0;y<3;++y)std::memcpy(box.pBits+z*box.slicePitch+y*box.rowPitch,volumeBytes.data()+(z*3+y)*5*4,5*4);volume.UnlockBox3D(0);
    C_Texture volumeCopy;require(d.CreateTexture3D(5,3,2,1,E_Flags_Dynamic,E_TextureFormat_A8R8G8B8,&volumeCopy)&&d.CopyTexture(&volumeCopy,&volume,false),"Actual volume GPU copy failed");
    for(C_Texture* texture:{&volumeCopy,&volume}){auto again=texture->LockBox3D(0,16);require(again.pBits,"Repeated/copied volume map failed");bool exact=true;for(U32 z=0;z<2;++z)for(U32 y=0;y<3;++y)exact&=!std::memcmp(again.pBits+z*again.slicePitch+y*again.rowPitch,volumeBytes.data()+(z*3+y)*5*4,5*4);texture->UnlockBox3D(0);require(exact,"Actual copied volume bytes or source mirror lost slice/row stride");}
    require(!b.LockRect(0x2000).pBits,"Invalid lock flag was accepted");auto live=b.LockRect();require(live.pBits&&!b.LockRect().pBits,"Duplicate map was accepted");std::atomic<bool> rejected{false};std::thread wrong([&]{try{b.UnlockRect();}catch(const std::logic_error&){rejected=true;}});wrong.join();require(rejected,"Wrong-thread map release succeeded");b.UnlockRect();
    c.SetColourSurface(0,nullptr);c.SetDepthStencilSurface(nullptr);a=C_Surface{};b=C_Surface{};d.Destroy(&volumeCopy);d.Destroy(&volume);d.Destroy(&tail);d.Destroy(&dest);d.Destroy(&source);require(d.WaitIdle(),"Copy mirror idle failed");d.Destroy();d.Destroy();
}
static void formatsAndRenderTargets(){
    C_MetalDevice d;require(d.Create(),"Format mirror device failed");
    for(const auto format:{E_TextureFormat_R16F,E_TextureFormat_R32F,E_TextureFormat_A16B16G16R16F}){
        C_Texture t;require(d.CreateTexture2D(7,3,1,E_Flags_Dynamic,format,&t),"Dynamic float texture creation failed");C_Surface s;require(d.GetTextureSurface(&t,0,0,&s),"Float texture alias failed");const auto count=7*3*s.GetGPUSurface()->bytes;std::vector<U8> expected(count);
        for(U32 i=0;i<count;++i)expected[i]=U8((i*13+7)&255);write(s,expected);rawCheck(s.GetGPUSurface(),expected);checkCpu(s,expected);
        auto map=s.LockRect();require(map.pBits,"Float partial update failed");map.pBits[0]^=0x53;expected[0]^=0x53;s.UnlockRect();checkCpu(s,expected);rawCheck(s.GetGPUSurface(),expected);s=C_Surface{};d.Destroy(&t);
    }
    C_Texture target;require(d.CreateTexture2D(4,3,1,E_Flags_Dynamic|E_Flags_Colour,E_TextureFormat_A8R8G8B8,&target),"Dynamic render texture creation failed");C_Surface s;require(d.GetTextureSurface(&target,0,0,&s),"Render texture alias failed");write(s,bytes(4,3,1,1,0));
    auto& c=d.GetContext();c.BeginScene();c.SetColourSurface(0,&s);c.Clear(E_Clear_Colour,0xff8f4c21);c.EndScene();std::vector<U8> expected(4*3*4);for(U32 i=0;i<12;++i){U32 p=0xff8f4c21;std::memcpy(expected.data()+i*4,&p,4);}checkCpu(s,expected);rawCheck(s.GetGPUSurface(),expected);c.SetColourSurface(0,nullptr);s=C_Surface{};d.Destroy(&target);
    C_Surface depth;require(d.CreateSurface(4,3,E_Flags_DepthStencil,E_SurfaceFormat_D24S8,E_Multisample_None,nullptr,&depth),"Opaque depth creation failed");require(!depth.LockRect().pBits,"Opaque depth became CPU mappable");d.Destroy(&depth);require(d.WaitIdle(),"Format mirror idle failed");d.Destroy();
}
static void gatedReuse(bool upload,bool foreignQueue){@autoreleasepool{
    C_MetalDevice d;require(d.Create(),"Gated mirror device creation failed");C_Texture t;require(d.CreateTexture2D(8,4,1,E_Flags_Dynamic,E_TextureFormat_A8R8G8B8,&t),"Gated texture failed");C_Surface alias;require(d.GetTextureSurface(&t,0,0,&alias),"Gated alias failed");auto expected=bytes(8,4,1,1,0);write(alias,expected);auto* s=alias.GetGPUSurface();require(d.WaitIdle(),"Gated setup idle failed");
    auto queue=foreignQueue?[s->texture.device newCommandQueue]:s->queue;auto event=[s->texture.device newSharedEvent];auto output=[s->texture.device newBufferWithLength:s->pitch*s->height options:MTLResourceStorageModeShared];auto command=[queue commandBuffer];require(queue&&event&&output&&command,"GPU gate allocation failed");
    [command encodeWaitForEvent:event value:1];auto blit=[command blitCommandEncoder];require(bool(blit),"GPU gate blit failed");
    if(upload)[blit copyFromBuffer:s->staging sourceOffset:0 sourceBytesPerRow:s->pitch sourceBytesPerImage:s->pitch*s->height sourceSize:MTLSizeMake(8,4,1) toTexture:s->texture destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
    [blit copyFromTexture:s->texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(8,4,1) toBuffer:output destinationOffset:0 destinationBytesPerRow:s->pitch destinationBytesPerImage:s->pitch*s->height];[blit endEncoding];
    {std::lock_guard<std::mutex> lock(s->mutex);s->pending=command;}[command commit];
    std::mutex mutex;std::condition_variable changed;bool started=false,done=false,correct=false;std::exception_ptr failure;
    std::thread worker([&]{@autoreleasepool{try{{std::lock_guard<std::mutex> lock(mutex);started=true;}changed.notify_all();auto map=alias.LockRect();require(map.pBits,"Gated map failed");correct=!std::memcmp(map.pBits,expected.data(),8*4);U32 pixel=0xff927153;std::memcpy(map.pBits,&pixel,4);alias.UnlockRect();}catch(...){failure=std::current_exception();}{std::lock_guard<std::mutex> lock(mutex);done=true;}changed.notify_all();}});
    bool premature;{std::unique_lock<std::mutex> lock(mutex);changed.wait(lock,[&]{return started;});premature=changed.wait_for(lock,std::chrono::milliseconds(100),[&]{return done;});}
    event.signaledValue=1;worker.join();[command waitUntilCompleted];require(!premature,"Cached map bypassed the original pending GPU ownership wait");if(failure)std::rethrow_exception(failure);require(correct&&command.status==MTLCommandBufferStatusCompleted,"Gated pending map lost original bytes/command status");
    for(U32 y=0;y<4;++y)require(!std::memcmp(static_cast<U8*>(output.contents)+y*s->pitch,expected.data()+y*8*4,8*4),"CPU map overwrote bytes still owned by the gated GPU operation");U32 pixel=0xff927153;std::memcpy(expected.data(),&pixel,4);rawCheck(s,expected);checkCpu(alias,expected);
    alias=C_Surface{};d.Destroy(&t);require(d.WaitIdle(),"Gated teardown idle failed");d.Destroy();
}}
int main(){try{@autoreleasepool{reuseProbe();copiesAndVolume();formatsAndRenderTargets();gatedReuse(true,false);gatedReuse(false,true);std::cout<<"Dynamic CPU mirrors, exact GPU bytes, partial/full/tail copies, cube/volume aliases and unchanged pending-upload/reader barriers PASS\n";return 0;}}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
