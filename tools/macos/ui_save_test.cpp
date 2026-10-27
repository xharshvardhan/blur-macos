// [Blur2PC] Actual original UI asynchronous save/load/enumerate/delete against isolated native files.
#include "stdafx.h"
#include <ui/Game/SaveSystem/SaveSystem.h>
#include <ui/Util/NativePaths.h>
#include <system/serialise.h>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <unistd.h>
static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
static bool failAssert(const char*,const char*,const char*,int){throw std::runtime_error("original UI save assertion");}
class Record final:public UI::C_SaveInfoInterface {
public:
    std::atomic<bool> blockSave{false},saveEntered{false},releaseSave{false};
    std::array<U8,64> expected{},loaded{};U32 saveCalls=0,loadCalls=0,saveFinished=0,loadFinished=0,deleteFinished=0;
    Record(){
        for(U32 i=0;i<expected.size();++i)expected[i]=U8(i*17+3);
        m_SaveRoot="SAVE";m_EnumerationFileExtension="sav";
        UI::C_SaveFileInfo info;info.m_FileName="profile.sav";info.m_uFileSize=expected.size();info.m_sUserData=7;m_FileInfo.PushBack(info);
    }
    bool Save(S32 pad,U8* bytes,U32 size,S32 user) override{require(pad==0&&size==expected.size()&&user==7,"Original save callback metadata changed");++saveCalls;saveEntered.store(true);
        while(blockSave.load()&&!releaseSave.load())std::this_thread::yield();
        std::memcpy(bytes,expected.data(),size);return true;}
    bool Load(S32 pad,U8* bytes,U32 size,S32 user) override{require(pad==0&&size==loaded.size()&&user==7,"Original load callback metadata changed");++loadCalls;std::memcpy(loaded.data(),bytes,size);return true;}
    void SaveFinished()override{++saveFinished;}void LoadFinished()override{++loadFinished;}void DeleteFinished()override{++deleteFinished;}
};
template<class Continue>static void wait(Continue done){const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);while(!done()){require(std::chrono::steady_clock::now()<deadline,"Original UI save worker did not complete");std::this_thread::yield();}}
int main(int argc,char** argv){try{
    require(argc==2,"UI save fixture needs ignored scratch parent");g_pCoreAssertFunction=failAssert;
    std::string scratch=(std::filesystem::path(argv[1])/"ui-save-XXXXXX").string();require(mkdtemp(scratch.data()),"Cannot create save scratch directory");
    struct Cleanup{std::filesystem::path path;~Cleanup(){Sys::SetDefaultFileSystem(nullptr);std::error_code error;std::filesystem::remove_all(path,error);}} cleanup{scratch};
    require(!setenv("BLUR2PC_SAVE_ROOT",scratch.c_str(),1),"Cannot set isolated save root");
    auto& fileSystem=Sys::C_FileSystem::Instance();Sys::SetDefaultFileSystem(&fileSystem);require(fileSystem.SetWorkingDirectory(scratch.c_str()),"Cannot set save working directory");
    require(std::filesystem::path(UI::NativePaths::ApplicationSupport()).is_absolute(),"Native platform application-support query failed");
    auto& system=UI::C_SaveSystem::Instance();Record record;system.SaveInfoInterface(&record);
    require(system.StartAsyncSave(0),"Original save worker did not start");wait([&]{return system.ContinueAsyncSave();});
    const auto file=std::filesystem::path(scratch)/"SAVE/profile.sav";
    require(system.GetSaveErrorCode()==UI::E_UiGameProfile_Error_Okay&&record.saveCalls==1&&record.saveFinished==1&&std::filesystem::exists(file),"First native save reported success without writing callback/file");
    require(system.StartAsyncLoad(0),"Original load worker did not start");wait([&]{return system.ContinueAsyncLoad();});
    require(system.GetLoadErrorCode()==UI::E_UiGameProfile_Error_Okay&&record.loadCalls==1&&record.loaded==record.expected,"Original native save/load changed record bytes");
    record.m_bCheckOverwrites=true;require(system.StartAsyncSave(0),"Original overwrite-check worker did not start");wait([&]{return system.ContinueAsyncSave();});
    require(system.GetSaveErrorCode()==UI::E_UiGameProfile_Error_FileExists&&std::filesystem::file_size(file)==record.expected.size()&&record.saveCalls==1,"Native overwrite refusal changed/truncated the existing save");
    std::array<U8,64> preserved{};std::ifstream unchanged(file,std::ios::binary);unchanged.read(reinterpret_cast<char*>(preserved.data()),preserved.size());require(preserved==record.expected,"Native overwrite refusal altered record contents");
    record.m_bCheckOverwrites=false;record.m_eType=UI::E_UiSaveInfoType_Enumerate;record.m_sEnumeration=-1;
    require(system.StartAsyncLoad(0),"Original enumerate worker did not start");wait([&]{return system.ContinueAsyncLoad();});
    require(system.GetLoadErrorCode()==UI::E_UiGameProfile_Error_Okay&&system.GetEnumerationCount()==1&&system.IsValidEnumerationFileType(0)&&UI::Unicode::Compare(system.GetEnumerationName(0),u"profile")==0,"Original native save enumeration changed file name/type/count");
    for(U32 i=0;i<MAX_SAVE_ENUMERATIONS+5;++i)std::ofstream(file.parent_path()/("slot"+std::to_string(i)+".sav"))<<"x";
    require(system.StartAsyncLoad(0),"Native capped enumerate worker did not start");wait([&]{return system.ContinueAsyncLoad();});require(system.GetEnumerationCount()==MAX_SAVE_ENUMERATIONS,"Native save enumeration exceeded fixed slot storage");
    record.m_eType=UI::E_UiSaveInfoType_Default;
    require(system.StartAsyncDelete(0),"Original delete worker did not start");wait([&]{return system.ContinueAsyncDelete();});
    require(system.GetDeleteErrorCode()==UI::E_UiGameProfile_Error_Okay&&record.deleteFinished==1&&!std::filesystem::exists(file),"Original native delete did not retire actual file");
    require(system.StartAsyncLoad(0),"Original missing-file load did not start");wait([&]{return system.ContinueAsyncLoad();});require(system.GetLoadErrorCode()==UI::E_UiGameProfile_Error_FileDoesNotExist,"Native missing save falsely reported a successful load");
    // [Blur2PC] Close must join a genuinely blocked file worker and its completion callback before detaching.
    record.saveEntered=false;record.blockSave=true;record.releaseSave=false;
    const U32 finishedBefore=record.saveFinished;
    require(system.StartAsyncSave(0),"Active close fixture did not start its real save worker");
    wait([&]{return record.saveEntered.load();});
    std::thread release([&]{std::this_thread::sleep_for(std::chrono::milliseconds(25));record.releaseSave=true;});
    system.NativeQuiesceAndDetach(&record);
    const bool completedBeforeCloseReturned=record.saveFinished==finishedBefore+1;
    release.join();
    require(completedBeforeCloseReturned&&system.GetSaveErrorCode()==UI::E_UiGameProfile_Error_Okay&&!system.NativeHasInterface(&record),"Close returned before the actual worker/callback or retained owned target");
    require(!system.StartAsyncSave(0)&&!system.StartAsyncLoad(0)&&!system.StartAsyncDelete(0),"Closed screen callbacks reacquired retired asynchronous workers");
    // Preserve an unrelated borrowed owner and its actual completed result across close, then allow a real UI generation.
    record.blockSave=false;system.NativeOpenRequests();system.SaveInfoInterface(&record);
    require(system.StartAsyncLoad(0),"Fresh UI generation could not acquire original loader");wait([&]{return system.ContinueAsyncLoad();});
    require(system.GetLoadErrorCode()==UI::E_UiGameProfile_Error_Okay,"Fresh UI generation changed saved payload");
    Record unrelated;system.NativeQuiesceAndDetach(&unrelated);
    require(system.NativeHasInterface(&record)&&system.GetLoadErrorCode()==UI::E_UiGameProfile_Error_Okay,"Retiring another owner cleared a borrowed target or fabricated its result");
    system.NativeOpenRequests();require(system.StartAsyncLoad(0),"Retained borrowed target could not be used after real reopen");wait([&]{return system.ContinueAsyncLoad();});
    require(system.GetLoadErrorCode()==UI::E_UiGameProfile_Error_Okay&&record.loaded==record.expected,"Borrowed interface did not survive exact-owner retirement");
    system.NativeQuiesceAndDetach(&record);
    std::cout<<"Original UI async storage plus active-worker/callback join, exact borrowed-owner isolation, closed request rejection and real reopen PASS (authored record; not career save compatibility)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
