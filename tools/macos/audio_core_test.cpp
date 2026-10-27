// [Blur2PC] Actual original audio archive, native workers, DSP and SDL output; no game banks.
#include "AudioPch.h"
#include "source/core/macos/Engine.h"
#include <array>
#include <iostream>
#include <stdexcept>

// This fixture has no application Xt types; the engine's schema machinery still links.
void XtLibrary_Application(){}
static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
template<class F> static void until(F condition,const char* message){
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(!condition()){require(std::chrono::steady_clock::now()<end,message);MT::Thread::Sleep(1);}
}
struct Worker:Audio::Core::Thread {
    std::atomic<bool> entered{false},release{false};int setups=0,finishes=0;
    void Setup()override{++setups;}
    void Execute(void* context)override{require(context==this,"Native audio thread lost its context");entered=true;while(!release)SleepThread(1);}
    S32 Shutdown()override{++finishes;return 37;}
};
struct MixProbe:Audio::Core::Engine {
    void Channels(U32 channels){m_uChannels=channels;}
    using Engine::MixMono;using Engine::MixStereo;using Engine::MixSurroundLFE;
};
static void workers(){
    Worker worker;require(worker.Start(&worker),"Native audio worker failed to start");
    until([&]{return worker.entered.load();},"Native audio worker did not execute");
    require(!worker.Start(&worker)&&worker.Suspend()==-1&&worker.Resume()==-1,"Native worker claimed duplicate/suspend support");
    worker.release=true;require(worker.WaitUntilTerminated()==37&&worker.GetExitCode()==37,"Native worker join lost its result");
    require(worker.WaitUntilTerminated()==37&&worker.Start(&worker),"Native worker could not restart after joining");
    require(worker.WaitUntilTerminated()==37&&worker.setups==2&&worker.finishes==2,"Native worker lifecycle callbacks were unbalanced");
    Audio::Core::ThreadSafeIndex index;index=17;std::vector<std::thread> threads;
    for(int i=0;i<4;++i)threads.emplace_back([&]{for(int j=0;j<10000;++j)index+=1;});
    for(auto& thread:threads)thread.join();require(U32(index)==40017,"Native audio queue atomics lost writes");
    StopWatch watch;MT::Thread::Sleep(12);require(watch.Time()>=.010f,"Native audio profiling did not measure elapsed time");
}
static void algorithms(){
    using namespace Audio::Core;
    static_assert(uFrameLength==128&&uSampleRate==24000,"Native audio must retain PC cadence");
    std::array<F32,uFrameLength> a,b,out;
    for(U32 i=0;i<uFrameLength;++i){a[i]=F32(i)/128;b[i]=F32(128-i)/128;}
    Audio::Frame::DoAdd(out.data(),a.data(),b.data());for(auto value:out)require(value==1.f,"Original frame add changed");
    Audio::Frame::DoMultiply(out.data(),a.data(),b.data());for(U32 i=0;i<uFrameLength;++i)require(out[i]==a[i]*b[i],"Original frame multiply changed");
    std::array<F32,2*uFrameLength> stereo;stereo.fill(.5f);
    DSP::Saturator saturator("native-fixture",2);ProcessInfo info{};
    saturator.Process(stereo.data(),info,true);stereo.fill(.5f);saturator.Process(stereo.data(),info,true);
    for(auto value:stereo)require(std::abs(value-.5f)<1e-6f,"Original steady saturator altered an unclipped signal");
    saturator.SetParam(DSP::Saturator::eDrive,4.f);saturator.Process(stereo.data(),info,true);stereo.fill(.5f);saturator.Process(stereo.data(),info,true);
    for(auto value:stereo)require(std::isfinite(value)&&std::abs(value-1.f)<1e-6f,"Original saturator drive/clip changed");
    std::array<F32,6*uFrameLength> planes;for(U32 c=0;c<6;++c)std::fill_n(planes.data()+c*uFrameLength,uFrameLength,F32(c+1));
    MixProbe mix;
    for(U32 channels:{2u,6u,8u}){
        mix.Channels(channels);std::vector<F32> result(channels*uFrameLength,0.f);mix.MixSurroundLFE(planes.data(),result.data(),channels);
        for(U32 i=0;i<uFrameLength;++i){
            const F32* row=result.data()+i*channels;
            if(channels==2)require(row[0]==2.5f&&row[1]==3.5f,"Original PC stereo downmix changed");
            else{for(U32 c=0;c<6;++c)require(row[c]==F32(c+1),"Original PC surround plane routing changed");if(channels==8)require(row[6]==1.25f&&row[7]==1.5f,"Original PC rear contribution changed");}
        }
        std::fill(result.begin(),result.end(),0.f);mix.MixMono(a.data(),result.data(),channels);
        for(U32 i=0;i<uFrameLength;++i)require(channels==2?(result[i*channels]==a[i]&&result[i*channels+1]==a[i]):result[i*channels+2]==a[i],"Original mono output routing changed");
        std::fill(result.begin(),result.end(),0.f);mix.MixStereo(stereo.data(),result.data(),channels);
        for(U32 i=0;i<uFrameLength;++i)require(result[i*channels]==stereo[i]&&result[i*channels+1]==stereo[i+1],"Original PC adjacent-sample stereo convention changed");
    }
}
struct RenderFixture {
    std::atomic<U64> begins{0},ends{0};
    static void Begin(void* pointer){++static_cast<RenderFixture*>(pointer)->begins;}
    static void End(void* pointer){++static_cast<RenderFixture*>(pointer)->ends;}
};
// [Blur2PC][macOS] Retail PC leaves spectral reverb dry; output quality must
// neither select the console FFT implementation nor change the mixer cadence.
static void spectralReverb(){
    using namespace Audio::Core;
    static_assert(uSampleRate==24000&&uFrameLength==128,"PC spectral contract uses the original mixer cadence");
    for(U32 rate:{24000u,36000u,48000u}){
        SetOutputSampleRate(rate);
        for(U16 channels:{U16(1),U16(2)}){
            DSP::SpectralReverb reverb("native-pc-spectral-contract",channels);
            require(reverb.GetType()==DSP::Effect::eSpectralReverb,"Original spectral component lost its effect type");
            reverb.SetParam(DSP::SpectralReverb::eMasterDecayTime,2.f);
            reverb.SetParam(DSP::SpectralReverb::eReverbGain,.75f);
            std::array<F32,2*uFrameLength+16> guarded;
            for(U32 i=0;i<guarded.size();++i)guarded[i]=F32(i)/512.f;
            const auto expected=guarded;ProcessInfo info{};
            require(reverb.Process(guarded.data()+8,info,true),"Original PC spectral path failed");
            require(guarded==expected,"Native spectral path changed original PC dry PCM or its bounds");
            reverb.SetActive(false);require(reverb.Process(guarded.data()+8,info,false)&&guarded==expected,"Inactive spectral effect changed dry PCM");
            require(reverb.DynamicBuffersRequired(info)==0,"Original PC spectral path requested unexpected dynamic buffers");
        }
    }
    ResetOutputSampleRate();
}
static void renderOutput(){
    using namespace Audio::Core;auto& engine=Engine::Instance();
    require(!engine.Init(nullptr)&&!engine.IsRunning(),"Native output accepted an absent renderer");
    require(!engine.ConfigureChannels(3)&&!engine.SetOutputSampleRate(44100),"Native output accepted unsupported game options");
    for(U32 channels:{2u,6u,8u}){
        require(engine.ConfigureChannels(channels),"Native output channel configuration failed");
        RenderFixture callbacks;
        {
            Renderer renderer(8);require(renderer.Init(&callbacks,RenderFixture::Begin,RenderFixture::End,0,false),engine.Error().c_str());renderer.Start();
            until([&]{return engine.SubmittedFrames()>=2048;},"Original renderer did not feed SDL PCM output");
            require(engine.IsRunning()&&engine.QueuedFrames()>=0,"Native renderer output did not remain live");
            require(!engine.ConfigureChannels(2),"Native output changed a live layout");
            renderer.MuteOutput();MT::Thread::Sleep(8);renderer.UnmuteOutput();
            for(U32 rate:{24000u,36000u,48000u}){
                require(engine.SetOutputSampleRate(rate),engine.Error().c_str());
                require(std::abs(F32(fOutputRateRatio)-24000.f/F32(rate))<1e-6f,"Native audio quality lost original pitch correction");
                until([&]{return engine.SubmittedFrames()>=1024;},"Quality change did not restart original renderer consumption");
                const auto measured=engine.Diagnostics();
                require(measured.output.deviceRate&&measured.output.devicePeriodFrames&&measured.maximumQueueTargetFrames,"Native output device period/usable-output target is unavailable");
                require(engine.QueuedFrames()<=S64(measured.maximumQueueTargetFrames+uFrameLength),"Native output queue grew beyond its measured usable-output target and one mixer quantum");
            }
        }
        require(!engine.IsRunning()&&engine.QueuedFrames()==-1,"Renderer destruction retained a native PCM owner");
        require(callbacks.begins>0&&callbacks.begins==callbacks.ends,"Original renderer callbacks did not balance");
    }
    for(U32 retry=0;retry<2;++retry){
        RenderFixture callbacks;auto* renderer=Audio::CreateRenderer();require(renderer==Audio::CreateRenderer(),"Original audio factory lost its singleton owner");
        require(renderer->Init(&callbacks,RenderFixture::Begin,RenderFixture::End,0,false),"Original audio factory could not initialize after prior retirement");renderer->Start();
        until([&]{return engine.SubmittedFrames()>=512;},"Original audio factory restart did not produce PCM");delete renderer;
        require(!engine.IsRunning()&&callbacks.begins==callbacks.ends,"Original audio factory retained a worker after deletion");
    }
    require(engine.Shutdown()&&engine.Shutdown(),"Native audio shutdown was not idempotent");
}
static void pitchShift(){
    using namespace Audio::Core;
    SetOutputSampleRate(48000);ProcessInfo info{};
    DSP::PitchShift pitch("native-public-soundtouch",2);
    std::array<F32,2*uFrameLength+16> guarded;guarded.fill(17.25f);auto* samples=guarded.data()+8;
    for(U32 i=0;i<2*uFrameLength;++i)samples[i]=F32(i)/512;
    pitch.Process(samples,info,true);for(U32 i=0;i<2*uFrameLength;++i)require(samples[i]==F32(i)/512,"Zero-pitch native effect altered PCM");
    pitch.SetParam(DSP::PitchShift::ePitch,1.f);
    std::vector<F32> left,right;
    constexpr F64 twoPi=6.2831853071795864769;
    for(U32 block=0;block<600;++block){
        for(U32 i=0;i<uFrameLength;++i){const F64 time=F64(block*uFrameLength+i)/48000;samples[i]=F32(.2*std::sin(twoPi*440*time));samples[uFrameLength+i]=F32(.2*std::sin(twoPi*660*time));}
        require(pitch.Process(samples,info,true),"Native upstream pitch effect failed");
        for(U32 i=0;i<8;++i)require(guarded[i]==17.25f&&guarded[8+2*uFrameLength+i]==17.25f,"Native 128-sample pitch effect crossed its PCM block bounds");
        for(U32 i=0;i<2*uFrameLength;++i)require(std::isfinite(samples[i]),"Native upstream pitch effect produced nonfinite PCM");
        if(block>=500){left.insert(left.end(),samples,samples+uFrameLength);right.insert(right.end(),samples+uFrameLength,samples+2*uFrameLength);}
    }
    auto frequency=[](const std::vector<F32>& data){U32 crossings=0;F64 energy=0;for(U32 i=1;i<data.size();++i){crossings+=data[i-1]<=0&&data[i]>0;energy+=F64(data[i])*data[i];}require(energy/data.size()>.001,"Native pitch output remained silent after its algorithmic latency");return F64(crossings)*48000/F64(data.size()-1);};
    require(std::abs(frequency(left)-880)<35&&std::abs(frequency(right)-1320)<35,"Native upstream pitch shift lost octave or stereo channel routing");
    pitch.SetState(DSP::PitchShift::eActive,0);std::fill_n(samples,2*uFrameLength,.125f);pitch.Process(samples,info,true);for(U32 i=0;i<2*uFrameLength;++i)require(samples[i]==.125f,"Inactive native pitch effect failed to bypass");
}
int main(){
    try{workers();algorithms();spectralReverb();Scheduler::Init();Scheduler::SetGroupPoolSize(64);Scheduler::jqEnableWorkers(Scheduler::JQ_CORE_1 | Scheduler::JQ_CORE_3);Scheduler::Start();renderOutput();pitchShift();Scheduler::Shutdown();std::cout<<"Original audio archive, native workers, shared PC mix/DSP, public SoundTouch pitch and silent SDL renderer output PASS\n";return 0;}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
