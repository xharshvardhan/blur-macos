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
#include <cstdlib>
#include <array>
using namespace Display;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>static void rejects(F call,const char* message){bool failed=false;try{call();}catch(const std::logic_error&){failed=true;}require(failed,message);}
struct Shaders{~Shaders(){Display::DestroyPixelShaders();Display::DestroyVertexShaders();}};
struct Target{C_Device& device;C_RenderTexture2D value;~Target(){device.DestroyRenderTexture2D(&value);}};
template<class F>static void pixels(C_Surface& surface,F expected){auto data=surface.LockRect(16);require(data.pBits,"Window composition GPU readback failed");bool correct=true;for(U32 y=0;y<surface.GetHeight();++y)for(U32 x=0;x<surface.GetWidth();++x){U32 colour;std::memcpy(&colour,data.pBits+y*data.pitch+x*4,4);correct&=colour==expected(x,y);}surface.UnlockRect();require(correct,"Window colour/mask/composition differs from independent GPU reference");}
int main(int argc,char** argv){try{
    struct PolicyEnvironment {
        bool present=std::getenv("BLUR_MAC_WINDOW_MSAA")!=nullptr;
        std::string previous=present?std::getenv("BLUR_MAC_WINDOW_MSAA"):"";
        PolicyEnvironment(){if(unsetenv("BLUR_MAC_WINDOW_MSAA"))throw std::runtime_error("Cannot isolate window policy fixture");}
        ~PolicyEnvironment(){if(present)setenv("BLUR_MAC_WINDOW_MSAA",previous.c_str(),1);else unsetenv("BLUR_MAC_WINDOW_MSAA");}
    } policyEnvironment;
    require(argc==2,"Window authored shader directory missing");auto& fs=Sys::C_FileSystem::Instance();Sys::SetDefaultFileSystem(&fs);require(fs.SetWorkingDirectory(argv[1]),"Window filesystem setup failed");
    C_Device device;require(device.AddWindow(Maths::C_Rect(Maths::HVec2(0,0),Maths::HVec2(.5,1)))==0&&device.AddWindow(Maths::C_Rect(Maths::HVec2(.5,0),Maths::HVec2(1,1)))==1,"Original window metadata registration failed");
    require(!device.GetDisplay2().SupportsMultisample(Display2::E_Multisample_None),"Uncreated device reported sample support");
    rejects([&]{device.AddWindow(Maths::C_Rect(Maths::HVec2(-.1f,0),Maths::HVec2(.5,1)));},"Invalid normalized window rectangle accepted");require(device.GetWindowCount()==2&&!device.GetWindow(0)->m_Width,"Window registration created GPU work before device creation");
    C_DeviceParams params;params.m_Width=64;params.m_Height=32;params.m_ScreenWidth=128;params.m_ScreenHeight=64;
    require(device.Create(params)&&device.CreateVolatile(params),"Native per-view device/resource creation failed");
    require(device.GetDisplay2().SupportsMultisample(Display2::E_Multisample_None)&&device.GetDisplay2().SupportsMultisample(Display2::E_Multisample_2)&&device.GetDisplay2().SupportsMultisample(Display2::E_Multisample_4)&&!device.GetDisplay2().SupportsMultisample(static_cast<Display2::E_Multisample>(8)),"Actual device sample/store+resolve capabilities differ from required fixture modes");
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
        // Reuse the authored quad colour shaders for actual geometric MSAA.
        // Different subpixel edge slopes avoid aligning all edges to a sample
        // pattern; no driver-specific positions or fractional-pixel count assumed.
        auto triangleCoverage=[&](E_Multisample samples){
            params.m_WindowBufferMultisample=samples;
            require(device.CreateVolatile(params)&&device.GetNativeColourSurface()->GetMultisample()==E_Multisample_None,"Coverage fixture changed the single-sample destination");
            require(device.BeginScene(),"Geometric coverage scene failed");device.SetWindow(0);
            C_Viewport viewport=device.GetViewport();viewport.m_X=viewport.m_Y=0;viewport.m_W=viewport.m_H=32;device.SetViewport(viewport);
            device.Clear(E_Clear_Colour,Maths::RGBA(0,0,0,255));device.SetColourWriteEnable(E_ColourWriteEnable_All);device.SetBlendMode(E_BlendMode_Solid);
            device.SetDepthEnable(E_Bool_False);device.SetDepthWriteEnable(E_Bool_False);device.SetStencilEnable(E_Bool_False);device.SetAlphaTestEnable(E_Bool_False);device.SetAlphaToMaskEnable(E_Bool_False);device.SetCullMode(E_CullMode_None);
            device.SetVertexDeclaration(Vertex_XY_C_UV::m_pDeclaration);device.SetVertexShader(&VertexShader_XY::m_VertexShaderVertexColour);device.SetPixelShader(&PixelShader::m_PixelShaderVertexColour);
            auto* vertices=static_cast<Vertex_XY_C_UV*>(device.BeginVertices(E_PrimitiveType_TriangleList,3,sizeof(Vertex_XY_C_UV),Vertex_XY_C_UV::m_pDeclaration,false));require(vertices,"Coverage triangle map failed");
            const F32 xy[3][2]={{2.25f,2.125f},{29.375f,5.25f},{5.5f,29.625f}};
            for(U32 i=0;i<3;++i){vertices[i].x=xy[i][0]/16.f-1.f;vertices[i].y=1.f-xy[i][1]/16.f;vertices[i].c=0xffffffff;vertices[i].u=vertices[i].v=0;}
            device.EndVertices(vertices+3);device.EndWindow();device.SetColourDepthStencilSurface();device.SetViewport(viewport);device.Clear(E_Clear_Colour,Maths::RGBA(0,0,0,255));device.RestoreWindow(0);device.EndWindow();device.EndScene();
            std::array<U32,32*32> result{};auto map=device.GetNativeColourSurface()->LockRect(16);require(map.pBits,"Restored geometric coverage readback failed");
            for(U32 y=0;y<32;++y)std::memcpy(result.data()+y*32,map.pBits+y*map.pitch,32*4);device.GetNativeColourSurface()->UnlockRect();
            return result;
        };
        const auto single=triangleCoverage(E_Multisample_None),multi=triangleCoverage(E_Multisample_2);
        U32 fractional=0;
        for(U32 i=0;i<single.size();++i){require(single[i]==0xff000000||single[i]==0xffffffff,"Single-sample same-size restore introduced fractional coverage");const U32 red=(multi[i]>>16)&255;require((multi[i]>>24)==255&&((multi[i]>>8)&255)==red&&(multi[i]&255)==red,"Resolved white triangle colour/alpha was altered");fractional+=red>0&&red<255;}
        // These unit-square pixels lie strictly inside/outside every possible
        // sample location, independently of the driver's sample pattern.
        for(const auto* image:{&single,&multi})require((*image)[8*32+8]==0xffffffff&&(*image)[0]==0xff000000&&(*image)[31*32+31]==0xff000000,"Analytical interior/exterior geometric coverage differs");
        require(fractional>0,"2x upper-window resolve produced no fractional geometric edge coverage");
        std::cout<<"Upper-window geometric coverage:1x binary;2x fractional="<<fractional<<"; exact interior/exterior PASS\n";
        params.m_WindowBufferMultisample=E_Multisample_None;require(device.CreateVolatile(params),"Coverage fixture authored-mode restoration failed");renderViews();
        require(!device.Present(),"Headless window composition reported a drawable presentation");
        Target offscreen{device,{}};require(device.CreateRenderTexture2D(32,32,1,E_Usage_Colour,E_Format_A8R8G8B8,E_Multisample_None,nullptr,&offscreen.value),"Window restore target failed");
        require(device.BeginScene(),"Window copy scene failed");device.SetColourDepthStencilSurface(&offscreen.value.GetSurface(),nullptr);device.Clear(E_Clear_Colour,Maths::RGBA(0,0,0,255));device.RestoreWindow(0);device.EndScene();pixels(offscreen.value.GetSurface(),[](U32,U32){return 0xffff0000;});device.Present();
        require(device.BeginScene(),"Window masked restore scene failed");device.SetColourDepthStencilSurface(&offscreen.value.GetSurface(),nullptr);device.Clear(E_Clear_Colour,Maths::RGBA(7,11,13,17));device.RestoreWindow(0,E_ColourWriteEnable_Red);device.EndScene();pixels(offscreen.value.GetSurface(),[](U32,U32){return 0x11ff0b0d;});device.Present();
        require(device.BeginScene(),"Window reset binding fixture failed");rejects([&]{device.DrawPrimitive(E_PrimitiveType_TriangleList,0,3);},"Native scene reset retained previous shaders/geometry");device.EndScene();device.Present();
        require(setenv("BLUR_MAC_WINDOW_MSAA","4",1)==0,"Cannot select authored window MSAA test");
        params.m_WindowBufferMultisample=E_Multisample_None;
        require(device.CreateVolatile(params)&&params.m_WindowBufferMultisample==E_Multisample_4&&params.m_Multisample==E_Multisample_None&&device.GetNativeColourSurface()->GetMultisample()==E_Multisample_None,"Window-only policy/caller parameter copyback changed backbuffer samples");
        for(S32 i=0;i<2;++i)require(device.GetWindow(i)->m_ColourRenderTexture.GetSurface().GetMultisample()==E_Multisample_4&&device.GetWindow(i)->GetDepthStencilSurface()->GetMultisample()==E_Multisample_4&&device.GetWindow(i)->m_MaskRenderTexture.GetSurface().GetMultisample()==E_Multisample_4,"Window colour/depth/mask samples disagree");
        renderViews();
        const auto* retainedColour=device.GetWindow(0)->GetColourTexture()->GetGPUTexture();
        const auto* retainedMask=device.GetWindow(0)->GetMaskTexture()->GetGPUTexture();
        require(setenv("BLUR_MAC_WINDOW_MSAA","2x",1)==0,"Cannot select invalid window MSAA test");
        params.m_Width=65;
        require(!device.CreateVolatile(params)&&params.m_WindowBufferMultisample==E_Multisample_4&&device.CanRender()&&device.GetWindow(0)->GetColourTexture()->GetGPUTexture()==retainedColour&&device.GetWindow(0)->GetMaskTexture()->GetGPUTexture()==retainedMask&&device.GetNativeError().find("BLUR_MAC_WINDOW_MSAA")!=std::string::npos,"Invalid policy retired live windows or changed caller params");
        require(params.m_Width==65&&device.m_DeviceParams.m_Width==64&&device.m_DeviceParams.m_WindowBufferMultisample==E_Multisample_4&&device.m_DeviceParams.m_Multisample==E_Multisample_None,"Rejected policy changed authoritative live-device params or caller request");
        renderViews();params.m_Width=64;
        require(setenv("BLUR_MAC_WINDOW_MSAA","authored",1)==0,"Cannot restore authored window policy");
        params.m_WindowBufferMultisample=static_cast<E_Multisample>(8);
        require(!device.CreateVolatile(params)&&params.m_WindowBufferMultisample==static_cast<E_Multisample>(8)&&device.CanRender()&&device.GetWindow(0)->GetColourTexture()->GetGPUTexture()==retainedColour&&device.GetWindow(0)->GetMaskTexture()->GetGPUTexture()==retainedMask,"Unsupported sample count retired live windows or copied fallback to caller");
        require(device.m_DeviceParams.m_WindowBufferMultisample==E_Multisample_4&&device.m_DeviceParams.m_Width==64&&device.m_DeviceParams.m_Multisample==E_Multisample_None,"Rejected sample count changed authoritative live-device params");
        params.m_WindowBufferMultisample=E_Multisample_4;renderViews();
        // A valid sample policy reaches real framebuffer allocation before
        // CreateWindows rejects D16: the original window mask needs stencil.
        require(setenv("BLUR_MAC_WINDOW_MSAA","4",1)==0,"Cannot select late-failure window MSAA policy");
        params.m_WindowBufferDepthStencilFormat=E_Format_D16;
        require(!device.CreateVolatile(params)&&!device.CanRender(),"Stencil-less window depth unexpectedly created a renderable graph");
        require(params.m_WindowBufferDepthStencilFormat==E_Format_D16&&params.m_WindowBufferMultisample==E_Multisample_4&&params.m_Multisample==E_Multisample_None,"Late failure rewrote the caller's request");
        require(device.m_DeviceParams.m_WindowBufferDepthStencilFormat==E_Format_D24S8&&device.m_DeviceParams.m_WindowBufferMultisample==E_Multisample_4&&device.m_DeviceParams.m_Width==64&&device.m_DeviceParams.m_Multisample==E_Multisample_None,"Late failure did not restore authoritative internal params");
        require(!device.GetNativeColourSurface()->IsCreated()&&!device.GetNativeDepthSurface()->IsCreated(),"Late window failure retained partial framebuffer owners");
        for(S32 i=0;i<2;++i)require(!device.GetWindow(i)->m_ColourRenderTexture.IsCreated()&&!device.GetWindow(i)->GetDepthStencilSurface()->IsCreated()&&!device.GetWindow(i)->m_MaskRenderTexture.IsCreated(),"Late window failure retained partial colour/depth/mask owners");
        params.m_WindowBufferDepthStencilFormat=E_Format_D24S8;
        require(device.CreateVolatile(params)&&device.CanRender()&&params.m_WindowBufferMultisample==E_Multisample_4,"Valid D24S8 window retry failed after late creation rejection");
        renderViews();
        require(unsetenv("BLUR_MAC_WINDOW_MSAA")==0,"Cannot clear isolated window policy");
        device.DestroyVolatile();params.m_WindowBufferMultisample=E_Multisample_2;require(device.CreateVolatile(params)&&device.CanRender()&&device.GetWindow(0)->m_ColourRenderTexture.GetSurface().GetMultisample()==E_Multisample_2,"Native 2x window MSAA creation failed");renderViews();
        device.DestroyVolatile();params.m_WindowBufferMultisample=E_Multisample_4;params.m_Multisample=E_Multisample_4;require(device.CreateVolatile(params)&&device.GetNativeColourSurface()->GetMultisample()==E_Multisample_4,"Native 4x back-buffer/window creation failed");renderViews();device.DestroyVolatile();params.m_Multisample=E_Multisample_None;
        params.m_WindowBufferMultisample=E_Multisample_None;params.m_WindowBufferWidth=128;params.m_WindowBufferHeight=64;require(device.CreateVolatile(params)&&device.GetWindow(0)->m_Width==64&&device.GetWindow(0)->m_Height==64,"Window volatile recovery/resolution scaling failed");
        device.DestroyVolatile();device.ClearWindows();require(!device.GetWindowCount()&&device.CreateVolatile(params),"Window metadata clear/recreation failed");
    }
    device.DestroyVolatile();device.Destroy();Sys::SetDefaultFileSystem(nullptr);
    std::cout<<"Native original window sizing/resources/stencil masks/split-screen quad composition/offscreen GPU copy/colour masks/reset/volatile rollback PASS (not game startup)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
