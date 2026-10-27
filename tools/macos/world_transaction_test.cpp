// [Blur2PC][macOS] Full original World type and real effect/resource failure boundaries; not race/gameplay success.
#include "../../code/amax/game/stdafx.h"
#include <GameApp.h>
#include <Game/AmaxCam/CameraSystem.h>
#include <system/artery/Artery.h>
#include <Game/Game.h>
#include <Game/GameWorld/GameWorld.h>
#include <Game/GameWorld/ViewManager.h>
#include <Game/GameWorld/View/Main.h>
#include <Game/GameWorld/View/Mirror.h>
#include <Game/GameWorld/View/CubeMap.h>
#include <Graphics/Lighting/Prepass.h>
#include <deferred/Manager.h>
#include <deferred/prepass/DynamicLights.h>
#include <render/postprocess/Manager.h>
#include <render/View/View.h>
#include <render/postprocess/Interface.h>
#include <display2/Utils/Utils.h>
#include <display/buffer/VertexDeclarations.h>
#include <fx/Fx.h>
#include <resource/Resource.h>
#include <system/serialise/C_FileSystem.h>
#include <system/serialise/FileSystemGlobals.h>
#include <system/io/ASync.h>
#include <system/scheduler/jobqueue.h>
#include <pakfs/PakFs.h>
#include <pakfs/PakGlobals.h>
#include <overlayfs/OverlayFs.h>
#include <atomic>
#include <filesystem>
#include <memory>
#include <chrono>
#include <iostream>
#include <new>
#include <stdexcept>
#include <thread>
namespace Game {extern Debug::Bool g_MainPrepassEnable;}
namespace {
void Require(bool v,const char* text){if(!v){std::cerr<<"World fixture failure: "<<text<<std::endl;throw std::runtime_error(text);}}
std::atomic<unsigned> workerLoads{0};
struct Payload : Resource::C_Resource<Payload> {
    static Util::C_String StaticGetResourceTypeName(){return "WorldTransactionWorkerProof";}
    bool FreeResourceData()override{return true;}
    bool LoadResourceData()override {
        std::unique_ptr<Sys::I_File> file(OpenResourceFile());U8 data=0;
        if(!file || file->Read(&data,1)!=1 || data!=0x5a)return false;
        ++workerLoads;return true;
    }
};
template<class T> void NeverCreatedView(){alignas(T) U8 storage[sizeof(T)];std::memset(storage,0x11,sizeof(storage));T* view=new(storage)T(nullptr);view->Destroy();view->Destroy();view->~T();}
struct Probe {bool throws;unsigned calls=0,releases=0;};
class FailingEffect final:public PostProcess::I_PostProcess {
    Probe& probe;Display::C_Texture2D texture;
public:
    FailingEffect(PostProcess::C_Manager& manager,Probe& p):I_PostProcess(manager),probe(p){}
    ~FailingEffect()override{Destroy();}
    bool Create()override {
        ++probe.calls;
        Require(Display::Device()->CreateTexture2D(2,2,1,0,Display::E_Format_A8R8G8B8,&texture),"Actual authored failure payload GPU allocation failed");
        if(probe.throws)throw std::runtime_error("Authored original-effect Create exception");
        return false;
    }
    void Destroy()override{if(texture.IsCreated()){Display::Device()->Destroy(&texture);++probe.releases;}}
    bool CreateVolatile()override{return true;}
    void DestroyVolatile()override{}
    void Update()override{}
};
struct IoScheduler {
    IoScheduler(){Require(IoInitialise(),"Original I/O initialization failed");static const Scheduler::Policy workers[8]={5,5,5,5,5,5,5,5};Pak::SetPacketCompressionPolicy(Scheduler::CreatePolicy(workers));Scheduler::Init();Scheduler::SetGroupPoolSize(128);Scheduler::jqEnableWorkers(Scheduler::JQ_CORE_1|Scheduler::JQ_CORE_3);Scheduler::Start();}
    ~IoScheduler(){Scheduler::Shutdown();Pak::SetPacketCompressionPolicy(-1);IoShutdown();}
};
S32 ResourceCount(){
#if TRACK_DISPLAY2_RESOURCES
    auto* d=Display2::Device();return d->GetVertexBufferCount()+d->GetIndexBufferCount()+d->GetVertexShaderCount()+d->GetPixelShaderCount()+d->GetSurfaceCount()+d->GetTextureCount()+d->GetRenderTexture2DCount()+d->GetRenderTextureCubeCount()+d->GetRenderBufferCount();
#else
    return 0;
#endif
}
void WorkerProof(Resource::C_Manager& manager,Sys::C_FileSystem& fs,const std::filesystem::path& scratch,unsigned index){
    const auto path=scratch/("worker-"+std::to_string(index)+".bin");
    {std::unique_ptr<Sys::I_File> file(fs.Open(path.c_str(),Sys::E_FileAccess_Write));const U8 data=0x5a;Require(file&&file->Write(&data,1)==1,"Authored worker-proof file could not write");}
    Resource::C_Handle<Payload> payload;const unsigned before=workerLoads.load();Require(payload.Load(path.c_str()),"Actual worker-proof load could not queue");
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(4);
    while(!static_cast<Payload*>(payload)->IsDataReady()&&std::chrono::steady_clock::now()<until){manager.PartialUpdate();MT::Thread::Sleep(1);}
    Require(static_cast<Payload*>(payload)->IsDataReady()&&workerLoads==before+1,"World rollback left the original resource worker paused or unable to load");
    payload=static_cast<Payload*>(nullptr);manager.PartialUpdate();std::filesystem::remove(path);
}
void StreamerPauseProof(Resource::C_Manager& manager,Sys::C_FileSystem& fs,const std::filesystem::path& scratch,bool cancel){
    Game::C_GameWorld world;
    struct OuterPause {Resource::C_Manager& manager;bool owned=true;explicit OuterPause(Resource::C_Manager& m):manager(m){Require(manager.PauseLoadingThread(),"Outer resource pause failed");}void Resume(){if(owned){manager.ResumeLoadingThread();owned=false;}}~OuterPause(){Resume();}} outer(manager);
    // Recovery makes an old-body negative control fail cleanly rather than
    // joining forever behind the intentionally retained pause. Normal candidate
    // Destroy already retired it; the second release is then an actual no-op.
    struct WorldPauseCleanup {Game::C_GameWorld& world;~WorldPauseCleanup(){world.NativeReleaseStreamerPause();}} cleanup{world};
    world.NativeAcquireStreamerPause();
    bool duplicateRejected=false;try{world.NativeAcquireStreamerPause();}catch(const std::logic_error&){duplicateRejected=true;}
    Require(duplicateRejected,"Duplicate streamer ownership consumed another pause depth");
    std::atomic<bool> foreignRejected{false};
    std::thread foreign([&]{try{world.NativeReleaseStreamerPause();}catch(const std::logic_error&){foreignRejected=true;}});foreign.join();
    Require(foreignRejected,"A different thread released the streamer's resource mutex");
    if(cancel){
        try{throw std::runtime_error("Authored render failure while streamer loading");}
        catch(const std::runtime_error&){Require(world.Destroy(),"Cancelled streamer world failed owner cleanup");}
    }else world.NativeReleaseStreamerPause();
    Require(world.Destroy()&&world.Destroy(),"Streamer pause retirement was not idempotent");
    const auto path=scratch/(cancel?"streamer-cancel.bin":"streamer-complete.bin");
    {std::unique_ptr<Sys::I_File> file(fs.Open(path.c_str(),Sys::E_FileAccess_Write));const U8 byte=0x5a;Require(file&&file->Write(&byte,1)==1,"Streamer worker proof file write failed");}
    Resource::C_Handle<Payload> payload;const auto before=workerLoads.load();Require(payload.Load(path.c_str()),"Streamer worker proof did not queue");
    MT::Thread::Sleep(25);
    Require(workerLoads==before,"World retirement consumed an unrelated caller's outer pause");
    outer.Resume();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(4);
    while(!static_cast<Payload*>(payload)->IsDataReady()&&std::chrono::steady_clock::now()<deadline){manager.PartialUpdate();MT::Thread::Sleep(1);}
    Require(static_cast<Payload*>(payload)->IsDataReady()&&workerLoads==before+1,"World cancellation retained its streamer pause and blocked the actual loader");
    payload=static_cast<Payload*>(nullptr);manager.PartialUpdate();std::filesystem::remove(path);
    Require(manager.NativeDrainUnreferencedResources(),"Streamer pause retirement did not permit actual worker join/drain/restart");
}
struct PrepassOwners final:C_LightingPrepass {
    ~PrepassOwners(){Destroy();}
    void CreateRequiredBases(){Require(m_pDeferredPointLights->Create(),"Actual point-light base creation failed");Require(m_pDeferredManager->Create(),"Actual deferred-manager base creation failed");}
};
void LightingResizeProof(Render::C_View& renderView){
    const auto flags=renderView.m_ViewFlags;const auto viewIndex=renderView.m_ViewIndex;
    struct RestoreView {Render::C_View& view;U32 flags;S32 index;~RestoreView(){view.m_ViewFlags=flags;view.m_ViewIndex=index;}} restore{renderView,flags,viewIndex};
    // Original deferred mappings use +index for normals/depth and -index for
    // lighting. Actual application views use a nonzero index; +0/-0 aliases.
    renderView.m_ViewIndex=1;
    renderView.m_ViewFlags|=Render::E_ViewFlag_EnableDeferred;
    const S32 originalCount=ResourceCount();
    {
        PrepassOwners prepass;Require(prepass.Create(),"Actual lighting owner creation failed");
        bool pointRejected=false;try{prepass.Resize();}catch(const std::runtime_error& error){pointRejected=std::string(error.what())=="Native required deferred point-light volatile resources failed during resize";}
        Require(pointRejected,"Required point-light base absence silently passed Resize");
        prepass.CreateRequiredBases();const S32 baseCount=ResourceCount();
        auto* manager=prepass.GetDeferredManager();
        auto healthy=[&]{prepass.Resize();Require(manager->Created(Display::E_RendererState_Volatile),"Actual lighting manager did not become volatile-ready");const auto* normal=prepass.GetNormalTexture(renderView.m_ViewIndex);const auto* depth=prepass.GetDepthStencilSurface(renderView.m_ViewIndex);Require(normal&&normal->IsCreated()&&depth&&depth->IsCreated(),"Successful original lighting resize has no real colour/depth owners");};
        healthy();
        for(bool rejectDepth:{false,true}){
            Deferred::C_RenderTargetSetup invalid{};for(auto& f:invalid.eColourFormat)f=Display::E_Format_Unknown;
            invalid.eColourFormat[0]=rejectDepth?Display::E_Format_A8R8G8B8:Display::E_Format_D24S8;
            invalid.eColourFormat[1]=Display::E_Format_R32F;
            invalid.eDepthStencilFormat=rejectDepth?Display::E_Format_A8R8G8B8:Display::E_Format_D24S8;
            invalid.eMultisample=Display::E_Multisample_None;invalid.iIndex=0;
            Require(manager->AddRenderTargetSetup(invalid),"Actual invalid target setup was not accepted as metadata");
            bool rejected=false;try{prepass.Resize();}catch(const std::runtime_error& error){rejected=std::string(error.what())=="Native required deferred lighting render targets failed during resize";}
            Require(rejected,"Original Prepass Resize hid a real deferred target-creation false result");
            Require(!manager->Created(Display::E_RendererState_Volatile)&&ResourceCount()==baseCount,"Failed lighting resize retained partial real GPU owners");
            manager->ClearRenderTargetSetups();prepass.SetupRenderTargets();healthy();
        }
        prepass.Destroy();prepass.Destroy();
    }
    Require(ResourceCount()==originalCount,"Lighting failure/retry/Destroy retained original GPU resources");
}
}
int main(int argc,char** argv){try {
    Require(argc==4,"Expected original data root, shader resource root and fixture scratch directory");
    const auto data=std::filesystem::absolute(argv[1]),shaders=std::filesystem::absolute(argv[2]),scratch=std::filesystem::absolute(argv[3]);
    std::filesystem::create_directories(scratch);
    Require(ArteryStartupEx(true,"Native World Transaction Fixture",16),"Original Artery startup failed");
    struct ArteryScope {~ArteryScope(){ArteryCleanup();}} artery;
    IoScheduler io;auto& nativeFs=Sys::C_FileSystem::Instance();Require(nativeFs.SetWorkingDirectory("/"),"Original filesystem root unavailable");
    LogicalFs::C_PakFs pak;Require(pak.OpenPak(&nativeFs,(data/"cache/root.pak").c_str(),nullptr),"Actual original root.pak could not mount");
    LogicalFs::C_OverlayFs fs;LogicalFs::C_OverlayLayer::C_Params layer;layer.flags.layerType=LogicalFs::C_OverlayLayer::E_Type_Overlay;
    fs.AddLayer("/",&pak,"/",layer,LogicalFs::E_MediaLayer_0,"Original game PAK");fs.AddLayer("/",&nativeFs,"/",layer,LogicalFs::E_MediaLayer_0,"Physical source shaders/probes");
    Require(fs.SetWorkingDirectory("/"),"Actual overlay filesystem root unavailable");Sys::SetDefaultFileSystem(&fs);Display2::SetNativeShaderResourceRoot(shaders.string().c_str());
    Display::C_Device device;device.AddWindow(Maths::C_Rect(Maths::HVec2(0,0),Maths::HVec2(1,1)));Display::C_DeviceParams params;params.m_Width=128;params.m_Height=64;Require(device.Create(params)&&device.CreateVolatile(params),"Actual native fixture display failed");
    Display::CreateVertexDeclarations();Display::CreateVertexShaders();Display::CreatePixelShaders();
    Render::C_View renderView;renderView.m_ViewFlags=Render::E_ViewFlag_EnablePostProcess;device.SetupView(&renderView.m_View,renderView.m_ViewSetup);
    Game::g_MainPrepassEnable=false;
    FX::Initialise();
    {
        Resource::C_Manager resources;C_GameApp application;
        const S32 baseline=ResourceCount();
        NeverCreatedView<Game::C_MainView>();NeverCreatedView<Game::C_MirrorView>();NeverCreatedView<Game::C_CubeMapView>();
        {Game::C_ViewManager views(nullptr,nullptr);views.Destroy();views.Destroy();}
        Require(ResourceCount()==baseline,"Never-created original view cleanup changed real GPU ownership");
        // Nonzero storage proves the full type's owner state does not depend on implicit static zero fill.
        {alignas(Game::C_GameWorld) U8 storage[sizeof(Game::C_GameWorld)];std::memset(storage,0x11,sizeof(storage));auto* world=new(storage)Game::C_GameWorld();Require(!world->GetParticleManager()&&!world->GetDynamicEffectRenderer()&&!world->GetPhysicsScene(),"Never-created world exposed indeterminate owner pointers");Require(world->Destroy()&&world->Destroy(),"Never-created full world could not clean up repeatedly");world->~C_GameWorld();}
        Require(ResourceCount()==baseline,"Never-created full world leaked actual GPU resources");
        StreamerPauseProof(resources,nativeFs,scratch,false);
        StreamerPauseProof(resources,nativeFs,scratch,true);
        LightingResizeProof(renderView);
        for(unsigned mode=0;mode<2;++mode){
            std::cerr<<"World effect failure boundary "<<mode<<std::endl;
            Probe probe{bool(mode)};
            {
                Game::C_GameWorld world;new FailingEffect(*world.GetManager(),probe);
                bool rejected=false;
                try {rejected=!world.Create(nullptr,1);}
                catch(const std::runtime_error& error){Require(probe.throws&&std::string(error.what())=="Authored original-effect Create exception","World rollback changed the actual first failure");rejected=true;}
                Require(rejected&&probe.calls==1&&probe.releases==1,"Full world did not roll back the real failing effect exactly once");
                Require(!world.GetPhysicsScene()&&!world.GetActorList()&&!world.GetSceneGraph(),"Failed pre-physics world retained later stage owners");
                Require(world.Destroy()&&world.Destroy(),"Failed world repeated Destroy was not idempotent");
                WorkerProof(resources,nativeFs,scratch,mode);
            }
            Require(ResourceCount()==baseline,"Failed world retained real renderer/effect GPU payloads");
        }
        Require(resources.NativeDrainUnreferencedResources(),"Actual fixture resource owner did not drain");
    }
    AmaxCam::CameraSystem().Exit();
    FX::Shutdown();Display::DestroyPixelShaders();Display::DestroyVertexShaders();Require(device.GetDisplay2().WaitIdle(),"Actual world fixture GPU completion failed");device.DestroyVolatile();device.Destroy();
    Display2::SetNativeShaderResourceRoot("");Sys::SetDefaultFileSystem(nullptr);pak.ClosePak();
    std::cout<<"Actual full GameWorld: poisoned never-Create/repeat Destroy (world/main/mirror/cubemap), real postprocess bool/throw rollback with original PAK+GPU payloads, resumed original resource worker and resource-baseline retirement PASS (no race or later-stage failure claim)\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
