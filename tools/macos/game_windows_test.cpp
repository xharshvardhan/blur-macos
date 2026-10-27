// [Blur2PC][macOS] Original per-view resources, stencil masks and split-screen/offscreen composition through real GPU commands.
#include <display/device/Device.h>
#include <display/window/Window.h>
#include <display/render/Render.h>
#include <display/buffer/VertexDeclarations.h>
#include <display/shader/VertexShaders.h>
#include <display/shader/PixelShaders.h>
#include <system/serialise.h>
#include <system/serialise/macos/NativeFile.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <filesystem>
using namespace Display;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>static void rejects(F call,const char* message){bool failed=false;try{call();}catch(const std::logic_error&){failed=true;}require(failed,message);}
struct Shaders{~Shaders(){Display::DestroyPixelShaders();Display::DestroyVertexShaders();}};
struct Target{C_Device& device;C_RenderTexture2D value;~Target(){device.DestroyRenderTexture2D(&value);}};
template<class F>static void pixels(C_Surface& surface,F expected){auto data=surface.LockRect(16);require(data.pBits,"Window composition GPU readback failed");bool correct=true;for(U32 y=0;y<surface.GetHeight();++y)for(U32 x=0;x<surface.GetWidth();++x){U32 colour;std::memcpy(&colour,data.pBits+y*data.pitch+x*4,4);correct&=colour==expected(x,y);}surface.UnlockRect();require(correct,"Window colour/mask/composition differs from independent GPU reference");}
int main(int argc,char** argv){try{
    require(argc==2,"Window authored shader directory missing");auto& fs=Sys::C_FileSystem::Instance();Sys::SetDefaultFileSystem(&fs);require(fs.SetWorkingDirectory(argv[1]),"Window filesystem setup failed");
    C_Device device;require(device.AddWindow(Maths::C_Rect(Maths::HVec2(0,0),Maths::HVec2(.5,1)))==0&&device.AddWindow(Maths::C_Rect(Maths::HVec2(.5,0),Maths::HVec2(1,1)))==1,"Original window metadata registration failed");
    rejects([&]{device.AddWindow(Maths::C_Rect(Maths::HVec2(-.1f,0),Maths::HVec2(.5,1)));},"Invalid normalized window rectangle accepted");require(device.GetWindowCount()==2&&!device.GetWindow(0)->m_Width,"Window registration created GPU work before device creation");
    C_DeviceParams params;params.m_Width=64;params.m_Height=32;params.m_ScreenWidth=128;params.m_ScreenHeight=64;
    require(device.Create(params)&&device.CreateVolatile(params),"Native per-view device/resource creation failed");
    require(device.GetBackBuffer().GetWidth()==64&&device.GetBackBuffer().GetHeight()==32&&device.GetBackBuffer().GetColourSurface()==device.GetNativeColourSurface()&&device.GetRenderTarget().GetColourSurface()==device.GetBackBuffer().GetColourSurface(),"Original back-buffer/attachment ownership disagrees");
    for(S32 i=0;i<2;++i){auto* window=device.GetWindow(i);require(window->m_Width==32&&window->m_Height==32&&window->m_ScreenWidth==64&&window->m_ScreenHeight==64&&window->GetColourTexture()->IsCreated()&&window->GetMaskTexture()->IsCreated()&&window->GetDepthStencilSurface()->IsCreated(),"Original window fractional sizing/resources failed");}
    {
        Shaders shaders;Display::CreateVertexDeclarations();Display::CreateVertexShaders();Display::CreatePixelShaders();
        auto renderViews=[&]{
        require(device.BeginScene(),"Window scene failed");device.SetColourWriteEnable(E_ColourWriteEnable_All);
        for(S32 i=0;i<2;++i){
            device.SetWindow(i);require(device.GetRenderTarget().GetColourWidth()==32&&device.GetRenderTarget().GetDepthStencilFormat()==E_Format_D24S8,"Window attachment state was not recorded");
            auto& context=device.GetDisplay2().GetContext();context.SetViewport(Display2::C_Viewport(0,0,16,32));context.Clear(Display2::E_Clear_Stencil,0,1,0xff);context.SetViewport(Display2::C_Viewport(0,0,32,32));
            Quad::DrawFullScreenColour(i?Maths::C_RGBA_F32(0,0,1,1):Maths::C_RGBA_F32(1,0,0,1));device.RenderWindowMask();device.EndWindow();
        }
        device.SetColourDepthStencilSurface();device.Clear(E_Clear_Colour,Maths::RGBA(0,0,0,255));C_Viewport viewport=device.GetViewport();viewport.m_W=32;device.SetViewport(viewport);device.RestoreWindow(0);viewport.m_X=32;device.SetViewport(viewport);device.RestoreWindow(1);device.EndWindow();device.EndScene();
        pixels(*device.GetNativeColourSurface(),[](U32 x,U32){return x<32?0xffff0000:0xff0000ff;});
        for(S32 i=0;i<2;++i)pixels(device.GetWindow(i)->m_MaskRenderTexture.GetSurface(),[](U32 x,U32){return x<16?0xffffffff:0;});
        };renderViews();
        require(!device.Present(),"Headless window composition reported a drawable presentation");
        Target offscreen{device,{}};require(device.CreateRenderTexture2D(32,32,1,E_Usage_Colour,E_Format_A8R8G8B8,E_Multisample_None,nullptr,&offscreen.value),"Window restore target failed");
        require(device.BeginScene(),"Window copy scene failed");device.SetColourDepthStencilSurface(&offscreen.value.GetSurface(),nullptr);device.Clear(E_Clear_Colour,Maths::RGBA(0,0,0,255));device.RestoreWindow(0);device.EndScene();pixels(offscreen.value.GetSurface(),[](U32,U32){return 0xffff0000;});device.Present();
        require(device.BeginScene(),"Window masked restore scene failed");device.SetColourDepthStencilSurface(&offscreen.value.GetSurface(),nullptr);device.Clear(E_Clear_Colour,Maths::RGBA(7,11,13,17));device.RestoreWindow(0,E_ColourWriteEnable_Red);device.EndScene();pixels(offscreen.value.GetSurface(),[](U32,U32){return 0x11ff0b0d;});device.Present();
        require(device.BeginScene(),"Window reset binding fixture failed");rejects([&]{device.DrawPrimitive(E_PrimitiveType_TriangleList,0,3);},"Native scene reset retained previous shaders/geometry");device.EndScene();device.Present();
        device.DestroyVolatile();params.m_WindowBufferMultisample=E_Multisample_2;require(device.CreateVolatile(params)&&device.CanRender()&&device.GetWindow(0)->m_ColourRenderTexture.GetSurface().GetMultisample()==E_Multisample_2,"Native 2x window MSAA creation failed");renderViews();
        device.DestroyVolatile();params.m_WindowBufferMultisample=E_Multisample_4;params.m_Multisample=E_Multisample_4;require(device.CreateVolatile(params)&&device.GetNativeColourSurface()->GetMultisample()==E_Multisample_4,"Native 4x back-buffer/window creation failed");renderViews();device.DestroyVolatile();params.m_Multisample=E_Multisample_None;
        params.m_WindowBufferMultisample=E_Multisample_None;params.m_WindowBufferWidth=128;params.m_WindowBufferHeight=64;require(device.CreateVolatile(params)&&device.GetWindow(0)->m_Width==64&&device.GetWindow(0)->m_Height==64,"Window volatile recovery/resolution scaling failed");
        device.DestroyVolatile();device.ClearWindows();require(!device.GetWindowCount()&&device.CreateVolatile(params),"Window metadata clear/recreation failed");
    }
    device.DestroyVolatile();device.Destroy();Sys::SetDefaultFileSystem(nullptr);
    std::cout<<"Native original window sizing/resources/stencil masks/split-screen quad composition/offscreen GPU copy/colour masks/reset/volatile rollback PASS (not game startup)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
