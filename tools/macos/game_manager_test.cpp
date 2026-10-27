// [Blur2PC][macOS] Original manager factories on a real Metal adapter, read-only display modes and actual shader/GPU lifecycle.
#include <display/manager/Manager.h>
#include <display/render/Render.h>
#include <display/shader/PixelShaders.h>
#include <system/serialise.h>
#include <system/serialise/macos/NativeFile.h>
#include <CoreGraphics/CoreGraphics.h>
#include <filesystem>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace Display;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>static void rejects(F call,const char* message){bool failed=false;try{call();}catch(const std::logic_error&){failed=true;}require(failed,message);}
int main(int argc,char** argv){try{
    require(argc==2,"Manager source-built shader directory missing");const auto root=std::filesystem::path(argv[1]);auto& fs=Sys::C_FileSystem::Instance();Sys::SetDefaultFileSystem(&fs);require(fs.SetWorkingDirectory(argv[1]),"Manager filesystem setup failed");
    C_DeviceParams params;params.m_Width=32;params.m_Height=16;
    {
        C_Manager manager;require(ManagerExists()&&Manager()==&manager&&!manager.IsInitialised(),"Original manager constructor/singleton failed");require(!manager.CreateDevice(params)&&!DeviceExists(),"Uninitialized native manager created a device or stranded a singleton");
        require(manager.Initialise()&&manager.Initialise()&&manager.IsInitialised()&&!manager.GetNativeDeviceName().empty(),"Actual Metal manager initialization failed");
        require(manager.GetMultisampleCount()>=1&&manager.GetMultisampleCount()<=3&&manager.FindMultisample(E_Multisample_None)==0,"Native manager sample enumeration is invalid");const auto& sample=manager.GetMultisample(0);require(sample.m_Windowed&&!sample.m_FullScreen&&sample.m_ColourFormat==E_Format_A8R8G8B8&&sample.m_DepthStencilFormat==E_Format_D24S8,"Native manager advertised unsupported mode/format behavior");
        {Display2::C_MetalDevice probe;require(probe.Create(),"Capability verification device failed");for(auto count:{E_Multisample_2,E_Multisample_4}){Display2::C_Surface colour,depth;const bool supported=probe.CreateSurface(16,16,Display2::E_Flags_Colour,Display2::E_SurfaceFormat_A8R8G8B8,Display2::E_Multisample(U32(count)),nullptr,&colour)&&probe.CreateSurface(16,16,Display2::E_Flags_DepthStencil,Display2::E_SurfaceFormat_D24S8,Display2::E_Multisample(U32(count)),nullptr,&depth);require((manager.FindMultisample(count)>=0)==supported,"Manager MSAA enumeration disagrees with actual GPU allocations");}}
        C_VideoMode desktop;require(manager.GetDesktopVideoMode(&desktop)&&desktop.m_Width&&desktop.m_Height&&desktop.m_Format==E_Format_A8R8G8B8&&!manager.GetDesktopVideoMode(nullptr),"Native desktop mode query failed");auto native=CGDisplayCopyDisplayMode(CGMainDisplayID());require(native,"Primary native display mode unavailable");const bool physical=desktop.m_Width==CGDisplayModeGetPixelWidth(native)&&desktop.m_Height==CGDisplayModeGetPixelHeight(native);CGDisplayModeRelease(native);require(physical,"Manager reported logical points rather than actual framebuffer pixels");
        require(manager.GetVideoModeCount()>0,"Native primary display mode enumeration was empty");for(S32 i=0;i<manager.GetVideoModeCount();++i){const auto& mode=manager.GetVideoMode(i);require(!(mode<mode)&&manager.FindVideoMode(mode.m_Width,mode.m_Height,mode.m_Format)==i,"Native mode sorting/lookup was inconsistent");if(i)require(manager.GetVideoMode(i-1)<mode,"Native video mode order was not strict/deduplicated");}
        rejects([&]{manager.GetVideoMode(-1);},"Invalid native mode index accepted");rejects([&]{manager.GetMultisample(manager.GetMultisampleCount());},"Unsupported native sample index accepted");
        const auto missing=root/"Shaders/psDefaultTextureD.bmsl",saved=root/"Shaders/psDefaultTextureD.saved";std::filesystem::rename(missing,saved);auto* failed=manager.CreateDevice(params);std::filesystem::rename(saved,missing);require(!failed&&!DeviceExists()&&!Display2::DeviceExists()&&!PixelShader::m_PixelShaderTexture.IsCreated()&&!manager.GetNativeError().empty(),"Original manager shader failure did not roll back actual device/global resources");manager.Destroy();
        auto* device=manager.CreateDevice(params);require(device&&device->IsCreated()&&!device->CanRender()&&Device()==device,"Original manager core factory reported volatile rendering too early");require(device->CreateVolatile(params)&&device->CanRender(),"Managed native volatile creation failed");require(!manager.CreateDevice(params)&&device->CanRender(),"Second native singleton device creation was accepted or damaged the first");
        require(device->BeginScene(),"Managed native scene failed");device->SetBlendMode(E_BlendMode_Solid);device->Clear(E_Clear_Colour,Maths::RGBA(0,0,0,255));Quad::DrawFullScreenColour(Maths::C_RGBA_F32(37.f/255,91.f/255,173.f/255,1));device->EndScene();auto* target=device->GetNativeColourSurface();auto data=target->LockRect(16);require(data.pBits,"Managed native GPU output unavailable");bool correct=true;for(U32 y=0;y<16;++y)for(U32 x=0;x<32;++x){U32 colour;std::memcpy(&colour,data.pBits+y*data.pitch+x*4,4);correct&=colour==0xff255bad;}target->UnlockRect();require(correct,"Original manager-created shaders/device did not draw expected GPU pixels");manager.DestroyDevice(device);require(!DeviceExists()&&!Display2::DeviceExists(),"Owned manager device destruction retained GPU/singleton state");
        {C_Device borrowed;require(manager.CreateDevice(&borrowed,params)&&borrowed.CreateVolatile(params),"Original indirect manager factory failed");manager.DestroyDeviceIndirect(&borrowed);require(!borrowed.IsCreated()&&Device()==&borrowed&&!Display2::DeviceExists(),"Indirect manager destruction deleted or retained caller-owned GPU state");}
        auto* grouped=manager.CreateDevice(params);require(grouped&&grouped->CreateVolatile(params),"Original manager group cleanup fixture failed");manager.Destroy();require(!DeviceExists()&&!Display2::DeviceExists(),"Original manager group destruction retained owned devices");
        manager.Shutdown();require(!manager.IsInitialised()&&!manager.GetVideoModeCount()&&!manager.GetMultisampleCount(),"Native manager shutdown retained capability ownership");require(manager.Initialise(),"Native manager adapter restart failed");manager.Shutdown();
    }
    require(!ManagerExists(),"Original manager destructor retained the singleton");
    {C_Manager implicit;require(implicit.Initialise(),"Implicit manager cleanup initialization failed");auto* owned=implicit.CreateDevice(params);require(owned&&owned->CreateVolatile(params),"Implicit manager owned device fixture failed");}
    require(!ManagerExists()&&!DeviceExists()&&!Display2::DeviceExists(),"Implicit manager destruction retained owned devices/adapters");Sys::SetDefaultFileSystem(nullptr);
    std::cout<<"Original manager Metal adapter/read-only pixel modes/backend capabilities/failed factory rollback/real shader draw/owned and indirect lifecycle/restart PASS (not game startup)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
