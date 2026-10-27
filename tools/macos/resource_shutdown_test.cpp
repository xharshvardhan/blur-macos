// [Blur2PC][macOS] Authored resource callbacks exercise the original asynchronous
// loader, dependency handles, cache deadlines and actual GPU payload lifetime.
#include <core/core.h>
#include <core/maths.h>
#include <display.h>
#include <display2/Utils/Utils.h>
#include <resource/Resource.h>
#include <system/serialise.h>
#include <atomic>
#include <memory>
#include <iostream>
#include <stdexcept>

static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
namespace {
std::atomic<unsigned> allocations{0},frees{0},posts{0},pres{0};
MT::Event* entered=nullptr;
MT::Event* resume=nullptr;
MT::ThreadId mainThread;
class Payload : public Resource::C_Resource<Payload> {
    U8* bytes=nullptr;
    U8 mode=0;
    Display::C_Texture2D texture;
    Resource::C_Handle<Payload> child;
public:
    static Util::C_String StaticGetResourceTypeName(){return "NativeShutdownFixture";}
    bool LoadResourceData() override {
        std::unique_ptr<Sys::I_File> file(OpenResourceFile());
        if(!file||!file->Read(&mode,1))return false;
        bytes=new U8[32];++allocations;
        if(mode==3){entered->Signal();require(resume->Wait(5000),"Authored blocked resource was not released");}
        if(mode==4){entered->Signal();throw std::runtime_error("authored resource decoder failure");}
        if(mode==5){delete[] bytes;bytes=nullptr;++frees;return false;}
        if(mode==2)require(child.Load("child.bin"),"Authored dependency failed to queue");
        return true;
    }
    bool PostLoadResourceData() override {
        require(MT::GetThreadId()==mainThread,"Original PostLoad callback ran off the owner thread");
        require(Display::Device()->CreateTexture2D(2,2,1,0,Display::E_Format_A8R8G8B8,&texture),"Authored GPU payload creation failed");
        ++posts;return true;
    }
    bool PreFreeResourceData() override {
        require(MT::GetThreadId()==mainThread,"Original PreFree callback ran off the owner thread");
        ++pres;return true;
    }
    bool FreeResourceData() override {
        require(MT::GetThreadId()==mainThread,"Shutdown Free callback ran off the owner thread");
        require(Display::DeviceExists(),"Payload outlived its device");
        if(texture.IsCreated())Display::Device()->DestroyTexture2D(&texture);
        child=static_cast<Payload*>(nullptr);
        if(bytes){delete[] bytes;bytes=nullptr;++frees;}
        return true;
    }
    S32 GetEvictDelay() override{return 1000000;} // Original cache Update epochs, not wall time.
    bool HasTexture()const{return texture.IsCreated();}
};
void write(Sys::C_FileSystem& fs,const char* path,U8 mode){std::unique_ptr<Sys::I_File> out(fs.Open(path,Sys::E_FileAccess_Write));require(out&&out->Write(&mode,1),"Authored resource file creation failed");}
}
int main(int argc,char** argv){try{
    require(argc==2,"Resource fixture directory missing");
    auto& fs=Sys::C_FileSystem::Instance();require(fs.SetWorkingDirectory(argv[1]),"Resource fixture filesystem setup failed");
    write(fs,"live.bin",1);write(fs,"parent.bin",2);write(fs,"child.bin",1);write(fs,"pending.bin",3);write(fs,"throw.bin",4);write(fs,"failed.bin",5);
    mainThread=MT::GetThreadId();MT::Event started,release;entered=&started;resume=&release;
    Display::C_Device device;Display::C_DeviceParams params;params.m_Width=params.m_Height=8;
    require(device.Create(params)&&device.CreateVolatile(params),"Resource fixture display initialization failed");
#if TRACK_DISPLAY2_RESOURCES
    const auto baseline=Display2::Device()->GetTextureCount();
#endif
    {
        Resource::C_Manager manager;
        require(manager.NativeDrainUnreferencedResources(),"Empty resource owner failed to drain");
        for(unsigned lifetime=0;lifetime<2;++lifetime){
            Resource::C_Handle<Payload> live;require(live.Load("live.bin"),"Original resource load failed to queue");
            require(Resource::WaitForResourceData(live),"Original resource did not become ready");
            require(static_cast<Payload*>(live)->HasTexture(),"Ready resource owns no actual GPU payload");
#if TRACK_DISPLAY2_RESOURCES
            require(Display2::Device()->GetTextureCount()==baseline+1,"Ready resource owns no actual tracked GPU payload");
#endif
            require(!manager.NativeDrainUnreferencedResources()&&static_cast<Payload*>(live)->HasTexture(),"Shutdown consumed a live resource handle");
            live=static_cast<Payload*>(nullptr);
            require(manager.NativeDrainUnreferencedResources(),"Unreferenced delayed resource did not drain");
#if TRACK_DISPLAY2_RESOURCES
            require(Display2::Device()->GetTextureCount()==baseline,"Cache retirement retained its real GPU payload");
#endif
        }
        Resource::C_Handle<Payload> parent;require(parent.Load("parent.bin"),"Parent resource did not queue");
        manager.WaitForAllResourceData();require(static_cast<Payload*>(parent)->HasTexture(),"Original parent never completed PostLoad");parent=static_cast<Payload*>(nullptr);
        require(manager.NativeDrainUnreferencedResources(),"Cascading child resource ownership did not drain");
        Resource::C_Handle<Payload> pending;require(pending.Load("pending.bin"),"Pending resource did not queue");
        require(started.Wait(5000),"Original resource worker did not enter authored Load");
        const auto postsBefore=posts.load();pending=static_cast<Payload*>(nullptr);release.Signal();
        require(manager.NativeDrainUnreferencedResources()&&posts==postsBefore,"Cancelled pending resource unnecessarily ran PostLoad or failed cleanup");
        Resource::C_Handle<Payload> failed;require(failed.Load("failed.bin"),"Failed resource did not queue");manager.WaitForAllResourceData();
        require(!static_cast<Payload*>(failed)->IsDataValid(),"Original failed decoder was published valid");failed=static_cast<Payload*>(nullptr);
        require(manager.NativeDrainUnreferencedResources(),"Failed resource owner did not drain");
        Resource::C_Handle<Payload> throwing;require(throwing.Load("throw.bin"),"Throwing resource did not queue");
        require(started.Wait(5000),"Original worker never entered the throwing decoder");
        throwing=static_cast<Payload*>(nullptr);
        require(!manager.NativeDrainUnreferencedResources(),"Shutdown hid an actual resource worker exception");
        Resource::C_Handle<Payload> retry;require(retry.Load("live.bin")&&Resource::WaitForResourceData(retry)&&static_cast<Payload*>(retry)->HasTexture(),"Original worker did not recover for a new application after failure");
        retry=static_cast<Payload*>(nullptr);
        require(manager.NativeDrainUnreferencedResources(),"Repeated resource owner drain failed");
        require(allocations==frees&&posts==pres,"Resource shutdown leaked allocations or unbalanced PostLoad/PreFree");
#if TRACK_DISPLAY2_RESOURCES
        require(Display2::Device()->GetTextureCount()==baseline,"Resource shutdown retained its real GPU payload");
#endif
    }
    Resource::C_TypeRegistry::DeleteInstance();
    device.DestroyVolatile();device.Destroy();
    std::cout<<"Original resource worker/live-handle refusal/cache deadline/dependency/pending failure/GPU ownership/retry shutdown PASS (authored payloads, not actual game cleanup)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
