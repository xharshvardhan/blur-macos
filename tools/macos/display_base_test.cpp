// [Blur2PC][macOS] Actual upper CPU device, projection/frustum math and renderer lifecycle; no GPU-device substitute.
#include <core/Core.h>
#include <core/containers.h>
#include <core/Maths.h>
#include <system/debuglog.h>
#include <display/device/DeviceBase.h>
#include <display/device/metal/NativeDeviceAccess.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace Display;
static void require(bool v,const char* text){if(!v)throw std::runtime_error(text);}
struct CPUDevice: C_DeviceBase {using C_DeviceBase::AssertEverythingHasBeenDestroyed;void Reverse(bool reverse){m_DepthCmpFlip=reverse;}};
struct Renderer: I_Renderer {
    U32 created=0,volatiles=0,initialised=0,shutdown=0,destroyed=0,volatileDestroyed=0,resets=0;bool fail=false;
    explicit Renderer(const char* name):I_Renderer(name){}
    ~Renderer(){Shutdown();DestroyRenderer();}
    bool OnCreate()override{++created;return !fail;}bool OnCreateVolatile()override{++volatiles;return true;}
    bool OnInitialise()override{++initialised;return true;}void OnShutdown()override{++shutdown;}
    void OnDestroy()override{++destroyed;}void OnDestroyVolatile()override{++volatileDestroyed;}void OnReset()override{++resets;}
};
static void projection(CPUDevice& device){
    require(device.AddView(E_ViewType_2D|E_ViewType_3D,3)==0&&device.GetViewCount()==1,"Original view allocation failed");
    auto* view=device.GetView(0);C_ViewSetup setup;require(view->GetUserIndex()==3&&view->GetIndex()==0,"Original view user/index metadata changed");
    setup.m_WindowCoords.SetBounds(0,0,512,768);view->Setup(setup,640,480);
    require(view->GetViewport().GetW()==320&&view->GetViewport().GetH()==480,"Original split view did not scale to physical device dimensions");
    for(bool ortho:{false,true})for(bool reverse:{false,true}){
        auto params=setup.m_ViewSetup3D;if(ortho)params.m_PerspDist=0;device.Reverse(reverse);Maths::HMat44 matrix;view->CalculateProjectionMatrix(params,matrix);
        auto nearPoint=Maths::HVec4(0,0,params.m_NearClip,1)*matrix;auto farPoint=Maths::HVec4(0,0,params.m_FarClip,1)*matrix;
        const F32 nearZ=F32(nearPoint.Z()/nearPoint.W()),farZ=F32(farPoint.Z()/farPoint.W());
        require(std::abs(nearZ-(reverse?1.f:0.f))<1e-4f&&std::abs(farZ-(reverse?0.f:1.f))<1e-4f,"Original projection near/far depth mapping changed");
    }
    device.Reverse(false);view->Setup(setup,640,480);
    const auto& frustum=view->GetViewFrustum();
    require(frustum.SphereCull(Maths::C_Sphere(Maths::HVec3(0,0,10),1))!=0&&frustum.SphereCull(Maths::C_Sphere(Maths::HVec3(100000,0,10),1))==0,"Original frustum rejected center or accepted distant outside sphere");
    device.SetAspectRatio(2.4f);require(std::abs(device.GetAspectRatio()-2.4f)<1e-6f,"Native CPU device lost original PC dynamic aspect");
    device.ClearViews();require(device.GetViewCount()==0,"Original view teardown leaked entries");
}
int main(){try{
    bool rejected=false;try{NativeDeviceBase();}catch(const std::logic_error&){rejected=true;}require(rejected,"Unbound upper CPU device fabricated state");
    {
        CPUDevice device;C_NativeDeviceBinding binding(device);require(&NativeDeviceBase()==&device&&!device.IsCreated(),"CPU binding falsely created a GPU device");
        CPUDevice other;rejected=false;try{C_NativeDeviceBinding duplicate(other);}catch(const std::logic_error&){rejected=true;}require(rejected&&&NativeDeviceBase()==&device,"Second CPU device replaced live binding");
        projection(device);
        Renderer good("native lifecycle fixture");require(device.CreateRenderers()&&device.CreateRenderersVolatile()&&device.InitialiseRenderers(),"Original renderer initialization failed");
        device.ResetRenderers();require(good.created==1&&good.volatiles==1&&good.initialised==1&&good.resets==1,"Original renderer phase counts changed");
        device.ShutdownRenderers();device.DestroyRenderersVolatile();device.DestroyRenderers();require(good.shutdown==1&&good.volatileDestroyed==1&&good.destroyed==1,"Original renderer shutdown order/count failed");
        Renderer failed("native partial-failure fixture");failed.fail=true;require(!failed.Create()&&!failed.Created(E_RendererState_Involatile)&&failed.destroyed==1,"Original partial renderer failure skipped cleanup");
        device.AssertEverythingHasBeenDestroyed();
    }
    rejected=false;try{NativeDeviceBase();}catch(const std::logic_error&){rejected=true;}require(rejected,"Upper CPU device binding outlived its scope");
    std::cout<<"Original upper Display CPU state/views/projections/frustum/renderer lifecycle PASS (GPU game device unimplemented)\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
