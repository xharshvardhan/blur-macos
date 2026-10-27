// [Blur2PC] Actual Output -> renderer Input -> producer ring -> native output
// adapter, with independent continuous channels. All automated output is muted.
#include "AudioPch.h"
#include "source/core/macos/Engine.h"
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <unistd.h>
void XtLibrary_Application(){}
static void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
struct OutputSource:Audio::Core::Output {
    OutputSource():Output("authored-stereo-route",2,0){m_uOutBuffer=0;}
};
struct Source {
    OutputSource output;
    std::array<F32,2*Audio::Core::uFrameLength> planes{};
    U64 frame=0;
    static void Begin(void* data){
        auto& s=*static_cast<Source*>(data);
        for(U32 i=0;i<Audio::Core::uFrameLength;++i){
            const F64 time=F64(s.frame*Audio::Core::uFrameLength+i)/48000.;
            s.planes[i]=F32(.25*std::sin(time*2*3.141592653589793*317));
            s.planes[Audio::Core::uFrameLength+i]=F32(.4*std::cos(time*2*3.141592653589793*733));
        }
        Audio::Core::ProcessInfo info{};info.pStaticBuffers=s.planes.data();
        s.output.SetActive(true);s.output.MoveAudio(info);++s.frame;
    }
};
struct NativeStereoProbe:Audio::Core::Engine {
    void Channels(U32 n){m_uChannels=n;}
    using Engine::MixStereo;
};
static void route(){
    using namespace Audio::Core;
    Source source;Renderer renderer(8);
    require(renderer.Init(&source,Source::Begin,nullptr,0,false),"Actual stereo renderer initialization failed");
    // Join the SDL consumer before starting the producer. The test becomes the
    // sole real ring consumer and never races the output Engine for frames.
    require(Engine::Instance().Shutdown(&renderer),"Actual SDL consumer did not join");
    source.output.AddInput(renderer.StereoInput());renderer.Start();
    NativeStereoProbe mixer;F32 maximumLeft=0,maximumRight=0,maximumBoundary=0,lastRight=0;
    for(U32 frame=0;frame<64;++frame){
        F32 *surroundLFE=nullptr,*surround=nullptr,*stereo=nullptr,*mono=nullptr;
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(!renderer.GetNextFrame(&surroundLFE,&surround,&stereo,&mono)){
            require(std::chrono::steady_clock::now()<deadline,"Actual original producer ring did not publish PCM");MT::Thread::Sleep(1);
        }
        require(stereo&&renderer.StereoInput(),"Actual stereo bus unavailable");
        for(U32 i=0;i<uFrameLength;++i){
            const F64 time=F64(U64(frame)*uFrameLength+i)/48000.;
            require(std::abs(stereo[i]-F32(.25*std::sin(time*2*3.141592653589793*317)))<1e-6f&&std::abs(stereo[uFrameLength+i]-F32(.4*std::cos(time*2*3.141592653589793*733)))<1e-6f,"Actual Output/Input/Renderer did not preserve the independent planar channels");
        }
        for(U32 channels:{2u,6u,8u}){
            std::array<F32,8*uFrameLength> interleaved{};mixer.Channels(channels);mixer.MixStereo(stereo,interleaved.data(),channels);
            for(U32 i=0;i<uFrameLength;++i){maximumLeft=std::max(maximumLeft,std::abs(interleaved[i*channels]-stereo[i]));maximumRight=std::max(maximumRight,std::abs(interleaved[i*channels+1]-stereo[uFrameLength+i]));for(U32 ch=2;ch<channels;++ch)require(interleaved[i*channels+ch]==0,"Native stereo adapter changed unrelated speaker channels");}
            if(channels==2){if(frame)maximumBoundary=std::max(maximumBoundary,std::abs(interleaved[1]-lastRight));lastRight=interleaved[(uFrameLength-1)*2+1];}
        }
    }
    std::cout<<"Actual Output/Input/Renderer stereo planes: left_error="<<maximumLeft<<" right_error="<<maximumRight<<" max_right_quantum_boundary="<<maximumBoundary<<std::endl;
    require(maximumLeft==0&&maximumRight==0,"Native stereo adapter substituted adjacent left samples for the actual right plane");
    require(maximumBoundary<.04f,"Native stereo adapter introduced a discontinuity at the 128-frame quantum boundary");
}

struct PCMFile {
    std::string path,previous;bool hadPrevious=false;
    PCMFile(){if(const char* old=std::getenv("BLUR_MAC_AUDIO_PCM_STATS_FILE")){hadPrevious=true;previous=old;}char name[]="/tmp/blur-stereo-stats-XXXXXX";const int fd=mkstemp(name);require(fd>=0,"PCM statistics fixture file failed");::close(fd);path=name;setenv("BLUR_MAC_AUDIO_PCM_STATS_FILE",path.c_str(),1);}
    ~PCMFile(){if(hadPrevious)setenv("BLUR_MAC_AUDIO_PCM_STATS_FILE",previous.c_str(),1);else unsetenv("BLUR_MAC_AUDIO_PCM_STATS_FILE");::unlink(path.c_str());}
    static F64 Field(const std::string& line,const std::string& key){auto i=line.find("\""+key+"\":");require(i!=std::string::npos,"PCM statistics field missing");return std::stod(line.substr(i+key.size()+3));}
    void Validate(){std::ifstream file(path);std::string line;U32 sessions=0,samples=0,nonzero=0;
        while(std::getline(file,line)){
            require(line.size()>2&&line.front()=='{'&&line.back()=='}',"PCM statistics line truncated");
            if(line.find("audio_pcm_session")!=std::string::npos){++sessions;require(Field(line,"overflow")==0&&line.find("\"pcm_written\":false")!=std::string::npos,"PCM statistics bound/privacy contract changed");continue;}
            ++samples;
            require(Field(line,"actual_stereo_left_mean_square_error")<1e-12&&Field(line,"actual_stereo_right_mean_square_error")<1e-12,"Actual Engine writer did not preserve source stereo planes");
            if(Field(line,"stereo_left_energy")>.005&&Field(line,"stereo_right_energy")>.005){++nonzero;require(Field(line,"legacy_stereo_right_mean_square_error")>.01,"Independent actual writer channels did not distinguish the old mapping");}
        }
        require(sessions==1&&samples>=2&&nonzero>=1,"Actual Engine writer did not flush bounded nonzero signal evidence");
    }
};
static void actualWriterStats(){
    PCMFile proof;Source source;
    {
        Audio::Core::Renderer renderer(8);require(renderer.Init(&source,Source::Begin,nullptr,0,false),"Actual statistical output owner failed");
        source.output.AddInput(renderer.StereoInput());renderer.Start();const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(4);
        while(Audio::Core::Engine::Instance().SubmittedFrames()<60000){require(Audio::Core::Engine::Instance().IsRunning(),"Actual output writer stopped");require(std::chrono::steady_clock::now()<deadline,"Actual statistical PCM window did not advance");MT::Thread::Sleep(1);}
    }
    proof.Validate();
}

struct SchedulerOwner{SchedulerOwner(){Scheduler::Init();Scheduler::SetGroupPoolSize(64);Scheduler::jqEnableWorkers(Scheduler::JQ_CORE_1|Scheduler::JQ_CORE_3);Scheduler::Start();}~SchedulerOwner(){Scheduler::Shutdown();}};
int main(){try{SchedulerOwner scheduler;route();actualWriterStats();require(Audio::Core::Engine::Instance().Shutdown(),"Stereo fixture final consumer shutdown failed");std::cout<<"Independent continuous stereo channels through actual output/input/renderer ring and native2/6/8-channel adapter PASS; no music asset/listening claim\n";return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
