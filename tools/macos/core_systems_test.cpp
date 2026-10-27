// [Blur2PC][macOS] Original CoreSystems startup/update/shutdown with real resource singleton, queued I/O and native debug services.
#include <core/Core.h>
#include <core/containers.h>
#include <system/serialise.h>
#include <system/serialise/macos/NativeFile.h>
#include <system/io/Async.h>
#include <system/io/macos/NativeIo.h>
#include <framework/application/CoreSystems.h>
#include <Resource/Resource.h>
#include <filesystem>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <array>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
using namespace Framework;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
class Core final:public C_CoreSystems{public:U32 shutdownCalls=0;Resource::C_Manager* Resources()const{return m_pResourceManager;}void Shutdown()override{++shutdownCalls;C_CoreSystems::Shutdown();}};
static void read(const std::filesystem::path& root){
    IoFile file;auto opened=IoFileOpen(file,(root/"payload.bin").c_str(),nullptr);opened.Wait();require(opened.Error()==kIoError_Okay&&file.Size()==64,"Core-owned I/O could not open the authored payload");
    std::array<U8,64> data{};auto request=IoFileRead(file,data.data(),0,data.size(),kIoPriority_High,nullptr);request.Wait();require(request.Error()==kIoError_Okay&&request.ActualSize()==data.size(),"Core-owned worker read failed");for(U32 i=0;i<data.size();++i)require(data[i]==U8(i*17+3),"Core-owned async payload bytes changed");auto closed=IoFileClose(file,nullptr);closed.Wait();require(closed.Error()==kIoError_Okay,"Core-owned I/O close failed");
}
int main(int argc,char** argv){try{
    require(argc==3,"Core system fixture directory/ownership mode missing");const auto root=std::filesystem::path(argv[1]);const bool borrowed=std::string(argv[2])=="borrowed";
    {Core unused;require(!unused.IsInitialised()&&!unused.Resources()&&!unused.NetworkInitialised(),"Unused core reported live services");unused.Shutdown();}
    if(borrowed)require(IoInitialise(),"External I/O owner setup failed");
    {
        Core core;C_CoreSystemsParams params;params.m_Name=borrowed?"NativeBorrowedCoreVerification":"NativeOwnedCoreVerification";params.m_WorkingDir=(root/"not-present").string().c_str();
        require(FAILED(core.Initialise(params))&&!core.IsInitialised()&&!core.Resources()&&!core.NetworkInitialised()&&!Resource::C_Manager::GetInstance(),"Invalid native working directory created or reported initialized services");
        params.m_WorkingDir=root.string().c_str();
#ifndef CFG_GOLD
        // [Blur2PC] Fail real debug-service startup before the base resource manager
        // exists. Base rollback must not dispatch a derived game's Shutdown().
        const int occupied=socket(AF_INET,SOCK_STREAM,0);require(occupied>=0,"Cannot create service-startup failure fixture");
        sockaddr_in address{};address.sin_len=sizeof(address);address.sin_family=AF_INET;address.sin_port=htons(6317);address.sin_addr.s_addr=htonl(INADDR_ANY);
        const int reuse=1;setsockopt(occupied,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
        if(bind(occupied,reinterpret_cast<sockaddr*>(&address),sizeof(address))||listen(occupied,1)){close(occupied);throw std::runtime_error("Cannot reserve isolated native debug-service port");}
        const U32 before=core.shutdownCalls;const HRESULT failed=core.Initialise(params);close(occupied);
        require(FAILED(failed)&&core.shutdownCalls==before&&!core.IsInitialised()&&!core.Resources()&&!core.NetworkInitialised()&&!Resource::C_Manager::GetInstance(),"Failed base startup entered derived shutdown or retained its services");
        require(Sys::IsNativeIoInitialised()==borrowed,"Failed base startup stopped borrowed I/O or retained its own worker");
#endif
        require(SUCCEEDED(core.Initialise(params))&&core.IsInitialised()&&core.Resources()&&Resource::C_Manager::GetInstance()==core.Resources()&& &C_CoreSystems::Instance()==&core,"Original core/resource startup failed");
        require(Sys::IsNativeIoInitialised()&&Sys::GetNativeIoStats().workers>0,"Original core startup did not use real I/O workers");
#ifndef CFG_GOLD
        require(core.NetworkInitialised(),"Original configured debug-network startup did not complete");
#endif
        require(FAILED(core.Initialise(params))&&core.IsInitialised()&&Resource::C_Manager::GetInstance()==core.Resources(),"Duplicate initialization damaged the existing core");
        read(root);for(U32 i=0;i<16;++i)core.Update();
        core.Shutdown();require(!core.IsInitialised()&&!core.Resources()&&!core.NetworkInitialised()&&!Resource::C_Manager::GetInstance(),"Original core shutdown retained live resource/service flags");core.Shutdown();
        require(Sys::IsNativeIoInitialised()==borrowed,"Core shutdown stopped an external I/O owner or retained its own workers");
    }
    if(borrowed){read(root);IoShutdown();}
    const auto counts=Sys::GetNativeIoStats();require(!Sys::IsNativeIoInitialised()&&!counts.files&&!counts.requests&&!counts.handles&&!counts.workers,"Native core I/O teardown retained resources");
    std::cout<<"Original CoreSystems invalid-root rollback/resource singleton/actual async bytes/update/owned versus borrowed I/O shutdown PASS (not game startup)\n";return 0;
}catch(const std::exception& error){IoShutdown();std::cerr<<error.what()<<'\n';return 1;}}
