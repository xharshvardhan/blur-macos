// [Blur2PC][macOS] A real native application resizes while an async graph creates.
// Two authored graph targets model the independently cached postprocess/deferred
// resources; no Amax scene/assets or fabricated game readiness are involved.
#include <framework/application/macos/ApplicationMac.h>
#include <framework/application/CoreSystems.h>
#include <framework/application/UserControls.h>
#include <framework/layer/Layer.h>
#include <camera2/Camera.h>
#include <camera2/stock/DebugFeed.h>
#include <display/manager/Manager.h>
#include <display/render/BasicRender.h>
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
static void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
struct Record {std::atomic<U32> creates{0},resizes{0},renders{0},destroys{0},deletes{0};std::atomic<bool> entered{false},creating{false},cleanup{false},creatorOwnedDevice{false};U32 capturedWidth=0,capturedHeight=0;};
class App;
class Graph final:public Framework::C_Layer {
    App& app;Record& record;MT::Event& release;Display::C_RenderTexture2D postprocess,deferred;
    void DestroyTargets();void CreateTargets(U32 width,U32 height);
public:
    Graph(App& app,Record& record,MT::Event& release):C_Layer("async graph resize fixture"),app(app),record(record),release(release){}
    ~Graph()override{++record.deletes;}
    void Verify();
private:HRESULT ProcessMessage(Framework::E_LayerMessage message)override;
};
class App final:public Framework::C_Application {
    Record& record;MT::Event& release;Cam2::C_Camera camera;std::chrono::steady_clock::time_point started;
public:
    Graph* graph=nullptr;U32 frames=0,deferredFrames=0,committedFrames=0,oldWidth=0,oldHeight=0;unsigned phase=0;
    App(Record& record,MT::Event& release):record(record),release(release){}
    Display::C_Device& Device(){return *m_pDisplayDevice;}
protected:
    HRESULT OnInitialise()override{
        oldWidth=GetDisplayWindowWidth();oldHeight=GetDisplayWindowHeight();started=std::chrono::steady_clock::now();
        camera.SetPosition(Maths::HVec3(0,0,0));camera.SetOrient(Maths::C_Quaternion(0,0,0,1));camera.SetFov(1);camera.SetShutterAngle(0);
        m_FullscreenView.DebugFeed().ShowFeed(&camera);for(S32 i=0;i<m_SplitscreenViews.Size();++i)m_SplitscreenViews[i].DebugFeed().ShowFeed(&camera);
        graph=new Graph(*this,record,release);InsertLayerAsync(graph);return S_OK;
    }
    void OnUpdate()override{CommonUpdate();}
    void OnRender()override{
        require(std::chrono::steady_clock::now()-started<std::chrono::seconds(12),"Async resize fixture exceeded bounded application deadline");
        auto window=(__bridge NSWindow*)GetNativeWindow().NativeWindow();
        if(record.entered&&phase==0){[window setContentSize:NSMakeSize(112,80)];phase=1;}
        else if(phase==1&&deferredFrames>=2){[window setContentSize:NSMakeSize(136,96)];phase=2;}
        require(BeginRender(),"Native resize fixture could not begin rendering");CommonRender();
        auto layer=(__bridge CAMetalLayer*)GetNativeWindow().MetalLayer();
        if(phase==1||phase==2){
            require(GetDisplayWindowWidth()==oldWidth&&GetDisplayWindowHeight()==oldHeight,"Resize committed while graph creator was in flight");
            require(layer.drawableSize.width==oldWidth&&layer.drawableSize.height==oldHeight,"Deferred resize stopped presenting committed framebuffer dimensions");
            require(record.resizes==0,"Resize callback entered an in-flight graph");++deferredFrames;
            if(phase==2&&deferredFrames>=4){release.Signal();phase=3;}
        }
        if(phase==3&&GetDisplayWindowWidth()!=oldWidth){
            require(GetDisplayWindowWidth()==GetNativeWindow().PixelWidth()&&GetDisplayWindowHeight()==GetNativeWindow().PixelHeight(),"Resize failed to coalesce latest physical dimensions");
            require(record.resizes==1&&!record.creating,"Completed graph missed one original Resize or resized during Create");
            require(record.capturedWidth==GetDisplayWindowWidth()&&record.capturedHeight==GetDisplayWindowHeight(),"Graph metadata did not catch up before rendering");
            ++committedFrames;
        }
        Device().SetColourDepthStencilSurface();Device().DefaultStates();Device().SetView(nullptr);Device().Clear(Display::E_Clear_Depth);
        Display::BasicRender()->DrawColoured2DRect(0,0,1024,768,Maths::RGBA(0x11,0x88,0x44,255),Display::E_BlendMode_Solid);
        EndRender();++frames;if(committedFrames>=2)RequestShutdown();
    }
};
void Graph::DestroyTargets(){Display::Device()->Destroy(&postprocess);Display::Device()->Destroy(&deferred);}
void Graph::CreateTargets(U32 width,U32 height){
    DestroyTargets();try{
        for(auto* target:{&postprocess,&deferred})require(Display::Device()->CreateRenderTexture2D(width,height,1,Display::E_Usage_Colour,Display::E_Format_A8R8G8B8,Display::E_Multisample_None,nullptr,target),"Real graph target allocation failed");
    }catch(...){DestroyTargets();throw;}
    record.capturedWidth=width;record.capturedHeight=height;
}
HRESULT Graph::ProcessMessage(Framework::E_LayerMessage message){
    using namespace Framework;
    if(message==E_LayerMessage_Create){
        ++record.creates;record.creating=true;
        try{CreateTargets(app.GetDisplayWindowWidth(),app.GetDisplayWindowHeight());record.entered=true;
            require(record.cleanup||release.Wait(10000),"Async graph creator release timed out");
            // The real deferred creator takes Display ownership inside its
            // lifecycle transaction. Exercise that lock order while the main
            // loop continues trying to render/defer resize.
            require(app.Device().AcquireThreadOwnership(5000),"Creator could not acquire Display ownership after main deferred resize");
            record.creatorOwnedDevice=true;app.Device().ReleaseThreadOwnership();record.creating=false;
        }catch(...){record.creating=false;DestroyTargets();throw;}
    }
    if(message==E_LayerMessage_Resize){require(!record.creating,"Resize overlapped async Create");++record.resizes;CreateTargets(app.GetDisplayWindowWidth(),app.GetDisplayWindowHeight());}
    if(message==E_LayerMessage_RenderFullscreen){
        require(!record.creating&&record.capturedWidth==app.GetDisplayWindowWidth()&&record.capturedHeight==app.GetDisplayWindowHeight(),"First graph render used stale dimensions");
        for(auto* target:{&postprocess,&deferred})require(target->GetSurface().GetWidth()==record.capturedWidth&&target->GetSurface().GetHeight()==record.capturedHeight,"Graph texture extent differs from its captured view");
        app.Device().SetColourDepthStencilSurface(&postprocess.GetSurface(),nullptr);app.Device().Clear(Display::E_Clear_Colour,Maths::RGBA(0x25,0x47,0x69,255));
        app.Device().SetColourDepthStencilSurface(&deferred.GetSurface(),nullptr);app.Device().Clear(Display::E_Clear_Colour,Maths::RGBA(0x79,0x53,0x31,255));
        app.Device().SetColourDepthStencilSurface();++record.renders;
    }
    if(message==E_LayerMessage_Destroy){++record.destroys;DestroyTargets();}
    return S_OK;
}
void Graph::Verify(){
    const U32 expected[]={0xff254769,0xff795331};U32 index=0;
    for(auto* target:{&postprocess,&deferred}){
        auto& surface=target->GetSurface();auto map=surface.LockRect(16);require(bool(map.pBits),"Resized actual graph target readback failed");
        bool exact=true;for(U32 y=0;y<surface.GetHeight();++y)for(U32 x=0;x<surface.GetWidth();++x){U32 value;std::memcpy(&value,map.pBits+y*map.pitch+x*4,4);exact&=value==expected[index];}
        surface.UnlockRect();require(exact,"Resized postprocess/deferred target lost exact GPU pixels");++index;
    }
}
struct ReleaseOnExit {Record& record;MT::Event& release;~ReleaseOnExit(){record.cleanup=true;release.Signal();}};
int main(int argc,char** argv){@autoreleasepool{try{
    require(argc==2,"Native resize fixture shader root required");Framework::C_CoreSystems core;Framework::C_CoreSystemsParams cp;cp.m_Name="NativeAsyncResizeVerification";const auto root=std::filesystem::path(argv[1]);cp.m_WorkingDir=root.string().c_str();
    require(SUCCEEDED(core.Initialise(cp)),"Resize fixture core initialization failed");
    Record record;MT::Event release;
    {
        App app(record,release);ReleaseOnExit cleanup{record,release};Framework::Mac::WindowConfig window;window.width=96;window.height=64;window.startMinimized=true;app.ConfigureNativeWindow(window);
        Framework::C_ApplicationParams params;params.m_InputUserCount=4;params.m_InputButtonCount=Framework::E_Button_Count;params.m_InputAxisCount=Framework::E_Axis_Count;
        require(SUCCEEDED(app.Initialise(params)),app.GetNativeError().c_str());require(SUCCEEDED(app.Run()),app.GetNativeError().c_str());
        require(app.deferredFrames>=4&&app.committedFrames>=2&&record.resizes==1&&record.renders>=2&&record.creatorOwnedDevice,"Actual deferred/coalesced graph path was not exercised");
        require(app.Device().GetDisplay2().GetSubmittedPresentCount()==app.frames,"Deferred resize dropped actual old-frame presentation");app.graph->Verify();app.Shutdown();app.Shutdown();
    }
    require(record.creates==1&&record.destroys==1&&record.deletes==1,"Async graph retirement was not exactly once");core.Shutdown();
    std::cout<<"Actual native application async creator/Ready-safe resize, coalesced latest physical dimensions, continued old-frame presents, two real graph resources/exact pixels and clean shutdown PASS (authored graph, not Amax)\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}}
