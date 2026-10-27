// [Blur2PC][macOS] The actual GameWorld postprocess graph through the original PAK/stream loader.
#include "stdafx.h"
#include <Game/GameWorld/PostProcessLayer.h>
#include <Graphics/HDR.h>
#include <display.h>
#include <display/buffer/VertexDeclarations.h>
#include <display/window/Window.h>
#include <render/view/View.h>
#include <render/view/ViewManager.h>
#include <render/postprocess/Manager.h>
#include <vehicle/customisation/PaintEditor.h>
#include <system/memory/MemoryTracker.h>
#include <render/utils/Globals.h>
#include <system/io/ASync.h>
#include <system/scheduler/jobqueue.h>
#include <system/serialise/C_FileSystem.h>
#include <system/serialise/FileSystemGlobals.h>
#include <pakfs/PakFs.h>
#include <pakfs/PakGlobals.h>
#include <overlayfs/OverlayFs.h>
#include <filesystem>
#include <cstring>
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace Game {extern Debug::Bool g_MainPrepassEnable;}
static void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
// An actual optional effect callback owns a real texture before returning its
// authored dependency failure. The production manager owns callback rollback.
struct VolatileBoundaryEffect : PostProcess::I_PostProcess {
    Display::C_Texture2D texture;int failure=0;U32 attempts=0,retirements=0;
    explicit VolatileBoundaryEffect(PostProcess::C_Manager& manager):I_PostProcess(manager){}
    ~VolatileBoundaryEffect(){DestroyVolatile();}
    bool Create()override{return true;}
    void Destroy()override{}
    bool CreateVolatile()override{
        ++attempts;require(Display::Device()->CreateTexture2D(4,4,1,0,Display::E_Format_A8R8G8B8,&texture),"Failure-boundary real texture acquisition failed");
        if(failure==2)throw std::runtime_error("Authored selected-graph volatile failure");
        return failure==0;
    }
    void DestroyVolatile()override{if(texture.IsCreated()){Display::Device()->DestroyTexture2D(&texture);++retirements;}}
    void Update()override{}
};
struct IoScheduler {
    IoScheduler(){require(IoInitialise(),"Original I/O initialization failed");static const Scheduler::Policy workers[8]={5,5,5,5,5,5,5,5};Pak::SetPacketCompressionPolicy(Scheduler::CreatePolicy(workers));Scheduler::Init();Scheduler::SetGroupPoolSize(64);Scheduler::jqEnableWorkers(Scheduler::JQ_CORE_1|Scheduler::JQ_CORE_3);Scheduler::Start();}
    ~IoScheduler(){Scheduler::Shutdown();Pak::SetPacketCompressionPolicy(-1);IoShutdown();}
};
int main(int argc,char**argv){try{
    require(argc==3,"Expected original game data root and source-built shader resource root");
    const auto data=std::filesystem::absolute(argv[1]),shaders=std::filesystem::absolute(argv[2]);
    IoScheduler io;auto& nativeFs=Sys::C_FileSystem::Instance();require(nativeFs.SetWorkingDirectory("/"),"Native filesystem root unavailable");
    LogicalFs::C_PakFs pak;require(pak.OpenPak(&nativeFs,(data/"cache/root.pak").c_str(),nullptr),"Actual original root.pak could not mount");
    LogicalFs::C_OverlayFs fs;LogicalFs::C_OverlayLayer::C_Params layer;layer.flags.layerType=LogicalFs::C_OverlayLayer::E_Type_Overlay;
    fs.AddLayer("/",&pak,"/",layer,LogicalFs::E_MediaLayer_0,"Original game PAK");
    fs.AddLayer("/",&nativeFs,"/",layer,LogicalFs::E_MediaLayer_0,"Source-built physical shaders");
    require(fs.SetWorkingDirectory("/"),"Original overlay filesystem root unavailable");Sys::SetDefaultFileSystem(&fs);
    Display2::SetNativeShaderResourceRoot(shaders.string().c_str());
    Display::C_Device device;device.AddWindow(Maths::C_Rect(Maths::HVec2(0,0),Maths::HVec2(1,1)));
    const auto paintContext=MEMORY_TRACKER_CURRENT_CONTEXT();
    require(!Vehicle::C_PaintJob::CreatePaintTextures(1),"Real uncreated device unexpectedly allocated vehicle paint resources");
    require(MEMORY_TRACKER_CURRENT_CONTEXT()==paintContext,"Vehicle paint allocation failure popped an unowned tracker context");
    Vehicle::C_PaintJob::DestroyPaintTextures();Vehicle::C_PaintJob::DestroyPaintTextures();
    Display::C_DeviceParams params;params.m_Width=128;params.m_Height=64;require(device.Create(params)&&device.CreateVolatile(params),"Real graph window/device creation failed");
    require(Vehicle::C_PaintJob::CreatePaintTextures(1),"Actual vehicle paint resource retry failed");Vehicle::C_PaintJob::DestroyPaintTextures();
    Display::CreateVertexDeclarations();Display::CreateVertexShaders();Display::CreatePixelShaders();
    Render::C_View view;view.m_ViewFlags=Render::E_ViewFlag_EnablePostProcess;device.SetupView(&view.m_View,view.m_ViewSetup);device.SetupView(&view.m_View,view.m_ViewSetup);
    // This is the original PC disabled-prepass resource, rather than substitute retail data.
    Game::g_MainPrepassEnable=false;Display::C_Texture2D depth;require(device.CreateTexture2D(1,1,1,Display::E_Usage_Managed,Display::E_Format_R32F,&depth),"Original PC disabled-prepass depth resource failed");
    auto lock=depth.Lock(0);require(lock.pBits,"Original PC disabled-prepass depth map failed");std::memset(lock.pBits,0,4);depth.Unlock(0);auto* window=device.GetWindow(0);window->m_pDepthTexture=&depth;
#if TRACK_DISPLAY2_RESOURCES
    const auto renderTargetBaseline=device.GetDisplay2().GetRenderTexture2DCount();
#endif
    {
        Game::C_GameWorldPostProcess graph;require(graph.Create(),"Actual game postprocess graph creation failed");
        require(graph.GetManager()->IsRendererCreated()&&graph.GetManager()->IsRendererCreated(Display::E_RendererState_Volatile),"Actual game graph falsely reported ready");
        F32 previous=0;bool changed=false;
        auto renderFrame=[&](U32 frame){
            require(device.BeginScene(),"Actual graph scene begin failed");device.SetWindow(0);device.SetView(&view.m_View);
            const F32 radiance=frame<7?.25f:.75f;device.Clear(Display::E_Clear_Colour|Display::E_Clear_Depth|Display::E_Clear_Stencil,Maths::RGBA(0xff000000U|U32(U8(radiance*255))*0x010101U),1,0);
            device.RenderWindowMask();device.EndWindow();
            Render::SetGlobalColourBuffer(window->GetColourTexture());Render::SetGlobalDepthStencilBuffer(window->GetDepthStencilTexture());Render::SetGlobalStencilBuffer(window->GetMaskTexture());
            device.SetColourDepthStencilSurface();device.SetView(&view.m_View);graph.GetManager()->SetFrameTime(1.f/60.f);graph.BeginRenderSplitscreenWithoutAA();device.RestoreWindow(0);graph.EndRenderSplitscreenWithoutAA();device.EndScene();
            const auto* luminance=graph.GetHDR()->GetAdaptedLuminanceTexture(0);require(luminance&&luminance->IsCreated(),"Actual adapted-luminance resource missing");
            auto pixels=luminance->LockRect2D(0,16);require(pixels.pBits,"Actual adapted-luminance readback failed");F32 value;std::memcpy(&value,pixels.pBits,4);luminance->UnlockRect2D(0);require(std::isfinite(value)&&value>=0,"Actual adapted-luminance pixels invalid");
            if(frame>7&&std::abs(value-previous)>1.e-7f)changed=true;previous=value;
        };
        for(U32 frame=0;frame<14;++frame)renderFrame(frame);
        require(changed,"Actual HDR temporal adaptation did not update with changed source pixels");
        {
            VolatileBoundaryEffect failure(*graph.GetManager());
            for(int mode:{1,2}){
                failure.failure=mode;const auto tracker=MEMORY_TRACKER_CURRENT_CONTEXT();const U32 retired=failure.retirements;
                bool rejected=false;try{graph.Resize();}catch(const std::runtime_error& error){rejected=std::string(error.what()).find(mode==1?"failed during resize":"Authored selected-graph volatile failure")!=std::string::npos;}
                require(rejected,"Required game graph resize swallowed a dependency failure");
                require(MEMORY_TRACKER_CURRENT_CONTEXT()==tracker,"Throwing selected postprocess creation retained its tracker context");
                require(!graph.GetManager()->IsRendererCreated(Display::E_RendererState_Volatile)&&graph.GetManager()->GetRenderTargetCount()==0&&!graph.GetManager()->Enabled(),"Failed volatile graph retained readiness/active targets");
                require(!failure.texture.IsCreated()&&failure.retirements>retired,"Actual postprocess callback texture was not rolled back");
#if TRACK_DISPLAY2_RESOURCES
                require(device.GetDisplay2().GetRenderTexture2DCount()==renderTargetBaseline,"Failed selected graph retained render texture owners");
#endif
                rejected=false;try{graph.GetManager()->Begin(0,view.m_View.GetViewport());}catch(const std::runtime_error& error){rejected=std::string(error.what()).find("not ready for rendering")!=std::string::npos;}
                require(rejected,"Retired selected graph entered rendering");
                failure.failure=0;graph.Resize();
                require(graph.GetManager()->IsRendererCreated(Display::E_RendererState_Volatile)&&failure.texture.IsCreated(),"Required selected graph retry falsely reported ready");
                renderFrame(14+mode);
            }
            graph.Destroy();require(!failure.texture.IsCreated(),"Game graph teardown retained failure-boundary texture");
        }
    }
    Render::SetGlobalColourBuffer(nullptr);Render::SetGlobalDepthStencilBuffer(nullptr);Render::SetGlobalStencilBuffer(nullptr);window->m_pDepthTexture=nullptr;
    device.Destroy(&depth);Display::DestroyPixelShaders();Display::DestroyVertexShaders();require(device.GetDisplay2().WaitIdle(),"Actual graph GPU completion failed");device.DestroyVolatile();device.Destroy();
    Display2::SetNativeShaderResourceRoot("");Sys::SetDefaultFileSystem(nullptr);pak.ClosePak();
    std::cout<<"Actual GameWorldPostProcess complete Create/CreateVolatile, original root.pak resource streams, real window mask, HDR/Bloom temporal draws/readback-ring, required resize false/throw rollback/retry, tracker/paint failure guards and teardown PASS; disabled PC prepass (motion/DoF execution requires actual game prepass)"<<std::endl;
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
