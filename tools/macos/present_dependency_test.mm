// [Blur2PC][macOS] Actual gated presentation plus deterministic command failures.
#include "../../code/mercury/library/graphics/display2/source/device/metal/MetalNative.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace Display2;
@interface BlurDependencyLayer:CAMetalLayer
@property(strong) id<MTLTexture> captured;
@end
@implementation BlurDependencyLayer
-(id<CAMetalDrawable>)nextDrawable {auto drawable=[super nextDrawable];self.captured=drawable.texture;return drawable;}
@end
// An authored command object exercises deterministic terminal errors without
// deliberately faulting a GPU. No production path creates this object.
@interface BlurDependencyCommand:NSObject
@property MTLCommandBufferStatus status;
@property MTLCommandBufferStatus terminalStatus;
@property BOOL retainedReferences;
@property(strong) id<MTLCommandQueue> commandQueue;
@property NSUInteger waits;
-(void)waitUntilCompleted;
@end
@implementation BlurDependencyCommand
-(void)waitUntilCompleted {++_waits;_status=_terminalStatus;}
@end
static void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
static BlurDependencyCommand* authored(MTLCommandBufferStatus terminal){
    auto command=[BlurDependencyCommand new];command.status=MTLCommandBufferStatusCommitted;
    command.terminalStatus=terminal;command.retainedReferences=YES;return command;
}
static id<MTLCommandBuffer> command(BlurDependencyCommand* value){return (id<MTLCommandBuffer>)value;}
static void deterministicErrors(){@autoreleasepool{
    for(bool producerError:{false,true}){
        __weak BlurDependencyCommand* weakProducer;__weak BlurDependencyCommand* weakConsumer;
        std::shared_ptr<Metal::PresentDependency> dependency;
        @autoreleasepool {
        auto producer=authored(producerError?MTLCommandBufferStatusError:MTLCommandBufferStatusCompleted);
        auto consumer=authored(producerError?MTLCommandBufferStatusCompleted:MTLCommandBufferStatusError);
        dependency=std::make_shared<Metal::PresentDependency>(command(producer),command(consumer));
        Metal::PresentRetirement deviceSlot;deviceSlot.outstanding=dependency;
        Metal::Surface alias;alias.presentation=dependency;
        require(!alias.wait()&&!alias.wait(),"Alias lost a cached producer/consumer failure");
        require(producer.waits==1&&consumer.waits==1,"Failure did not retire both commands exactly once");
        require(!deviceSlot.Retire(Metal::Trace::Phase::DeviceIdleWait,&deviceSlot)&&!deviceSlot.outstanding,
                "Device did not see the Surface-retired failure");
        require(!deviceSlot.Retire(Metal::Trace::Phase::DeviceIdleWait,&deviceSlot),"Device failure was not sticky");
        weakProducer=producer;weakConsumer=consumer;producer=nil;consumer=nil;
        }
        require(!weakProducer&&!weakConsumer,"Terminal dependency retained GPU command objects");
    }
    auto producer=authored(MTLCommandBufferStatusCompleted),consumer=authored(MTLCommandBufferStatusCompleted);
    auto dependency=std::make_shared<Metal::PresentDependency>(command(producer),command(consumer));
    Metal::PresentRetirement slot;slot.outstanding=dependency;
    require(slot.Retire(Metal::Trace::Phase::DeviceIdleWait,&slot)&&slot.Retire(Metal::Trace::Phase::DeviceIdleWait,&slot),"Successful retirement did not remain successful");
    require(producer.waits==1&&consumer.waits==1,"Repeated retirement re-waited completed work");
    // Queue identity belongs to the last submitted command, not allocation metadata.
    id<MTLCommandQueue> queueA=(id<MTLCommandQueue>)[NSObject new],queueB=(id<MTLCommandQueue>)[NSObject new];
    producer.commandQueue=queueA;producer.status=MTLCommandBufferStatusCommitted;
    require(Metal::PresentDependency::CanOverlap(command(producer),queueA),"Committed retained same-queue producer rejected");
    require(!Metal::PresentDependency::CanOverlap(command(producer),queueB),"Different queue producer accepted");
    producer.retainedReferences=NO;require(!Metal::PresentDependency::CanOverlap(command(producer),queueA),"Unretained producer accepted");producer.retainedReferences=YES;
    for(auto status:{MTLCommandBufferStatusNotEnqueued,MTLCommandBufferStatusEnqueued,MTLCommandBufferStatusError}){
        producer.status=status;require(!Metal::PresentDependency::CanOverlap(command(producer),queueA),"Unsubmitted/error producer accepted");
    }
}}
// Every owned worker unblocks its event and joins even when a negative control
// fails, so an old synchronous Present never strands the fixture or device.
class Operation {
public:
    Operation(id<MTLSharedEvent> unblock,std::function<bool()> action):m_Event(unblock),m_Thread([this,action]{@autoreleasepool{
        try{m_Result=action();}catch(...){m_Error=std::current_exception();}
        {std::lock_guard<std::mutex> lock(m_Mutex);m_Done=true;}m_Changed.notify_one();
    }}){}
    ~Operation(){m_Event.signaledValue=1;if(m_Thread.joinable())m_Thread.join();}
    bool Finished(){std::unique_lock<std::mutex> lock(m_Mutex);return m_Changed.wait_for(lock,std::chrono::milliseconds(300),[&]{return m_Done;});}
    bool Join(){if(m_Thread.joinable())m_Thread.join();if(m_Error)std::rethrow_exception(m_Error);return m_Result;}
private:
    id<MTLSharedEvent> m_Event;std::mutex m_Mutex;std::condition_variable m_Changed;
    bool m_Done=false,m_Result=false;std::exception_ptr m_Error;std::thread m_Thread;
};
constexpr U32 width=64,height=8,pitch=256,colour=0xff254769;
static BlurDependencyLayer* attach(C_MetalDevice& device){
    auto layer=[BlurDependencyLayer layer];layer.bounds=CGRectMake(0,0,width,height);
    require(device.SetPresentationLayer((__bridge void*)layer,width,height,false),"Presentation layer attachment failed");return layer;
}
static void allocate(C_MetalDevice& device,C_Surface& surface){require(device.CreateSurface(width,height,E_Flags_Colour,E_SurfaceFormat_A8R8G8B8,E_Multisample_None,nullptr,&surface),"Presentation surface allocation failed");}
struct Gate {
    id<MTLSharedEvent> event;id<MTLCommandBuffer> producer;
    __weak id<MTLBuffer> staging;__weak id<MTLTexture> texture;
};
static Gate upload(C_Surface& surface,id<MTLCommandQueue> queue,bool retained=true){
    auto* native=surface.GetGPUSurface();auto event=[native->texture.device newSharedEvent];
    auto staging=[native->texture.device newBufferWithLength:pitch*height options:MTLResourceStorageModeShared];
    require(event&&staging,"Gated producer resources unavailable");
    for(U32 i=0;i<width*height;++i)static_cast<U32*>(staging.contents)[i]=colour;
    auto descriptor=[MTLCommandBufferDescriptor new];descriptor.retainedReferences=retained;
    auto producer=[queue commandBufferWithDescriptor:descriptor];require(producer&&producer.retainedReferences==retained,"Gated producer ownership mode differs");
    [producer encodeWaitForEvent:event value:1];auto blit=[producer blitCommandEncoder];require(bool(blit),"Gated producer encoder failed");
    [blit copyFromBuffer:staging sourceOffset:0 sourceBytesPerRow:pitch sourceBytesPerImage:pitch*height sourceSize:MTLSizeMake(width,height,1) toTexture:native->texture destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
    [blit endEncoding];[producer commit];native->pending=producer;
    // Unretained fallback needs the original Surface's staging lifetime barrier.
    if(!retained)native->staging=staging;
    return {event,producer,staging,native->texture};
}
static void drawablePixels(BlurDependencyLayer* layer){
    auto texture=layer.captured;require(bool(texture),"Actual presentation drawable missing");
    auto output=[texture.device newBufferWithLength:pitch*height options:MTLResourceStorageModeShared];auto queue=[texture.device newCommandQueue];
    auto command=[queue commandBuffer];auto blit=[command blitCommandEncoder];require(output&&command&&blit,"Drawable readback allocation failed");
    [blit copyFromTexture:texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(width,height,1) toBuffer:output destinationOffset:0 destinationBytesPerRow:pitch destinationBytesPerImage:pitch*height];
    [blit endEncoding];[command commit];[command waitUntilCompleted];require(command.status==MTLCommandBufferStatusCompleted,"Drawable readback command failed");
    for(U32 i=0;i<width*height;++i)require(static_cast<const U32*>(output.contents)[i]==colour,"Gated presentation lost producer pixels");
}
static void sameQueue(const char* mode){@autoreleasepool{
    C_MetalDevice device;require(device.Create(),"Same-queue device creation failed");auto layer=attach(device);
    C_Surface surface,other;allocate(device,surface);
    if(!std::strcmp(mode,"cap")){
        // B must have no A dependency; otherwise Surface alias waiting could
        // hide a regression in the device-wide outstanding-present cap.
        allocate(device,other);auto map=other.LockRect();require(bool(map.pBits),"Second source initialization failed");
        for(U32 y=0;y<height;++y)for(U32 x=0;x<width;++x)std::memcpy(map.pBits+y*map.pitch+x*4,&colour,4);
        other.UnlockRect();require(device.WaitIdle(),"Second source setup completion failed");
    }
    auto* native=surface.GetGPUSurface();auto gate=upload(surface,native->queue);
    // Allocation-queue metadata may differ from the last producer's real queue.
    auto otherQueue=[native->texture.device newCommandQueue];native->queue=otherQueue;
    Operation first(gate.event,[&]{return device.PresentSurface(&surface);});
    const bool returned=first.Finished();if(!returned)gate.event.signaledValue=1;
    const bool submitted=first.Join();require(returned&&submitted,"Same-queue retained Present CPU-waited its gated producer");
    require(gate.producer.status!=MTLCommandBufferStatusCompleted&&native->presentation&&gate.staging&&gate.texture,"First Present did not retain a genuinely gated producer");
    if(!std::strcmp(mode,"cap")){
        // Invalid source must neither consume the slot nor add a presentation.
        require(device.SetPresentationLayer((__bridge void*)layer,width/2,height,false),"Temporary resize failed");
        require(!device.PresentSurface(&surface)&&device.GetSubmittedPresentCount()==1,"Invalid source advanced the outstanding present");
        require(device.SetPresentationLayer((__bridge void*)layer,width,height,false),"Restore layer failed");
        Operation second(gate.event,[&]{return device.PresentSurface(&other);});const bool early=second.Finished();
        gate.event.signaledValue=1;const bool ok=second.Join();
        require(!early&&ok&&device.GetSubmittedPresentCount()==2,"Second Present failed the one-outstanding cap");
    }else if(!std::strcmp(mode,"alias")){
        auto alias=surface;Operation lock(gate.event,[&]{auto map=alias.LockRect(16);if(!map.pBits)return false;
            bool exact=true;for(U32 y=0;y<height;++y)for(U32 x=0;x<width;++x){U32 actual;std::memcpy(&actual,map.pBits+y*map.pitch+x*4,4);exact&=actual==colour;}alias.UnlockRect();return exact;});
        const bool early=lock.Finished();gate.event.signaledValue=1;const bool exact=lock.Join();
        require(!early&&exact,"Surface alias failed to wait/read the real producer bytes");
    }else{
        gate.producer=nil;Operation retire(gate.event,[&]{device.Destroy(&surface);return true;});
        const bool early=retire.Finished();const bool alive=gate.staging&&gate.texture;
        gate.event.signaledValue=1;const bool ok=retire.Join();
        require(early&&ok&&alive,"CPU-wrapper retirement blocked or lost resources before presentation completed");
    }
    require(device.WaitIdle(),"Same-queue idle retirement failed");drawablePixels(layer);device.Destroy();device.Destroy();
    std::cout<<"gated_same_queue="<<mode<<" first_present_nonblocking=1 exact_pixels=1 clean_teardown=1\n";
}}
static void fallback(bool crossQueue){@autoreleasepool{
    C_MetalDevice device;require(device.Create(),"Fallback device creation failed");auto layer=attach(device);C_Surface surface;allocate(device,surface);
    auto* native=surface.GetGPUSurface();auto queue=crossQueue?[native->texture.device newCommandQueue]:native->queue;
    auto gate=upload(surface,queue,crossQueue); // same-queue case deliberately unretained
    Operation present(gate.event,[&]{return device.PresentSurface(&surface);});const bool early=present.Finished();
    gate.event.signaledValue=1;const bool ok=present.Join();
    require(!early&&ok&&device.GetSubmittedPresentCount()==1,"Cross-queue/unretained producer lost synchronous fallback");
    require(device.WaitIdle(),"Fallback idle retirement failed");drawablePixels(layer);device.Destroy();device.Destroy();
    std::cout<<"cross_queue="<<crossQueue<<" synchronous_fallback=1 exact_pixels=1 clean_teardown=1\n";
}}
static void rejectedAndSticky(){@autoreleasepool{
    C_MetalDevice device;require(device.Create(),"Error-path device creation failed");auto layer=attach(device);C_Surface surface;allocate(device,surface);auto* native=surface.GetGPUSurface();
    auto unsubmitted=[native->queue commandBuffer];native->pending=unsubmitted;
    require(!device.PresentSurface(&surface)&&device.GetSubmittedPresentCount()==0,"Uncommitted producer was presented");
    require(native->pending==unsubmitted,"Uncommitted rejection replaced producer identity");native->pending=nil;
    require(device.WaitIdle(),"Uncommitted refusal incorrectly poisoned a healthy device");
    auto failed=authored(MTLCommandBufferStatusError);failed.status=MTLCommandBufferStatusError;native->pending=command(failed);
    require(!device.PresentSurface(&surface)&&failed.waits==1,"Completed-error predecessor was hidden by presentation");
    require(!device.WaitIdle()&&!device.WaitIdle()&&!device.PresentSurface(&surface),"Device did not retain predecessor failure across idle/present");
    require(device.GetSubmittedPresentCount()==0,"Rejected/error presentation changed submission count");
    device.Destroy();device.Destroy();
}}
static void cachedFailureDevice(){@autoreleasepool{
    for(bool producerError:{false,true}){
        C_MetalDevice device;require(device.Create(),"Cached-error device creation failed");auto layer=attach(device);
        C_Surface surface;allocate(device,surface);auto alias=surface;
        auto producer=authored(producerError?MTLCommandBufferStatusError:MTLCommandBufferStatusCompleted);
        auto consumer=authored(producerError?MTLCommandBufferStatusCompleted:MTLCommandBufferStatusError);
        surface.GetGPUSurface()->presentation=std::make_shared<Metal::PresentDependency>(command(producer),command(consumer));
        require(!alias.LockRect(16).pBits&&!alias.LockRect(16).pBits,"Alias CPU lock accepted a failed predecessor/consumer");
        require(!device.PresentSurface(&surface)&&!device.WaitIdle()&&!device.WaitIdle(),"Public device lost an alias-retired dependency failure");
        require(producer.waits==1&&consumer.waits==1&&device.GetSubmittedPresentCount()==0,"Cached failure changed wait/submission counts");
        device.Destroy();device.Destroy();
    }
}}
int main(){try{@autoreleasepool{
    deterministicErrors();sameQueue("cap");sameQueue("alias");sameQueue("retire");fallback(true);fallback(false);rejectedAndSticky();cachedFailureDevice();
    std::cout<<"Bounded present overlap: deterministic producer/consumer errors, cached alias/device failure, command release, real shared-event gates, cap one, alias CPU read, retained lifetime, queue/unretained/uncommitted fallbacks PASS\n";return 0;
}}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
