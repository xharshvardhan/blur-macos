// [Blur2PC][macOS] Real MSAA framebuffer -> one PNG; actual application shutdown timer.
#include <framework/application/macos/RunDiagnostics.h>
#include <framework/application/macos/ApplicationMac.h>
#include <display.h>
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <unistd.h>
#include <cerrno>
static void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
static std::vector<U8> bytes(const std::filesystem::path& path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
static void verify(const std::filesystem::path& path){
    NSURL* url=[NSURL fileURLWithPath:[NSString stringWithUTF8String:path.c_str()]];CGImageSourceRef source=CGImageSourceCreateWithURL((__bridge CFURLRef)url,nullptr);require(source,"Captured file is not an ImageIO image");CGImageRef image=CGImageSourceCreateImageAtIndex(source,0,nullptr);CFRelease(source);require(image,"Captured PNG could not be decoded");const auto w=CGImageGetWidth(image),h=CGImageGetHeight(image);require(w==19&&h==11,"Captured PNG dimensions differ from the actual surface");std::vector<U8> rgba(w*h*4);CGColorSpaceRef colour=CGColorSpaceCreateDeviceRGB();CGContextRef context=CGBitmapContextCreate(rgba.data(),w,h,8,w*4,colour,kCGImageAlphaPremultipliedLast|kCGBitmapByteOrder32Big);CGColorSpaceRelease(colour);require(context,"PNG verification bitmap creation failed");CGContextDrawImage(context,CGRectMake(0,0,w,h),image);CGContextRelease(context);CGImageRelease(image);
    for(U32 y=0;y<h;++y)for(U32 x=0;x<w;++x){const auto* p=rgba.data()+(y*w+x)*4;const U32 colour=x<7?0x9abcde:y<4?0x123456:0x27496b;require(p[0]==U8(colour>>16)&&p[1]==U8(colour>>8)&&p[2]==U8(colour)&&p[3]==255,"PNG RGB pixels/orientation differ from actual GPU clears");}
}
class StderrCapture {
    FILE* file=nullptr;int saved=-1;
public:
    StderrCapture(){std::fflush(stderr);file=std::tmpfile();require(file,"Diagnostic log capture failed");saved=dup(STDERR_FILENO);if(saved<0||dup2(fileno(file),STDERR_FILENO)<0){if(saved>=0)close(saved);std::fclose(file);throw std::runtime_error("Diagnostic stderr redirect failed");}}
    ~StderrCapture(){if(saved>=0){std::fflush(stderr);dup2(saved,STDERR_FILENO);close(saved);}if(file)std::fclose(file);}
    std::string text(){
        std::fflush(stderr);
        // stderr and tmpfile share a descriptor offset through dup2, but they
        // are different FILE streams. Do not use tmpfile's cached stdio offset
        // after stderr writes; read the actual descriptor without changing it.
        const int descriptor=fileno(file);const off_t length=lseek(descriptor,0,SEEK_END);require(length>=0,"Diagnostic log length failed");
        std::string output(std::size_t(length),'\0');std::size_t offset=0;
        while(offset<output.size()){const auto count=pread(descriptor,output.data()+offset,output.size()-offset,off_t(offset));if(count<0&&errno==EINTR)continue;require(count>0,"Diagnostic log read failed");offset+=std::size_t(count);}
        return output;
    }
};
struct EnvironmentRestore {
    bool present=std::getenv("BLUR_MAC_FRAME_DIAGNOSTICS")!=nullptr;
    std::string value=present?std::getenv("BLUR_MAC_FRAME_DIAGNOSTICS"):"";
    ~EnvironmentRestore(){if(present)setenv("BLUR_MAC_FRAME_DIAGNOSTICS",value.c_str(),1);else unsetenv("BLUR_MAC_FRAME_DIAGNOSTICS");}
};
static void frameDiagnostics(){
    using namespace Framework::Mac;
    {StderrCapture capture;unsetenv("BLUR_MAC_FRAME_DIAGNOSTICS");ConfigureRunDiagnostics("",-1,-1);TickRunDiagnostics(0);TickRunDiagnostics(100);TickRunDiagnostics(std::numeric_limits<double>::quiet_NaN());require(capture.text().empty(),"Default-disabled frame diagnostics performed work or printed data");}
    setenv("BLUR_MAC_FRAME_DIAGNOSTICS","1",1);
    {StderrCapture capture;ConfigureRunDiagnostics("",-1,-1);TickRunDiagnostics(0);TickRunDiagnostics(1);TickRunDiagnostics(2);TickRunDiagnostics(4.99);require(capture.text().empty(),"Frame diagnostics reported an incomplete window");TickRunDiagnostics(5);
        double elapsed,window,hz,mean,maximum,submitHz;unsigned long long callbacks,presents;unsigned available;
        const auto text=capture.text();const int parsed=std::sscanf(text.c_str(),"Native frame diagnostics: elapsed=%lf window=%lf callbacks=%llu callback_hz=%lf interval_mean_ms=%lf interval_max_ms=%lf submitted_presents=%llu submit_hz=%lf counter_available=%u",&elapsed,&window,&callbacks,&hz,&mean,&maximum,&presents,&submitHz,&available);if(parsed!=9)throw std::runtime_error("Frame diagnostics parse count="+std::to_string(parsed)+" captured=["+text+"]");
        require(elapsed==5&&window==5&&callbacks==4&&std::abs(hz-.8)<.001&&mean==1250&&maximum==2990&&presents==0&&submitHz==0&&available==0,"Frame diagnostics did not use actual elapsed callback intervals/counter availability");}
    {StderrCapture capture;ConfigureRunDiagnostics("",-1,-1);TickRunDiagnostics(10);TickRunDiagnostics(12);TickRunDiagnostics(1);TickRunDiagnostics(5.99);require(capture.text().empty(),"Backward elapsed clock retained the old diagnostic window");TickRunDiagnostics(6);require(capture.text().find("elapsed=6.000000 window=5.000000 callbacks=2")!=std::string::npos,"Clock-reset diagnostic window has the wrong callback count");}
    {StderrCapture capture;ConfigureRunDiagnostics("",-1,-1);TickRunDiagnostics(0);TickRunDiagnostics(5);ConfigureRunDiagnostics("",-1,-1);TickRunDiagnostics(0);TickRunDiagnostics(5);const auto text=capture.text();const auto first=text.find("window=5.000000 callbacks=1");require(first!=std::string::npos&&text.find("window=5.000000 callbacks=1",first+1)!=std::string::npos,"Reconfiguration retained counters from a previous run");}
    bool invalid=false;try{TickRunDiagnostics(-1);}catch(const std::invalid_argument&){invalid=true;}require(invalid,"Enabled diagnostic negative elapsed time was accepted");invalid=false;try{TickRunDiagnostics(std::numeric_limits<double>::quiet_NaN());}catch(const std::invalid_argument&){invalid=true;}require(invalid,"Enabled diagnostic NaN elapsed time was accepted");
    unsetenv("BLUR_MAC_FRAME_DIAGNOSTICS");ConfigureRunDiagnostics("",-1,-1);
}
// [Blur2PC] Verify that collection is bounded, ordered and silent until Run ends.
static void frameTrace(const std::filesystem::path& output){
    using namespace Framework::Mac;
    struct RestoreTrace {bool present=std::getenv("BLUR_MAC_FRAME_TRACE_FILE")!=nullptr;std::string value=present?std::getenv("BLUR_MAC_FRAME_TRACE_FILE"):"";~RestoreTrace(){if(present)setenv("BLUR_MAC_FRAME_TRACE_FILE",value.c_str(),1);else unsetenv("BLUR_MAC_FRAME_TRACE_FILE");}} restore;
    std::filesystem::remove(output);unsetenv("BLUR_MAC_FRAME_TRACE_FILE");ConfigureRunDiagnostics("",-1,-1);
    require(!NativeFrameTraceEnabled(),"Default frame trace was enabled");RecordNativeFrameTiming({5,4,3,2,1,99});FinishRunDiagnostics();require(!std::filesystem::exists(output),"Disabled frame trace wrote a file");
    setenv("BLUR_MAC_FRAME_TRACE_FILE","relative-frame.jsonl",1);bool rejected=false;try{ConfigureRunDiagnostics("",-1,-1);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"Relative frame trace destination was accepted");
    setenv("BLUR_MAC_FRAME_TRACE_FILE",output.c_str(),1);ConfigureRunDiagnostics("",-1,-1);require(NativeFrameTraceEnabled()&&NativeFrameClockNS()>0,"Configured monotonic frame trace is unavailable");
    for(std::uint64_t i=0;i<65537;++i){const auto t=1000+i*100;RecordNativeFrameTiming({t,t+10,t+20,t+60,t+80,std::uint32_t(i%5)});}
    require(!std::filesystem::exists(output),"Frame trace performed file I/O during collection");
    rejected=false;try{RecordNativeFrameTiming({1,2,3,4,5,0});}catch(const std::invalid_argument&){rejected=true;}require(rejected,"Frame trace accepted clock reversal");
    rejected=false;try{RecordNativeFrameTiming({9000000,9000001,9000003,9000002,9000004,0});}catch(const std::invalid_argument&){rejected=true;}require(rejected,"Frame trace accepted unordered update/render phases");
    FinishRunDiagnostics();require(!NativeFrameTraceEnabled(),"Finished trace remained active");const auto encoded=bytes(output);const std::string text(encoded.begin(),encoded.end());
    require(text.find("\"clock\":\"std::chrono::steady_clock_ns\"")!=std::string::npos&&text.find("\"steady_ns\":1000,\"poll_end_ns\":1010,\"update_end_ns\":1020,\"render_end_ns\":1060,\"diagnostics_end_ns\":1080,\"simulation_ticks\":0")!=std::string::npos,"Frame trace lost original clock/phase values");
    require(text.find("\"written\":65536,\"dropped\":1,\"complete\":true")!=std::string::npos,"Frame trace saturation was not explicit or overwrote retained records");FinishRunDiagnostics();require(bytes(output)==encoded,"Trace flushed twice");
    unsetenv("BLUR_MAC_FRAME_TRACE_FILE");ConfigureRunDiagnostics("",-1,-1);
}
class Application:public Framework::C_Application{public:bool Quitting()const{return m_RunState==Framework::E_RunState_Quit;}};
int main(int argc,char** argv){@autoreleasepool{try{
    EnvironmentRestore environment;
    require(argc==2,"Expected private PNG output path");const auto path=std::filesystem::absolute(argv[1]);std::filesystem::remove(path);using namespace Framework::Mac;frameDiagnostics();frameTrace(path.string()+".frames.jsonl");
    TickRunDiagnostics(0);require(!std::filesystem::exists(path),"Disabled diagnostics created an output file");
    bool invalid=false;try{ConfigureRunDiagnostics(path.string(),std::numeric_limits<double>::quiet_NaN(),-1);}catch(const std::invalid_argument&){invalid=true;}require(invalid,"Invalid diagnostic time was accepted");
    ConfigureRunDiagnostics(path.string(),2,-1);TickRunDiagnostics(1.99);require(!std::filesystem::exists(path),"Capture happened before its elapsed-time deadline");
    {Display::C_Device device;Display::C_DeviceParams config;config.m_Width=19;config.m_Height=11;config.m_Multisample=Display::E_Multisample_4;require(device.Create(config)&&device.CreateVolatile(config),"Diagnostic GPU framebuffer setup failed");require(device.BeginScene(),"Diagnostic actual GPU scene failed");auto& context=device.GetDisplay2().GetContext();context.Clear(Display2::E_Clear_Colour,0x00123456);context.SetViewport(Display2::C_Viewport(0,0,7,11));context.Clear(Display2::E_Clear_Colour,0x009abcde);context.SetViewport(Display2::C_Viewport(7,4,12,7));context.Clear(Display2::E_Clear_Colour,0x0027496b);device.EndScene();TickRunDiagnostics(2);require(std::filesystem::is_regular_file(path),"Due capture did not produce a real PNG");verify(path);const auto original=bytes(path);require(device.BeginScene(),"Diagnostic second GPU scene failed");context.SetViewport(Display2::C_Viewport(0,0,19,11));context.Clear(Display2::E_Clear_Colour,0xffffffff);device.EndScene();TickRunDiagnostics(3);require(bytes(path)==original,"Run diagnostics captured more than one frame");
        setenv("BLUR_MAC_FRAME_DIAGNOSTICS","1",1);ConfigureRunDiagnostics("",-1,-1);{StderrCapture capture;TickRunDiagnostics(0);TickRunDiagnostics(5);const auto report=capture.text();require(report.find("submitted_presents=0 submit_hz=0.000 counter_available=1")!=std::string::npos,"Completed offscreen GPU work was mislabeled as submitted presentation");}unsetenv("BLUR_MAC_FRAME_DIAGNOSTICS");ConfigureRunDiagnostics("",-1,-1);
        device.DestroyVolatile();device.Destroy();}
    ConfigureRunDiagnostics("",-1,2);{Application application;TickRunDiagnostics(1.99);require(!application.Quitting(),"Shutdown occurred before its elapsed-time deadline");TickRunDiagnostics(2);require(application.Quitting(),"Due shutdown did not call the actual application request");}
    ConfigureRunDiagnostics("",-1,-1);TickRunDiagnostics(100);std::cout<<"Native run diagnostics: disabled/default/deadline behavior, actual MSAA resolved GPU RGB/PNG round-trip, row pitch/orientation, single capture, actual application shutdown request and opt-in elapsed callback/submitted-present windows passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}}
