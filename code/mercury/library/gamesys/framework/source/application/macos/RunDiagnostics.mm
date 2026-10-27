// [Blur2PC][macOS] Read the actual completed Display framebuffer and encode its RGB with ImageIO.
#include <framework/application/macos/RunDiagnostics.h>
#include <framework/application/macos/ApplicationMac.h>
#include <display/device/Device.h>
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <vector>
#include <chrono>
#include <fstream>
namespace Framework::Mac {
namespace {
struct Diagnostics {
    std::string pngPath;
    double captureAfter=-1,quitAfter=-1;
    bool captureAttempted=false,quitAttempted=false;
    bool framesEnabled=false,framesStarted=false,counterAvailable=false;
    double windowStart=0,lastFrame=0,maxInterval=0;
    U64 callbacks=0,basePresents=0,lastPresents=0;
    const Display::C_Device* counterOwner=nullptr;
    // [Blur2PC] Reserve before Run; never allocate or write files in a frame.
    std::string tracePath;
    std::vector<NativeFrameTiming> trace;
    std::uint64_t traceDropped=0,traceLastEnd=0;
    bool traceFinished=false;
} diagnostics;
constexpr std::size_t FrameTraceCapacity=65536;
// Tick is invoked once after actual OnRender/endRender. This wall-clock
// observation is independent of the fixed simulation-tick timer. The native
// counter measures submitted presents, not compositor/displayed GPU frames.
void ObserveFrame(double elapsed){
    bool available=Display::DeviceExists()&&Display::Device()->IsCreated();
    const auto* owner=available?Display::Device():nullptr;
    const U64 presents=available?Display::Device()->GetDisplay2().GetSubmittedPresentCount():0;
    auto restart=[&]{diagnostics.framesStarted=true;diagnostics.windowStart=diagnostics.lastFrame=elapsed;diagnostics.maxInterval=0;diagnostics.callbacks=0;diagnostics.counterAvailable=available;diagnostics.counterOwner=owner;diagnostics.basePresents=diagnostics.lastPresents=presents;};
    if(!diagnostics.framesStarted||elapsed<diagnostics.lastFrame||available!=diagnostics.counterAvailable||owner!=diagnostics.counterOwner||presents<diagnostics.lastPresents){restart();return;}
    const double interval=elapsed-diagnostics.lastFrame;
    diagnostics.lastFrame=elapsed;diagnostics.lastPresents=presents;
    diagnostics.maxInterval=std::max(diagnostics.maxInterval,interval);++diagnostics.callbacks;
    const double window=elapsed-diagnostics.windowStart;
    if(window<5)return;
    const U64 submitted=presents-diagnostics.basePresents;
    std::fprintf(stderr,"Native frame diagnostics: elapsed=%.6f window=%.6f callbacks=%llu callback_hz=%.3f interval_mean_ms=%.3f interval_max_ms=%.3f submitted_presents=%llu submit_hz=%.3f counter_available=%u\n",elapsed,window,static_cast<unsigned long long>(diagnostics.callbacks),double(diagnostics.callbacks)/window,window*1000/double(diagnostics.callbacks),diagnostics.maxInterval*1000,static_cast<unsigned long long>(submitted),double(submitted)/window,unsigned(available));
    restart();
}
template<class T> struct CFHandle {
    T value=nullptr;
    explicit CFHandle(T object):value(object){}
    ~CFHandle(){if(value)CFRelease(value);}
    CFHandle(const CFHandle&)=delete;
    CFHandle& operator=(const CFHandle&)=delete;
};
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
void CaptureFrame(const std::string& filename){
    @autoreleasepool {
        check(Display::DeviceExists(),"No Display device exists at the capture deadline");
        auto* device=Display::Device();
        check(device->IsCreated()&&!device->InScene(),"Capture requires a completed Display scene");
        auto* surface=device->GetNativeColourSurface();
        check(surface&&surface->IsCreated(),"No actual game colour framebuffer exists");
        const U32 width=surface->GetWidth(),height=surface->GetHeight();
        const auto format=surface->GetFormat();
        check(width&&height&&(format==Display2::E_SurfaceFormat_A8R8G8B8||format==Display2::E_SurfaceFormat_A8B8G8R8),"Capture requires an actual 8-bit BGRA/RGBA framebuffer");
        std::vector<U8> rgba(std::size_t(width)*height*4);
        const auto lock=surface->LockRect(16); // Synchronizes real GPU work; never uploads staging bytes.
        check(lock.pBits,"Completed framebuffer GPU readback failed");
        if(lock.pitch<U64(width)*4){surface->UnlockRect();throw std::runtime_error("Framebuffer readback pitch is shorter than its pixels");}
        for(U32 y=0;y<height;++y){
            const U8* source=lock.pBits+U64(y)*lock.pitch;U8* destination=rgba.data()+U64(y)*width*4;
            if(format==Display2::E_SurfaceFormat_A8R8G8B8)for(U32 x=0;x<width;++x){destination[x*4]=source[x*4+2];destination[x*4+1]=source[x*4+1];destination[x*4+2]=source[x*4];destination[x*4+3]=source[x*4+3];}
            else std::memcpy(destination,source,std::size_t(width)*4);
        }
        surface->UnlockRect();
        const std::filesystem::path path(filename);if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());
        CFHandle<CFDataRef> data(CFDataCreate(kCFAllocatorDefault,rgba.data(),CFIndex(rgba.size())));check(data.value,"PNG pixel allocation failed");
        CFHandle<CGDataProviderRef> provider(CGDataProviderCreateWithCFData(data.value));check(provider.value,"PNG pixel provider allocation failed");
        CFHandle<CGColorSpaceRef> colour(CGColorSpaceCreateDeviceRGB());check(colour.value,"PNG colour space allocation failed");
        // Native windows present opaque RGB. The render target's unused alpha
        // channel is not window transparency and must not hide real RGB pixels.
        CFHandle<CGImageRef> image(CGImageCreate(width,height,8,32,std::size_t(width)*4,colour.value,kCGImageAlphaNoneSkipLast,provider.value,nullptr,false,kCGRenderingIntentDefault));check(image.value,"PNG image creation failed");
        NSString* string=[NSString stringWithUTF8String:path.c_str()];check(string,"PNG path is not valid UTF-8");NSURL* url=[NSURL fileURLWithPath:string];
        CFHandle<CGImageDestinationRef> destination(CGImageDestinationCreateWithURL((__bridge CFURLRef)url,CFSTR("public.png"),1,nullptr));check(destination.value,"PNG file could not be opened for writing");
        CGImageDestinationAddImage(destination.value,image.value,nullptr);check(CGImageDestinationFinalize(destination.value),"PNG encoding/write failed");
        std::fprintf(stderr,"Native run capture saved: %s (%u x %u, actual Display RGB)\n",path.c_str(),width,height);
    }
}
}
void ConfigureRunDiagnostics(const std::string& pngPath,double captureAfterSeconds,double quitAfterSeconds){
    if(!std::isfinite(captureAfterSeconds)||!std::isfinite(quitAfterSeconds))throw std::invalid_argument("Native run diagnostic deadlines must be finite");
    diagnostics={pngPath,captureAfterSeconds,quitAfterSeconds,false,false};
    const char* frames=std::getenv("BLUR_MAC_FRAME_DIAGNOSTICS");
    diagnostics.framesEnabled=frames&&std::strcmp(frames,"1")==0;
    const char* trace=std::getenv("BLUR_MAC_FRAME_TRACE_FILE");
    if(trace&&*trace){
        if(!std::filesystem::path(trace).is_absolute())throw std::invalid_argument("Native frame trace requires an absolute output path");
        diagnostics.tracePath=trace;diagnostics.trace.reserve(FrameTraceCapacity);
    }
}
bool NativeFrameTraceEnabled(){return !diagnostics.tracePath.empty()&&!diagnostics.traceFinished;}
std::uint64_t NativeFrameClockNS(){return std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
void RecordNativeFrameTiming(const NativeFrameTiming& timing){
    if(!NativeFrameTraceEnabled())return;
    if(!(timing.beginNS>=diagnostics.traceLastEnd&&timing.pollEndNS>=timing.beginNS&&timing.updateEndNS>=timing.pollEndNS&&timing.renderEndNS>=timing.updateEndNS&&timing.diagnosticsEndNS>=timing.renderEndNS&&timing.simulationTicks<=4))
        throw std::invalid_argument("Native frame trace has unordered phases or invalid simulation ticks");
    diagnostics.traceLastEnd=timing.diagnosticsEndNS;
    if(diagnostics.trace.size()<FrameTraceCapacity)diagnostics.trace.push_back(timing);
    else ++diagnostics.traceDropped;
}
void FinishRunDiagnostics(){
    if(!NativeFrameTraceEnabled())return;
    std::ofstream output(diagnostics.tracePath,std::ios::binary|std::ios::trunc);
    if(!output)throw std::runtime_error("Native frame trace output could not be opened");
    output<<"{\"kind\":\"header\",\"version\":1,\"clock\":\"std::chrono::steady_clock_ns\",\"capacity\":"<<FrameTraceCapacity<<"}\n";
    for(const auto& frame:diagnostics.trace)
        output<<"{\"kind\":\"frame\",\"steady_ns\":"<<frame.beginNS<<",\"poll_end_ns\":"<<frame.pollEndNS<<",\"update_end_ns\":"<<frame.updateEndNS<<",\"render_end_ns\":"<<frame.renderEndNS<<",\"diagnostics_end_ns\":"<<frame.diagnosticsEndNS<<",\"simulation_ticks\":"<<frame.simulationTicks<<"}\n";
    output<<"{\"kind\":\"summary\",\"written\":"<<diagnostics.trace.size()<<",\"dropped\":"<<diagnostics.traceDropped<<",\"complete\":true}\n";
    output.close();
    if(!output)throw std::runtime_error("Native frame trace output write failed");
    diagnostics.traceFinished=true;
}
void TickRunDiagnostics(double elapsedSeconds){
    const bool capture=!diagnostics.captureAttempted&&!diagnostics.pngPath.empty()&&diagnostics.captureAfter>=0;
    const bool quit=!diagnostics.quitAttempted&&diagnostics.quitAfter>=0;
    if(!capture&&!quit&&!diagnostics.framesEnabled)return;
    if(!std::isfinite(elapsedSeconds)||elapsedSeconds<0)throw std::invalid_argument("Native run diagnostic elapsed time must be finite and nonnegative");
    if(diagnostics.framesEnabled)ObserveFrame(elapsedSeconds);
    if(capture&&elapsedSeconds>=diagnostics.captureAfter){
        diagnostics.captureAttempted=true;
        try{CaptureFrame(diagnostics.pngPath);}catch(const std::exception& error){std::fprintf(stderr,"Native run capture failed: %s\n",error.what());}
    }
    if(quit&&elapsedSeconds>=diagnostics.quitAfter){
        diagnostics.quitAttempted=true;
        if(auto* application=C_Application::GetInstance())application->RequestShutdown();
        else std::fprintf(stderr,"Native run shutdown failed: no actual application instance exists\n");
    }
}
}
