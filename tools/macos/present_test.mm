// [Blur2PC][macOS] Present actual engine render-target bytes to a real drawable.
#include <display2/device/MetalDevice.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <AppKit/NSScreen.h>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
@interface BlurTestLayer:CAMetalLayer
@property(strong) id<MTLTexture> captured;
@end
@implementation BlurTestLayer
-(id<CAMetalDrawable>)nextDrawable {auto drawable=[super nextDrawable];self.captured=drawable.texture;return drawable;}
@end
static void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
int main(){@autoreleasepool{try{
    Display2::C_MetalDevice device;require(device.Create(),"Metal device creation failed");
    auto layer=[BlurTestLayer layer];layer.bounds=CGRectMake(0,0,64,64);
    require(device.SetPresentationLayer((__bridge void*)layer,64,64,false),"Native layer attachment failed");
    require(layer.device&&!layer.framebufferOnly&&!layer.displaySyncEnabled,"Layer configuration lost native presentation policy");
    bool foreign=false;std::thread thread([&]{foreign=!device.SetPresentationLayer((__bridge void*)layer,64,64);});thread.join();require(foreign,"Foreign thread reconfigured a host layer");
    std::thread timingThread([&]{foreign=!device.SetPresentationInterval(Display2::E_PresentInterval_Two);});timingThread.join();require(foreign,"Foreign thread reconfigured drawable timing");
    require(!device.SetPresentationInterval(Display2::E_PresentInterval(3)),"Unknown interval bits reported applied success");
    const Display2::E_PresentInterval intervals[]={Display2::E_PresentInterval_Immediate,Display2::E_PresentInterval_Default,Display2::E_PresentInterval_One,Display2::E_PresentInterval_Two,Display2::E_PresentInterval_Three,Display2::E_PresentInterval_Four};
    Display2::C_Surface surface;for(auto samples:{Display2::E_Multisample_None,Display2::E_Multisample_2,Display2::E_Multisample_4}){device.Destroy(&surface);require(device.CreateSurface(64,64,Display2::E_Flags_Colour,Display2::E_SurfaceFormat_A8R8G8B8,samples,nullptr,&surface),"Presentation source allocation failed");
    auto& context=device.GetContext();context.SetColourSurface(0,&surface);context.SetViewport(Display2::C_Viewport(0,0,64,64));
    for(U32 frame=0;frame<6;++frame){
        context.BeginScene();require(device.SetPresentationInterval(intervals[frame]),"Original presentation interval could not be applied");require(bool(layer.displaySyncEnabled)==(frame!=0),"Original immediate/sync policy was lost");context.Clear(Display2::E_Clear_Colour,0xff123456+frame,1,0);
        require(!device.PresentSurface(&surface),"Unsubmitted scene was presented");context.EndScene();
        if(frame==0){auto lock=surface.LockRect(16);require(lock.pBits&&!device.PresentSurface(&surface),"CPU-locked target was presented");surface.UnlockRect();}
        require(device.PresentSurface(&surface)&&device.WaitIdle(),"Real Core Animation drawable presentation failed");
        auto destination=layer.captured;require(destination&&destination.width==64&&destination.height==64,"Actual drawable texture missing");
        auto staging=[layer.device newBufferWithLength:64*256 options:MTLResourceStorageModeShared];auto queue=[layer.device newCommandQueue];auto command=[queue commandBuffer];auto blit=[command blitCommandEncoder];
        [blit copyFromTexture:destination sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(64,64,1) toBuffer:staging destinationOffset:0 destinationBytesPerRow:256 destinationBytesPerImage:64*256];
        [blit endEncoding];[command commit];[command waitUntilCompleted];require(command.status==MTLCommandBufferStatusCompleted,"Drawable GPU readback failed");
        for(U32 i=0;i<64*64;++i){U32 value;std::memcpy(&value,static_cast<const U8*>(staging.contents)+i*4,4);require(value==0xff123456+frame,"Drawable bytes differ from the engine render target");}
    }
    }
    require(device.GetSubmittedPresentCount()==18,"Failed presentation changed submission count");
    require(device.SetPresentationLayer((__bridge void*)layer,32,32)&&!device.PresentSurface(&surface),"Resized layer accepted old target dimensions");
    require(device.SetPresentationLayer(nullptr,0,0)&&!device.PresentSurface(&surface),"Detached layer accepted a presentation");
    require(device.GetSubmittedPresentCount()==18,"Resize/detach failure changed presentation count");
    std::cout<<"Engine render target -> real CAMetalDrawable: eighteen single/2x/4x presents, original Immediate/Default/One/Two/Three/Four policies, exact pixels, ownership, resize/detach PASS (layer only, not a game window)\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}}
