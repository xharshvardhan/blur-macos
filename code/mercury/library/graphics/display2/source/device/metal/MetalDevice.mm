// [Blur2PC][macOS] Shared Apple GPU storage with explicit CPU/GPU ownership.
#include <display2/device/MetalDevice.h>
#include "MetalNative.h"
#import <Metal/Metal.h>
#include <mutex>
#include <stdexcept>
#include <thread>

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
        return true;
    }
}
void C_MetalDevice::Destroy() { if(m_State)m_State->context.reset(); WaitIdle(); m_State.reset(); }
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
    // Include surface staging uploads, which use the same native queue but
    // are not submitted through the context's last-draw field.
    auto barrier=[m_State->queue commandBuffer];
    if(!barrier)return false;
    [barrier commit];[barrier waitUntilCompleted];
    const bool ok=barrier.status==MTLCommandBufferStatusCompleted&&
        (!m_State->last||m_State->last.status!=MTLCommandBufferStatusError);
    m_State->last=nil;return ok;
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
