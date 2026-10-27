// [Blur2PC][macOS] Engine surface -> actual Core Animation drawable presentation.
#include "MetalNative.h"
#import <AppKit/NSScreen.h>
#import <AppKit/NSView.h>
#import <AppKit/NSWindow.h>
#include <cmath>
namespace Display2 {
bool C_MetalDevice::SetPresentationLayer(void* native,U32 width,U32 height,bool vsync){
    @autoreleasepool {
        if(!m_State||(native&&![NSThread isMainThread]))return false;
        CAMetalLayer* layer=(__bridge CAMetalLayer*)native;
        if(layer&&(![layer isKindOfClass:[CAMetalLayer class]]||!width||!height||width>16384||height>16384))return false;
        std::lock_guard<std::mutex> guard(m_State->mutex);
        if(layer){layer.device=m_State->device;layer.pixelFormat=MTLPixelFormatBGRA8Unorm;
            // Presentation copies the engine's final render target. A blit
            // cannot target a framebuffer-only drawable.
            layer.framebufferOnly=NO;layer.drawableSize=CGSizeMake(width,height);
            layer.displaySyncEnabled=vsync;layer.allowsNextDrawableTimeout=YES;
        }
        m_State->layer=layer;m_State->minimumPresentDuration=0;return true;
    }
}
bool C_MetalDevice::SetPresentationInterval(E_PresentInterval interval){
    @autoreleasepool {
        if(!m_State)return false;
        U32 refreshes=0;
        switch(interval){case E_PresentInterval_Immediate:break;case E_PresentInterval_Default:case E_PresentInterval_One:refreshes=1;break;case E_PresentInterval_Two:refreshes=2;break;case E_PresentInterval_Three:refreshes=3;break;case E_PresentInterval_Four:refreshes=4;break;default:return false;}
        std::lock_guard<std::mutex> guard(m_State->mutex);auto layer=m_State->layer;
        if(layer&&![NSThread isMainThread])return false;
        CFTimeInterval duration=0;
        if(layer&&refreshes>1){
            // Use the screen actually owning the host layer; standalone probe
            // layers use the system's main screen. Never assume 60/120 Hz.
            NSScreen* screen=nil;id delegate=layer.delegate;
            if([delegate isKindOfClass:[NSView class]])screen=[(NSView*)delegate window].screen;
            if(!screen)screen=[NSScreen mainScreen];
            if(!screen)return false;
            const auto refresh=screen.minimumRefreshInterval;
            if(!std::isfinite(refresh)||refresh<=0)return false;
            duration=refresh*refreshes;
        }
        if(layer)layer.displaySyncEnabled=refreshes!=0;
        m_State->minimumPresentDuration=duration;return true;
    }
}
U64 C_MetalDevice::GetSubmittedPresentCount()const{if(!m_State)return 0;std::lock_guard<std::mutex> guard(m_State->mutex);return m_State->presents;}
bool C_MetalDevice::PresentSurface(const C_Surface* surface){
    @autoreleasepool {
        if(!m_State||!surface||!surface->m_Surface)return false;
        Metal::Trace::Span presentTrace(Metal::Trace::Phase::Present,surface);
        auto resource=surface->m_Surface;
        if(resource->samples>1){if(!resource->resolve)return false;resource=resource->resolve;}
        std::scoped_lock guard(m_State->mutex,resource->mutex);
        auto layer=m_State->layer;
        if(!layer||resource->locked||resource->recording||resource->texture.device!=m_State->device||
           resource->texture.pixelFormat!=MTLPixelFormatBGRA8Unorm||resource->depth!=1||
           resource->width!=layer.drawableSize.width||resource->height!=layer.drawableSize.height)return false;
        id<MTLCommandBuffer> producer;
        {Metal::Trace::Span trace(Metal::Trace::Phase::PresentResourceWait,resource.get());
            // Cap outstanding presentations at one. This preserves predecessor
            // errors and permits the next CPU update to overlap this frame's
            // rendering without retaining an unbounded chain of commands.
            if(!m_State->presentation.Retire(Metal::Trace::Phase::PresentResourceWait,resource.get()))return false;
            if(!resource->waitPresentation()){m_State->presentation.failed=true;return false;}
            if(resource->pending&&resource->pending.status<MTLCommandBufferStatusCommitted)return false;
            if(resource->texture.hazardTrackingMode==MTLHazardTrackingModeTracked&&
               Metal::PresentDependency::CanOverlap(resource->pending,m_State->queue))producer=resource->pending;
            else if(!resource->wait()){m_State->presentation.failed=true;return false;}
        }
        id<CAMetalDrawable> drawable;
        {Metal::Trace::Span trace(Metal::Trace::Phase::NextDrawable,resource.get());drawable=[layer nextDrawable];}
        if(!drawable)return false;
        auto destination=drawable.texture;
        if(destination.pixelFormat!=resource->texture.pixelFormat||destination.width!=resource->width||destination.height!=resource->height)return false;
        auto command=[m_State->queue commandBuffer];if(!command||!command.retainedReferences)return false;
        auto blit=[command blitCommandEncoder];if(!blit)return false;
        [blit copyFromTexture:resource->texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
                   sourceSize:MTLSizeMake(resource->width,resource->height,1) toTexture:destination destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
        [blit endEncoding];
        // Allocate the only dependency record before GPU submission. Both Metal
        // command buffers retain their resources; this record owns no Surface
        // or device and installs no asynchronous C++ completion callbacks.
        auto dependency=std::make_shared<Metal::PresentDependency>(producer,command);
        // Apple's drawable scheduler keeps the prior image visible for the
        // requested refresh interval without changing simulation/update time.
        if(m_State->minimumPresentDuration>0)[command presentDrawable:drawable afterMinimumDuration:m_State->minimumPresentDuration];
        else [command presentDrawable:drawable];
        {Metal::Trace::Span trace(Metal::Trace::Phase::PresentSubmit,resource.get(),(__bridge const void*)command);[command commit];}
        resource->presentation=dependency;m_State->presentation.outstanding=std::move(dependency);
        resource->pending=command;m_State->last=command;++m_State->presents;
        if(Metal::Trace::enabled.load(std::memory_order_relaxed))Metal::Trace::PresentEncodingSummary(m_State->presents);
        return true;
    }
}
}
