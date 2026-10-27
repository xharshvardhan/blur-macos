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
static void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
static std::vector<U8> bytes(const std::filesystem::path& path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
static void verify(const std::filesystem::path& path){
    NSURL* url=[NSURL fileURLWithPath:[NSString stringWithUTF8String:path.c_str()]];CGImageSourceRef source=CGImageSourceCreateWithURL((__bridge CFURLRef)url,nullptr);require(source,"Captured file is not an ImageIO image");CGImageRef image=CGImageSourceCreateImageAtIndex(source,0,nullptr);CFRelease(source);require(image,"Captured PNG could not be decoded");const auto w=CGImageGetWidth(image),h=CGImageGetHeight(image);require(w==19&&h==11,"Captured PNG dimensions differ from the actual surface");std::vector<U8> rgba(w*h*4);CGColorSpaceRef colour=CGColorSpaceCreateDeviceRGB();CGContextRef context=CGBitmapContextCreate(rgba.data(),w,h,8,w*4,colour,kCGImageAlphaPremultipliedLast|kCGBitmapByteOrder32Big);CGColorSpaceRelease(colour);require(context,"PNG verification bitmap creation failed");CGContextDrawImage(context,CGRectMake(0,0,w,h),image);CGContextRelease(context);CGImageRelease(image);
    for(U32 y=0;y<h;++y)for(U32 x=0;x<w;++x){const auto* p=rgba.data()+(y*w+x)*4;const U32 colour=x<7?0x9abcde:y<4?0x123456:0x27496b;require(p[0]==U8(colour>>16)&&p[1]==U8(colour>>8)&&p[2]==U8(colour)&&p[3]==255,"PNG RGB pixels/orientation differ from actual GPU clears");}
}
class Application:public Framework::C_Application{public:bool Quitting()const{return m_RunState==Framework::E_RunState_Quit;}};
int main(int argc,char** argv){@autoreleasepool{try{
    require(argc==2,"Expected private PNG output path");const auto path=std::filesystem::absolute(argv[1]);std::filesystem::remove(path);using namespace Framework::Mac;
    TickRunDiagnostics(0);require(!std::filesystem::exists(path),"Disabled diagnostics created an output file");
    bool invalid=false;try{ConfigureRunDiagnostics(path.string(),std::numeric_limits<double>::quiet_NaN(),-1);}catch(const std::invalid_argument&){invalid=true;}require(invalid,"Invalid diagnostic time was accepted");
    ConfigureRunDiagnostics(path.string(),2,-1);TickRunDiagnostics(1.99);require(!std::filesystem::exists(path),"Capture happened before its elapsed-time deadline");
    {Display::C_Device device;Display::C_DeviceParams config;config.m_Width=19;config.m_Height=11;config.m_Multisample=Display::E_Multisample_4;require(device.Create(config)&&device.CreateVolatile(config),"Diagnostic GPU framebuffer setup failed");require(device.BeginScene(),"Diagnostic actual GPU scene failed");auto& context=device.GetDisplay2().GetContext();context.Clear(Display2::E_Clear_Colour,0x00123456);context.SetViewport(Display2::C_Viewport(0,0,7,11));context.Clear(Display2::E_Clear_Colour,0x009abcde);context.SetViewport(Display2::C_Viewport(7,4,12,7));context.Clear(Display2::E_Clear_Colour,0x0027496b);device.EndScene();TickRunDiagnostics(2);require(std::filesystem::is_regular_file(path),"Due capture did not produce a real PNG");verify(path);const auto original=bytes(path);require(device.BeginScene(),"Diagnostic second GPU scene failed");context.SetViewport(Display2::C_Viewport(0,0,19,11));context.Clear(Display2::E_Clear_Colour,0xffffffff);device.EndScene();TickRunDiagnostics(3);require(bytes(path)==original,"Run diagnostics captured more than one frame");device.DestroyVolatile();device.Destroy();}
    ConfigureRunDiagnostics("",-1,2);{Application application;TickRunDiagnostics(1.99);require(!application.Quitting(),"Shutdown occurred before its elapsed-time deadline");TickRunDiagnostics(2);require(application.Quitting(),"Due shutdown did not call the actual application request");}
    ConfigureRunDiagnostics("",-1,-1);TickRunDiagnostics(100);std::cout<<"Native run diagnostics: disabled/default/deadline behavior, actual MSAA resolved GPU RGB/PNG round-trip, row pitch/orientation, single capture and actual application shutdown request passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}}
