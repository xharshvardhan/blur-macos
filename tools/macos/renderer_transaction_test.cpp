// [Blur2PC][macOS] Real tracked GPU allocation rollback at renderer lifecycle boundaries.
#include <display.h>
#include <display/utils/Renderer.h>
#include <stdexcept>
#include <string>
#include <iostream>
static void require(bool value,const char* why){if(!value){std::cerr<<why<<std::endl;throw std::runtime_error(why);}}
namespace {
enum class Outcome {Success,False,Throw};
struct CreationError:std::runtime_error{using std::runtime_error::runtime_error;};
class Renderer final:public Display::I_Renderer {
public:
    Display::C_Device& device;
    Display2::C_VertexBuffer base;
    Display2::C_Surface surface;
    Display2::C_IndexBuffer initialised;
    Outcome create=Outcome::Success,volatileCreate=Outcome::Success,initialise=Outcome::Success;
    U32 creates=0,destroys=0,volatileCreates=0,volatileDestroys=0,initialises=0,shutdowns=0;
    bool cleanupThrows=false;
    Renderer(Display::C_Device& device):I_Renderer("Native transaction fixture"),device(device){}
    ~Renderer()override{require(!base.IsCreated()&&!surface.IsCreated()&&!initialised.IsCreated(),"Renderer retained a real GPU allocation");}
private:
    static bool finish(Outcome outcome,const char* stage){if(outcome==Outcome::Throw)throw CreationError(stage);return outcome==Outcome::Success;}
    void cleanupError(){if(cleanupThrows)throw std::runtime_error("secondary cleanup error");}
    bool OnCreate()override{++creates;require(device.GetDisplay2().CreateVertexBuffer(64,0,&base),"Renderer base GPU allocation failed");return finish(create,"original base creation error");}
    void OnDestroy()override{++destroys;device.GetDisplay2().Destroy(&base);cleanupError();}
    bool OnCreateVolatile()override{++volatileCreates;require(device.GetDisplay2().CreateSurface(4,3,Display2::E_Flags_Colour,Display2::E_SurfaceFormat_A8R8G8B8,Display2::E_Multisample_None,nullptr,&surface),"Renderer volatile GPU allocation failed");return finish(volatileCreate,"original volatile creation error");}
    void OnDestroyVolatile()override{++volatileDestroys;device.GetDisplay2().Destroy(&surface);cleanupError();}
    bool OnInitialise()override{++initialises;require(device.GetDisplay2().CreateIndexBuffer(16,0,Display2::E_IndexFormat_16,&initialised),"Renderer initialization GPU allocation failed");return finish(initialise,"original initialization error");}
    void OnShutdown()override{++shutdowns;device.GetDisplay2().Destroy(&initialised);cleanupError();}
};
template<class Callback> void failure(Outcome outcome,const char* expected,Callback callback){
    if(outcome==Outcome::False){require(!callback(),"Failed lifecycle reported success");return;}
    bool original=false;try{callback();}catch(const CreationError& error){original=std::string(error.what())==expected;}
    require(original,"Rollback replaced/lost the original creation exception type/message");
}
void base(Display::C_Device& device,Outcome outcome,bool cleanupThrows){
    Renderer r(device);require(!r.CreateVolatile()&&r.volatileCreates==0&&!r.IsRendererCreated(),"An uncreated base renderer reported volatile readiness");
    r.create=outcome;r.cleanupThrows=cleanupThrows;failure(outcome,"original base creation error",[&]{return r.Create();});
    require(!r.base.IsCreated()&&!r.Created(Display::E_RendererState_Involatile)&&r.destroys==1,"Base failure skipped GPU rollback or published readiness");
    r.cleanupThrows=false;r.create=Outcome::Success;require(r.Create()&&r.Create()&&r.creates==2,"Base retry/repeated Create failed");require(r.CreateVolatile()&&r.Initialise(),"Successful renderer retry failed later lifecycle");r.Shutdown();r.Shutdown();r.DestroyRenderer();r.DestroyRenderer();
    require(r.destroys==2&&r.volatileDestroys==1&&r.shutdowns==1,"Successful retry teardown/initialization ran more than once");
}
void volatileStage(Display::C_Device& device,Outcome outcome,bool cleanupThrows){
    Renderer r(device);require(r.Create(),"Volatile fixture base creation failed");r.volatileCreate=outcome;r.cleanupThrows=cleanupThrows;failure(outcome,"original volatile creation error",[&]{return r.CreateVolatile();});
    require(r.base.IsCreated()&&!r.surface.IsCreated()&&!r.Created(Display::E_RendererState_Volatile)&&r.volatileDestroys==1&&r.destroys==0,"Volatile rollback lost its base or retained partial GPU surface");
    r.cleanupThrows=false;r.volatileCreate=Outcome::Success;require(r.CreateVolatile()&&r.CreateVolatile()&&r.volatileCreates==2,"Volatile retry/repeated Create failed");r.DestroyRenderer();r.DestroyRenderer();require(r.volatileDestroys==2&&r.destroys==1,"Volatile retry teardown did not complete once");
}
void initialiseStage(Display::C_Device& device,Outcome outcome,bool cleanupThrows){
    Renderer r(device);require(r.Create()&&r.CreateVolatile(),"Initialization fixture GPU creation failed");r.initialise=outcome;r.cleanupThrows=cleanupThrows;failure(outcome,"original initialization error",[&]{return r.Initialise();});
    require(!r.Initialised()&&!r.initialised.IsCreated()&&r.shutdowns==1&&r.IsRendererCreated(),"Initialization failure retained GPU state or destroyed independent creation resources");
    r.Shutdown();require(r.shutdowns==1,"Failed initialization rollback was delivered twice");r.cleanupThrows=false;r.initialise=Outcome::Success;require(r.Initialise()&&r.Initialise()&&r.initialises==2,"Initialization retry was not transactional/idempotent");require(r.Reinitialise()&&r.initialises==3&&r.shutdowns==2,"Reinitialise lost its successful shutdown/recreation contract");r.Shutdown();r.Shutdown();r.DestroyRenderer();require(r.shutdowns==3,"Final initialization shutdown repeated");
}
}
int main(){try{
    Display::C_Device device;Display::C_DeviceParams params;params.m_Width=16;params.m_Height=12;require(device.Create(params)&&device.CreateVolatile(params),"Renderer fixture native device failed");
    for(auto outcome:{Outcome::False,Outcome::Throw}){base(device,outcome,false);volatileStage(device,outcome,false);initialiseStage(device,outcome,false);}
    base(device,Outcome::Throw,true);volatileStage(device,Outcome::Throw,true);initialiseStage(device,Outcome::Throw,true);
    require(device.GetDisplay2().WaitIdle(),"Renderer fixture GPU completion failed");device.DestroyVolatile();device.Destroy();
    std::cout<<"Original I_Renderer real GPU Create/CreateVolatile/Initialise false+exception rollback, original error preservation, truthful readiness, successful retry/reinitialise and exactly-once teardown PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
