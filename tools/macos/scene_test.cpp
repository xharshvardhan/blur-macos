// [Blur2PC][macOS] Original scene graph, resource and world objects linked as a complete closure.
#include <core/core.h>
#include <core/maths.h>
#include <display.h>
#include <fx/Fx.h>
#include <scenegraph/SceneGraphImpl.h>
#include <scenegraph/SceneGraph.h>
#include <scenegraph/light/Manager.h>
#include <btl/utility/autopointer.h>
#include <misc_shared/archive/archive.h>
#include <renderingdata/target.h>
#include <renderingdata/streaming.h>
#include <iostream>
#include <stdexcept>
static void require(bool value,const char* message){if(!value){std::cerr<<message<<'\n';throw std::runtime_error(message);}}
int main(){try{
    struct SchedulerLifetime{SchedulerLifetime(){Scheduler::Init();Scheduler::SetGroupPoolSize(32);Scheduler::jqEnableWorkers(Scheduler::JQ_CORE_1);Scheduler::Start();}~SchedulerLifetime(){Scheduler::Shutdown();}} scheduler;
    Display::C_Device device;Display::C_DeviceParams params;params.m_Width=params.m_Height=16;
    require(device.Create(params)&&device.CreateVolatile(params),"Scene device initialization failed");FX::Initialise();
    SceneGraph::C_PointLightHandle expiredPoint;
    SceneGraph::C_SpotLightHandle expiredSpot;
    SceneGraph::C_DropShadowHandle expiredShadow;
    {
        SceneGraph::C_SceneGraph scene;scene.SetNumRenderPasses(2);scene.RegisterRenderPass(0);scene.RegisterRenderPass(1);
        require(scene.GetNumRenderPasses()==2,"Original scene render-pass configuration failed");
        auto* lights=scene.GetLightManager();lights->ReservePointLights(1);
        auto point=SceneGraph::AddPointLight(&scene);require(point.Object(),"Original scene point-light pool allocation failed");
        point.Object()->SetDimensions(Maths::HVec3(1,2,3),4);point.Object()->SetColour(.25f,.5f,1.f);
        require(!SceneGraph::AddPointLight(&scene).Object(),"Original scene light limit was ignored");
        SceneGraph::C_PointLight copiedPoint(*point.Object());SceneGraph::C_PointLightHandle copiedPointHandle(&copiedPoint);
        SceneGraph::C_PointLight assignedPoint;SceneGraph::C_PointLightHandle assignedPointHandle(&assignedPoint);assignedPoint=*point.Object();
        SceneGraph::RemovePointLight(&scene,point);
        require(copiedPointHandle.Object()==&copiedPoint&&assignedPointHandle.Object()==&assignedPoint,"Destroying source light invalidated a distinct copy/assignment");
        require(!point.Object(),"Original scene stale light handle remained valid");
        auto replacement=SceneGraph::AddPointLight(&scene);require(replacement.Object(),"Original scene pool did not recover its freed slot");
        require(!point.Object(),"A stale point-light handle revived after pool reuse");
        const auto constPoint=point;require(!constPoint.Object(),"Const stale point-light handle revived after pool reuse");
        SceneGraph::RemovePointLight(&scene,point);require(replacement.Object(),"Removing a stale handle removed its replacement");
        expiredPoint=replacement;SceneGraph::RemovePointLight(&scene,replacement);
        auto spot=SceneGraph::AddSpotLight(&scene);auto shadow=SceneGraph::AddDropShadow(&scene);require(spot.Object()&&shadow.Object(),"Original scene secondary light allocation failed");
        expiredSpot=spot;expiredShadow=shadow;
        SceneGraph::RemoveSpotLight(&scene,spot);SceneGraph::RemoveDropShadow(&scene,shadow);
        require(!spot.Object()&&!shadow.Object(),"Removed secondary lights retained valid handles");
        auto nextSpot=SceneGraph::AddSpotLight(&scene);auto nextShadow=SceneGraph::AddDropShadow(&scene);
        require(nextSpot.Object()&&nextShadow.Object()&&!spot.Object()&&!shadow.Object(),"Secondary light handle revived after pool reuse");
        SceneGraph::RemoveSpotLight(&scene,nextSpot);SceneGraph::RemoveDropShadow(&scene,nextShadow);
        scene.SetNumMatrixPalettes(2);const Maths::Mat43 matrix;scene.SetMatrixPalette(1,&matrix);require(scene.GetNumMatrixPalettes()==2&&scene.GetMatrixPalette(1)==&matrix,"Original scene palette ownership changed");scene.ClearMatrixPalettes(0);
    }
    require(!expiredPoint.Object()&&!expiredSpot.Object()&&!expiredShadow.Object(),"A stale handle read storage after its scene manager was destroyed");
    RenderingData::Target::StreamManager::Instance().Destroy(); // Stop streaming before the fixture scheduler.
    FX::Shutdown();device.DestroyVolatile();device.Destroy();std::cout<<"Original scene graph/render passes/light pools/handle invalidation/palette ownership PASS (authored fixture, not a game scene)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
