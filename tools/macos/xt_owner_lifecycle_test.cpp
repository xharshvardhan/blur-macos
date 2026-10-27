// [Blur2PC][macOS] Actual registered creator records and explicit global Xt owner retirement, not gameplay.
#include "../../code/amax/game/GameLogic/stdafx.h"
#include <GameApp.h>
#include <GameLogic/GameData/GameXtLoader.h>
#include <GameLogic/GameData/MultiplayerData.h>
#define SCENEGRAPH_ALLOW_INCLUDE
#include <scenegraph/SceneGraphImpl.h>
#include <dynamic/Renderer.h>
#include <dynamic/ManagerInterface.h>
#include <resource/Resource.h>
#include <system/scheduler/jobqueue.h>
#include <system/memory/MemoryTracker.h>
#include <atomic>
#include <chrono>
#include <actor_system/GameComponent.h>
#include <shared_actors/Components/Creation/MovementComponentsCreate.h>
#include <dynamic/Generic/XtInterface.h>
#include <racingphysics/racingphysics.h>
#include "../../code/mercury/library/audio/include/XtAudio.h"
#include <system/serialise/C_FileSystem.h>
#include <system/serialise/FileSystemGlobals.h>
#include "../../code/mercury/library/common/xt2/source/Common/XtFlask.h"
#include "../../code/mercury/library/common/xt2/source/game/XtGameImpl.h"
#include <filesystem>
#include <vector>
#include <cstring>
#include <iostream>
#include <fstream>
#include <stdexcept>
namespace Game {HRESULT NativeLoadRequiredRaceBookXt();extern Debug::Bool g_bootResourceManagerPause;}
namespace {
using Bytes=std::vector<XtU8>;
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void Align(Bytes& bytes){bytes.resize((bytes.size()+3)&~size_t(3));}
template<class T> FlaskBlob Blob(Bytes& bytes,const std::vector<T>& values){Align(bytes);FlaskBlob b{XtU32(bytes.size()),XtU32(values.size())};if(!values.empty()){const auto* p=reinterpret_cast<const XtU8*>(values.data());bytes.insert(bytes.end(),p,p+values.size()*sizeof(T));}return b;}
XtU16 Name(Bytes& bytes,const char* text){const auto offset=bytes.size();bytes.insert(bytes.end(),text,text+std::strlen(text)+1);return XtU16(offset);}
Bytes Pack(XtId id,XtU32 uid){
    Bytes names;const XtType* atom=nullptr;auto* schema=XtGetApplicationSchema();
    for(int i=0;i<schema->typeCount;++i)if(schema->types[i].behaviour==kXtTbAtom&&schema->types[i].atom==kXtU32)atom=&schema->types[i];
    Require(atom,"Actual U32 schema atom absent");
    // PC32 inheritance contains the persistent UID word, with no native vtable/pointer image.
    std::vector<FlaskType> types{
        {Name(names,atom->name),0,0,kXtTbAtom,kXtU32,4},
        {Name(names,"ComponentCreator"),1,0,kXtTbStruct,kXtNotAtom,4},
        {Name(names,"ComponentCreateBase"),0,1,kXtTbStruct,kXtNotAtom,4},
        {Name(names,"MovementComponentBase"),0,1,kXtTbStruct,kXtNotAtom,4},
        {Name(names,"TranslatorMovementComponent"),0,1,kXtTbStruct,kXtNotAtom,4}};
    std::vector<FlaskField> fields{{Name(names,"SerialisableUid"),0,0,kXtFbAtom,0}};
    Bytes bytes(sizeof(FlaskHeader));FlaskHeader header{};header.tag=kXtFlaskTag;header.version=1;
    header.types=Blob(bytes,types);header.bases=Blob(bytes,std::vector<FlaskBase>{{1,0},{2,0},{3,0}});header.fields=Blob(bytes,fields);header.strings=Blob(bytes,names);
    header.refs=Blob(bytes,std::vector<FlaskRef>{{id,4,0}});header.records=Blob(bytes,std::vector<FlaskRecord>{{0,1,4,0}});
    header.components=Blob(bytes,std::vector<FlaskComponent>{{4,1,kXtCbObject}});Bytes data(sizeof(uid));std::memcpy(data.data(),&uid,sizeof(uid));header.data=Blob(bytes,data);std::memcpy(bytes.data(),&header,sizeof(header));return bytes;
}
void OpenDiagnostic(XtPackOpenStatus* status,void*){std::cerr<<"Actual Xt import: "<<status->message<<std::endl;}
XtPack* Open(XtDb* database,XtId id,XtU32 uid){auto bytes=Pack(id,uid);XtPackOpenParams params;params.callback=OpenDiagnostic;auto* pack=XtPackOpen(database,bytes.data(),bytes.size(),&params);Require(pack,"Actual registered creator PC32 Flask import failed");return pack;}
Component::I_ComponentCreator::C_DataRef Ref(XtDb* db,XtId id){XtHandle<Components::C_MovementTranslatorCreateInfo> creator(XtGetRecord(db,id));Require(bool(creator),"Actual creator record lookup failed");auto value=DataRef::SorceryLookup(static_cast<const Component::I_ComponentCreator*>(creator.Object()));Require(!value.IsNull(),"Actual component Sorcery callback did not register UID");return value;}
void Write(const std::filesystem::path& path,const Bytes& bytes){std::filesystem::create_directories(path.parent_path());std::ofstream stream(path,std::ios::binary);stream.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());Require(bool(stream),"Cannot write authored owner fixture pack");}
void RecordsAndHoles(){
    Require(XtInitialise(),"Actual schema initialization failed");XtCallbackContext context(0);
    for(unsigned cycle=0;cycle<3;++cycle){
        auto* database=XtDbCreate(XtGetApplicationSchema(),16,&context);Require(database,"Actual record owner DB allocation failed");
        auto* first=Open(database,0x100,0x7ed14701);auto* second=Open(database,0x200,0x7ed14702);
        auto ref=Ref(database,0x200);auto* storage=DataRef::C_StorageSorcery<Component::I_ComponentCreator>::Lookup(0x7ed14702);Require(storage,"Actual storage missing");
        XtPackClose(database,first);Require(database->count==1&&database->index[0]!=&database->records[0],"Authored close did not exercise a non-dense record slot");
        XtDbDestroy(database); // Independent required residual-record cleanup boundary.
        Require(ref.IsNull()&&storage->GetData()==nullptr,"DbDestroy skipped live record above compacted count");
        delete second; // Metadata only: its DB has deliberately been destroyed by the boundary under test.
        Require(DataRef::C_StorageSorcery<Component::I_ComponentCreator>::Lookup(0x7ed14702)==storage,"Retained DataRef storage retired before last claim");
        ref.SetNull();Require(!DataRef::C_StorageSorcery<Component::I_ComponentCreator>::Lookup(0x7ed14702),"Final actual Sorcery storage claim was not retired");
        Require(!DataRef::C_StorageSorcery<Component::I_ComponentCreator>::Lookup(0x7ed14701),"Unreferenced record storage was retained after pack close");
    }
    XtShutdown();Require(gXt2.references==1,"Record fixture leaked schema acquisitions");
}
void AppAndBorrowedOwners(){
    auto& fs=Sys::C_FileSystem::Instance();const auto previous=fs.GetWorkingDirectory();
    const auto scratch=std::filesystem::absolute(std::filesystem::path(__FILE__).parent_path()/"../../_work/build/macos/xt-owner-agent/files");std::filesystem::create_directories(scratch);
    Require(fs.SetWorkingDirectory(scratch.c_str()),"Cannot use authored Xt owner directory");Sys::SetDefaultFileSystem(&fs);
    for(unsigned cycle=0;cycle<2;++cycle){
        Component::I_ComponentCreator::C_DataRef retained;
        {
            C_GameApp application;application.CreateGlobalXtDatabase();auto* database=application.GetGlobalXtDatabase();Require(database&&gXt2.references==2,"Application did not own exactly one schema ref");application.CreateGlobalXtDatabase();Require(gXt2.references==2,"Repeated app database acquisition leaked ref");
            bool failed=false;try{DynamicEffect::XtInterface::PackOpen(database,"missing/","absent");}catch(const std::runtime_error&){failed=true;}
            Require(failed&&gXt2.references==2&&!DynamicEffect::XtInterface::GetPack(),"Failed effect pack acquisition leaked owner/ref");DynamicEffect::XtInterface::PackClose();
            std::filesystem::remove(scratch/"gamedata/Xt/racingphysics.bin");failed=false;try{RacingPhysics::CreateXt(database);}catch(const std::runtime_error&){failed=true;}
            Require(failed&&gXt2.references==2,"Failed physics pack acquisition leaked schema ref");RacingPhysics::DestroyXt();
            Write(scratch/"dynamiceffects.bin",Pack(0x300,0x7ed14703));DynamicEffect::XtInterface::PackOpen(database,"./");Require(gXt2.references==3,"Effect pack did not claim exact schema ref");DynamicEffect::XtInterface::PackOpen(database,"./");Require(gXt2.references==3,"Duplicate effect open leaked ref");DynamicEffect::XtInterface::PackClose();DynamicEffect::XtInterface::PackClose();Require(gXt2.references==2&&!DataRef::C_StorageSorcery<Component::I_ComponentCreator>::Lookup(0x7ed14703),"Effect close did not retire records/ref exactly");
            Write(scratch/"gamedata/Xt/racingphysics.bin",Pack(0x400,0x7ed14704));RacingPhysics::CreateXt(database);RacingPhysics::CreateXt(database);Require(gXt2.references==3,"Physics borrowed owner ref count changed");RacingPhysics::DestroyXt();RacingPhysics::DestroyXt();Require(gXt2.references==2&&!DataRef::C_StorageSorcery<Component::I_ComponentCreator>::Lookup(0x7ed14704),"Physics close did not retire records/ref exactly");
            Require(!Audio::C_XtAudioDb::NativeIsCreated(),"Unentered audio singleton was constructed by cleanup");
            Audio::C_XtAudioDb::NativeReleaseBorrowed(nullptr);Require(Audio::C_XtAudioDb::Create(),"Actual Audio Xt owner creation failed");
            Write(scratch/"audio-owner.bin",Pack(0x600,0x7ed14706));Audio::C_XtAudioDb::Get()->AudioPack().LoadDefs(Sys::C_FilePath("audio-owner.bin"),database);auto audioRef=Ref(database,0x600);
            Audio::C_XtAudioDb::NativeReleaseBorrowed(database);Audio::C_XtAudioDb::NativeReleaseBorrowed(database);Require(!Audio::C_XtAudioDb::NativeIsCreated()&&audioRef.IsNull()&&gXt2.references==2,"Audio owned pack/singleton retirement changed borrowed schema owner");audioRef.SetNull();Require(!DataRef::C_StorageSorcery<Component::I_ComponentCreator>::Lookup(0x7ed14706),"Audio wrapper retained actual creator storage after final claim");
            auto& loader=Game::GameXtLoader::Instance();loader.Init();loader.AddPack(Game::E_XTPACK_GLOBAL_ACTOR_LIST,"owner.bin");Write(scratch/"owner.bin",Pack(0x500,0x7ed14705));Require(SUCCEEDED(loader.LoadXt()),"Actual global loader failed authored real creator pack");retained=Ref(database,0x500);
            auto* entry=loader.GetPack(Game::E_XTPACK_GLOBAL_ACTOR_LIST);Require(entry,"Actual loader entry vanished");
            entry->mFileTime=0;Require(SUCCEEDED(loader.ReLoadXt()),"Actual successful pack replacement failed");
            Require(!retained.IsNull()&&XtGetRecord(database,0x500)->strongRef==1&&XtGetRecord(database,0x500)->weakRef==1,"Successful replacement retained the prior metadata's record refs");
            std::filesystem::remove(scratch/"owner.bin");entry->mLoaded=false;Require(FAILED(loader.LoadXt())&&!retained.IsNull(),"Failed replacement discarded the previously owned record");
            // Final application owner must close this acquired pack without a test-side pool reset.
        }
        Require(gXt2.references==1&&retained.IsNull(),"Final app close did not retire DB data/exact schema ownership");retained.SetNull();Require(!DataRef::C_StorageSorcery<Component::I_ComponentCreator>::Lookup(0x7ed14705),"Final retained creator reference did not retire storage");
    }
    Sys::SetDefaultFileSystem(nullptr);fs.SetWorkingDirectory(previous);
}

std::atomic<unsigned> startupProbeLoads{0};
class StartupProbe final:public Resource::C_Resource<StartupProbe> {
public:
    static Util::C_String StaticGetResourceTypeName(){return "RequiredStartupResumeProof";}
    bool LoadResourceData()override{std::unique_ptr<Sys::I_File> file(OpenResourceFile());U8 byte=0;if(!file||!file->Read(&byte,1)||byte!=0x5a)return false;++startupProbeLoads;return true;}
    bool FreeResourceData()override{return true;}
};
void VerifyResourceResumed(Resource::C_Manager& resources,const std::filesystem::path& scratch){
    Write(scratch/"resume-proof.bin",Bytes{0x5a});Resource::C_Handle<StartupProbe> proof;const unsigned before=startupProbeLoads.load();Require(proof.Load("resume-proof.bin"),"Resume proof could not queue original resource worker");
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(!static_cast<StartupProbe*>(proof)->IsDataReady()&&std::chrono::steady_clock::now()<until){resources.PartialUpdate();MT::Thread::Sleep(1);}
    Require(static_cast<StartupProbe*>(proof)->IsDataReady()&&startupProbeLoads==before+1,"Failed required startup left original resource worker paused");proof=static_cast<StartupProbe*>(nullptr);Require(resources.NativeDrainUnreferencedResources(),"Resume proof actual resource did not retire");
}
void RequiredStartupBoundaries(){
    auto& fs=Sys::C_FileSystem::Instance();const auto previous=fs.GetWorkingDirectory();const auto scratch=std::filesystem::absolute(std::filesystem::path(__FILE__).parent_path()/"../../_work/build/macos/startup-required-agent/files");std::filesystem::create_directories(scratch);Require(fs.SetWorkingDirectory(scratch.c_str()),"Cannot set required-startup authored directory");Sys::SetDefaultFileSystem(&fs);
    {
        Resource::C_Manager resources;C_GameApp application;application.CreateGlobalXtDatabase();auto* database=application.GetGlobalXtDatabase();auto& loader=Game::GameXtLoader::Instance();
        const bool oldPause=Game::g_bootResourceManagerPause;struct RestorePause{bool value;~RestorePause(){Game::g_bootResourceManagerPause=value;}} restore{oldPause};Game::g_bootResourceManagerPause=true;
        const auto tracker=MEMORY_TRACKER_CURRENT_CONTEXT();loader.AddPack(Game::E_XTPACK_GAMEDATA,"missing-required.bin");
        Require(FAILED(Game::NativeLoadRequiredRaceBookXt()),"Failed configured Xt pack advanced the required startup boundary");Require(MEMORY_TRACKER_CURRENT_CONTEXT()==tracker,"Required Xt failure stranded its tracker scope");VerifyResourceResumed(resources,scratch);loader.NativeClosePacks(database);
        std::filesystem::remove(scratch/"gamedata/Xt/Multiplayer.bin");Require(FAILED(C_MultiplayerData::Instance().LoadMultiplayerPackFile("gamedata/Xt/Multiplayer.bin")),"Missing required Multiplayer config entered InitSubsystems");
        loader.AddPack(Game::E_XTPACK_GAMEDATA,"required-valid.bin");Write(scratch/"required-valid.bin",Pack(0x700,0x7ed15401));Write(scratch/"gamedata/Xt/Multiplayer.bin",Pack(0x800,0x7ed15402));
        Require(FAILED(C_MultiplayerData::Instance().LoadMultiplayerPackFile("gamedata/Xt/Multiplayer.bin"))&&!C_MultiplayerData::Instance().m_pConfig,"Parsed pack without required config entered UI-dependent subsystem initialization");Require(!DataRef::C_StorageSorcery<Component::I_ComponentCreator>::Lookup(0x7ed15402),"Rejected config candidate retained its actual record owner");Require(MEMORY_TRACKER_CURRENT_CONTEXT()==tracker,"Missing root changed startup tracker context");VerifyResourceResumed(resources,scratch);
        auto wrong=Pack(0x900,0x7ed15403);auto* transferred=new U8[wrong.size()];std::memcpy(transferred,wrong.data(),wrong.size());Require(!C_MultiplayerData::Instance().LoadMultiplayerPackData(transferred,wrong.size(),true),"Downloaded missing-root config entered InitSubsystems");Require(!DataRef::C_StorageSorcery<Component::I_ComponentCreator>::Lookup(0x7ed15403),"Rejected transferred config retained its actual record owner");
        loader.NativeClosePacks(database);
    }
    Resource::C_TypeRegistry::DeleteInstance();Sys::SetDefaultFileSystem(nullptr);fs.SetWorkingDirectory(previous);
}
// [Blur2PC] Register a real owned manager: its rejected Create exercises the actual
// renderer rollback after both real default textures have been acquired and initialized.
class DefaultBoundaryManager final:public DynamicEffect::I_DynamicEffectManager {
public:
    explicit DefaultBoundaryManager(DynamicEffect::C_Renderer* renderer):I_DynamicEffectManager(renderer,0){}
    bool reject=true;unsigned creates=0,destroys=0;
protected:
    bool Create()override{++creates;Require(m_pRenderer->GetWhiteTexture().IsCreated()&&m_pRenderer->GetGreyTexture().IsCreated(),"Manager boundary ran before actual required defaults existed");return !reject;}
    void Destroy()override{++destroys;}
    void CleanNode(const DynamicEffect::C_Node*)override{}
    void CleanEffect(const DynamicEffect::I_DynamicEffect*)override{}
};
void RequiredRendererDefaults(){
    Scheduler::Init();Scheduler::SetGroupPoolSize(128);Scheduler::Start();struct SchedulerScope{~SchedulerScope(){Scheduler::Shutdown();}} scheduler;
    Display::C_Device device;Display::C_DeviceParams params;params.m_Width=params.m_Height=16;Require(device.Create(params)&&device.CreateVolatile(params),"Required Dynamic renderer real device creation failed");
    {
        // Original SceneGraph creates device contexts; all its borrowers retire before device.
        SceneGraph::C_SceneGraph graph;DynamicEffect::C_Renderer renderer(&graph);auto* manager=new DefaultBoundaryManager(&renderer);const auto tracker=MEMORY_TRACKER_CURRENT_CONTEXT();
#if TRACK_DISPLAY2_RESOURCES
        const auto baseline=Display2::Device()->GetTextureCount();
#endif
        Require(!renderer.Create()&&!renderer.IsRendererCreated(Display::E_RendererState_Involatile),"Rejected real manager published required Dynamic renderer readiness");Require(!renderer.GetWhiteTexture().IsCreated()&&!renderer.GetGreyTexture().IsCreated(),"Failed Dynamic renderer retained incomplete default owners");Require(manager->creates==1&&manager->destroys==1,"Actual renderer did not roll back its rejected manager boundary");Require(MEMORY_TRACKER_CURRENT_CONTEXT()==tracker,"Failed Dynamic renderer changed tracker context");
#if TRACK_DISPLAY2_RESOURCES
        Require(Display2::Device()->GetTextureCount()==baseline,"Rejected renderer retained tracked default GPU owners");
#endif
        manager->reject=false;
        for(unsigned cycle=0;cycle<2;++cycle){Require(renderer.Create()&&renderer.IsRendererCreated(Display::E_RendererState_Involatile),"Required Dynamic renderer defaults could not retry");
            auto& white=const_cast<DynamicEffect::C_Texture&>(renderer.GetWhiteTexture());auto& grey=const_cast<DynamicEffect::C_Texture&>(renderer.GetGreyTexture());
            auto verify=[](DynamicEffect::C_Texture& texture,U32 expected){auto mapped=texture.Lock(0);Require(mapped.pBits,"Actual Dynamic default map failed");U32 texel=0;std::memcpy(&texel,mapped.pBits,sizeof(texel));texture.Unlock(0);Require(texel==expected,"Dynamic default backing was not initialized to original exact contents");};verify(white,0xffffffff);verify(grey,0xff808080);
            renderer.Destroy();Require(!renderer.IsRendererCreated(Display::E_RendererState_Involatile)&&!white.IsCreated()&&!grey.IsCreated(),"Dynamic default retirement left readiness/backing published");
#if TRACK_DISPLAY2_RESOURCES
            Require(Display2::Device()->GetTextureCount()==baseline,"Dynamic default retry retained a tracked GPU owner");
#endif
        }
        Require(manager->creates==3&&manager->destroys==3,"Renderer retry did not pair each actual manager acquisition/retirement");
    }
    Require(device.GetDisplay2().WaitIdle(),"Dynamic default GPU work did not quiesce");device.DestroyVolatile();device.Destroy();
}
}
int main(){try{Require(XtInitialise(),"Fixture schema owner initialization failed");RecordsAndHoles();AppAndBorrowedOwners();RequiredStartupBoundaries();RequiredRendererDefaults();XtShutdown();Require(gXt2.references==0,"Final fixture schema owner did not retire");std::cout<<"Actual creator records/owners plus required configuration failure, worker resume, Dynamic default contents/retry/retirement PASS; process must exit0 (not gameplay)\n";return 0;}catch(const std::exception& error){std::cerr<<"Xt owner fixture failed: "<<error.what()<<'\n';return 1;}}
