// [Blur2PC][macOS] Wider engine system startup, not the Amax game executable.
#include <core/Core.h>
#include <core/Maths.h>
#include <core/containers.h>
#include <core/threads.h>
#include <system/debuglog.h>
#include <system/debug/log/DebugLog.h>
#include <system/debug/log/Subscribers/ConsoleSubscriber.h>
#include <system/serialise/C_FileSystem.h>
#include <system/xml/XmlDom.h>
#include <system/perf/CPUTiming.h>
#include <system/io/ASync.h>
#include <system/debug/BuildStamp.h>
#include <misc_shared/messaging/MessageDebug.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

static void require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
int main(int argc, char** argv) {
    try {
        require(argc == 2, "fixture root missing");
        require(IoInitialise(), "system I/O initialization failed");
        Sys::SetDefaultFileSystem(&Sys::C_FileSystem::Instance());
        Debug::GetRootLogger().AddSubscriber(new Debug::ConsoleSubscriber);
        auto& logger = Debug::GetLogger("macos.system.integration");
        LOGINFO(logger, "Native original system services initialized");
        const auto path = std::filesystem::path(argv[1]) / "native-system-config.xml";
        { std::ofstream file(path); file << "<Settings><Video mode=\"native\"/><Audio enabled=\"true\"/></Settings>"; }
        {
            Sys::C_XmlDom document;
            require(document.Read(path.c_str()), "actual XML DOM could not read through filesystem");
            const auto* root = document.GetRootNodePtr();
            require(root && root->GetChildren().Size() == 1 &&
                    root->GetChildren()[0]->GetName() == "settings" &&
                    root->GetChildren()[0]->GetChildren().Size() == 2, "actual XML DOM structure changed");
        }
        {
            Perf::C_CPUTimingStats stats(0, Maths::RGBA(1.f, 1.f, 1.f, 1.f), Util::C_String80("native timing"));
            { Perf::C_CPUTimingEvent event(stats); MT::Thread::Sleep(2); }
            require(stats.GetCallCount(Perf::E_TimingFrame_Curr) == 1 &&
                    stats.GetTotalTime(Perf::E_TimingFrame_Curr) > 0, "CPU timing did not record elapsed work");
            require(Perf::C_CPUTimingStats::FindStats("native timing") == &stats, "CPU timing registry failed");
            Util::C_List<Perf::C_CPUTimingStats>::C_Iterator iterator(stats.m_StatsList);
            require(iterator.Begin() && iterator.ReverseBegin(), "native nested list iterator lost owner");
            while (iterator) ++iterator;
            require(iterator.Begin() && iterator.ReverseBegin(), "end iterator lost list owner");
        }
        static const char* const names[] = {"NativeFixtureMessageB", "NativeFixtureMessageA"};
        const auto begin = static_cast<Messaging::TMsgType>(0x76540000);
        Messaging::RegisterMessageDebugInfo(names, 2, begin);
        require(std::strcmp(Messaging::FindDebugName(begin), names[0]) == 0,
                "actual messaging registry did not preserve debug metadata");
        auto* variable = Messaging::CreateDebugMessageType("System.NativeFixtureMessageType");
        Messaging::SetDebugMessageType(*variable, begin);
        require(Messaging::GetDebugMessageType(*variable) == begin, "message debug variable lost type");
        Messaging::DestroyDebugMessageType(variable);
        Messaging::UnregisterDebugInfo();
        require(std::strcmp(Debug::GetBuildVersion(), "native-development") == 0 &&
                std::strlen(Debug::GetBuildRevision()) >= 12 && std::strlen(Debug::GetBuildTime()) > 0 &&
                std::strlen(Debug::GetBuildDate()) == 10 && !Debug::GetServerBuild(),
                "native build stamp does not describe this local build");
        Debug::GetRootLogger().DeleteAllSubscribers();
        IoShutdown();
        std::filesystem::remove(path);
        std::cout << "Whole system archive linked; original logging/XML/CPU timing/native I/O integration PASS\n";
    } catch (const std::exception& error) { IoShutdown(); std::cerr << error.what() << '\n'; return 1; }
}
