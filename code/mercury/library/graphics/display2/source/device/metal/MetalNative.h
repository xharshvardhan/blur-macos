// [Blur2PC][macOS] Objective-C++ device state, private to the native backend.
#pragma once
#include <display2/device/MetalDevice.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <mutex>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <array>
#include "MetalVertexData.h"
#include "MetalTrace.h"
#include "MetalPresentDependency.h"
namespace Display2 {
namespace Metal {
struct Adapter {id<MTLDevice> device;};
struct Buffer {
    id<MTLBuffer> resource;
    id<MTLCommandBuffer> pending;
    std::mutex mutex;
    bool locked = false;
    const void* recording = nullptr;
    bool dynamic = false;
    std::vector<std::pair<U64,U64>> ranges;
    std::thread::id owner;
    ~Buffer() { @autoreleasepool { if (pending) TraceCommandWait(pending,Trace::Phase::BufferRetireWait,this); } }
    bool wait() {
        @autoreleasepool {
        if (!pending) return true;
        TraceCommandWait(pending,Trace::Phase::BufferWait,this);
        const bool ok = pending.status == MTLCommandBufferStatusCompleted;
        pending = nil;
        ranges.clear();
        return ok;
        }
    }
    void* lock(U32 offset, U32 size, U32 flags) {
        @autoreleasepool {
        std::lock_guard<std::mutex> guard(mutex);
        const bool discard=flags&0x2000U, noOverwrite=flags&0x1000U;
        if(locked||flags&~0x3810U||offset>resource.length||(size&&size>resource.length-offset)||
           (discard&&noOverwrite)||((discard||noOverwrite)&&(!dynamic||(flags&16U))))return nullptr;
        if(discard) {
            id<MTLBuffer> replacement=[resource.device newBufferWithLength:resource.length options:MTLResourceStorageModeShared];
            if(!replacement)return nullptr;
            resource=replacement;pending=nil;ranges.clear();
        } else if(noOverwrite) {
            const U64 end=offset+(size?U64(size):resource.length-offset);
            for(const auto& range:ranges)if(offset<range.second&&end>range.first)return nullptr;
        } else if(recording||!wait())return nullptr;
        locked = true; owner = std::this_thread::get_id();
        return static_cast<U8*>(resource.contents) + offset;
        }
    }
    void unlock() {
        std::lock_guard<std::mutex> guard(mutex);
        if (!locked || owner != std::this_thread::get_id()) throw std::logic_error("Metal buffer unlock requires the locking thread");
        locked = false; owner = {};
    }
};
}
namespace Metal {
struct UniformBinding { U32 slot, size, kind; };
struct TextureBinding { U32 slot; E_TextureType type; };
inline std::atomic<U64> nextShaderIdentity{1};
struct Shader {
    id<MTLLibrary> library;
    id<MTLFunction> function;
    std::vector<U8> blob;
    std::unordered_map<std::string, C_ShaderParam> parameters;
    std::vector<C_ShaderSemantic> semantics;
    std::vector<U32> locations;
    std::vector<UniformBinding> uniforms;
    std::vector<TextureBinding> textures;
    std::vector<U32> samplers;
    bool unsupportedImages = false;
    U32 lodGradientSamplers = 0, lodUnsupportedSamplers = 0;
    U64 identity = nextShaderIdentity.fetch_add(1, std::memory_order_relaxed);
};
struct Surface {
    id<MTLTexture> texture;
    // [Blur2PC] Resolved storage remains single-sampled and follows the same command ownership.
    std::shared_ptr<Surface> resolve;
    std::weak_ptr<C_Context> context;
    U32 samples=1;
    id<MTLCommandQueue> queue;
    id<MTLCommandBuffer> pending;
    id<MTLBuffer> staging;
    std::shared_ptr<PresentDependency> presentation;
    std::mutex mutex;
    const void* recording = nullptr;
    bool locked = false, readOnly = false;
    // [Blur2PC] Dynamic CPU-authored, non-renderable subresources may retain their
    // plain-lock contents. GPU destination copies invalidate these canonical bytes.
    bool cpuMirror = false, cpuMirrorValid = false;
    std::thread::id owner;
    U32 width=0, height=0, depth=1, pitch=0, bytes=0, block=1;
    E_SurfaceFormat format=E_SurfaceFormat_Unknown;
    ~Surface() { @autoreleasepool {
        // Ordinary Metal commands retain the actual texture/staging resources
        // until GPU completion. They do not use this CPU ownership wrapper.
        // An explicitly unretained command still needs the old lifetime barrier.
        if(pending&&!pending.retainedReferences) TraceCommandWait(pending,Trace::Phase::SurfaceRetireWait,this);
    } }
    bool waitPresentation() {
        if(!presentation)return true;
        if(!presentation->Wait(Trace::Phase::SurfaceWait,this))return false;
        presentation.reset();return true;
    }
    bool wait() { if(!waitPresentation())return false; if(!pending)return true; TraceCommandWait(pending,Trace::Phase::SurfaceWait,this); bool ok=pending.status==MTLCommandBufferStatusCompleted; pending=nil; return ok; }
};
struct Texture {
    id<MTLTexture> linear,srgb;
    E_TextureFormat format=E_TextureFormat_Unknown;
    E_TextureType type=E_TextureType_Unknown;
    U32 width=0,height=0,depth=1,levels=0,size=0;
    std::vector<std::shared_ptr<Surface>> surfaces;
};
}
// [Blur2PC] Each encoder writes its own 64-bit slot. Summing segments preserves
// query counts across render-target changes without relying on GPU accumulation.
struct C_OcclusionQuery::State {
    struct Segment {id<MTLBuffer> buffer;id<MTLCommandBuffer> command;};
    std::weak_ptr<C_Context> context;
    std::vector<Segment> segments;
    bool active=false,issued=false;
    std::atomic<bool> retired{false}; // Wrapper retirement may occur on another thread; encoder cleanup stays with its owner.
};
struct C_MetalDevice::State {
    id<MTLDevice> device;
    id<MTLCommandQueue> queue;
    id<MTLCommandBuffer> last;
    CAMetalLayer* layer;
    U64 presents=0;
    Metal::PresentRetirement presentation;
    bool traceSession=false;
    CFTimeInterval minimumPresentDuration=0;
    std::mutex mutex;
    std::vector<std::unique_ptr<C_VertexDeclaration>> declarations;
    std::shared_ptr<C_Context> context;
    C_ContextNoCheckNoTrack noCheckNoTrack;C_ContextNoCheck noCheck;
};
}
