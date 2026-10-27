// [Blur2PC] Actual full audio system/native renderer and original bank loader on authored PCM data.
#include "AudioPch.h"
#include "source/core/macos/Engine.h"
#include "source/common/WaveBankReader.h"
#include <system/serialise/C_FileSystem.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
void XtLibrary_Application(){} // This authored host does not register application-specific Xt schemas.
static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
template<class F>static void until(F condition,const char* message){const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(!condition()){require(std::chrono::steady_clock::now()<end,message);MT::Thread::Sleep(1);}}
static void bankFile(const std::filesystem::path& path,bool corrupt=false){
    Audio::WaveBank::BankHeader bank{};std::memcpy(&bank.uTag,corrupt?"FAIL":"BANK",4);bank.uSize=sizeof(bank);bank.uVersion=Audio::g_WbVersion;bank.uNumWaves=1;std::strcpy(bank.pchName,"authored");
    Audio::WaveBank::WaveBaseHeader wave{};std::memcpy(&wave.uTag,"WAVE",4);wave.uSize=sizeof(wave)+sizeof(Audio::WaveBank::PCMWave);wave.uFormat=Audio::Core::uPCM16BitSignedInt;std::strcpy(wave.pchName,"authored_pcm");wave.uDataStart=sizeof(bank)+wave.uSize+sizeof(Audio::WaveBank::ChunkHeader);wave.uDataLength=8;wave.fDefaultLevel=.5f;
    Audio::WaveBank::PCMWave pcm{};pcm.uSampleRate=24000;pcm.uBitsPerSample=16;pcm.uNumChannels=1;pcm.uStride=2;
    Audio::WaveBank::ChunkHeader data{};std::memcpy(&data.uTag,"DATA",4);data.uSize=sizeof(data)+wave.uDataLength;
    const S16 samples[4]={-32768,-1000,1000,32767};std::ofstream file(path,std::ios::binary);
    for(auto block:{std::pair<const void*,size_t>{&bank,sizeof(bank)},{&wave,sizeof(wave)},{&pcm,sizeof(pcm)},{&data,sizeof(data)},{samples,sizeof(samples)}})file.write(static_cast<const char*>(block.first),block.second);
    require(bool(file),"Cannot write authored PCM bank fixture");
}
int main(int argc,char** argv){try{
    require(argc==2,"Full audio fixture needs an ignored scratch parent");
    std::string pattern=(std::filesystem::path(argv[1])/"audio-bank-XXXXXX").string();require(mkdtemp(pattern.data()),"Cannot create isolated audio bank directory");
    struct Cleanup{std::filesystem::path path;~Cleanup(){Sys::SetDefaultFileSystem(nullptr);std::error_code error;std::filesystem::remove_all(path,error);}} cleanup{pattern};
    auto& fs=Sys::C_FileSystem::Instance();Sys::SetDefaultFileSystem(&fs);
    Scheduler::Init();Scheduler::SetGroupPoolSize(64);Scheduler::jqEnableWorkers(Scheduler::JQ_CORE_1|Scheduler::JQ_CORE_3);Scheduler::Start();
    Audio::ISystem::InitParams params;params.uMessageQueueNumBytes=512*1024;params.uReturnMessageQueueNumBytes=128*1024;params.uTempFrameBufferNumBytes=64*256*sizeof(F32);params.uInstanceStateDataBufferSize=8*1024;params.eDistributedUpdate=Audio::ISystem::eDistribUpdate_Disabled;params.hPolicy=0;params.hPolicyB=0;params.hGroup=Scheduler::CreateGroup(true);
    auto* system=Audio::CreateSystem(&params);require(system&&system==Audio::GetSystem(),"Original audio system factory did not own its singleton");
    require(system->Init(),"Original full audio system failed native renderer initialization");
    until([]{return Audio::Core::Engine::Instance().SubmittedFrames()>=1024;},"Original Audio::System callbacks did not feed native PCM output");
    // An invalid authored connector must be consumed once and rejected by the actual receiver;
    // it must not wedge queue advancement or the original renderer callback.
    const auto beforeMessage=Audio::Core::Engine::Instance().SubmittedFrames();
    system->SetMaster(Audio::InstanceComponentHandle());
    until([&]{return Audio::System::Get()->GetQueue()->PeakQueuedBytes()>0&&Audio::Core::Engine::Instance().SubmittedFrames()>=beforeMessage+2048;},"Original audio message receiver did not advance its native queue/render callbacks");
    void* physical=Audio::System::Alloc(333,128,Audio::System::ePhysical);require(physical&&reinterpret_cast<std::uintptr_t>(physical)%128==0,"Native physical-bank allocation lost requested alignment");Audio::System::Free(physical,Audio::System::ePhysical);
    const auto good=std::filesystem::path(pattern)/"authored.baf",bad=std::filesystem::path(pattern)/"corrupt.baf";bankFile(good);bankFile(bad,true);
    {
        Audio::WaveBank bank(good.c_str());Audio::WaveBankReader reader(&bank);require(reader.Read()==Audio::Success&&bank.NumWaves()==1,"Original bank reader rejected a valid authored PCM bank");auto* wave=bank.GetWave(0);
        const S16 expected[4]={-32768,-1000,1000,32767};require(wave&&wave->SampleRate()==24000&&wave->BitsPerSample()==16&&wave->NumChannels()==1&&wave->DataLength()==sizeof(expected)&&!std::memcmp(wave->m_pvWaveData,expected,sizeof(expected)),"Original bank reader changed PCM metadata or sample bytes");
    }
    {
        Audio::WaveBank bank(bad.c_str());Audio::WaveBankReader reader(&bank);require(reader.Read()==Audio::WaveBankCorrupt,"Original bank reader accepted an invalid bank tag");
    }
    auto* manager=Audio::CreateWaveManager();const auto handle=manager->LoadBank(good.c_str());require(handle!=Audio::INVALID_HANDLE,"Original wave manager failed an actual resident-bank load");require(manager->ReleaseBank(handle)==Audio::Success,"Original wave manager failed resident-bank release");Audio::ReleaseWaveManager(manager);
    Audio::ReleaseSystem();require(!Audio::GetSystem()&&!Audio::Core::Engine::Instance().IsRunning(),"Original full audio shutdown retained a renderer/system owner");Scheduler::Shutdown();
    std::cout<<"Original full audio system/native PCM output, message queue consumption, aligned physical buffers and actual authored resident-bank loading/metadata/PCM/release/error PASS (no game banks or audible signal)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
