// [Blur2PC][macOS] Actual resource manager, mounts and Xt over authored fixtures.
#include <core/Core.h>
#include <core/Maths.h>
#include <core/containers.h>
#include <core/threads.h>
#include <system/serialise/C_FileSystem.h>
#include <system/io/ASync.h>
#include <resource/Resource.h>
#include <memoryfs/MemoryFs.h>
#include <overlayfs/OverlayFs.h>
#include <pakfs/PakFs.h>
#include <pakfs/PakGlobals.h>
#include <system/scheduler/jobqueue.h>
#include <Xt/XtGame.h>
#include <Xt/XtStd.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

// A real concrete object probes an abstract, non-default reflected base.
struct ProbeLeading { virtual ~ProbeLeading()=default;U64 marker=0x1122334455667788ULL; };
struct ProbeBase { virtual ~ProbeBase()=default;S32 kind=0; };
struct ProbeCount:ProbeLeading,ProbeBase { explicit ProbeCount(S32 value){kind=value;}virtual void ConcreteKind()=0; };
struct ProbeConcrete:ProbeCount { ProbeConcrete():ProbeCount(17){}void ConcreteKind()override{} };
namespace Xt {template<>struct FaBase_LayoutProbe<ProbeCount>{using Type=ProbeConcrete;};}
static const int probeBaseOffset=Xt::FaBase_Offset<ProbeCount,ProbeBase>();

struct FixtureDefinition { XtU32 id; XtF32 value; };
XT_TYPE(FixtureDefinition, "NativeFixtureDefinition")
    XT_FIELD(id, "id")
    XT_FIELD(value, "value")
XT_END_TYPE
XT_APPLICATION
    XT_REGISTER_TYPE(FixtureDefinition)
XT_END_APPLICATION

static void require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
static std::vector<U8> readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "fixture cannot be read");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
class FixtureResource : public Resource::C_Resource<FixtureResource> {
public:
    static inline std::atomic<int> live{0}, loaded{0}, freed{0}, postLoaded{0};
    std::vector<U8> bytes;
    FixtureResource() { ++live; }
    ~FixtureResource() override { --live; }
    static Util::C_String StaticGetResourceTypeName() { return "NativeFixtureResource"; }
    bool LoadResourceData() override {
        std::unique_ptr<Sys::I_File> file(OpenResourceFile());
        if (!file) return false;
        bytes.resize(static_cast<std::size_t>(file->Length()));
        if (!file->Read(bytes.data(), static_cast<U32>(bytes.size()))) { bytes.clear(); return false; }
        ++loaded; return true;
    }
    bool FreeResourceData() override { bytes.clear(); ++freed; return true; }
    bool PostLoadResourceData() override { ++postLoaded; return true; }
};
struct Listener : Resource::C_Listener {
    std::atomic<int> ready{0}, errors{0};
    void ResourceNotify(Resource::C_ResourceBase*, Resource::E_Notify event, void*) override {
        if (event == Resource::E_Notify_DataReady) ++ready;
        if (event == Resource::E_Notify_DataError) ++errors;
    }
};
template<class F> static void pump(Resource::C_Manager& manager, F done) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(6);
    while (!done()) {
        require(std::chrono::steady_clock::now() < deadline, "resource state transition timed out");
        manager.Update(); std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
static void resources(const std::filesystem::path& path, const std::vector<U8>& expected, Sys::I_FileSystem* filesystem = nullptr) {
    require(FixtureResource::live == 0, "previous resource manager left live objects");
    FixtureResource::loaded = FixtureResource::freed = FixtureResource::postLoaded = 0;
    Resource::C_Manager manager;
    if (filesystem) manager.SetFileSystems(filesystem, nullptr);
    {
        Resource::C_Handle<FixtureResource> handle, other;
        Resource::C_HandleBase empty;
        require(handle == empty && !(handle != empty), "empty typed handle comparison changed");
        Listener listener;
        require(handle.Load(path.c_str(), &listener), "resource could not enter load queue");
        FixtureResource* resource = handle;
        require(Resource::WaitForResourceData(resource), "resource loading failed");
        require(resource->bytes == expected && listener.ready == 1 && listener.errors == 0,
                "resource data/notification changed");
        require(FixtureResource::postLoaded == 1, "main-thread resource post-load did not run");
        Resource::C_HandleBase base(handle);
        other = base;
        require(other == base && other == handle && FixtureResource::live == 1,
                "typed/base copy lost shared identity or loaded twice");
        Resource::C_Handle<FixtureResource> repeated;
        require(repeated.Load(path.c_str()) && repeated == handle && FixtureResource::loaded == 1,
                "resource cache failed deduplication");
        handle = static_cast<Resource::C_ResourceBase*>(nullptr);
        other = static_cast<Resource::C_ResourceBase*>(nullptr);
        require(static_cast<FixtureResource*>(repeated)->bytes == expected,
                "releasing copied handles freed live resource data");
        base = static_cast<Resource::C_ResourceBase*>(nullptr);
        repeated = static_cast<Resource::C_ResourceBase*>(nullptr);
    }
    pump(manager, [] { return FixtureResource::live == 0; });
    require(FixtureResource::freed == 1 && manager.Validate(), "resource final release/manager invariants failed");
    {
        Listener listener; Resource::C_Handle<FixtureResource> missing;
        require(missing.Load((path.string() + ".missing").c_str(), &listener), "missing resource did not enter load queue");
        FixtureResource* resource = missing;
        require(!Resource::WaitForResourceData(resource) && listener.errors == 1,
                "missing resource reported ready data");
    }
    pump(manager, [] { return FixtureResource::live == 0; });
}
static void mounts(const std::filesystem::path& root, std::vector<U8>& expected) {
    LogicalFs::C_MemoryFs memory;
    Sys::C_FileInfo info;
    info.Clear(); info.m_FileName = "/entry.bin"; info.m_FileType = Sys::E_FileType_File;
    info.m_FileSize = expected.size(); info.m_FileAccess = Sys::E_FileAccess_Read;
    require(memory.InjectFile(info, expected.data()), "original memory filesystem injection failed");
    LogicalFs::C_OverlayFs overlay;
    LogicalFs::C_OverlayLayer::C_Params params;
    params.flags.layerType = LogicalFs::C_OverlayLayer::E_Type_Overlay;
    overlay.AddLayer("/virtual", &memory, "/", params);
    {
        std::unique_ptr<Sys::I_File> file(overlay.Open("/virtual/entry.bin"));
        std::vector<U8> bytes(expected.size());
        require(file && file->Read(bytes.data(), U32(bytes.size())) && bytes == expected,
                "original overlay/memory mount changed bytes");
    }
    for (int version : {1, 2}) for (int codec : {1, 2, 4}) {
        LogicalFs::C_PakFs pak;
        const auto path = root / ("v" + std::to_string(version) + "-c" + std::to_string(codec) + ".pak");
        require(pak.OpenPak(&Sys::C_FileSystem::Instance(), path.c_str()), "original engine PAK could not mount fixture");
        {
            std::unique_ptr<Sys::I_File> file(pak.Open("/fixture/entry.bin"));
            std::vector<U8> bytes(expected.size());
            require(file && file->Length() == expected.size() && file->Read(bytes.data(), U32(bytes.size())) &&
                    bytes == expected, "original engine PAK decode differs from independent payload");
            // The original engine deliberately asserts on backward physical
            // seeks in compressed entries. Exercise its supported forward path
            // on a freshly opened stream, retaining that performance invariant.
            std::unique_ptr<Sys::I_File> seek(pak.Open("/fixture/entry.bin"));
            seek->SetPosition(17); U8 sample[53];
            require(seek->Read(sample, sizeof(sample)) && std::equal(sample, sample + sizeof(sample), expected.begin() + 17),
                    "original entry forward seek failed");
        }
        for (U32 buffering : {0U, Sys::C_OpenParams::kBufferSizeHintDefault}) {
            Sys::C_OpenParams params(buffering, false, false);
            std::unique_ptr<Sys::I_File> stream(pak.Open("/fixture/entry.bin", Sys::E_FileAccess_Read, params));
            require(bool(stream), "buffered/unbuffered archive stream could not open");
            std::vector<U8> bytes(expected.size());
            for (std::size_t offset = 0; offset < bytes.size();) {
                const U32 count = U32(std::min<std::size_t>(997, bytes.size() - offset));
                require(stream->Read(bytes.data() + offset, count), "fragmented archive read failed");
                offset += count;
                require(stream->GetPosition() == offset, "archive cursor changed across buffer refill");
            }
            require(bytes == expected, "fragmented buffered/unbuffered archive bytes changed");
            U8 sentinel = 0xa7;
            require(!stream->Read(&sentinel, 1) && sentinel == 0xa7 && stream->GetPosition() == expected.size(),
                    "archive EOF read exposed stale bytes or advanced cursor");
            stream.reset(pak.Open("/fixture/entry.bin", Sys::E_FileAccess_Read, params));
            const auto tail = expected.size() - 19;
            require(stream->Read(nullptr, U32(tail)), "archive null-target skip failed");
            U8 last[32]; std::fill(std::begin(last), std::end(last), 0xa7);
            require(!stream->Read(last, sizeof(last)) &&
                    std::equal(last, last + 19, expected.begin() + tail) && last[19] == 0xa7 &&
                    stream->GetPosition() == expected.size(), "archive short read lost tail or wrote past EOF");
            if (codec == 1) {
                stream->SetPosition(tail);
                require(stream->Read(last, 19) && std::equal(last, last + 19, expected.begin() + tail),
                        "raw archive backward seek within partial final buffer failed");
                stream->SetPosition(23);
                require(stream->Read(last, sizeof(last)) &&
                        std::equal(std::begin(last), std::end(last), expected.begin() + 23),
                        "raw archive backward physical seek failed");
            }
        }
        // Exercise the actual game-style path: manager worker -> mount ->
        // original codec -> native async disk I/O, then post-load on main.
        resources("/fixture/entry.bin", expected, &pak);
        pak.ClosePak();
    }
}
static void schema() {
    require(XtInitialise(), "actual Xt application schema could not initialize");
    auto* application = XtGetApplicationSchema();
    auto* fixture = XtFindTypeByName(application, "NativeFixtureDefinition");
    require(fixture && fixture->size == sizeof(FixtureDefinition) && fixture->fieldCount == 2 &&
            XtFindFieldByName(fixture, "id")->offset == offsetof(FixtureDefinition, id) &&
            XtFindFieldByName(fixture, "value")->offset == offsetof(FixtureDefinition, value),
            "actual Xt annotation lost native field layouts");
    XtU8* bytes = nullptr; int size = 0;
    require(XtExportSchema(application, bytes, size), "Xt schema export failed");
    std::unique_ptr<XtU8[]> owner(bytes);
    auto* imported = XtImportSchema(bytes, size);
    require(imported && imported->typeCount == application->typeCount &&
            XtFindTypeByName(imported, "NativeFixtureDefinition"), "Xt schema import failed");
    XtDestroySchema(imported);
    const char text[] = "123456789";
    require(XtCrc(reinterpret_cast<const XtU8*>(text), 9) == 0xcbf43926, "Xt CRC differs from independent CRC-32 vector");
    XtShutdown();
}
struct LoadingScheduler {
    LoadingScheduler() {
        static const Scheduler::Policy workers[8] = {5, 5, 5, 5, 5, 5, 5, 5};
        Pak::SetPacketCompressionPolicy(Scheduler::CreatePolicy(workers));
        Scheduler::Init();
        Scheduler::SetGroupPoolSize(64);
        Scheduler::jqEnableWorkers(Scheduler::JQ_CORE_1 | Scheduler::JQ_CORE_3);
        Scheduler::Start();
    }
    ~LoadingScheduler() { Scheduler::Shutdown(); Pak::SetPacketCompressionPolicy(-1); }
};
int main(int argc, char** argv) {
    try {
    ProbeConcrete probe;ProbeCount* reflected=&probe;
    auto* restored=reinterpret_cast<ProbeBase*>(reinterpret_cast<char*>(reflected)+probeBaseOffset);
    require(probeBaseOffset>0&&restored==static_cast<ProbeBase*>(reflected)&&restored->kind==17,
        "Abstract reflection layout probe changed its nonzero base offset/real constructor");

        require(argc == 2, "fixture directory missing");
        const std::filesystem::path root(argv[1]); auto expected = readFile(root / "payload.bin");
        require(IoInitialise(), "native I/O could not start");
        LoadingScheduler scheduler;
        Sys::SetDefaultFileSystem(&Sys::C_FileSystem::Instance());
        resources(root / "payload.bin", expected);
        mounts(root, expected);
        schema();
        Resource::C_TypeRegistry::DeleteInstance();
        IoShutdown();
        std::cout << "Original resource/mount/PAK/Xt loading integration PASS\n";
    } catch (const std::exception& error) { IoShutdown(); std::cerr << error.what() << '\n'; return 1; }
}
