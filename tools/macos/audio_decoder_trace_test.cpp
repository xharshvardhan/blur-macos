// [Blur2PC] Observe real mono/stereo streamed ADPCM decisions and owner retirement.
// Authored packets only; all hardware output must be stream-muted by the runner.
#include "AudioPch.h"
#include "source/SystemHeaders.h"
#include "source/core/macos/Engine.h"
#include "source/core/macos/DecoderTrace.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
void XtLibrary_Application(){}
namespace {
using namespace Audio;using namespace Audio::Core;
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<class P> void until(P predicate,const char* why){const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(4);while(!predicate()){require(std::chrono::steady_clock::now()<deadline,why);MT::Thread::Sleep(1);}}
struct SchedulerOwner {SchedulerOwner(){Scheduler::Init();Scheduler::SetGroupPoolSize(64);Scheduler::jqEnableWorkers(Scheduler::JQ_CORE_1|Scheduler::JQ_CORE_3);Scheduler::Start();}~SchedulerOwner(){Scheduler::Shutdown();}};
U64 number(const std::string& line,const char* key){const auto at=line.find(std::string("\"")+key+"\":");require(at!=std::string::npos,"Trace field missing");return std::stoull(line.substr(at+std::strlen(key)+3));}
std::vector<std::string> lines(const std::string& path){std::ifstream input(path);std::vector<std::string> result;for(std::string s;std::getline(input,s);)result.push_back(s);return result;}
template<class Decoder,U32 Channels> struct WaveCase {
    alignas(128) std::array<U8,1024> storage{};
    alignas(128) std::array<U8,512> encoded{};
    Instance instance;Decoder decoder{false,false};ProcessInfo process{};WaveStreamData* stream;
    std::array<F32,Channels*uFrameLength> pcm{};
    WaveCase(){
        instance.SetCoreData(storage.data());instance.SetWaveServerOffset(16);
        *instance.GetNumStreamsNeedingUpdatePointer()=0;
        stream=new(instance.GetNextStreamInfoSlot())WaveStreamData();++*instance.GetNumStreamsCreatedPointer();
        stream->pData=encoded.data()+32;stream->hWaveData=StreamHandle(Channels);stream->u32SourceDataPosition=123;
        for(U32 packet=0;packet<3;++packet)for(U32 channel=0;channel<Channels;++channel){auto* p=stream->pData+(packet*Channels+channel)*33;p[0]=0;std::fill_n(p+1,32,channel?0xff:0x11);}
        ADPCM4bitDecoder::WaveData wave{};wave.uChannels=Channels;wave.uLength=99*Channels;wave.uSampleRate=24000;wave.uPCMLoopPosEnd=192;wave.hWaveServerIndex=WaveServerLocalIndex(0);
        decoder.SetWaveData(wave);process.decoderDataInfo=instance.GetDecoderDataInfo();process.uFrameInt=1;process.fWidth=1;
        decoder.Init(process.decoderDataInfo);decoder.SetPitch(1);decoder.Reset(process.decoderDataInfo);
        stream->u32SourceDataPosition=123;
    }
    ~WaveCase(){decoder.Shutdown(process.decoderDataInfo);stream->~WaveStreamData();}
    void zero(){pcm.fill(1);require(decoder.FillBuffer(pcm.data(),process),"Original zero-fill unexpectedly became inactive");for(F32 sample:pcm)require(sample==0,"Original streamed hole did not zero the output");require(stream->u32BytesConsumed==0,"Zero-fill changed original consumed-byte accounting");}
    void cases(){
        zero();require(stream->IsPlaying()&&!stream->IsPrimed(),"Original GetData did not request priming");
        stream->bResourcesAllocated=true;zero();stream->u32BytesAvailable=1;zero();
        stream->u32BytesAvailable=99*Channels;
        require(decoder.FillBuffer(pcm.data(),process),"Primed actual decoder failed to recover");
        bool signal=false;for(F32 sample:pcm){require(std::isfinite(sample),"Actual decoder produced nonfinite PCM");signal|=sample!=0;}require(signal,"Recovered real decoder was silence only");
        require(stream->u32BytesConsumed>0,"Actual decoder failed to consume primed packets");
    }
};
struct Gate {
    WaveCase<MonoADPCM4bitDecoder,1>& wave;
    std::atomic<bool> entered{false},release{false},complete{false};
    explicit Gate(WaveCase<MonoADPCM4bitDecoder,1>& value):wave(value){}
    static void Begin(void* data){auto& g=*static_cast<Gate*>(data);if(g.entered.exchange(true))return;while(!g.release.load())MT::Thread::Sleep(1);g.wave.zero();g.complete=true;}
};
struct GateRelease {Gate& gate;~GateRelease(){gate.release=true;}};
void test(const std::string& path){
    auto& engine=Engine::Instance();
    unsetenv("BLUR_MAC_AUDIO_DECODER_TRACE_FILE");
    {Renderer renderer(8);require(renderer.Init(nullptr,nullptr,nullptr,0,false),"Disabled diagnostic renderer failed");WaveCase<MonoADPCM4bitDecoder,1> mono;mono.cases();require(!Audio::Mac::DecoderTraceEnabled(),"Disabled trace enabled itself");}
    setenv("BLUR_MAC_AUDIO_DECODER_TRACE_FILE","relative-invalid",1);
    {Renderer renderer(8);require(!renderer.Init(nullptr,nullptr,nullptr,0,false),"Relative diagnostic path was accepted");require(!engine.IsRunning()&&!Audio::Mac::DecoderTraceEnabled(),"Failed diagnostic init retained its writer/session");}
    setenv("BLUR_MAC_AUDIO_DECODER_TRACE_FILE",path.c_str(),1);
    {
        Renderer renderer(8);require(renderer.Init(nullptr,nullptr,nullptr,0,false),"Enabled diagnostic renderer failed");
        require(!renderer.Init(nullptr,nullptr,nullptr,0,false),"Duplicate renderer initialization was accepted");
        // Renderer owns one original static batch queue: a second simultaneous
        // Renderer construction is not a supported host. Test trace ownership
        // directly, without overwriting that singleton before Init can reject it.
        int otherOwner=0;require(!Audio::Mac::BeginDecoderTrace(&otherOwner),"Second diagnostic owner was accepted");
        Audio::Mac::EndDecoderTrace(&otherOwner);
        require(Audio::Mac::DecoderTraceEnabled()&&lines(path).empty(),"Non-owner destruction flushed the live trace");
        require(engine.SetOutputSampleRate(36000)&&engine.SetOutputSampleRate(48000),"Quality writer restart failed");
        require(Audio::Mac::DecoderTraceEnabled()&&lines(path).empty(),"Quality restart retired the producer's trace");
        WaveCase<MonoADPCM4bitDecoder,1> mono;WaveCase<StereoADPCM4bitDecoder,2> stereo;mono.cases();stereo.cases();
        // Separate real decoder instances; original Init/Shutdown stay on the owner
        // thread. All callers are joined before the renderer retires the session.
        WaveCase<MonoADPCM4bitDecoder,1> manyMono;WaveCase<StereoADPCM4bitDecoder,2> manyStereo;
        std::thread a([&]{for(U32 i=0;i<270000;++i)manyMono.zero();});
        std::thread b([&]{for(U32 i=0;i<270000;++i)manyStereo.zero();});a.join();b.join();
    }
    auto output=lines(path);require(!output.empty(),"Joined owner did not flush its diagnostic session");
    const auto& session=output.front();require(number(session,"mono_attempts")==270004&&number(session,"stereo_attempts")==270004,"Usage counters lost actual FillBuffer calls");
    require(number(session,"mono_successful_decode")==1&&number(session,"stereo_successful_decode")==1,"Successful real decode was not counted");
    require(number(session,"mono_zero_fill")==270003&&number(session,"stereo_zero_fill")==270003,"Actual source-hole counters disagree");
    require(number(session,"mono_streamed_attempts")==270004&&number(session,"stereo_streamed_attempts")==270004,"Streamed coverage was not distinguished");
    require(number(session,"zero_fill_count")==540006&&number(session,"retained")==8192&&number(session,"overflow")>0,"Bounded concurrent trace did not report saturation");
    require(number(session,"sampled_reservations")==number(session,"retained")+number(session,"overflow"),"Reservation/overflow accounting mismatch");
    require(output.size()==8193&&!Audio::Mac::DecoderTraceEnabled(),"Retired trace length/enabled state invalid");
    bool startup=false,empty=false,shortRead=false;
    for(U32 i=1;i<output.size();++i){require(number(output[i],"stream_handle")==number(output[i],"channels"),"Trace lost actual global stream handle/generation");const auto reason=number(output[i],"reason");startup|=reason&1;empty|=reason&2;shortRead|=reason==4;require(number(output[i],"source_position")==123&&number(output[i],"sample_counter")==0,"Original source/playhead context changed");}
    require(startup&&empty&&shortRead,"Distinct real priming/data/short-read decisions were not retained");
    // Prove the SDL writer is not the decoder's lifetime boundary: the original
    // producer may still be inside its frame callback after that writer joins.
    WaveCase<MonoADPCM4bitDecoder,1> gatedWave;Gate gate(gatedWave);
    {
        Renderer renderer(8);require(renderer.Init(&gate,Gate::Begin,nullptr,0,false),"Repeated diagnostic lifetime failed");
        GateRelease safety{gate};renderer.Start();until([&]{return gate.entered.load();},"Actual producer did not enter the authored gate");
        require(engine.Shutdown(&renderer),"Actual SDL writer did not join");
        require(Audio::Mac::DecoderTraceEnabled()&&lines(path).size()==output.size(),"Writer stop prematurely drained decoder jobs");
        gate.release=true;until([&]{return gate.complete.load();},"Actual producer failed to observe decoder after writer stop");
    }
    const auto repeated=lines(path);require(repeated.size()==output.size()+2,"Repeated owner did not flush a separate bounded session");
    require(number(repeated[output.size()],"mono_attempts")==1&&number(repeated[output.size()],"mono_zero_fill")==1,"Repeated session reused previous counters");
    require(!Audio::Mac::DecoderTraceEnabled()&&!engine.IsRunning(),"Actual producer retirement did not close owner state");
    unsetenv("BLUR_MAC_AUDIO_DECODER_TRACE_FILE");
}
}
int main(){const std::string path="/tmp/blur-audio-decoder-trace-"+std::to_string(getpid())+".jsonl";try{require(std::getenv("BLUR_MAC_AUDIO_MUTE")&&std::string(std::getenv("BLUR_MAC_AUDIO_MUTE"))=="1","Automated audio fixture requires BLUR_MAC_AUDIO_MUTE=1");SchedulerOwner scheduler;test(path);std::filesystem::remove(path);std::cout<<"Actual streamed mono/stereo priming/hole/recovery, usage counts, bounded concurrent trace, failed/duplicate/restarted owner and producer-after-writer join PASS\n";return 0;}catch(const std::exception& error){std::cerr<<error.what()<<" trace="<<path<<'\n';return 1;}}
