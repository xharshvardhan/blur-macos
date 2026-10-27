// [Blur2PC][macOS] Actual shared upper C_Device lifecycle and minimized Cocoa presentation; not a game startup.
#include <core/Maths.h>
#include <system/debuglog.h>
#include <display/device/Device.h>
#include <display/view/View.h>
#include <display/view/ViewSetup.h>
#include <framework/application/macos/MacWindow.h>
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#include <iostream>
#include <stdexcept>
#include <cstring>
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){@autoreleasepool{try{
    Framework::Mac::Window host;Framework::Mac::WindowConfig config;config.width=96;config.height=64;config.title="Blur upper device verification";require(host.Create(config),"Upper device Cocoa host failed");
    auto native=(__bridge NSWindow*)host.NativeWindow();const auto deadline=[NSDate dateWithTimeIntervalSinceNow:2];while(!native.isMiniaturized&&deadline.timeIntervalSinceNow>0){host.PollEvents();[[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.01]];}
    require(native.isMiniaturized&&!native.isKeyWindow,"Upper device host was not minimized");
    {
        Display::C_Device device;require(Display::DeviceExists()&&Display::Device()==&device&&!device.IsCreated(),"Shared upper device constructor/global registration failed");
        require(!device.HaveThreadOwnership()&&device.AcquireThreadOwnership(0)&&device.AcquireThreadOwnership(0)&&device.HaveThreadOwnership(),"Native recursive renderer ownership failed");
        bool blocked=false,wrongRelease=false;std::thread contender([&]{blocked=!device.HaveThreadOwnership()&&!device.AcquireThreadOwnership(10);try{device.ReleaseThreadOwnership();}catch(const std::logic_error&){wrongRelease=true;}});contender.join();require(blocked&&wrongRelease,"A different renderer thread acquired or released another thread's ownership");
        device.ReleaseThreadOwnership();require(device.HaveThreadOwnership(),"Native nested ownership released too early");device.ReleaseThreadOwnership();require(!device.HaveThreadOwnership(),"Native ownership retained after final release");
        bool acquired=false;std::thread successor([&]{acquired=device.AcquireThreadOwnership(100)&&device.HaveThreadOwnership();if(acquired)device.ReleaseThreadOwnership();});successor.join();require(acquired,"Native renderer ownership did not transfer between threads");
        Display::C_DeviceParams params;require(!device.Create(params)&&!device.IsCreated(),"Upper device accepted missing host dimensions");
        params.m_Width=host.PixelWidth();params.m_Height=host.PixelHeight();params.m_PresentationLayer=host.MetalLayer();params.m_PresentInterval=Display::E_PresentInterval_Immediate;
        require(device.Create(params)&&device.IsCreated()&&Display2::DeviceExists(),"Shared upper Create did not acquire actual Metal device");
        const S32 category=device.MemoryRegSystem("native %s literal diagnostic");{Display::C_MemoryCounter outer(category);require(device.MemoryGetSystemID()==category,"Upper graphics memory TLS category failed");{Display::C_MemoryCounter inner(1);require(device.MemoryGetSystemID()==1,"Nested graphics memory category failed");}require(device.MemoryGetSystemID()==category,"Graphics memory category did not restore its parent");}
        device.MemoryPrintSystems();
        require(device.CreateVolatile(params)&&device.CanRender()&&!device.GetNativeDepthSurface()->LockRect(16).pBits,"Default D24S8 native framebuffer failed or exposed an invented packed CPU layout");require(device.BeginScene(),"Default D24S8 scene failed");device.Clear(Display::E_Clear_Colour|Display::E_Clear_Depth|Display::E_Clear_Stencil,Maths::RGBA(0,0,0,255),1,7);device.EndScene();device.DestroyVolatile();
        params.m_DepthStencilFormat=Display::E_Format_D16;require(device.CreateVolatile(params)&&device.CanRender(),"Explicit D16 diagnostic framebuffers failed");
        auto layer=(__bridge CAMetalLayer*)host.MetalLayer();device.SetPresentInterval(Display::E_PresentInterval_One);require(layer.displaySyncEnabled&&device.m_DeviceParams.m_PresentInterval==Display::E_PresentInterval_One,"Native vsync enable did not reach CAMetalLayer");device.SetPresentInterval(Display::E_PresentInterval_Immediate);require(!layer.displaySyncEnabled,"Native immediate presentation retained vsync");
        bool intervalRejected=false;try{device.SetPresentInterval(Display::E_PresentInterval_Two);}catch(const std::invalid_argument&){intervalRejected=true;}require(intervalRejected&&!layer.displaySyncEnabled&&device.m_DeviceParams.m_PresentInterval==Display::E_PresentInterval_Immediate,"Unsupported presentation interval changed the active mode");
        Display::C_View view;Display::C_ViewSetup setup;device.SetupView(&view,setup);require(view.GetViewport().m_W==params.m_Width&&view.GetViewport().m_H==params.m_Height,"Original implicit view setup lost framebuffer geometry");device.SetupView(&view,setup,80,40);require(view.GetViewport().m_W==80&&view.GetViewport().m_H==40,"Original explicit view setup lost target geometry");
        require(device.BeginScene(),"GPU wait rejection fixture failed");device.SetPresentInterval(Display::E_PresentInterval_One);require(layer.displaySyncEnabled,"Original CommonRender could not update native timing within a recording scene");device.SetPresentInterval(Display::E_PresentInterval_Immediate);bool waitRejected=false;try{device.BlockUntilIdle();}catch(const std::logic_error&){waitRejected=true;}require(waitRejected,"Unsubmitted native commands reported GPU completion");device.EndScene();device.BlockUntilIdle();
        Display::C_VertexElement elements[]={Display::C_VertexElement(0,0,Display::E_VertexElementType_F32_3,Display::E_VertexElementUsage_Position,0),VERTEXELEMENT_END()};
        auto* declaration=device.CreateVertexDeclaration(elements);require(declaration&&declaration==device.CreateVertexDeclaration(1,elements),"Shared upper declaration API did not reuse actual native cache");
        Display::C_VertexBuffer buffer;require(device.CreateVertexBuffer(1024,0,&buffer),"Upper native vertex allocation failed");auto data=buffer.Lock();require(data,"Upper native vertex map failed");std::memset(data,0x73,1024);buffer.Unlock();device.Destroy(&buffer);
        for(U32 frame=0;frame<6;++frame){require(device.BeginScene()&&!device.BeginScene(),"Upper scene lifecycle failed");device.Clear(Display::E_Clear_Colour|Display::E_Clear_Depth,Maths::RGBA(0x12,0xab,0x34,0xff),.375f,0);device.EndScene();require(device.Present()&&device.GetDisplay2().WaitIdle(),"Upper shared device did not present an actual drawable");host.PollEvents();}
        auto target=device.GetNativeColourSurface();auto pixels=target->LockRect(16);require(pixels.pBits,"Upper native framebuffer readback failed");for(U32 y=0;y<target->GetHeight();++y)for(U32 x=0;x<target->GetWidth();++x){U32 pixel;std::memcpy(&pixel,pixels.pBits+y*pixels.pitch+x*4,4);require(pixel==0xff12ab34,"Upper clear/default state changed GPU framebuffer colour");}target->UnlockRect();
        auto depth=device.GetNativeDepthSurface();auto values=depth->LockRect(16);require(values.pBits,"Upper depth framebuffer readback failed");for(U32 y=0;y<depth->GetHeight();++y)for(U32 x=0;x<depth->GetWidth();++x){U16 value;std::memcpy(&value,values.pBits+y*values.pitch+x*2,2);require(std::abs(F32(value)/65535.f-.375f)<2.f/65535.f,"Upper clear did not forward depth to actual GPU storage");}depth->UnlockRect();
        require(device.GetDisplay2().GetSubmittedPresentCount()==6,"Upper frame submissions did not reach native presentation");
        device.DestroyVolatile();require(!device.CanRender()&&!device.BeginScene()&&!device.GetNativeColourSurface()->IsCreated()&&!device.GetNativeDepthSurface()->IsCreated(),"Upper volatile shutdown retained framebuffers");require(device.CreateVolatile(params),"Upper volatile framebuffer recreation failed");device.DestroyVolatile();device.Destroy();require(!device.IsCreated()&&!Display2::DeviceExists(),"Shared upper device shutdown retained native ownership");
    }
    require(!Display::DeviceExists(),"Upper C_Device destructor retained the shared singleton");
    {Display::C_Device implicit;Display::C_DeviceParams params;params.m_Width=host.PixelWidth();params.m_Height=host.PixelHeight();params.m_PresentationLayer=host.MetalLayer();require(implicit.Create(params)&&implicit.CreateVolatile(params),"Implicit upper owner cleanup fixture failed");}
    require(!Display::DeviceExists()&&!Display2::DeviceExists(),"Implicit upper destruction retained singleton/GPU ownership");host.Destroy();
    std::cout<<"Original shared upper C_Device/native GPU/framebuffers/six minimized presents/readback/recreation PASS (not game startup)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}}
