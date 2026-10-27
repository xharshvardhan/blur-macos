// An authored independent rendering client, not a port of a second game.
#include <native_host/Window.h>
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <exception>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static id<MTLTexture> target(id<MTLDevice> device,std::uint32_t width,std::uint32_t height){
    auto description=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:width height:height mipmapped:NO];
    description.usage=MTLTextureUsageRenderTarget;description.storageMode=MTLStorageModePrivate;
    auto texture=[device newTextureWithDescriptor:description];require(texture!=nil,"Authored render target allocation failed");return texture;
}
// Client-owned async resource transaction. It deliberately captures old pixel
// dimensions, then withholds publication while the host processes two resizes.
// The consuming client must reconcile the Ready resource before its first draw.
class PendingTarget {
    std::mutex mutex;std::condition_variable wake;bool made=false,released=false;
    id<MTLTexture> texture=nil;std::exception_ptr error;std::thread worker;
public:
    PendingTarget(id<MTLDevice> device,std::uint32_t width,std::uint32_t height):worker([this,device,width,height]{@autoreleasepool{
        id<MTLTexture> next=nil;std::exception_ptr failure;
        try{next=target(device,width,height);}catch(...){failure=std::current_exception();}
        std::unique_lock<std::mutex> lock(mutex);texture=next;error=failure;made=true;wake.notify_all();
        wake.wait(lock,[&]{return released;});
    }}){}
    ~PendingTarget(){Release();if(worker.joinable())worker.join();}
    void WaitCreated(){std::unique_lock<std::mutex> lock(mutex);require(wake.wait_for(lock,std::chrono::seconds(3),[&]{return made;}),"Async target creation deadline expired");if(error)std::rethrow_exception(error);}
    void Release(){std::lock_guard<std::mutex> lock(mutex);released=true;wake.notify_all();}
    id<MTLTexture> Take(){Release();worker.join();if(error)std::rethrow_exception(error);return texture;}
};
static void completed(id<MTLCommandBuffer> command){[command waitUntilCompleted];require(command.status==MTLCommandBufferStatusCompleted,command.error.localizedDescription.UTF8String?:"Authored Metal command failed");}

int main(int argc,char** argv){@autoreleasepool{try{
    bool test=false,show=false;double seconds=3;
    for(int i=1;i<argc;++i){const std::string argument=argv[i];if(argument=="--test"){test=true;seconds=2;}else if(argument=="--show")show=true;else throw std::runtime_error("Use --test (minimized, finite) or --show (visible, finite)");}
    require(!(test&&show),"Automated --test cannot foreground a window");
    NativeHost::Window window;NativeHost::WindowConfig config;config.width=160;config.height=90;config.title="NativeHost authored Metal scene";config.startMinimized=!show;
    std::uint32_t width=0,height=0;bool held=false;
    NativeHost::WindowCallbacks callbacks;
    callbacks.resized=[&](auto w,auto h){width=w;height=h;};
    callbacks.key=[&](const auto& event){if(!event.modifiersOnly)held=event.down;};
    callbacks.focused=[&](bool active){if(!active)held=false;};
    require(window.Create(config,callbacks),"Independent host creation failed");window.PollEvents();
    auto device=MTLCreateSystemDefaultDevice();require(device!=nil,"Metal device unavailable");
    auto queue=[device newCommandQueue];require(queue!=nil,"Metal queue unavailable");
    CAMetalLayer* layer=(__bridge CAMetalLayer*)window.MetalLayer();layer.device=device;layer.pixelFormat=MTLPixelFormatBGRA8Unorm;layer.framebufferOnly=NO;layer.allowsNextDrawableTimeout=YES;
    NSError* error=nil;
    NSString* source=@"#include <metal_stdlib>\nusing namespace metal;\nstruct V {float4 position [[position]];};\nvertex V triangle(uint id [[vertex_id]], constant float& x [[buffer(0)]]) {const float2 p[3]={float2(-.6,-.5),float2(.6,-.5),float2(0,.6)};V v;v.position=float4(p[id]+float2(x,0),0,1);return v;}\nfragment float4 colour(){return float4(1,.5,.125,1);}";
    auto library=[device newLibraryWithSource:source options:nil error:&error];require(library!=nil,error.localizedDescription.UTF8String?:"Authored shader compile failed");
    auto description=[MTLRenderPipelineDescriptor new];description.vertexFunction=[library newFunctionWithName:@"triangle"];description.fragmentFunction=[library newFunctionWithName:@"colour"];description.colorAttachments[0].pixelFormat=layer.pixelFormat;
    auto pipeline=[device newRenderPipelineStateWithDescriptor:description error:&error];require(pipeline!=nil,error.localizedDescription.UTF8String?:"Authored pipeline compile failed");
    const auto initialWidth=width,initialHeight=height;PendingTarget pending(device,width,height);pending.WaitCreated();
    NSWindow* native=(__bridge NSWindow*)window.NativeWindow();
    [native setContentSize:NSMakeSize(192,108)];window.PollEvents();
    [native setContentSize:NSMakeSize(224,126)];window.PollEvents();
    require(width==window.PixelWidth()&&height==window.PixelHeight()&&width!=initialWidth,"Host did not coalesce observed dimensions");
    id<MTLTexture> image=pending.Take();
    require(image.width==initialWidth&&image.height==initialHeight,"Async authored resource did not retain captured dimensions");
    const bool caughtUp=image.width!=width||image.height!=height;
    require(caughtUp,"Async resize fixture did not create a stale Ready resource");
    image=target(device,width,height); // Reconcile before publishing to rendering.
    const auto start=std::chrono::steady_clock::now();auto previousTime=start;
    double debt=0,current=0,previous=0;constexpr double tick=1.0/60.0;
    std::uint64_t ticks=0,frames=0;float lastX=0;id<MTLCommandBuffer> last=nil;
    bool fractional=false;bool resizedAgain=false;
    while(!window.CloseRequested()){@autoreleasepool{
        const auto now=std::chrono::steady_clock::now();const double elapsed=std::chrono::duration<double>(now-start).count();
        if(elapsed>=seconds)break;
        debt+=std::min(0.25,std::chrono::duration<double>(now-previousTime).count());previousTime=now;
        // This independent client chooses its own fixed simulation and render
        // interpolation; the host does not set engine/game timing policy.
        while(debt>=tick){previous=current;current+=tick;debt-=tick;++ticks;}
        const double alpha=debt/tick;fractional|=alpha>0.01&&alpha<0.99;
        lastX=float(std::sin(previous+(current-previous)*alpha)*0.25);
        window.PollEvents();
        if(!resizedAgain&&elapsed>0.7){[native setContentSize:NSMakeSize(256,144)];window.PollEvents();resizedAgain=true;}
        if(image.width!=width||image.height!=height)image=target(device,width,height);
        require(image.width==window.PixelWidth()&&image.height==window.PixelHeight(),"Client rendered with stale target dimensions");
        auto drawable=[layer nextDrawable];require(drawable!=nil,"Authored client could not obtain minimized drawable");
        require(drawable.texture.width==width&&drawable.texture.height==height,"Drawable pixels differ from committed client target");
        auto command=[queue commandBuffer];require(command!=nil,"Authored command allocation failed");
        auto pass=[MTLRenderPassDescriptor renderPassDescriptor];pass.colorAttachments[0].texture=image;pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;pass.colorAttachments[0].clearColor=MTLClearColorMake(.04,.06,.1,1);
        auto encoder=[command renderCommandEncoderWithDescriptor:pass];require(encoder!=nil,"Authored render encoder failed");
        [encoder setRenderPipelineState:pipeline];[encoder setVertexBytes:&lastX length:sizeof(lastX) atIndex:0];[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];[encoder endEncoding];
        auto blit=[command blitCommandEncoder];require(blit!=nil,"Authored present copy encoder failed");
        [blit copyFromTexture:image sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(width,height,1) toTexture:drawable.texture destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];[blit endEncoding];
        [command presentDrawable:drawable];[command commit];completed(command);last=command;++frames;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }}
    if(window.CloseRequested()&&!test){
        if(last)completed(last);layer.device=nil;window.Destroy();
        std::cout<<"Closed authored scene cleanly after "<<frames<<" presents\n";return 0;
    }
    require(frames>=3&&ticks>=30&&fractional&&resizedAgain,"Finite demo did not exercise real elapsed fixed ticks, interpolation and resize");
    if(last)completed(last);
    // One CPU readback after the finite scene; no captures or audio subsystem.
    const NSUInteger pitch=((NSUInteger(width)*4+255)/256)*256;
    auto pixels=[device newBufferWithLength:pitch*height options:MTLResourceStorageModeShared];require(pixels!=nil,"Readback allocation failed");
    auto read=[queue commandBuffer];auto blit=[read blitCommandEncoder];require(read!=nil&&blit!=nil,"Readback command failed");
    [blit copyFromTexture:image sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(width,height,1) toBuffer:pixels destinationOffset:0 destinationBytesPerRow:pitch destinationBytesPerImage:pitch*height];[blit endEncoding];[read commit];completed(read);
    const auto bytes=static_cast<const unsigned char*>(pixels.contents);const auto centre=bytes+(height/2)*pitch+(width/2)*4;
    require(centre[0]>=31&&centre[0]<=33&&centre[1]>=127&&centre[1]<=129&&centre[2]==255&&centre[3]==255,"Actual authored triangle centre pixel differs");
    require(bytes[0]<40&&bytes[1]<40&&bytes[2]<40&&bytes[3]==255,"Actual authored background pixel differs");
    layer.device=nil;window.Destroy();require(!window.IsCreated(),"Demo host teardown failed");
    std::cout<<"PASS installed independent Metal scene: presents="<<frames<<" fixed_ticks="<<ticks<<" interpolated=1 async_ready_caught_up="<<caughtUp<<" size="<<width<<'x'<<height<<" exact_triangle_pixels=1 audio=none held_client_state="<<held<<"\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}}
