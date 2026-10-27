// [Blur2PC][macOS] Shared Apple GPU storage with explicit CPU/GPU ownership.
#include <display2/device/MetalDevice.h>
#include "MetalNative.h"
#import <Metal/Metal.h>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits.h>
#include <pthread.h>
#include "MetalTrace.h"


// [Blur2PC] Preallocated numeric trace: no allocation/logging on encoding or wait paths.
// A span includes begin/end steady-clock timestamps, not Metal GPU start/end times.
namespace Display2::Metal::Trace {
namespace {
constexpr unsigned capacity=262144;
struct Record {
    std::uint64_t start,end,thread,resource,command;
    std::uint64_t totals[4],counts[4];
    unsigned phase; int before,after;
};
Record records[capacity];
std::atomic<unsigned> next{0},active{0};
std::atomic_flag sessionLock=ATOMIC_FLAG_INIT;
unsigned owners=0;
char output[PATH_MAX]{};
thread_local std::uint64_t encodingTotals[4]{},encodingCounts[4]{};
std::atomic<unsigned> generation{0};
thread_local unsigned encodingGeneration=0;
struct Lock { Lock(){while(sessionLock.test_and_set(std::memory_order_acquire))std::this_thread::yield();}
    ~Lock(){sessionLock.clear(std::memory_order_release);} };
std::uint64_t Thread(){ std::uint64_t id=0;pthread_threadid_np(nullptr,&id);return id; }
const char* names[]={"buffer_wait","buffer_retire_wait","surface_wait","surface_retire_wait",
    "download_wait","device_idle_wait","present_resource_wait","next_drawable","present_submit","present","encoding"};
}
std::uint64_t Now() noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
void BeginSession() noexcept {
    Lock lock;
    if(owners++){return;}
    next.store(0,std::memory_order_relaxed);output[0]=0;
    auto path=std::getenv("BLUR_MAC_METAL_TRACE_FILE");
    if(!path||path[0]!='/'||std::strlen(path)>=sizeof(output))return;
    std::strcpy(output,path);generation.fetch_add(1,std::memory_order_relaxed);enabled.store(true);
}
void EndSession() noexcept {
    Lock lock;
    if(!owners||--owners)return;
    enabled.store(false);
    // Only diagnostic writers are joined here, after the device's existing WaitIdle.
    // No GPU completion callback uses the trace or any engine-owned allocation.
    while(active.load())std::this_thread::yield();
    if(!output[0])return;
    auto file=std::fopen(output,"w");
    if(!file){std::fprintf(stderr,"Native Metal trace output could not be opened\n");return;}
    unsigned total=next.load();unsigned written=std::min(total,capacity);
    std::fprintf(file,"{\"type\":\"metal_trace\",\"version\":1,\"clock\":\"steady_clock_ns\",\"capacity\":%u,\"events\":%u,\"dropped\":%u}\n",capacity,written,total-written);
    for(unsigned i=0;i<written;++i){const auto& r=records[i];
        std::fprintf(file,"{\"type\":\"%s\",\"steady_ns\":%llu,\"end_ns\":%llu,\"duration_ns\":%llu,\"thread\":%llu,\"resource\":%llu,\"command\":%llu,\"status_before\":%d,\"status_after\":%d",names[r.phase],r.start,r.end,r.end-r.start,r.thread,r.resource,r.command,r.before,r.after);
        if(r.phase==unsigned(Phase::Encoding))std::fprintf(file,",\"draw_ns\":%llu,\"pipeline_ns\":%llu,\"constants_ns\":%llu,\"textures_ns\":%llu,\"draw_count\":%llu,\"pipeline_count\":%llu,\"constants_count\":%llu,\"textures_count\":%llu",r.totals[0],r.totals[1],r.totals[2],r.totals[3],r.counts[0],r.counts[1],r.counts[2],r.counts[3]);
        std::fputs("}\n",file);
    }
    std::fclose(file);output[0]=0;
}
Span::Span(Phase phase,const void* resource,const void* command,int status) noexcept {
    if(!enabled.load(std::memory_order_relaxed))return;
    active.fetch_add(1);
    if(!enabled.load()){active.fetch_sub(1);return;}
    m_Index=next.fetch_add(1,std::memory_order_relaxed);
    m_Status=status;
    if(m_Index<capacity){auto& r=records[m_Index];r={};r.start=Now();r.phase=unsigned(phase);r.resource=reinterpret_cast<std::uintptr_t>(resource);r.command=reinterpret_cast<std::uintptr_t>(command);r.thread=Thread();r.before=status;}
}
Span::~Span(){
    if(m_Index==~0u)return;
    if(m_Index<capacity){records[m_Index].end=Now();records[m_Index].after=m_Status;}
    active.fetch_sub(1);
}
EncodingScope::~EncodingScope(){if(m_Start){
    const unsigned current=generation.load(std::memory_order_relaxed);
    if(encodingGeneration!=current){for(unsigned i=0;i<4;++i){encodingTotals[i]=0;encodingCounts[i]=0;}encodingGeneration=current;}
    unsigned k=unsigned(m_Kind);encodingTotals[k]+=Now()-m_Start;++encodingCounts[k];}}
void PresentEncodingSummary(std::uint64_t present) noexcept {
    if(!enabled.load(std::memory_order_relaxed))return;
    const unsigned current=generation.load(std::memory_order_relaxed);
    if(encodingGeneration!=current){for(unsigned i=0;i<4;++i){encodingTotals[i]=0;encodingCounts[i]=0;}encodingGeneration=current;}
    Span span(Phase::Encoding,reinterpret_cast<const void*>(std::uintptr_t(present)));
    // Present and draw encoding run on the same original render owner. These are
    // inclusive CPU totals since its prior Present; draw contains the other fields.
    // Never sum them as mutually exclusive costs or call them GPU durations.
    if(span.m_Index<capacity){auto& r=records[span.m_Index];for(unsigned i=0;i<4;++i){r.totals[i]=encodingTotals[i];r.counts[i]=encodingCounts[i];}}
    for(unsigned i=0;i<4;++i){encodingTotals[i]=0;encodingCounts[i]=0;}
}
}

namespace Display2 {

C_MetalDevice::C_MetalDevice() = default;
C_MetalDevice::~C_MetalDevice() { Destroy(); }
bool C_MetalDevice::Create(){return CreateNative(nullptr);}
bool C_MetalDevice::CreateNative(void* selected) {
    @autoreleasepool {
        if (m_State) return false;
        auto state = std::make_unique<State>();
        state->device = selected?(__bridge id<MTLDevice>)selected:MTLCreateSystemDefaultDevice();
        if (!state->device||![state->device conformsToProtocol:@protocol(MTLDevice)]) return false;
        state->queue = [state->device newCommandQueue];
        if (!state->queue) return false;
        m_State = std::move(state);
        m_State->context=std::shared_ptr<C_Context>(new C_Context(*this));
        m_State->noCheckNoTrack=C_ContextNoCheckNoTrack(m_State->context);
        m_State->noCheck=C_ContextNoCheck(m_State->context);
        Metal::Trace::BeginSession();m_State->traceSession=true;
        return true;
    }
}
void C_MetalDevice::Destroy() { const bool traced=m_State&&m_State->traceSession;if(m_State)m_State->context.reset(); WaitIdle(); m_State.reset();if(traced)Metal::Trace::EndSession(); }
C_Context& C_MetalDevice::GetContext() {
    if(!m_State)throw std::logic_error("Metal device is not created");
    if(!m_State->context)m_State->context=std::shared_ptr<C_Context>(new C_Context(*this));
    return *m_State->context;
}
const C_Context& C_MetalDevice::GetContext() const {
    if(!m_State||!m_State->context)throw std::logic_error("Metal context has not been initialized");
    return *m_State->context;
}
C_ContextNoCheckNoTrack& C_MetalDevice::GetContextNoCheckNoTrack(){if(!m_State)throw std::logic_error("Metal device is not created");return m_State->noCheckNoTrack;}
const C_ContextNoCheckNoTrack& C_MetalDevice::GetContextNoCheckNoTrack()const{if(!m_State)throw std::logic_error("Metal device is not created");return m_State->noCheckNoTrack;}
C_ContextNoCheck& C_MetalDevice::GetContextNoCheck(){if(!m_State)throw std::logic_error("Metal device is not created");return m_State->noCheck;}
const C_ContextNoCheck& C_MetalDevice::GetContextNoCheck()const{if(!m_State)throw std::logic_error("Metal device is not created");return m_State->noCheck;}
void C_ContextNoCheckNoTrack::Bind(C_MetalDevice& device){*this=device.GetContextNoCheckNoTrack();}
U32 C_MetalDevice::IsCreated() const { return bool(m_State); }
std::string C_MetalDevice::GetDeviceName() const { return m_State ? m_State->device.name.UTF8String : ""; }
bool C_MetalDevice::WaitIdle() {
    @autoreleasepool {
    if (!m_State) return true;
    std::lock_guard<std::mutex> guard(m_State->mutex);
    const bool presentationOK=m_State->presentation.Retire(Metal::Trace::Phase::DeviceIdleWait,m_State.get());
    // Include surface staging uploads, which use the same native queue but
    // are not submitted through the context's last-draw field.
    auto barrier=[m_State->queue commandBuffer];
    if(!barrier)return false;
    [barrier commit];Metal::TraceCommandWait(barrier,Metal::Trace::Phase::DeviceIdleWait,m_State.get());
    const bool ok=barrier.status==MTLCommandBufferStatusCompleted&&
        (!m_State->last||m_State->last.status!=MTLCommandBufferStatusError);
    if(!ok)m_State->presentation.failed=true;
    m_State->last=nil;return presentationOK&&ok;
    }
}
bool C_MetalDevice::CreateVertexBuffer(U32 size, U32 flags, C_VertexBuffer* buffer, void* memory) {
    @autoreleasepool {
        if (!m_State || !buffer || buffer->IsCreated() || !size || memory || size > m_State->device.maxBufferLength) return false;
        auto native = std::make_shared<Metal::Buffer>();
        native->resource = [m_State->device newBufferWithLength:size options:MTLResourceStorageModeShared];
        if (!native->resource) return false;
        native->dynamic=flags&E_Flags_Dynamic;
        buffer->m_Buffer = std::move(native); buffer->m_Flags = flags; return true;
    }
}
bool C_MetalDevice::CreateIndexBuffer(U32 size, U32 flags, E_IndexFormat format, C_IndexBuffer* buffer, void* memory) {
    const U32 width = format == E_IndexFormat_16 ? 2 : format == E_IndexFormat_32 ? 4 : 0;
    if (!buffer || !width || size % width || buffer->IsCreated()) return false;
    C_VertexBuffer temporary;
    if (!CreateVertexBuffer(size, flags, &temporary, memory)) return false;
    buffer->m_Buffer = std::move(temporary.m_Buffer); buffer->m_Flags = flags; buffer->m_Format = format; return true;
}
void C_MetalDevice::DestroyVertexBuffer(C_VertexBuffer* buffer) {
    if (buffer) { buffer->m_Buffer.reset(); buffer->m_Flags = 0; }
}
void C_MetalDevice::DestroyIndexBuffer(C_IndexBuffer* buffer) {
    if (buffer) { buffer->m_Buffer.reset(); buffer->m_Flags = 0; buffer->m_Format = E_IndexFormat_Unknown; }
}
bool C_MetalDevice::CreateVertexDeclaration(const C_VertexFormat& format,C_VertexDeclaration* declaration) {
    if(format.m_ElementCount<=0||format.m_ElementCount>=format.m_MaxElementCount)return false;
    return CreateVertexDeclaration(format.m_Elements,declaration);
}
bool C_MetalDevice::CreateVertexDeclaration(const C_VertexElement* elements,C_VertexDeclaration* declaration) {
    if(!m_State||!elements||!declaration||declaration->IsCreated())return false;
    auto data=std::make_shared<Metal::VertexDeclaration>();
    for(U32 i=0;i<C_VertexDeclaration::m_MaxElementCount;++i) {
        const auto& element=elements[i];
        if(element.Stream==0xff&&element.Type==E_VertexElementType_Unknown) {
            if(data->elements.empty())return false;
            std::lock_guard<std::mutex> guard(m_State->mutex);declaration->m_Declaration=std::move(data);return true;
        }
        if(element.Stream>=8||element.Type>=E_VertexElementType_Unknown||element.Usage>=E_VertexElementUsage_Unknown||element.Method!=0)return false;
        for(const auto& previous:data->elements)if(previous.Usage==element.Usage&&previous.UsageIndex==element.UsageIndex)return false;
        data->elements.push_back(element);
    }
    return false;
}
C_VertexDeclaration* C_MetalDevice::CreateVertexDeclaration(const C_VertexFormat& format) {
    if(format.m_ElementCount<=0||format.m_ElementCount>=format.m_MaxElementCount)return nullptr;
    return CreateVertexDeclaration(format.m_Elements);
}
C_VertexDeclaration* C_MetalDevice::CreateVertexDeclaration(const C_VertexElement* elements) {
    if(!m_State)return nullptr;
    auto declaration=std::make_unique<C_VertexDeclaration>();
    if(!CreateVertexDeclaration(elements,declaration.get()))return nullptr;
    std::lock_guard<std::mutex> guard(m_State->mutex);
    for(const auto& old:m_State->declarations)if(old->m_Declaration&&old->m_Declaration->elements==declaration->m_Declaration->elements)return old.get();
    auto result=declaration.get();m_State->declarations.push_back(std::move(declaration));return result;
}
void C_MetalDevice::DestroyVertexDeclaration(C_VertexDeclaration* declaration) {
    if(!declaration)return;
    if(m_State) {
        std::lock_guard<std::mutex> guard(m_State->mutex);
        declaration->m_Declaration.reset();return;
    }
    declaration->m_Declaration.reset();
}
void C_MetalDevice::DestroyVertexDeclarations() { if(m_State){std::lock_guard<std::mutex> guard(m_State->mutex);m_State->declarations.clear();} }
bool C_MetalDevice::CopyBuffer(Metal::Buffer* destination, U32 destinationOffset, Metal::Buffer* source, U32 sourceOffset, U32 bytes) {
    @autoreleasepool {
        if (!m_State || !destination || !source || source == destination || !bytes ||
            ((destinationOffset | sourceOffset | bytes) & 3U)) return false;
        std::scoped_lock guard(m_State->mutex, destination->mutex, source->mutex);
        if (source->locked || destination->locked || source->recording || destination->recording || source->resource.device != m_State->device || destination->resource.device != m_State->device ||
            destinationOffset > destination->resource.length || bytes > destination->resource.length - destinationOffset ||
            sourceOffset > source->resource.length || bytes > source->resource.length - sourceOffset) return false;
        // A resource can be shared by devices/queues for the same physical GPU.
        // Resolve earlier submissions before issuing a command on this queue.
        if (!source->wait() || !destination->wait()) return false;
        id<MTLCommandBuffer> command = [m_State->queue commandBuffer];
        id<MTLBlitCommandEncoder> encoder = [command blitCommandEncoder];
        if (!command || !encoder) return false;
        [encoder copyFromBuffer:source->resource sourceOffset:sourceOffset toBuffer:destination->resource destinationOffset:destinationOffset size:bytes];
        [encoder endEncoding];
        [command commit];
        source->pending = destination->pending = m_State->last = command;
        source->ranges.push_back({sourceOffset,U64(sourceOffset)+bytes});
        destination->ranges.push_back({destinationOffset,U64(destinationOffset)+bytes});
        return true;
    }
}
#define METAL_BUFFER_METHODS(Type) \
    bool Type::IsCreated() const { return bool(m_Buffer); } \
    U32 Type::GetSize() const { if(!m_Buffer)return 0;std::lock_guard<std::mutex> guard(m_Buffer->mutex);return static_cast<U32>(m_Buffer->resource.length); } \
    void* Type::GetAddress() const { if(!m_Buffer)return nullptr;std::lock_guard<std::mutex> guard(m_Buffer->mutex);return m_Buffer->resource.contents; } \
    void Type::SetSize(U32 size) { if (size != GetSize()) throw std::invalid_argument("Metal buffer size changes require device allocation"); } \
    void Type::SetAddress(void* address) { if (address != GetAddress()) throw std::invalid_argument("Metal buffer cannot adopt external memory"); } \
    void* Type::Lock(U32 offset, U32 size, U32 flags) { return m_Buffer ? m_Buffer->lock(offset, size, flags) : nullptr; } \
    void Type::Unlock() { if (!m_Buffer) throw std::logic_error("Metal buffer is not created"); m_Buffer->unlock(); }
METAL_BUFFER_METHODS(C_VertexBuffer)
METAL_BUFFER_METHODS(C_IndexBuffer)
#undef METAL_BUFFER_METHODS
}
