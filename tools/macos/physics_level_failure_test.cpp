// [Blur2PC][macOS] Original block writer/importer ownership and failure propagation.
#include <core/Core.h>
#include <core/Maths.h>
#include <core/containers.h>
#include <system/serialise.h>
#include <system/serialise/C_FileBlock.h>
#include <system/serialise/macos/NativeFile.h>
#include <game_resource/PhysicsLevel.h>
#include <gravity/sdk/PhysicsSDK.h>
#include <gravity/sharedphysics/PhysicsVersion.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace GameResource { bool ValidatePlatformId(Sys::I_File*); }
namespace {
void require(bool v,const char* text){if(!v)throw std::runtime_error(text);}
std::vector<U8> emptyImport(){
    U32 head;std::memcpy(&head,"Head",4);
    const U32 fields[]={head,GRAVITY_CURRENT_VERSION,0x0fffffff,0xffffffff};
    return std::vector<U8>(reinterpret_cast<const U8*>(fields),reinterpret_cast<const U8*>(fields)+sizeof(fields));
}
void writeBlock(Sys::C_FileBlock& parent,const char* name,const std::vector<U8>& bytes){
    Sys::C_FileBlock block(&parent,name);require(block.Write(bytes.data(),U32(bytes.size())),"Original block writer failed");
}
void writeLevel(const std::filesystem::path& path,int scenario,const std::vector<U8>& main=emptyImport()){
    auto file=Sys::NativeFile::Open(path,Sys::E_FileAccess_Write);
    Sys::C_FileBlock root(file.get(),"Physic");
    if(scenario!=12){Sys::C_FileBlock header(&root,"Header");U32 version=0;S32 routes=scenario==2?2:scenario==3?0:scenario==4?-1:scenario==5?0x7fffffff:scenario==6?0:1;
     require(header.Write(&version,sizeof(version)),"Header version write failed");if(scenario!=7)require(header.Write(&routes,sizeof(routes)),"Header count write failed");}
    if(scenario!=1)writeBlock(root,"Main",main);
    if(scenario==8)writeBlock(root,"Main",main);
    if(scenario!=6&&scenario!=12){Sys::C_FileBlock routes(&root,"Routes");auto payload=emptyImport();if(scenario==9)payload.pop_back();writeBlock(routes,"Route",payload);}
}
bool rejected(GameResource::C_PhysicsLevel& level,const std::filesystem::path& path){
    auto file=Sys::NativeFile::Open(path,Sys::E_FileAccess_Read,17);file->SetVersionNumber(73);
    bool failed=false;try{failed=!level.Load(file.get());}catch(const std::runtime_error&){failed=true;}
    require(file->GetVersionNumber()==73,"Failed import changed its caller's parser version");return failed;
}
void readSuccess(GameResource::C_PhysicsLevel& level,const std::filesystem::path& path,S32 routes){
    auto file=Sys::NativeFile::Open(path,Sys::E_FileAccess_Read,19);file->SetVersionNumber(73);
    require(level.Load(file.get())&&level.GetMainImportScene()&&level.GetRouteCount()==routes,"Valid original container import failed");
    require(file->GetVersionNumber()==73&&file->GetPosition()==file->Length(),"Successful import changed caller version or stopped early");
    for(S32 i=0;i<routes;++i)require(level.GetRouteImportScene(i),"Declared route was not published");
}
std::vector<U8> originalMain(const char* path){
    auto file=Sys::NativeFile::Open(path);file->SetPosition(4);Sys::C_FileBlock root(file.get());require(root.HeaderId()=="Physic","Private original asset is not a physics container");
    std::vector<U8> bytes;
    while(root.GetPosition()<root.Length()){Sys::C_FileBlock block(&root);if(block.HeaderId()=="Main"){bytes.resize(block.Length()-block.GetPosition());require(block.Read(bytes.data(),U32(bytes.size())),"Original Main payload read failed");}else block.SetPosition(block.Length());}
    require(bytes.size()>1,"Original Main payload missing");return bytes;
}
}
int main(int argc,char** argv){try{
    require(argc==2||argc==3,"Supply scratch directory and optional private original Level.levelphysics");
    const auto directory=std::filesystem::path(argv[1])/"physics-level-failure-fixtures";std::filesystem::create_directories(directory);
    const auto good=directory/"valid.physics";writeLevel(good,0);
    for(int cycle=0;cycle<2;++cycle){
        GameResource::C_PhysicsLevel level;readSuccess(level,good,1);auto* previous=level.GetMainImportScene();auto* route=level.GetRouteImportScene(0);
        for(int scenario:{1,2,3,4,5,7,8,9}){
            const auto path=directory/(std::to_string(scenario)+".physics");writeLevel(path,scenario);
            require(rejected(level,path),"Malformed original physics container reported success");
            require(level.GetMainImportScene()==previous&&level.GetRouteCount()==1&&level.GetRouteImportScene(0)==route,"Failed load published partial scenes or retired prior successful ownership");
            GameResource::C_PhysicsLevel empty;require(rejected(empty,path)&&!empty.GetMainImportScene()&&empty.GetRouteCount()==0,"Failed first load published a partial scene");
        }
        const auto shortHeader=directory/"short-header.physics";{std::ofstream file(shortHeader,std::ios::binary);file.write("Physic",6);}
        require(rejected(level,shortHeader),"Truncated original block header reported success");
        const auto overrun=directory/"overrun.physics";std::filesystem::copy_file(good,overrun,std::filesystem::copy_options::overwrite_existing);
        {std::fstream file(overrun,std::ios::binary|std::ios::in|std::ios::out);file.seekp(8);U32 size=0xffffffff;file.write(reinterpret_cast<const char*>(&size),4);}
        require(rejected(level,overrun),"Block declaring bytes beyond its parent reported success");
        const auto noRoutes=directory/"zero-route.physics";writeLevel(noRoutes,6);readSuccess(level,noRoutes,0);readSuccess(level,good,1);const auto noHeader=directory/"headerless.physics";writeLevel(noHeader,12);readSuccess(level,noHeader,0);readSuccess(level,good,1);
        const auto legacy=directory/"legacy.physics";{auto raw=emptyImport();std::ofstream file(legacy,std::ios::binary);file.write(reinterpret_cast<const char*>(raw.data()),raw.size());}
        readSuccess(level,legacy,0);readSuccess(level,good,1);
        if(argc==3){
            auto originalFile=Sys::NativeFile::Open(argv[2],Sys::E_FileAccess_Read,37);
            require(GameResource::ValidatePlatformId(originalFile.get()),"Private original physics platform rejected");originalFile->SetVersionNumber(73);
            require(level.Load(originalFile.get())&&level.GetMainImportScene()&&originalFile->GetVersionNumber()==73&&originalFile->GetPosition()==originalFile->Length(),"Complete original physics container failed native transaction");
            for(S32 i=0;i<level.GetRouteCount();++i)require(level.GetRouteImportScene(i),"Original declared route missing after commit");
            std::cout<<"Original complete physics container routes="<<level.GetRouteCount()<<" loaded\n";
            auto payload=originalMain(argv[2]);const auto original=directory/"actual-main.physics";writeLevel(original,6,payload);readSuccess(level,original,0);payload.pop_back();const auto clipped=directory/"actual-main-truncated.physics";writeLevel(clipped,6,payload);auto* actual=level.GetMainImportScene();require(rejected(level,clipped)&&level.GetMainImportScene()==actual,"Truncated actual Main payload escaped rollback");readSuccess(level,good,1);}
    }
    std::cout<<"Original physics container/importer: Main/route/count/header failures, parser version, unpublished partial owners, prior owner preservation, legacy fallback and retry PASS"<<(argc==3?"; actual Main/truncation included":"; authored data only")<<" (not a simulation or race)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
