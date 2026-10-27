// [Blur2PC] Real file/stream/graph owners, bounded queue rejection, physical read join and final retirement.
#include "AudioPch.h"
#include "source/core/coreheaders.h"
#include "source/SystemHeaders.h"
#include <system/serialise/C_FileSystem.h>
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unistd.h>

extern MT::Event AudioStreamingEvent;
extern Debug::Bool g_bProcessFileStreamRequests;
void XtLibrary_Application(){} // Authored host, no game schemas or gameplay substitution.
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Gate {
    std::mutex mutex;std::condition_variable condition;bool released=false;bool suppressReadsAfterPair=false;
    std::atomic<unsigned> entered{0},completed{0},opened{0},closed{0};
    void release(){std::lock_guard<std::mutex> lock(mutex);released=true;condition.notify_all();}
    void waitForRead(){std::unique_lock<std::mutex> lock(mutex);require(condition.wait_for(lock,std::chrono::seconds(3),[&]{return entered.load()!=0;}),"Actual file loader did not enter the bounded read gate");}
};
// The adapter blocks a real read, then delegates its bytes/result to the real native filesystem.
class GateFile final:public Sys::I_File {
    std::unique_ptr<Sys::I_File> file;Gate& gate;
public:
    GateFile(Sys::I_File* value,Gate& state):file(value),gate(state){++gate.opened;}
    ~GateFile()override{++gate.closed;}
    const Sys::C_StreamSize Length()override{return file->Length();}
    const Sys::C_StreamPos GetPosition()const override{return file->GetPosition();}
    void SetPosition(const Sys::C_StreamPos position)override{file->SetPosition(position);}
    const bool IsWriting()const override{return file->IsWriting();}
    Btl::String Name()const override{return file->Name();}
    const bool GetEndianSwapFlag()const override{return file->GetEndianSwapFlag();}
    void SetEndianSwapFlag(const bool value)override{file->SetEndianSwapFlag(value);}
    U32 GetVersionNumber()const override{return file->GetVersionNumber();}
    void SetVersionNumber(U32 value)override{file->SetVersionNumber(value);}
    void Flush()override{file->Flush();}
    bool Overflow()override{return file->Overflow();}
    bool Underflow(UnderflowContext& context)override{
        {std::unique_lock<std::mutex> lock(gate.mutex);++gate.entered;gate.condition.notify_all();
         if(!gate.condition.wait_for(lock,std::chrono::seconds(5),[&]{return gate.released;}))return false;}
        const bool result=file->Read(context.readTarget,context.readSize);
        context.bytesRead=result?context.readSize:0;
        // Toggle on the loader itself after both real reads, before its queued frees.
        // No renderer is acquired, so no unrelated thread reads/writes this debug flag.
        const auto completed=++gate.completed;
        if(completed==2&&gate.suppressReadsAfterPair)g_bProcessFileStreamRequests=false;
        return result;
    }
};
// Borrow the existing native filesystem; constructing a second C_FileSystem would violate its singleton owner.
class GateFs final:public Sys::I_FileSystem {
    Sys::I_FileSystem& backing;Gate& gate;
public:
    GateFs(Sys::I_FileSystem& source,Gate& value):backing(source),gate(value){}
    Sys::I_File* Open(const Sys::C_FilePath& path,const Sys::E_FileAccess access=Sys::E_FileAccess_Read,const Sys::C_OpenParams& params=Sys::C_OpenParams())override{
        auto* file=backing.Open(path,access,params);return file?new GateFile(file,gate):nullptr;
    }
    bool FileExists(const Sys::C_FilePath& path)override{return backing.FileExists(path);}
    bool FileInfo(const Sys::C_FilePath& path,Sys::C_FileInfo* info)override{return backing.FileInfo(path,info);}
    bool FileAccess(const Sys::C_FilePath& path,U32 access)override{return backing.FileAccess(path,access);}
    bool FileTime(const Sys::C_FilePath& path,const Time::C_Time& time)override{return backing.FileTime(path,time);}
    bool DeleteFile(const Sys::C_FilePath& path)override{return backing.DeleteFile(path);}
    bool MoveFile(const Sys::C_FilePath& from,const Sys::C_FilePath& to)override{return backing.MoveFile(from,to);}
    bool DirectoryInfo(const Sys::C_FilePath& path,Sys::C_DirectoryInfo* info)override{return backing.DirectoryInfo(path,info);}
    bool CreateSingleDirectory(const Sys::C_FilePath& path)override{return backing.CreateSingleDirectory(path);}
    bool SetWorkingDirectory(const Sys::C_FilePath& path)override{return backing.SetWorkingDirectory(path);}
    Sys::C_FilePath GetWorkingDirectory()const override{return backing.GetWorkingDirectory();}
    Sys::C_FilePath GetSystemFilePath(const Sys::C_FilePath& path)const override{return backing.GetSystemFilePath(path);}
};
static Audio::ISystem::InitParams systemParams(){
    Audio::ISystem::InitParams params;params.uMessageQueueNumBytes=512*1024;params.uReturnMessageQueueNumBytes=128*1024;
    params.uTempFrameBufferNumBytes=64*256*sizeof(F32);params.uInstanceStateDataBufferSize=8*1024;
    params.eDistributedUpdate=Audio::ISystem::eDistribUpdate_Disabled;params.hPolicy=0;params.hPolicyB=0;params.hGroup=Scheduler::CreateGroup(true);return params;
}
static void cycle(const std::filesystem::path& path,bool ordinaryRetry){
    const bool previousReadFlag=g_bProcessFileStreamRequests;
    struct RestoreReadFlag {bool value;~RestoreReadFlag(){g_bProcessFileStreamRequests=value;}} restoreFlag{previousReadFlag};
    g_bProcessFileStreamRequests=true;
    auto& backing=Sys::C_FileSystem::Instance();
    auto* previousFs=Sys::GetDefaultFileSystem();
    struct RestoreFs {Sys::I_FileSystem* previous;~RestoreFs(){Sys::SetDefaultFileSystem(previous);}} restoreFs{previousFs};
    Gate gate;gate.suppressReadsAfterPair=true;GateFs fs(backing,gate);Sys::SetDefaultFileSystem(&fs);
    // An independently owned real file is outside WaveServer ownership and must remain usable.
    std::unique_ptr<Sys::I_File> borrowed(backing.Open(path.c_str()));require(bool(borrowed),"Cannot open unrelated real file");
    auto params=systemParams();auto* system=Audio::System::Create(&params);require(system,"Original System factory failed");
    struct SystemOwner {Gate& gate;~SystemOwner(){gate.release();Audio::g_bQuitAllTasks=false;Audio::ReleaseSystem();}} systemOwner{gate};
    // Deliberately no renderer Init: deterministic partial-acquisition owner, real loader thread still runs.
    // The existing full audio bank test covers actual renderer Init/consumer retirement.
    auto& server=*system->GetWaveServer();auto& manager=server.GetFileStreamManager();
    Audio::StreamSourceHandle sources[3];Audio::StreamHandle streams[3];Audio::Core::WaveStreamData states[3];
    for(unsigned i=0;i<3;++i){
        Audio::Core::WaveServerRegisterSourceParams source;source.m_pSourceData=const_cast<char*>(path.c_str());
        require(server.RegisterStreamSource(sources[i],source),"Actual stream source registration failed");
        Audio::Core::WaveServerStreamCreateParams create;create.m_hSourceHandle=sources[i];create.m_u32StreamEncodedDataFormat=Audio::Core::uPCM16BitSignedInt;
        create.m_u32DataProcessMaxChunkSize=4096;create.m_u32FileReadChunkSize=4096;create.m_u32NumInitialWriteChunks=1;create.m_u32BufferSizeInReadChunks=4;
        create.m_u32FileEndPosition=65536;create.m_u32ReturnBufferAlignment=128;create.m_u32MaxReturnBufferSize=4096;
        require(server.CreateStream(streams[i],create),"Actual graph/stream creation failed");states[i].hWaveData=streams[i];
    }
    auto play=[&](unsigned i,bool playing){states[i].bPlaying=playing;U32 pending=1;require(server.UpdateStream(states[i],&pending),"Original graph activation failed");};
    play(0,true);play(1,true);AudioStreamingEvent.Signal();gate.waitForRead();
    auto before=server.NativeOwnershipCounts();require(before.graphs==3&&before.sources==3&&before.streams==3&&before.pendingReads==2&&before.chunks==2&&before.files==3,"Populated actual owner counts did not match acquired graph/file/chunk stages");
    // Fill the actual queue while a real file read is blocked. Accepted requests own their new[] chunks.
    unsigned accepted=0;
    for(;accepted<AUDIO_FILE_STREAMER_QUEUE_SIZE;++accepted){
        auto* bytes=new U8[17];Audio::Core::WaveStreamReadRequest request{};request.m_bDeallocateResources=true;request.m_pDestBuffer=bytes;request.m_u8ReadRequestType=Audio::Core::eFileStream;
        if(!manager.LoadData(request)){delete[] bytes;break;}
    }
    require(accepted>0&&accepted<AUDIO_FILE_STREAMER_QUEUE_SIZE,"Actual request queue did not reject its bounded full condition");
    play(2,true); // Its physical read is rejected: must not publish logical pending progress.
    auto rejected=server.NativeOwnershipCounts();require(rejected.pendingReads==2&&rejected.chunks==3,"Queue-rejected read falsely published pending progress or lost its allocated chunks");
    play(2,false); // Its real initial chunk cannot enqueue frees: retained ownership, no early deletion.
    auto retained=server.NativeOwnershipCounts();require(retained.chunks==3&&retained.pendingReads==2,"Rejected chunk frees lost their actual owner before loader join");
    if(ordinaryRetry){
        gate.release();const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(gate.completed.load()<2){require(std::chrono::steady_clock::now()<deadline,"Actual accepted reads did not complete");MT::Thread::Sleep(1);}
        // Drain original results and permit the ordinary manager's retry to transfer the real initial chunk.
        for(unsigned i=0;i<200&&server.NativeOwnershipCounts().chunks!=2;++i){manager.Update();MT::Thread::Sleep(1);}
        require(server.NativeOwnershipCounts().chunks==2,"Ordinary manager update did not retry retained chunk frees");
    }
    std::atomic<bool> shutdownReturned{false};
    std::thread unblock;
    if(!ordinaryRetry)unblock=std::thread([&]{std::this_thread::sleep_for(std::chrono::milliseconds(40));require(!shutdownReturned,"Shutdown returned before the actual blocked read completed");gate.release();});
    Audio::g_bQuitAllTasks=true; // Accepted requests must still drain under the original global quit condition.
    require(system->Shutdown(),"Actual System Shutdown failed");shutdownReturned=true;Audio::g_bQuitAllTasks=false;
    if(unblock.joinable())unblock.join();
    auto after=server.NativeOwnershipCounts();require(!after.graphs&&!after.sources&&!after.streams&&!after.pendingReads&&!after.chunks&&!after.files,"Actual final owner retirement retained graph/source/stream/chunk/wrapper objects");
    require(!g_bProcessFileStreamRequests,"Actual queued frees were not tested with debug reads suppressed");
    require(gate.completed==2&&gate.opened==2&&gate.closed==2,"Joined accepted reads/wrapper destruction did not balance actual IO ownership");
    Audio::Core::WaveServerRegisterSourceParams late;late.m_pSourceData=const_cast<char*>(path.c_str());Audio::StreamSourceHandle source;
    require(!server.RegisterStreamSource(source,late)&&!manager.PrepareFile(path.c_str()).IsValid(),"Retired source/file manager accepted a new generation");
    Audio::Core::WaveServerStreamCreateParams lateParams;Audio::StreamHandle lateStream;
    require(!server.CreateStream(lateStream,lateParams),"Retired graph owner accepted a late stream");
    Audio::Core::WaveStreamReadRequest lateRead{};lateRead.m_bDeallocateResources=true;auto* externalBytes=new U8[1];lateRead.m_pDestBuffer=externalBytes;
    require(!manager.LoadData(lateRead),"Stopped physical loader accepted late ownership");delete[] externalBytes;
    U8 first=255;borrowed->SetPosition(0);require(borrowed->Read(&first,1)&&first==0,"Final stream retirement closed or changed an unrelated borrowed file");
    require(system->Shutdown(),"Repeated System shutdown was not idempotent");
    std::cerr<<"Real stream/chunk/wrapper retirement, late generation and borrowed file PASS; ordinaryRetry="<<ordinaryRetry<<'\n';
    Audio::ReleaseSystem();require(!Audio::GetSystem(),"Original System singleton survived Release");
    borrowed.reset();Sys::SetDefaultFileSystem(previousFs);
}
static void partialFailureOwner(){
    auto params=systemParams();auto* system=Audio::System::Create(&params);require(system,"Partial owner factory failed");
    struct ReleaseOwner {~ReleaseOwner(){Audio::ReleaseSystem();}} releaseOwner;
    std::string oversized(MAX_STREAMER_FILE_PATH_LENGTH,'x');
    Audio::Core::WaveServerRegisterSourceParams source;source.m_pSourceData=oversized.data();Audio::StreamSourceHandle handle;
    require(!system->GetWaveServer()->RegisterStreamSource(handle,source),"Original bounded bank-name rejection failed");
    require(system->Shutdown(),"Never-activated original System did not retire its loader");
    const auto state=system->GetWaveServer()->NativeOwnershipCounts();
    require(!state.graphs&&!state.sources&&!state.streams&&!state.chunks&&!state.files,"Rejected source acquisition left actual owners");
    std::cerr<<"Partial actual System/rejected source/never-activated loader shutdown PASS\n";
}
int main(int argc,char** argv){try{
    require(argc==2,"Stream retirement fixture needs an ignored scratch parent");
    const auto previousDirectory=std::filesystem::current_path();
    std::string pattern=(std::filesystem::absolute(argv[1])/"audio-stream-retirement-XXXXXX").string();
    require(mkdtemp(pattern.data()),"Cannot create isolated stream fixture directory");
    struct Cleanup {std::filesystem::path path,previous;~Cleanup(){std::error_code error;std::filesystem::current_path(previous,error);std::filesystem::remove_all(path,error);}} cleanup{pattern,previousDirectory};
    const auto file=std::filesystem::path(pattern)/"stream.pcm";{std::ofstream out(file,std::ios::binary);for(unsigned i=0;i<65536;++i)out.put(static_cast<char>(i&255));require(bool(out),"Cannot write real authored stream bytes");}
    Scheduler::Init();Scheduler::SetGroupPoolSize(64);Scheduler::jqEnableWorkers(Scheduler::JQ_CORE_1|Scheduler::JQ_CORE_3);Scheduler::Start();
    struct SchedulerOwner {~SchedulerOwner(){Scheduler::Shutdown();}} schedulerOwner;
    // A leaf host owns the application default and cwd; retain original 64-byte bank-name bounds.
    auto& filesystem=Sys::C_FileSystem::Instance();Sys::SetDefaultFileSystem(&filesystem);
    struct DefaultOwner {~DefaultOwner(){Sys::SetDefaultFileSystem(nullptr);}} defaultOwner;
    require(filesystem.SetWorkingDirectory(pattern.c_str()),"Cannot establish isolated real audio working directory");
    partialFailureOwner();cycle("stream.pcm",false);cycle("stream.pcm",true);
    std::cout<<"Actual System partial-owner + queued/blocked real file reads, full-queue read rejection, retained/retried chunk frees, joined graph/source/stream/wrapper retirement, closed-generation/idempotence and borrowed-file isolation PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
