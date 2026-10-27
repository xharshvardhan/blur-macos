// [Blur2PC][macOS] Private render storage with synchronized CPU staging locks.
#include "MetalNative.h"
#include <stdexcept>
namespace Display2 {
static MTLPixelFormat surfaceFormat(id<MTLDevice> device,E_SurfaceFormat format,U32,U32& bytes) {
    switch(format) {
    // Original PC render-target views are linear even when SRGB metadata is
    // set; SRGBTEXTURE chooses a sampling view, not a render-write conversion.
    case E_SurfaceFormat_A8R8G8B8:bytes=4;return MTLPixelFormatBGRA8Unorm;
    case E_SurfaceFormat_A8B8G8R8:bytes=4;return MTLPixelFormatRGBA8Unorm;
    case E_SurfaceFormat_A16B16G16R16F:bytes=8;return MTLPixelFormatRGBA16Float;
    case E_SurfaceFormat_A32B32G32R32F:bytes=16;return MTLPixelFormatRGBA32Float;
    case E_SurfaceFormat_R32F:bytes=4;return MTLPixelFormatR32Float;
    case E_SurfaceFormat_D16:bytes=2;return MTLPixelFormatDepth16Unorm;
    // D24S8 is opaque/non-lockable on PC. Apple GPUs use supported 32-bit float depth plus 8-bit stencil.
    case E_SurfaceFormat_D24S8:if(device.depth24Stencil8PixelFormatSupported){bytes=4;return MTLPixelFormatDepth24Unorm_Stencil8;}bytes=8;return MTLPixelFormatDepth32Float_Stencil8;
    default:return MTLPixelFormatInvalid;
    }
}
bool C_MetalDevice::CreateSurface(U32 width,U32 height,U32 flags,E_SurfaceFormat format,E_Multisample samples,
                                  const C_SurfaceParams*,C_Surface* surface,void* memory,U32 memorySize) {
    @autoreleasepool {
        if(!m_State||!surface||surface->IsCreated()||!width||!height||width>16384||height>16384||(samples!=E_Multisample_None&&samples!=E_Multisample_2&&samples!=E_Multisample_4)||memory||memorySize)return false;
        const U32 count=samples==E_Multisample_None?1:U32(samples);
        if(![m_State->device supportsTextureSampleCount:count])return false;
        // Combined store+resolve preserves samples across the engine's encoder/query boundaries.
        if(count>1&&![m_State->device supportsFamily:MTLGPUFamilyApple3]&&![m_State->device supportsFamily:MTLGPUFamilyMac2])return false;
        // Float32 resolve support is a separate device capability from sample-count support.
        if(count>1&&(format==E_SurfaceFormat_R32F||format==E_SurfaceFormat_A32B32G32R32F)&&!m_State->device.supports32BitMSAA)return false;
        U32 bytes=0;auto pixel=surfaceFormat(m_State->device,format,flags,bytes);if(pixel==MTLPixelFormatInvalid)return false;
        const bool depth=format==E_SurfaceFormat_D16||format==E_SurfaceFormat_D24S8;
        if((depth&&(flags&E_Flags_Colour))||(!depth&&(flags&E_Flags_DepthStencil)))return false;
        U64 pitch=(U64(width)*bytes+255)&~U64(255);
        if(pitch*height>m_State->device.maxBufferLength||U64(width)*height*bytes*count>~U32(0))return false;
        auto native=std::make_shared<Metal::Surface>();
        auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pixel width:width height:height mipmapped:NO];
        descriptor.storageMode=MTLStorageModePrivate;
        descriptor.sampleCount=count;
        if(count>1)descriptor.textureType=MTLTextureType2DMultisample;
        descriptor.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
        native->texture=[m_State->device newTextureWithDescriptor:descriptor];
        if(!native->texture)return false;
        native->queue=m_State->queue;native->width=width;native->height=height;
        native->pitch=U32(pitch);native->bytes=bytes;native->format=format;native->samples=count;
        if(count>1&&format!=E_SurfaceFormat_D24S8){
            C_Surface resolved;
            if(!CreateSurface(width,height,flags,format,E_Multisample_None,nullptr,&resolved))return false;
            native->resolve=std::move(resolved.m_Surface);
        }
        surface->m_Surface=std::move(native);surface->m_Flags=flags;return true;
    }
}
void C_MetalDevice::DestroySurface(C_Surface* surface) { if(surface){surface->m_Surface.reset();surface->m_Flags=0;} }
U32 C_Surface::GetWidth()const{return m_Surface?m_Surface->width:0;}
U32 C_Surface::GetHeight()const{return m_Surface?m_Surface->height:0;}
U32 C_Surface::GetSize()const{return m_Surface?((m_Surface->width+m_Surface->block-1)/m_Surface->block)*((m_Surface->height+m_Surface->block-1)/m_Surface->block)*m_Surface->bytes*m_Surface->depth*m_Surface->samples:0;}
E_Multisample C_Surface::GetMultisample()const{return m_Surface&&m_Surface->samples>1?E_Multisample(m_Surface->samples):E_Multisample_None;}
E_SurfaceFormat C_Surface::GetFormat()const{return m_Surface?m_Surface->format:E_SurfaceFormat_Unknown;}
C_LockedRect C_Surface::LockRect(U32 flags) {
    @autoreleasepool {
        if(!m_Surface||m_Surface->format==E_SurfaceFormat_D24S8||flags&~16U)return {}; // No invented packed CPU layout for opaque depth/stencil.
        // Multisample CPU reads expose the real native resolve; writes cannot initialize individual samples.
        if(m_Surface->samples>1&&(!(flags&16U)||!m_Surface->resolve))return {};
        auto& s=*(m_Surface->resolve?m_Surface->resolve:m_Surface);std::lock_guard<std::mutex> guard(s.mutex);
        if(s.locked||s.recording||!s.wait())return {};
        const U64 imageBytes=U64(s.pitch)*((s.height+s.block-1)/s.block);
        s.staging=[s.texture.device newBufferWithLength:imageBytes*s.depth options:MTLResourceStorageModeShared];
        if(!s.staging)return {};
        auto command=[s.queue commandBuffer];auto blit=[command blitCommandEncoder];
        if(!command||!blit){s.staging=nil;return {};}
        [blit copyFromTexture:s.texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
                   sourceSize:MTLSizeMake(s.width,s.height,s.depth) toBuffer:s.staging destinationOffset:0
          destinationBytesPerRow:s.pitch destinationBytesPerImage:imageBytes];
        [blit endEncoding];[command commit];[command waitUntilCompleted];
        if(command.status!=MTLCommandBufferStatusCompleted){s.staging=nil;return {};}
        s.locked=true;s.readOnly=flags&16U;s.owner=std::this_thread::get_id();
        return {static_cast<U8*>(s.staging.contents),s.pitch};
    }
}
void C_Surface::UnlockRect() {
    @autoreleasepool {
        if(!m_Surface)throw std::logic_error("Surface is not created");
        auto& s=*(m_Surface->resolve?m_Surface->resolve:m_Surface);std::lock_guard<std::mutex> guard(s.mutex);
        if(!s.locked||s.owner!=std::this_thread::get_id())throw std::logic_error("Surface unlock requires its CPU owner");
        if(!s.readOnly) {
            auto command=[s.queue commandBuffer];auto blit=[command blitCommandEncoder];
            if(!command||!blit)throw std::runtime_error("Surface staging upload failed");
            [blit copyFromBuffer:s.staging sourceOffset:0 sourceBytesPerRow:s.pitch sourceBytesPerImage:U64(s.pitch)*((s.height+s.block-1)/s.block)
                       sourceSize:MTLSizeMake(s.width,s.height,s.depth) toTexture:s.texture destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
            [blit endEncoding];[command commit];s.pending=command;
        }
        s.locked=false;s.owner={};s.staging=nil;
    }
}
}
