// [Blur2PC][macOS] GPU object lifetime is distinct from the retired CPU wrapper.
#include "../../code/mercury/library/graphics/display2/source/device/metal/MetalNative.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
using namespace Display2;
static void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
static void gatedRetirement(id<MTLDevice> device,bool retained,bool view,bool unwind,bool releaseCommand=false){@autoreleasepool{
    constexpr U32 width=16,height=4,pitch=256;
    auto queue=[device newCommandQueue];auto event=[device newSharedEvent];
    require(queue&&event,"Retirement gate device resources unavailable");
    auto surface=std::make_shared<Metal::Surface>();
    auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:width height:height mipmapped:NO];
    descriptor.storageMode=MTLStorageModePrivate;descriptor.usage=MTLTextureUsageShaderRead|MTLTextureUsagePixelFormatView;
    id<MTLTexture> parent=[device newTextureWithDescriptor:descriptor];require(bool(parent),"Retirement texture allocation failed");
    surface->texture=view?[parent newTextureViewWithPixelFormat:MTLPixelFormatBGRA8Unorm]:parent;
    surface->staging=[device newBufferWithLength:pitch*height options:MTLResourceStorageModeShared];
    require(surface->texture&&surface->staging,"Retirement texture/view/staging allocation failed");
    auto* input=static_cast<U32*>(surface->staging.contents);
    for(U32 y=0;y<height;++y)for(U32 x=0;x<width;++x)input[y*pitch/4+x]=0xff000000|((x*11+y)<<16)|((y*37+x)<<8)|(x+y*3);
    id<MTLBuffer> output=[device newBufferWithLength:pitch*height options:MTLResourceStorageModeShared];require(bool(output),"Retirement readback allocation failed");
    auto commandDescriptor=[MTLCommandBufferDescriptor new];commandDescriptor.retainedReferences=retained;
    id<MTLCommandBuffer> command=[queue commandBufferWithDescriptor:commandDescriptor];require(command&&command.retainedReferences==retained,"Retirement command ownership differs from requested mode");
    [command encodeWaitForEvent:event value:1];auto blit=[command blitCommandEncoder];require(bool(blit),"Retirement blit encoder failed");
    [blit copyFromBuffer:surface->staging sourceOffset:0 sourceBytesPerRow:pitch sourceBytesPerImage:pitch*height sourceSize:MTLSizeMake(width,height,1) toTexture:surface->texture destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
    [blit copyFromTexture:surface->texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(width,height,1) toBuffer:output destinationOffset:0 destinationBytesPerRow:pitch destinationBytesPerImage:pitch*height];
    [blit endEncoding];surface->pending=command;
    __weak id<MTLTexture> weakTexture=surface->texture;__weak id<MTLBuffer> weakStaging=surface->staging;
    parent=nil;blit=nil;[command commit];
    __weak id<MTLCommandBuffer> weakCommand=command;
    if(releaseCommand)command=nil;
    std::mutex mutex;std::condition_variable changed;bool done=false,caught=false;
    std::thread retire([owner=std::move(surface),&mutex,&changed,&done,&caught,unwind]()mutable{@autoreleasepool{
        if(unwind){try{auto local=std::move(owner);throw std::runtime_error("authored CPU owner failure");}catch(const std::runtime_error&){caught=true;}}
        else owner.reset();
        {std::lock_guard<std::mutex> lock(mutex);done=true;}changed.notify_one();
    }});
    bool returnedBeforeGate=false;
    {std::unique_lock<std::mutex> lock(mutex);returnedBeforeGate=changed.wait_for(lock,std::chrono::milliseconds(250),[&]{return done;});}
    const bool objectsAlive=weakTexture!=nil&&weakStaging!=nil;
    const bool wasPending=weakCommand!=nil&&weakCommand.status!=MTLCommandBufferStatusCompleted&&weakCommand.status!=MTLCommandBufferStatusError;
    // Always unblock and join the owned thread before checking a negative
    // result. The old synchronous destructor therefore exits cleanly too.
    event.signaledValue=1;retire.join();
    if(command){[command waitUntilCompleted];require(command.status==MTLCommandBufferStatusCompleted,"Retirement GPU command failed");}
    // A queue-owned command may already have been released at completion; a
    // fresh ordered barrier plus exact output proves that path without adding
    // an artificial caller reference to the retired command.
    auto barrier=[queue commandBuffer];require(bool(barrier),"Retirement completion barrier allocation failed");[barrier commit];[barrier waitUntilCompleted];require(barrier.status==MTLCommandBufferStatusCompleted,"Retirement queue completion failed");
    const auto* actual=static_cast<const U32*>(output.contents);
    for(U32 y=0;y<height;++y)for(U32 x=0;x<width;++x)require(actual[y*pitch/4+x]==(0xff000000|((x*11+y)<<16)|((y*37+x)<<8)|(x+y*3)),"Retired GPU texture/view/staging lost original pixels");
    require(objectsAlive&&wasPending,"Retirement test did not preserve objects behind an actual unsignaled GPU event");
    require(!unwind||caught,"CPU failure did not unwind the real surface owner");
    std::cout<<"retained="<<retained<<" view="<<view<<" unwind="<<unwind<<" caller_command_released="<<releaseCommand<<" returned_before_gate="<<returnedBeforeGate<<" exact_pixels=1\n";
    require(returnedBeforeGate==retained,retained?"Retained GPU resources still forced synchronous CPU-wrapper retirement":"Unretained command lost its required resource lifetime barrier");
}}
static std::vector<U8> shader(bool vertex){
    const std::string meta=vertex?R"({"parameters":{},"input_semantics":[],"reflection":{"ubos":[]}})":R"({"parameters":{},"input_semantics":[],"reflection":{"ubos":[],"textures":[{"binding":0,"type":"sampler2D"}]}})";
    const std::string common="#include <metal_stdlib>\nusing namespace metal;struct V{float4 p[[position]];float2 uv[[user(locn0)]];};\n";
    const std::string source=common+(vertex?"vertex V blur_main(uint id[[vertex_id]]){float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};return {float4(p[id],0,1),float2(.5+.5*p[id].x,.5-.5*p[id].y)};}":"fragment float4 blur_main(V i[[stage_in]],texture2d<float> t[[texture(0)]],sampler s[[sampler(0)]]){return t.sample(s,i.uv);}");
    const std::string entry="blur_main";std::vector<U8> result(40);std::memcpy(result.data(),"BMSL",4);auto word=[&](U32 p,U32 n){for(U32 i=0;i<4;++i)result[p+i]=U8(n>>(8*i));};word(4,1);word(8,vertex?0:1);word(16,40);word(20,U32(meta.size()));word(24,U32(40+meta.size()));word(28,U32(source.size()));word(32,U32(40+meta.size()+source.size()));word(36,U32(entry.size()));result.insert(result.end(),meta.begin(),meta.end());result.insert(result.end(),source.begin(),source.end());result.insert(result.end(),entry.begin(),entry.end());word(12,U32(result.size()));return result;
}
static void engineSnapshots(){
    C_MetalDevice device;require(device.Create(),"Snapshot retirement engine device failed");auto& c=device.GetContext();
    C_VertexShader vs;C_PixelShader ps;const auto v=shader(true),p=shader(false);std::string error;
    require(device.CreateVertexShader(U32(v.size()),v.data(),0,&vs,nullptr,&error),error.c_str());require(device.CreatePixelShader(U32(p.size()),p.data(),0,&ps,nullptr,&error),error.c_str());
    C_VertexFormat format;format.AddElement(0,0,E_VertexElementType_F32_3,E_VertexElementUsage_Position,0);C_VertexDeclaration declaration;require(device.CreateVertexDeclaration(format,&declaration),"Snapshot retirement declaration failed");
    C_Surface output;require(device.CreateSurface(64,8,E_Flags_Colour,E_SurfaceFormat_A8R8G8B8,E_Multisample_None,nullptr,&output),"Snapshot retirement output failed");
    c.SetVertexShader(&vs);c.SetPixelShader(&ps);c.SetVertexDeclaration(&declaration);c.SetDepthEnable(E_Bool_False);c.SetBlendEnable(E_Bool_False);c.SetCullMode(E_CullMode_None);c.SetColourWriteEnable(15);
    for(U32 pass=0;pass<12;++pass){
        C_RenderTexture2D source;require(device.CreateRenderTexture2D(64,8,1,E_Flags_Colour,E_TextureFormat_A8R8G8B8,E_SurfaceFormat_A8R8G8B8,E_Multisample_None,nullptr,&source),"Snapshot retirement source failed");
        const U32 expected=0xff000000|(pass*17<<16)|(pass*11<<8)|(pass*7);
        c.BeginScene();c.SetColourSurface(0,&source.GetSurface());c.Clear(E_Clear_Colour,expected);
        c.SetTextureWithColourSnapshot(0,&source.GetTexture());c.SetColourSurface(0,&output);c.DrawPrimitive(E_PrimitiveType_TriangleList,0,3);
        c.SetTexture(0,nullptr);device.Destroy(&source);c.EndScene(); // last transient wrappers retire during submit
        const auto map=output.LockRect(16);require(map.pBits,"Snapshot retirement engine readback failed");
        bool exact=true;for(U32 y=0;y<8;++y)for(U32 x=0;x<64;++x){U32 actual;std::memcpy(&actual,map.pBits+y*map.pitch+x*4,4);exact&=actual==expected;}
        output.UnlockRect();require(exact,"Engine snapshot retirement changed actual prior colour contents");
    }
    c.SetColourSurface(0,nullptr);c.SetVertexShader(nullptr);c.SetPixelShader(nullptr);c.SetVertexDeclaration(nullptr);device.Destroy(&output);device.Destroy(&vs);device.Destroy(&ps);device.Destroy(&declaration);
    require(device.WaitIdle(),"Snapshot retirement engine completion failed");device.Destroy();device.Destroy();
}
int main(){try{@autoreleasepool{
    engineSnapshots();id<MTLDevice> device=MTLCreateSystemDefaultDevice();require(bool(device),"Retirement native device unavailable");
    gatedRetirement(device,true,false,false);gatedRetirement(device,true,true,false);gatedRetirement(device,true,true,true);gatedRetirement(device,true,true,false,true);gatedRetirement(device,false,true,false,true);gatedRetirement(device,false,true,false);gatedRetirement(device,false,false,true);
    // No-pending and repeated device teardown remain ordinary synchronous CPU ownership.
    {auto idle=std::make_shared<Metal::Surface>();idle.reset();}
    std::cout<<"Retained texture/view/staging lifetime, nonblocking wrapper retirement, CPU-failure unwind, unretained fallback and real engine snapshot/readback passed\n";return 0;
}}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
