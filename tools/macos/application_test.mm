// [Blur2PC][macOS] Original framework lifecycle/rendering on the actual native host. This fixture is not Amax or game startup.
#include <framework/application/macos/ApplicationMac.h>
#include <framework/application/CoreSystems.h>
#include <framework/application/UserControls.h>
#include <camera2/Camera.h>
#include <world_render/ModelInstance.h>
#include <camera2/stock/DebugFeed.h>
#include <input/DeviceManager.h>
#include <input/GameInput.h>
#include <display/manager/Manager.h>
#include <display/render/BasicRender.h>
#include <Resource/Resource.h>
#import <Cocoa/Cocoa.h>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
static void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
class App final:public Framework::C_Application {
public:
    U32 detailChanges=0,lastDetail=0;
    U32 starts=0,stops=0,frames=0,updates=0,ticks=0,begins=0,ends=0;bool failUser=false,deferredShutdown=false,failRender=false;Cam2::C_Camera camera;
    bool SceneRetired()const{return m_pDisplayDevice&&!m_pDisplayDevice->InScene()&&!m_pDisplayDevice->HaveThreadOwnership();}
    void Hooks(){SetNativeInterpolationCallbacks({[this]{++ticks;},[this]{++ends;},[](F32 seconds){require(seconds>0&&seconds<=4.f/60+.00001f&&std::abs(seconds*60-std::round(seconds*60))<.0001f,"Native update did not consume whole original simulation ticks");},[this](F32 alpha){require(alpha>=0&&alpha<=1,"Native render fraction left its simulation interval");++begins;}});}
    void VerifyPixels(){auto* surface=m_pDisplayDevice->GetNativeColourSurface();auto read=surface->LockRect(0x10);require(read.pBits,"Original framework framebuffer readback failed");bool good=true;U32 mismatches=0,firstPixel=0,firstX=0,firstY=0;for(U32 y=0;y<surface->GetHeight();++y)for(U32 x=0;x<surface->GetWidth();++x){U32 pixel;std::memcpy(&pixel,read.pBits+y*read.pitch+x*4,4);if(pixel!=0xff118844){good=false;if(!mismatches){firstPixel=pixel;firstX=x;firstY=y;}++mismatches;}}surface->UnlockRect();if(!good)std::cerr<<"Actual BasicRender output "<<surface->GetWidth()<<'x'<<surface->GetHeight()<<" mismatches="<<mismatches<<" first="<<firstX<<','<<firstY<<" pixel="<<std::hex<<firstPixel<<std::dec<<" frames="<<frames<<std::endl;require(good,"Original framework render loop lost actual BasicRender GPU output");require(m_pDisplayDevice->GetDisplay2().GetSubmittedPresentCount()==frames,"Original framework frames did not reach actual drawable presentation");}
protected:
    void ApplyGraphicalDetail(U32 detail)override{++detailChanges;lastDetail=detail;}
    void Attach(){m_FullscreenView.DebugFeed().ShowFeed(&camera);for(S32 i=0;i<m_SplitscreenViews.Size();++i)m_SplitscreenViews[i].DebugFeed().ShowFeed(&camera);}
    HRESULT OnInitialise()override{++starts;camera.SetPosition(Maths::HVec3(0,0,0));camera.SetOrient(Maths::C_Quaternion(0,0,0,1));camera.SetFov(1);camera.SetShutterAngle(0);Attach();return failUser?E_FAIL:S_OK;}
    void OnShutdown()override{++stops;}
    void OnUpdate()override{CommonUpdate();++updates;const F32 dt=GetTimer().GetFrameTime();require(dt>0&&dt<=4.f/60+.00001f,"Original framework timer lost the native fixed update interval");camera.StartUpdate(dt);camera.SetPosition(camera.Position()+Maths::HVec3(dt,0,0));camera.EndUpdate();}
    void OnRender()override{
        if(frames==2){SetViewSetup(Framework::E_ViewSetup_4);Attach();}if(frames==4){SetViewSetup(Framework::E_ViewSetup_2v);Attach();}
        if(frames==3&&!deferredShutdown){auto native=(__bridge NSWindow*)GetNativeWindow().NativeWindow();[native setContentSize:NSMakeSize(112,80)];}
        require(BeginRender(),"Original framework could not begin a native render pass");CommonRender();if(failRender)throw std::runtime_error("Intentional native renderer callback failure");
        m_pDisplayDevice->DefaultStates();m_pDisplayDevice->SetView(nullptr);m_pDisplayDevice->SetColourWriteEnable(Display::E_ColourWriteEnable_All);
        // [Blur2PC] CommonRender owns the window-depth clears. This extra authored
        // backbuffer draw owns its depth precondition; preserve the PC default
        // comparison and initialize this attachment through the actual clear policy.
        m_pDisplayDevice->Clear(Display::E_Clear_Depth);
        Display::BasicRender()->DrawColoured2DRect(0,0,1024,768,Maths::RGBA(0x11,0x88,0x44,255),Display::E_BlendMode_Solid);EndRender();++frames;
        if(deferredShutdown&&frames==2)Shutdown(true);else if(frames==8)RequestShutdown();
    }
};
static void clean(){require(!Display::DeviceExists()&&!Display::ManagerExists()&&!Display2::DeviceExists()&&!Input::C_DeviceManager::GetInstance()&&!Input::C_GameInputManager::GetInstance(),"Failed native application startup/shutdown retained shared owners");}
int main(int argc,char** argv){@autoreleasepool{try{
    require(argc==2,"Native application shader fixture root missing");const auto root=std::filesystem::path(argv[1]);Framework::C_CoreSystems core;Framework::C_CoreSystemsParams coreParams;coreParams.m_Name="NativeFrameworkHostVerification";coreParams.m_WorkingDir=root.string().c_str();
    {
        App app;require(app.GetGraphicalDetail()==2,"Native PC preset lost original default2");
        app.SetGraphicalDetail(3);app.SetGraphicalDetail(3);require(app.detailChanges==1&&app.lastDetail==3&&app.GetGraphicalDetail()==3,"Native preset did not propagate exactly once per change");
        app.SetGraphicalDetail(2);require(app.detailChanges==2&&app.lastDetail==2,"Native original preset could not be restored");Framework::C_ApplicationParams params;params.m_InputUserCount=4;params.m_InputButtonCount=Framework::E_Button_Count;params.m_InputAxisCount=Framework::E_Axis_Count;
        require(FAILED(app.Initialise(params))&&!app.IsInitialised(),"Native application accepted missing initialized core services");clean();require(SUCCEEDED(core.Initialise(coreParams)),"Native fixture original core startup failed");
        Framework::Mac::WindowConfig window;window.width=0;window.height=64;window.title="Blur framework host verification";app.ConfigureNativeWindow(window);require(FAILED(app.Initialise(params))&&!app.IsInitialised()&&!app.GetNativeWindow().IsCreated()&&app.starts==0&&app.stops==0,"Failed native window startup ran user callbacks or retained ownership");clean();
        window.width=96;app.ConfigureNativeWindow(window);app.failUser=true;require(FAILED(app.Initialise(params))&&!app.IsInitialised()&&app.starts==1&&app.stops==1,"Failed user startup did not roll back exactly once");clean();app.failUser=false;
        const auto shader=root/"Shaders/vsBasicRenderColoured2D.bmsl",held=root/"Shaders/held.bmsl";std::filesystem::rename(shader,held);const HRESULT missing=app.Initialise(params);std::filesystem::rename(held,shader);require(FAILED(missing)&&!app.IsInitialised()&&app.starts==2&&app.stops==2,"Missing original renderer resource reported initialized success or skipped cleanup");clean();
        require(SUCCEEDED(app.Initialise(params))&&app.IsInitialised()&&app.starts==3&&app.stops==2,"Native original framework retry/startup failed");require(FAILED(app.Initialise(params))&&app.IsInitialised(),"Duplicate native initialization damaged its active owners");const U32 initialWidth=app.GetDisplayWindowWidth();app.Hooks();require(SUCCEEDED(app.Run()),app.GetNativeError().c_str());require(app.GetDisplayWindowWidth()!=initialWidth&&app.GetDisplayWindowWidth()==app.GetNativeWindow().PixelWidth(),"Native window resize did not recreate the original per-view/render resources at physical pixel dimensions");
        auto native=(__bridge NSWindow*)app.GetNativeWindow().NativeWindow();require(native.isMiniaturized&&!native.isKeyWindow,"Native application fixture did not remain minimized");require(app.frames==8&&app.updates>0&&app.ticks==app.updates&&app.begins==app.frames&&app.ends==app.frames,"Native simulation/render interpolation bridge lost scope or update counts");app.VerifyPixels();
        app.Shutdown();app.Shutdown();require(!app.IsInitialised()&&app.stops==3&&FAILED(app.Run()),"Native application shutdown repeated user cleanup or permitted a closed run");clean();
        app.frames=app.updates=app.ticks=app.begins=app.ends=0;app.deferredShutdown=true;require(SUCCEEDED(app.Initialise(params)),"Original native application could not restart");const U32 worldTick=WorldRender::Blur2PCInterpCurrentTick();require(SUCCEEDED(app.Run())&&!app.IsInitialised()&&app.frames==2&&app.stops==4&&WorldRender::Blur2PCInterpCurrentTick()>worldTick&&WorldRender::Blur2PCInterpRenderAlpha()==1.f,"Shutdown inside a render callback did not defer teardown until the frame retired");clean();
        app.frames=0;app.deferredShutdown=false;app.failRender=true;require(SUCCEEDED(app.Initialise(params)),"Native renderer exception fixture could not initialize");app.Hooks();require(FAILED(app.Run())&&app.GetNativeError()=="Intentional native renderer callback failure"&&app.SceneRetired(),"Native callback failure retained a recording scene or renderer ownership");app.Shutdown();require(app.stops==5,"Native failed run did not clean its user exactly once");clean();
    }
    core.Shutdown();require(!Resource::C_Manager::GetInstance(),"Native application fixture retained its original core resource manager");std::cout<<"Original framework/native minimized host/input/shaders/fixed updates/unlocked rendering/split views/GPU pixels/presents/startup rollback/retry/deferred shutdown PASS (not Amax/game startup)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}}
