// [Blur2PC][macOS] Original framework startup/rendering on the native host; simulation cadence follows the existing PC loop.
#include <framework/application/macos/ApplicationMac.h>
#include <framework/application/macos/RunDiagnostics.h>
#include <framework/Framework.h>
#include <framework/application/CoreSystems.h>
#include <framework/layer/Layer.h>
#include <camera2/Feed.h>
#include <world_render/ModelInstance.h>
#include <system/time/Time.h>
#include <algorithm>
#include <stdexcept>
#include <utility>
namespace Framework {
extern F32 g_Blur2PCForcedFrameTime;
C_Application::C_Application(){m_WindowConfig.title="Blur native engine";}
C_Application::~C_Application(){try{Shutdown(false);}catch(const std::exception& error){LOGERROR(g_Log,"Native application cleanup failed: %s",error.what());}}
// [Blur2PC] Preserve PC's change-only hook and world preset propagation.
// Native graphics-settings persistence is separate from the playable slice.
void C_Application::SetGraphicalDetail(U32 graphicalDetail){
    if(m_GraphicalDetail!=graphicalDetail){m_GraphicalDetail=graphicalDetail;ApplyGraphicalDetail(graphicalDetail);}
}
void C_Application::ConfigureNativeWindow(const Mac::WindowConfig& config){if(m_Window.IsCreated()||m_Initialised||m_Running)throw std::logic_error("Cannot reconfigure a live native window");m_WindowConfig=config;}
void C_Application::SetNativeInterpolationCallbacks(InterpolationCallbacks callbacks){
    if(m_Running)throw std::logic_error("Cannot replace interpolation owners while the native application is running");
    const bool any=bool(callbacks.tick)||bool(callbacks.tickSeconds)||bool(callbacks.beginRender)||bool(callbacks.endRender);
    if(any&&(!callbacks.tick||!callbacks.tickSeconds||!callbacks.beginRender||!callbacks.endRender))throw std::invalid_argument("Native world interpolation hooks must be installed together");m_Interpolation=std::move(callbacks);
}
HRESULT C_Application::Initialise(const C_ApplicationParams& params){
    if(m_Initialised||m_Running||m_Window.IsCreated()){m_NativeError="Native application is already initialized";return E_FAIL;}
    if(!C_CoreSystems::NativeInstance()||!C_CoreSystems::Instance().IsInitialised()){m_NativeError="Native application requires initialized core services";return E_FAIL;}
    m_RunState=E_RunState_Run;m_ShutdownPending=false;m_NativeError.clear();
    try{const HRESULT result=C_ApplicationBase::Initialise(params);if(SUCCEEDED(result)){
        // [Blur2PC] Use the actual world interpolation implementation by default.
        if(!m_Interpolation.tick)m_Interpolation={WorldRender::Blur2PCInterpTick,WorldRender::Blur2PCInterpEndRender,WorldRender::Blur2PCInterpSetTickSeconds,WorldRender::Blur2PCInterpBeginRender};
        WorldRender::Blur2PCInterpEnable(true);return result;
    }if(m_NativeError.empty())m_NativeError="Original framework native startup failed";Shutdown(false);return result;}
    catch(const std::exception& error){m_NativeError=error.what();try{Shutdown(false);}catch(const std::exception& cleanup){m_NativeError+="; cleanup: ";m_NativeError+=cleanup.what();}return E_FAIL;}
}
HRESULT C_Application::PlatformInitialise(const C_ApplicationParams&){
    Mac::WindowCallbacks callbacks;
    callbacks.resized=[this](U32 w,U32 h){m_PendingWidth=w;m_PendingHeight=h;};
    callbacks.close=[this]{RequestShutdown();};
    callbacks.focused=[this](bool active){m_HasFocus=active;if(m_InputEvents.focus)m_InputEvents.focus(active);};
    callbacks.key=[this](const Mac::KeyEvent& event){if(m_InputEvents.key)m_InputEvents.key(event.code,event.down,event.modifiers,event.modifiersOnly);};
    callbacks.mouse=[this](const Mac::MouseEvent& event){
        if(event.kind==Mac::MouseEvent::Move&&m_InputEvents.pointer)m_InputEvents.pointer(event.x,event.y);
        if((event.kind==Mac::MouseEvent::Down||event.kind==Mac::MouseEvent::Up)&&m_InputEvents.button)m_InputEvents.button(event.button,event.kind==Mac::MouseEvent::Down);
        if(event.kind==Mac::MouseEvent::Scroll&&m_InputEvents.scroll)m_InputEvents.scroll(event.scrollY);
    };
    if(!m_Window.Create(m_WindowConfig,callbacks))return E_FAIL;
    m_DisplayDeviceParams.m_Width=m_Window.PixelWidth();m_DisplayDeviceParams.m_Height=m_Window.PixelHeight();
    m_DisplayDeviceParams.m_ScreenWidth=m_DisplayDeviceParams.m_WindowBufferWidth=m_DisplayDeviceParams.m_Width;
    m_DisplayDeviceParams.m_ScreenHeight=m_DisplayDeviceParams.m_WindowBufferHeight=m_DisplayDeviceParams.m_Height;
    m_DisplayDeviceParams.m_PresentationLayer=m_Window.MetalLayer();m_PendingWidth=m_PendingHeight=0;m_Created=true;return S_OK;
}
void C_Application::PlatformShutdown(){m_InputEvents={};m_Window.Destroy();m_Created=false;m_HasFocus=false;m_PendingWidth=m_PendingHeight=0;m_DisplayDeviceParams.m_PresentationLayer=nullptr;}
HRESULT C_Application::CreateInputManager(const C_ApplicationParams& params){const HRESULT result=C_ApplicationBase::CreateInputManager(params);if(SUCCEEDED(result)){m_InputEvents=Input::Mac::GetEvents(m_pInputDeviceManager);if(m_InputEvents.focus)m_InputEvents.focus(m_HasFocus);}return result;}
HRESULT C_Application::CreateDevice(){
    const HRESULT result=C_ApplicationBase::CreateDevice();if(FAILED(result)){if(m_pDisplayManager)m_NativeError=m_pDisplayManager->GetNativeError();return result;}
    // Native CreateVolatile resizes and creates its original per-view resources.
    if(!m_pDisplayDevice->CreateVolatile(m_DisplayDeviceParams)){m_NativeError=m_pDisplayDevice->GetNativeError();return E_FAIL;}return S_OK;
}
bool C_Application::CanRender(){return m_Created&&m_Window.IsCreated()&&!m_Window.CloseRequested()&&m_pDisplayDevice&&m_pDisplayDevice->CanRender();}
void C_Application::UpdateGraphicsSettings(){
    if(!m_PendingWidth||!m_PendingHeight||!m_pDisplayDevice)return;
    const U32 width=m_PendingWidth,height=m_PendingHeight;m_PendingWidth=m_PendingHeight=0;
    if(width==m_DisplayDeviceParams.m_Width&&height==m_DisplayDeviceParams.m_Height)return;
    C_LayerManager::NativeResizeLease lifecycle(m_LayerManager);
    if(!lifecycle){
        // Cocoa has already changed drawableSize. Keep presenting the committed
        // framebuffer (scaled by its view) while an async graph owns creation,
        // or until a completed Ready layer joins the normal Resize broadcast.
        // This path must never wait for lifecycle while holding Display.
        m_PendingWidth=width;m_PendingHeight=height;
        auto& native=m_pDisplayDevice->GetDisplay2();
        if(!native.SetPresentationLayer(m_Window.MetalLayer(),m_DisplayDeviceParams.m_Width,m_DisplayDeviceParams.m_Height,
                m_DisplayDeviceParams.m_PresentInterval!=Display::E_PresentInterval_Immediate)||
           !native.SetPresentationInterval(static_cast<Display2::E_PresentInterval>(m_DisplayDeviceParams.m_PresentInterval)))
            throw std::runtime_error("Native deferred resize could not retain committed presentation");
        return;
    }
    m_pDisplayDevice->BlockUntilIdle();m_pDisplayDevice->DestroyRenderersVolatile();m_pDisplayDevice->DestroyVolatile();
    m_DisplayDeviceParams.m_Width=m_DisplayDeviceParams.m_ScreenWidth=m_DisplayDeviceParams.m_WindowBufferWidth=width;
    m_DisplayDeviceParams.m_Height=m_DisplayDeviceParams.m_ScreenHeight=m_DisplayDeviceParams.m_WindowBufferHeight=height;
    if(!m_pDisplayDevice->CreateVolatile(m_DisplayDeviceParams)||!m_pDisplayDevice->CreateRenderersVolatile())throw std::runtime_error("Native framebuffer/renderer resize failed");
    const auto& back=m_pDisplayDevice->GetBackBuffer();for(S32 i=0;i<m_SplitscreenViews.Size();++i){auto& view=m_SplitscreenViews[i];view.m_View.Setup(view.m_ViewSetup,back.GetWidth(),back.GetHeight());SetWindowViewports(view,m_pDisplayDevice->GetWindow(i),width,height);}SendMessageToLayers(E_LayerMessage_Resize);
}
void C_Application::Shutdown(bool force){
    if(m_Running){RequestShutdown(force);m_ShutdownPending=true;return;}
    m_InputEvents={};C_ApplicationBase::Shutdown(force);m_Interpolation={};m_ShutdownPending=false;
}
HRESULT C_Application::Run(){
    if(!m_Initialised||m_Running||m_RunState!=E_RunState_Run){m_NativeError="Native run requires an initialized, idle application in the run state";return E_FAIL;}
    m_Running=true;HRESULT result=S_OK;
    // Exact existing PC cadence: accumulate real time, consume at most four
    // whole 1/60-second ticks in one update, drop excess hitch debt, render independently.
    F64 accumulated=1.;auto last=Time::GetPerformanceCounter();const auto runStart=last;constexpr F64 tick=1./60.;
    const bool traceFrames=Mac::NativeFrameTraceEnabled();
    try{while(m_RunState==E_RunState_Run){
        Mac::NativeFrameTiming timing;
        if(traceFrames){timing.beginNS=Mac::NativeFrameClockNS();timing.simulationTicks=0;}
        m_Window.PollEvents();PollPlatformCallbacks();if(m_Window.CloseRequested())RequestShutdown();if(m_RunState!=E_RunState_Run)break;
        if(traceFrames)timing.pollEndNS=Mac::NativeFrameClockNS();
        const auto now=Time::GetPerformanceCounter();accumulated+=F64(now-last)/F64(Time::GetPerformanceFrequency());last=now;
        if(accumulated>=tick){const U32 ticks=accumulated>=4*tick?4:U32(accumulated/tick);accumulated-=ticks*tick;if(accumulated>=tick)accumulated=0.;g_Blur2PCForcedFrameTime=F32(ticks*tick);if(m_Interpolation.tick){m_Interpolation.tick();m_Interpolation.tickSeconds(g_Blur2PCForcedFrameTime);}Cam2::I_Feed::Blur2PCAdvanceTick();if(traceFrames)timing.simulationTicks=ticks;OnUpdate();}
        if(m_RunState!=E_RunState_Run)break;
        if(traceFrames)timing.updateEndNS=Mac::NativeFrameClockNS();
        const F32 alpha=F32(std::min(1.,std::max(0.,accumulated/tick)));Cam2::I_Feed::SetRenderOffset(alpha-1);
        if(m_Interpolation.beginRender)m_Interpolation.beginRender(alpha);
        try{
            if(!m_pDisplayDevice->AcquireThreadOwnership())throw std::runtime_error("Native render thread ownership acquisition failed");
            try{OnRender();}catch(...){m_pDisplayDevice->ReleaseThreadOwnership();throw;}m_pDisplayDevice->ReleaseThreadOwnership();
        }catch(...){Cam2::I_Feed::SetRenderOffset(0);if(m_Interpolation.endRender)m_Interpolation.endRender();throw;}
        Cam2::I_Feed::SetRenderOffset(0);if(m_Interpolation.endRender)m_Interpolation.endRender();
        if(traceFrames)timing.renderEndNS=Mac::NativeFrameClockNS();
        Mac::TickRunDiagnostics(F64(Time::GetPerformanceCounter()-runStart)/F64(Time::GetPerformanceFrequency()));
        if(traceFrames){timing.diagnosticsEndNS=Mac::NativeFrameClockNS();Mac::RecordNativeFrameTiming(timing);}
    }}catch(const std::exception& error){m_NativeError=error.what();m_RunState=E_RunState_QuitForced;result=E_FAIL;}
    catch(...){m_NativeError="Native application callback raised a non-standard exception";m_RunState=E_RunState_QuitForced;result=E_FAIL;}
    try{Mac::FinishRunDiagnostics();}catch(const std::exception& error){if(!m_NativeError.empty())m_NativeError+="; ";m_NativeError+=error.what();result=E_FAIL;}
    Cam2::I_Feed::SetRenderOffset(0);g_Blur2PCForcedFrameTime=-1;m_Running=false;
    if(FAILED(result)&&m_pDisplayDevice&&m_pDisplayDevice->InScene()){try{if(m_pDisplayDevice->BeginVerticesLocked())m_pDisplayDevice->FakeEndVertices();m_pDisplayDevice->EndScene();}catch(const std::exception& error){m_NativeError+="; scene cleanup: ";m_NativeError+=error.what();}}
    if(m_ShutdownPending)Shutdown(m_RunState==E_RunState_QuitForced);return result;
}
}
