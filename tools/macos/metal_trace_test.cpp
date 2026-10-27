// [Blur2PC][macOS] CPU-only diagnostic ownership/clock checks; no GPU timing claims.
#include "../../code/mercury/library/graphics/display2/source/device/metal/MetalTrace.h"
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>
#include <unistd.h>
using namespace Display2::Metal::Trace;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static std::string read(const std::string& path){std::ifstream file(path);return {std::istreambuf_iterator<char>(file),{}};}
int main(){
    char directory[]="/tmp/blur-metal-trace-XXXXXX";
    if(!mkdtemp(directory))return 1;
    const auto path=std::string(directory)+"/trace.jsonl";
    try {
        unsetenv("BLUR_MAC_METAL_TRACE_FILE");BeginSession();
        {Span disabled(Phase::Present);EncodingScope ignored(Encoding::Draw);}
        check(!enabled.load(),"Tracing became enabled without opt-in");EndSession();
        setenv("BLUR_MAC_METAL_TRACE_FILE","relative-unusable-trace",1);BeginSession();
        check(!enabled.load(),"Relative diagnostic path was accepted");EndSession();
        setenv("BLUR_MAC_METAL_TRACE_FILE",path.c_str(),1);BeginSession();BeginSession();
        check(enabled.load(),"Absolute trace did not enable");
        {Span status(Phase::BufferWait,reinterpret_cast<void*>(17),reinterpret_cast<void*>(29),3);status.Status(4);}
        try {Span unwind(Phase::DownloadWait);throw 1;}catch(int){}
        {EncodingScope draw(Encoding::Draw);{EncodingScope nested(Encoding::Constants);std::this_thread::yield();}}
        PresentEncodingSummary(1);
        EndSession();check(enabled.load()&&access(path.c_str(),F_OK)!=0,"First device flushed a shared live session");
        std::atomic<bool> started{false},release{false},ending{false},finished{false};
        std::thread worker([&]{Span held(Phase::SurfaceWait);started=true;while(!release.load())std::this_thread::yield();});
        while(!started.load())std::this_thread::yield();
        std::thread finish([&]{ending=true;EndSession();finished=true;});
        while(!ending.load())std::this_thread::yield();
        for(int i=0;i<1000;++i)std::this_thread::yield();
        check(!finished.load(),"Trace flushed while a writer owned a span");
        release=true;worker.join();finish.join();
        const auto text=read(path);
        check(text.find("\"events\":4,\"dropped\":0")!=std::string::npos,"Trace event count/drop metadata is incorrect");
        check(text.find("\"status_before\":3,\"status_after\":4")!=std::string::npos,"Wait statuses were lost");
        check(text.find("\"resource\":17,\"command\":29")!=std::string::npos,"Resource identities were lost");
        check(text.find("\"draw_count\":1,\"pipeline_count\":0,\"constants_count\":1,\"textures_count\":0")!=std::string::npos,"Inclusive encoding fields were lost");
        std::size_t cursor=0;unsigned spans=0;
        while((cursor=text.find("\"steady_ns\":",cursor))!=std::string::npos){
            cursor+=12;const auto begin=std::stoull(text.substr(cursor));
            auto endPosition=text.find("\"end_ns\":",cursor);const auto end=std::stoull(text.substr(endPosition+9));
            check(begin>0&&end>=begin,"Trace clock/span order is invalid");++spans;
        }
        check(spans==4,"Expected begin/end timestamps are missing");
        BeginSession();PresentEncodingSummary(2);EndSession();
        const auto reset=read(path);
        check(reset.find("\"events\":1,\"dropped\":0")!=std::string::npos&&reset.find("\"draw_count\":0")!=std::string::npos,"Restart retained prior session counters/events");
        unsetenv("BLUR_MAC_METAL_TRACE_FILE");BeginSession();{Span disabled(Phase::Present);}EndSession();
        check(read(path)==reset,"Disabled session rewrote prior trace");
        unlink(path.c_str());rmdir(directory);
        std::cout<<"Native Metal CPU trace lifecycle/clock/disabled/restart checks PASS\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<"\n";unlink(path.c_str());rmdir(directory);return 1;}
}
