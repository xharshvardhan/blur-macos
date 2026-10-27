// [Blur2PC][macOS] Real native mip/cube resources and staging via shared surface views.
#include "MetalNative.h"
#include <display2/context/Context.h>
#include <algorithm>
namespace Display2 {
namespace {
struct Format { MTLPixelFormat linear=MTLPixelFormatInvalid,srgb=MTLPixelFormatInvalid;U32 bytes=0,block=1;
    MTLTextureSwizzleChannels swizzle=MTLTextureSwizzleChannelsMake(MTLTextureSwizzleRed,MTLTextureSwizzleGreen,MTLTextureSwizzleBlue,MTLTextureSwizzleAlpha); };
Format formatInfo(E_TextureFormat value){
    Format f;
    switch(value){
    case E_TextureFormat_A8R8G8B8:case E_TextureFormat_X8R8G8B8:f.linear=MTLPixelFormatBGRA8Unorm;f.srgb=MTLPixelFormatBGRA8Unorm_sRGB;f.bytes=4;break;
    case E_TextureFormat_A8B8G8R8:case E_TextureFormat_X8B8G8R8:f.linear=MTLPixelFormatRGBA8Unorm;f.srgb=MTLPixelFormatRGBA8Unorm_sRGB;f.bytes=4;break;
    case E_TextureFormat_A8:f.linear=MTLPixelFormatR8Unorm;f.bytes=1;f.swizzle=MTLTextureSwizzleChannelsMake(MTLTextureSwizzleZero,MTLTextureSwizzleZero,MTLTextureSwizzleZero,MTLTextureSwizzleRed);break;
    case E_TextureFormat_L8:case E_TextureFormat_L16:f.linear=value==E_TextureFormat_L8?MTLPixelFormatR8Unorm:MTLPixelFormatR16Unorm;f.bytes=value==E_TextureFormat_L8?1:2;f.swizzle=MTLTextureSwizzleChannelsMake(MTLTextureSwizzleRed,MTLTextureSwizzleRed,MTLTextureSwizzleRed,MTLTextureSwizzleOne);break;
    case E_TextureFormat_A8L8:f.linear=MTLPixelFormatRG8Unorm;f.bytes=2;f.swizzle=MTLTextureSwizzleChannelsMake(MTLTextureSwizzleRed,MTLTextureSwizzleRed,MTLTextureSwizzleRed,MTLTextureSwizzleGreen);break;
    case E_TextureFormat_V8U8:f.linear=MTLPixelFormatRG8Snorm;f.bytes=2;break;
    case E_TextureFormat_D16:f.linear=MTLPixelFormatDepth16Unorm;f.bytes=2;break;
    case E_TextureFormat_D24S8:f.linear=MTLPixelFormatDepth32Float_Stencil8;f.bytes=4;break; // [Blur2PC][macOS] Static PC size remains nominal; runtime device selection below accounts for native storage.
    case E_TextureFormat_R16F:f.linear=MTLPixelFormatR16Float;f.bytes=2;break;
    case E_TextureFormat_G16R16F:f.linear=MTLPixelFormatRG16Float;f.bytes=4;break;
    case E_TextureFormat_A16B16G16R16F:f.linear=MTLPixelFormatRGBA16Float;f.bytes=8;break;
    case E_TextureFormat_R32F:f.linear=MTLPixelFormatR32Float;f.bytes=4;break;
    case E_TextureFormat_G32R32F:f.linear=MTLPixelFormatRG32Float;f.bytes=8;break;
    case E_TextureFormat_A32B32G32R32F:f.linear=MTLPixelFormatRGBA32Float;f.bytes=16;break;
    case E_TextureFormat_DXT1:f.linear=MTLPixelFormatBC1_RGBA;f.srgb=MTLPixelFormatBC1_RGBA_sRGB;f.bytes=8;f.block=4;break;
    case E_TextureFormat_DXT2:case E_TextureFormat_DXT3:f.linear=MTLPixelFormatBC2_RGBA;f.srgb=MTLPixelFormatBC2_RGBA_sRGB;f.bytes=16;f.block=4;break;
    case E_TextureFormat_DXT4:case E_TextureFormat_DXT5:f.linear=MTLPixelFormatBC3_RGBA;f.srgb=MTLPixelFormatBC3_RGBA_sRGB;f.bytes=16;f.block=4;break;
    default:break;
    }
    if(value==E_TextureFormat_X8R8G8B8||value==E_TextureFormat_X8B8G8R8)f.swizzle.alpha=MTLTextureSwizzleOne;
    // D3D9 supplies one for undefined channels, except the alpha-only A8
    // format. Metal's default zero-fill for missing colour channels differs.
    if(value==E_TextureFormat_R16F||value==E_TextureFormat_R32F)f.swizzle=MTLTextureSwizzleChannelsMake(MTLTextureSwizzleRed,MTLTextureSwizzleOne,MTLTextureSwizzleOne,MTLTextureSwizzleOne);
    if(value==E_TextureFormat_G16R16F||value==E_TextureFormat_G32R32F||value==E_TextureFormat_V8U8)f.swizzle=MTLTextureSwizzleChannelsMake(MTLTextureSwizzleRed,MTLTextureSwizzleGreen,MTLTextureSwizzleOne,MTLTextureSwizzleOne);
    return f;
}
}
bool C_MetalDevice::CreateTextureNative(U32 width,U32 height,U32 depth,U32 levels,U32 flags,E_TextureFormat format,E_TextureType type,C_Texture* texture,void* memory,U32 memorySize){
    @autoreleasepool {
        if(!m_State||!texture||texture->IsCreated()||memory||memorySize||!width||!height||!depth||width>16384||height>16384)return false;
        auto f=formatInfo(format);if(f.linear==MTLPixelFormatInvalid||(f.block==4&&!m_State->device.supportsBCTextureCompression))return false;
        const bool depthFormat=format==E_TextureFormat_D16||format==E_TextureFormat_D24S8;
        if(format==E_TextureFormat_D24S8){ // [Blur2PC][macOS] Opaque native depth/stencil allocation; CPU locks remain forbidden.
            if(type!=E_TextureType_2D)return false; // Cube/volume depth-stencil paths require independent integration.
            if(m_State->device.depth24Stencil8PixelFormatSupported){f.linear=MTLPixelFormatDepth24Unorm_Stencil8;f.bytes=4;}
            else{f.linear=MTLPixelFormatDepth32Float_Stencil8;f.bytes=8;}
        }
        if((flags&E_Flags_DepthStencil)&&!depthFormat)return false;
        if((flags&E_Flags_Colour)&&(depthFormat||f.block!=1))return false;
        if(type==E_TextureType_3D&&(width>2048||height>2048||depth>2048||f.block!=1||depthFormat||flags&(E_Flags_Colour|E_Flags_DepthStencil)))return false;
        levels=levels?levels:1;U32 maxLevels=1;for(U32 n=std::max({width,height,depth});n>1;n>>=1)++maxLevels;
        if(levels>maxLevels)return false;
        // Metal BC root images require complete blocks. Preserve PC dimensions
        // for uncompressed resources; partial BC base images need a later adapter.
        if(f.block==4&&(width%4||height%4))return false;
        const U32 faces=type==E_TextureType_Cube?6:1;U64 total=0;
        for(U32 level=0;level<levels;++level){
            const U64 row=U64((std::max(1U,width>>level)+f.block-1)/f.block)*f.bytes;
            const U64 rows=(std::max(1U,height>>level)+f.block-1)/f.block;
            const U64 slices=std::max(1U,depth>>level);
            if(((row+255)&~U64(255))*rows*slices>m_State->device.maxBufferLength)return false;
            total+=row*rows*slices*faces;if(total>~U32(0))return false;
        }
        auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:f.linear width:width height:height mipmapped:levels>1];
        descriptor.mipmapLevelCount=levels;descriptor.textureType=type==E_TextureType_Cube?MTLTextureTypeCube:type==E_TextureType_3D?MTLTextureType3D:MTLTextureType2D;descriptor.depth=depth;
        descriptor.storageMode=MTLStorageModePrivate;descriptor.usage=MTLTextureUsageShaderRead|MTLTextureUsagePixelFormatView;
        if(flags&(E_Flags_Colour|E_Flags_DepthStencil))descriptor.usage|=MTLTextureUsageRenderTarget;
        auto storage=[m_State->device newTextureWithDescriptor:descriptor];if(!storage)return false;
        auto native=std::make_shared<Metal::Texture>();
        // [Blur2PC][macOS] Depth textures retain their native identity view; channel swizzles only apply to colour formats.
        native->linear=depthFormat?storage:[storage newTextureViewWithPixelFormat:f.linear textureType:descriptor.textureType levels:NSMakeRange(0,levels) slices:NSMakeRange(0,faces) swizzle:f.swizzle];
        if(!native->linear)return false;
        if(f.srgb!=MTLPixelFormatInvalid)native->srgb=[storage newTextureViewWithPixelFormat:f.srgb textureType:descriptor.textureType levels:NSMakeRange(0,levels) slices:NSMakeRange(0,faces) swizzle:f.swizzle];
        if(f.srgb!=MTLPixelFormatInvalid&&!native->srgb)return false;
        native->width=width;native->height=height;native->depth=depth;native->levels=levels;native->format=format;native->type=type;
        for(U32 face=0;face<faces;++face)for(U32 level=0;level<levels;++level){
            auto s=std::make_shared<Metal::Surface>();s->width=std::max(1U,width>>level);s->height=std::max(1U,height>>level);s->depth=std::max(1U,depth>>level);s->bytes=f.bytes;s->block=f.block;
            const U64 row=U64((s->width+f.block-1)/f.block)*f.bytes;s->pitch=U32((row+255)&~U64(255));
            s->texture=[storage newTextureViewWithPixelFormat:f.linear textureType:type==E_TextureType_3D?MTLTextureType3D:MTLTextureType2D levels:NSMakeRange(level,1) slices:NSMakeRange(face,1)];
            if(!s->texture)return false;
            s->queue=m_State->queue;s->format=E_SurfaceFormat(U32(format));
            // [Blur2PC] Preserve plain-lock CPU contents for repeatedly updated
            // textures without retaining a duplicate of static retail assets.
            s->cpuMirror=(flags&E_Flags_Dynamic)&&!depthFormat&&!(descriptor.usage&MTLTextureUsageRenderTarget);
            native->surfaces.push_back(std::move(s));
        }
        native->size=U32(total);texture->m_Texture=std::move(native);texture->m_Flags=flags;return true;
    }
}
bool C_MetalDevice::CreateTexture2D(U32 w,U32 h,U32 l,U32 flags,E_TextureFormat f,C_Texture* t,void* m,U32 size){return CreateTextureNative(w,h,1,l,flags,f,E_TextureType_2D,t,m,size);}
bool C_MetalDevice::CreateTextureCube(U32 w,U32 l,U32 flags,E_TextureFormat f,C_Texture* t,void* m,U32 size){return CreateTextureNative(w,w,1,l,flags,f,E_TextureType_Cube,t,m,size);}
bool C_MetalDevice::CreateTexture3D(U32 w,U32 h,U32 d,U32 l,U32 flags,E_TextureFormat f,C_Texture* t,void* m,U32 size){return CreateTextureNative(w,h,d,l,flags,f,E_TextureType_3D,t,m,size);}
void C_MetalDevice::DestroyTexture(C_Texture* t){if(t){t->m_Texture.reset();t->m_Flags=0;}}
bool C_MetalDevice::CopyTexture(C_Texture* destination,const C_Texture* source,bool commonMipTail){@autoreleasepool{
    if(!m_State||!destination||!source||!destination->m_Texture||!source->m_Texture||destination->m_Texture==source->m_Texture)return false;
    const auto& a=*source->m_Texture;auto& b=*destination->m_Texture;
    if(a.type!=b.type||a.format!=b.format)return false;
    if(!commonMipTail&&(a.width!=b.width||a.height!=b.height||a.depth!=b.depth||a.levels!=b.levels||a.surfaces.size()!=b.surfaces.size()))return false;
    // [Blur2PC] Preserve the PC streaming copy's common mip tail, face by face.
    const U32 sourceFirst=commonMipTail&&a.levels>b.levels?a.levels-b.levels:0;
    const U32 destinationFirst=commonMipTail&&b.levels>a.levels?b.levels-a.levels:0;
    const U32 levels=std::min(a.levels,b.levels),faces=a.type==E_TextureType_Cube?6:1;
    std::vector<std::pair<std::shared_ptr<Metal::Surface>,std::shared_ptr<Metal::Surface>>> copies;
    for(U32 face=0;face<faces;++face)for(U32 level=0;level<levels;++level){
        auto from=a.surfaces[face*a.levels+sourceFirst+level],to=b.surfaces[face*b.levels+destinationFirst+level];
        if(from->width!=to->width||from->height!=to->height||from->depth!=to->depth)return false;
        copies.emplace_back(from,to);
    }
    auto surfaces=a.surfaces;surfaces.insert(surfaces.end(),b.surfaces.begin(),b.surfaces.end());
    std::sort(surfaces.begin(),surfaces.end(),[](const auto& x,const auto& y){return std::less<Metal::Surface*>()(x.get(),y.get());});surfaces.erase(std::unique(surfaces.begin(),surfaces.end()),surfaces.end());
    std::lock_guard<std::mutex> deviceLock(m_State->mutex);std::vector<std::unique_lock<std::mutex>> locks;
    for(auto& s:surfaces)locks.emplace_back(s->mutex);
    for(auto& s:surfaces)if(s->locked||s->recording||s->texture.device!=m_State->device||!s->wait())return false;
    auto command=[m_State->queue commandBuffer];auto blit=[command blitCommandEncoder];if(!command||!blit)return false;
    for(const auto& copy:copies){const auto& from=copy.first;const auto& to=copy.second;
        [blit copyFromTexture:from->texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
                   sourceSize:MTLSizeMake(from->width,from->height,from->depth) toTexture:to->texture destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
        to->cpuMirrorValid=false; // [Blur2PC] GPU copy changes the destination, including common mip tails.
    }
    [blit endEncoding];[command commit];for(auto& s:surfaces)s->pending=command;m_State->last=command;return true;
}}
bool C_MetalDevice::GetTextureSurface(const C_Texture* t,U32 face,U32 level,C_Surface* s){
    if(!t||!t->m_Texture||t->GetType()==E_TextureType_3D||!s||s->IsCreated()||level>=t->GetLevels()||face>=(t->GetType()==E_TextureType_Cube?6U:1U))return false;
    s->m_Surface=t->m_Texture->surfaces[face*t->GetLevels()+level];s->m_Flags=t->m_Flags;return true;
}
E_TextureType C_Texture::GetType()const{return m_Texture?m_Texture->type:E_TextureType_Unknown;}
E_TextureFormat C_Texture::GetFormat()const{return m_Texture?m_Texture->format:E_TextureFormat_Unknown;}
U32 C_Texture::GetSize()const{return m_Texture?m_Texture->size:0;}
U32 C_Texture::GetLevels()const{return m_Texture?m_Texture->levels:0;}
U32 C_Texture::GetWidth(U32 l)const{return l<GetLevels()?std::max(1U,m_Texture->width>>l):0;}
U32 C_Texture::GetHeight(U32 l)const{return l<GetLevels()?std::max(1U,m_Texture->height>>l):0;}
U32 C_Texture::GetDepth(U32 l)const{return l<GetLevels()?std::max(1U,m_Texture->depth>>l):0;}
void C_Texture::SetSRGB(bool enable){if(enable)m_Flags|=E_Flags_SRGB;else m_Flags&=~E_Flags_SRGB;}
C_LockedRect C_Texture::LockRectCube(U32 face,U32 level,U32 flags){
    if(!m_Texture||GetType()!=E_TextureType_Cube||face>=6||level>=GetLevels())return {};
    C_Surface s;s.m_Surface=m_Texture->surfaces[face*GetLevels()+level];return s.LockRect(flags);
}
C_LockedRect C_Texture::LockRect2D(U32 level,U32 flags){
    if(!m_Texture||GetType()!=E_TextureType_2D||level>=GetLevels())return {};
    C_Surface s;s.m_Surface=m_Texture->surfaces[level];return s.LockRect(flags);
}
C_ConstLockedRect C_Texture::LockRect2D(U32 l,U32 flags)const{auto r=const_cast<C_Texture*>(this)->LockRect2D(l,flags);return {r.pBits,r.pitch};}
C_ConstLockedRect C_Texture::LockRectCube(U32 face,U32 l,U32 flags)const{auto r=const_cast<C_Texture*>(this)->LockRectCube(face,l,flags);return {r.pBits,r.pitch};}
void C_Texture::UnlockRect2D(U32 level)const{if(GetType()!=E_TextureType_2D||level>=GetLevels())throw std::logic_error("Invalid texture mip unlock");C_Surface s;s.m_Surface=m_Texture->surfaces[level];s.UnlockRect();}
void C_Texture::UnlockRectCube(U32 face,U32 level)const{if(GetType()!=E_TextureType_Cube||face>=6||level>=GetLevels())throw std::logic_error("Invalid cube face/mip unlock");C_Surface s;s.m_Surface=m_Texture->surfaces[face*GetLevels()+level];s.UnlockRect();}
C_LockedBox C_Texture::LockBox3D(U32 level,U32 flags){if(GetType()!=E_TextureType_3D||level>=GetLevels())return {};C_Surface s;s.m_Surface=m_Texture->surfaces[level];auto r=s.LockRect(flags);return {r.pBits,r.pitch,r.pitch*GetHeight(level)};}
C_ConstLockedBox C_Texture::LockBox3D(U32 level,U32 flags)const{auto r=const_cast<C_Texture*>(this)->LockBox3D(level,flags);return {r.pBits,r.rowPitch,r.slicePitch};}
void C_Texture::UnlockBox3D(U32 level)const{if(GetType()!=E_TextureType_3D||level>=GetLevels())throw std::logic_error("Invalid volume mip unlock");C_Surface s;s.m_Surface=m_Texture->surfaces[level];s.UnlockRect();}
bool C_SamplerState::IsSame(const C_SamplerState& o)const{return m_TextureAddressU==o.m_TextureAddressU&&m_TextureAddressV==o.m_TextureAddressV&&m_TextureAddressW==o.m_TextureAddressW&&m_TextureFilterMag==o.m_TextureFilterMag&&m_TextureFilterMin==o.m_TextureFilterMin&&m_TextureFilterMip==o.m_TextureFilterMip&&m_TextureGamma==o.m_TextureGamma&&m_TextureBorderColour==o.m_TextureBorderColour&&m_TextureLodBias==o.m_TextureLodBias;}
namespace {
U32 size32(U64 value){if(value>~U32(0))throw std::overflow_error("Engine texture size exceeds its PC field");return U32(value);}
Format checkedFormat(E_TextureFormat f){auto result=formatInfo(f);if(!result.bytes)throw std::invalid_argument("Unsupported native texture format size");return result;}
}
// PC serialized rows are tightly packed. Metal staging alignment remains a
// property of LockRect/LockBox and never changes disk-layout arithmetic.
U32 GetTexturePitchFromWidth(U32 width,U32,E_TextureFormat format){auto f=checkedFormat(format);return size32(((U64(width)+f.block-1)/f.block)*f.bytes);}
U32 GetTextureSizeFromHeightAndPitch(U32 height,U32 pitch,U32,E_TextureFormat format){auto f=checkedFormat(format);return size32(((U64(height)+f.block-1)/f.block)*pitch);}
U32 GetTexturePitch(U32 w,U32 flags,E_TextureFormat f){return GetTexturePitchFromWidth(w,flags,f);}
U32 GetTextureRowPitch(U32 w,U32 flags,E_TextureFormat f){return GetTexturePitchFromWidth(w,flags,f);}
U32 GetTextureSlicePitch(U32 w,U32 h,U32 levels,U32 flags,E_TextureFormat f){return size32(U64(GetTextureSizeFromHeightAndPitch(h,GetTexturePitchFromWidth(w,flags,f),flags,f))*levels);}
U32 GetTexture2DSize(U32 w,U32 h,U32 flags,E_TextureFormat f){return GetTextureSizeFromHeightAndPitch(h,GetTexturePitchFromWidth(w,flags,f),flags,f);}
U32 GetTexture2DSize(U32 w,U32 h,U32 levels,U32 flags,E_TextureFormat f){
    if(levels>32)throw std::invalid_argument("Texture mip count exceeds dimension bits");
    U64 size=0;for(U32 l=0;l<levels;++l){size+=GetTexture2DSize(w,h,flags,f);w=std::max(1U,w>>1);h=std::max(1U,h>>1);}return size32(size);
}
void GetTexture2DSizeAndPitch(U32 w,U32 h,U32 levels,U32 flags,E_TextureFormat f,U32& size,U32& pitch){pitch=GetTexturePitchFromWidth(w,flags,f);size=GetTexture2DSize(w,h,levels,flags,f);}
U32 GetTexture3DSize(U32 w,U32 h,U32 depth,U32 flags,E_TextureFormat f){return size32(U64(GetTexture2DSize(w,h,flags,f))*depth);}
U32 GetTexture3DSize(U32 w,U32 h,U32 depth,U32 levels,U32 flags,E_TextureFormat f){
    if(levels>32)throw std::invalid_argument("Volume mip count exceeds dimension bits");
    U64 size=0;for(U32 l=0;l<levels;++l){size+=GetTexture3DSize(w,h,depth,flags,f);w=std::max(1U,w>>1);h=std::max(1U,h>>1);depth=std::max(1U,depth>>1);}return size32(size);
}
void GetTexture3DSizeAndPitch(U32 w,U32 h,U32 d,U32 levels,U32 flags,E_TextureFormat f,U32& size,U32& pitch){pitch=GetTexturePitchFromWidth(w,flags,f);size=GetTexture3DSize(w,h,d,levels,flags,f);}
U32 GetTextureCubeSize(U32 w,U32 flags,E_TextureFormat f){return size32(U64(GetTexture2DSize(w,w,flags,f))*6);}
U32 GetTextureCubeSize(U32 w,U32 levels,U32 flags,E_TextureFormat f){return size32(U64(GetTexture2DSize(w,w,levels,flags,f))*6);}
void GetTextureCubeSizeAndPitch(U32 w,U32 levels,U32 flags,E_TextureFormat f,U32& size,U32& pitch){pitch=GetTexturePitchFromWidth(w,flags,f);size=GetTextureCubeSize(w,levels,flags,f);}
U32 GetSurfacePitchFromWidth(U32 w,U32 flags,E_SurfaceFormat f,E_Multisample samples){
    if(samples!=E_Multisample_None&&samples!=E_Multisample_2&&samples!=E_Multisample_4)throw std::invalid_argument("Invalid surface multisample count");
    const U64 pitch=f==E_SurfaceFormat_D24S8?U64(w)*4:GetTexturePitchFromWidth(w,flags,E_TextureFormat(U32(f)));
    return size32(pitch*(samples==E_Multisample_None?1:2)); // Original PC layout: 2x wide, and 4x also 2x high.
}
U32 GetSurfaceSizeFromHeightAndPitch(U32 h,U32 pitch,U32 flags,E_SurfaceFormat f,E_Multisample samples){
    if(samples!=E_Multisample_None&&samples!=E_Multisample_2&&samples!=E_Multisample_4)throw std::invalid_argument("Invalid surface multisample count");
    const U64 height=U64(h)*(samples==E_Multisample_4?2:1);
    if(f==E_SurfaceFormat_D24S8)return size32(height*pitch);
    return GetTextureSizeFromHeightAndPitch(size32(height),pitch,flags,E_TextureFormat(U32(f)));
}
U32 GetSurfaceSize(U32 w,U32 h,U32 flags,E_SurfaceFormat f,E_Multisample samples){return GetSurfaceSizeFromHeightAndPitch(h,GetSurfacePitchFromWidth(w,flags,f,samples),flags,f,samples);}
void GetSurfaceSizeAndPitch(U32 w,U32 h,U32 flags,E_SurfaceFormat f,E_Multisample samples,U32& size,U32& pitch){pitch=GetSurfacePitchFromWidth(w,flags,f,samples);size=GetSurfaceSizeFromHeightAndPitch(h,pitch,flags,f,samples);}
const Util::C_String GetFlagsAsString(U32 flags){
    struct Flag {U32 bit;const char* name;};const Flag names[]={{E_Flags_Video,"Video"},{E_Flags_System,"System"},{E_Flags_Indirect,"Indirect"},{E_Flags_Dynamic,"Dynamic"},
        {E_Flags_Colour,"Colour"},{E_Flags_DepthStencil,"DepthStencil"},{E_Flags_Raw,"Raw"},{E_Flags_Tiled,"Tiled"},{E_Flags_Linear,"Linear"},{E_Flags_SRGB,"SRGB"},{E_Flags_DoNotTrack,"DoNotTrack"},{E_Flags_DoNotReload,"DoNotReload"}};
    Util::C_String result="";for(const auto& f:names)if(flags&f.bit){if(result!="")result+="|";result+=f.name;}return result;
}
const Util::C_String GetIndexFormatAsString(E_IndexFormat f){return f==E_IndexFormat_16?"16":f==E_IndexFormat_32?"32":"Unknown";}
const Util::C_String GetMultisampleAsString(E_Multisample v){return v==E_Multisample_None?"1xAA":v==E_Multisample_2?"2xAA":v==E_Multisample_4?"4xAA":"Unknown";}
const Util::C_String GetTextureTypeAsString(E_TextureType v){return v==E_TextureType_2D?"2D":v==E_TextureType_3D?"3D":v==E_TextureType_Cube?"Cube":"Unknown";}
const Util::C_String GetTextureFormatAsString(E_TextureFormat f){
    switch(f){
#define FORMAT_NAME(Name) case E_TextureFormat_##Name:return #Name;
    FORMAT_NAME(A8R8G8B8) FORMAT_NAME(X8R8G8B8) FORMAT_NAME(A8B8G8R8) FORMAT_NAME(X8B8G8R8)
    FORMAT_NAME(A8) FORMAT_NAME(L8) FORMAT_NAME(A8L8) FORMAT_NAME(V8U8) FORMAT_NAME(D16) FORMAT_NAME(D24S8) FORMAT_NAME(L16)
    FORMAT_NAME(R16F) FORMAT_NAME(G16R16F) FORMAT_NAME(A16B16G16R16F) FORMAT_NAME(R32F) FORMAT_NAME(G32R32F) FORMAT_NAME(A32B32G32R32F)
    FORMAT_NAME(DXT1) FORMAT_NAME(DXT2) FORMAT_NAME(DXT3) FORMAT_NAME(DXT4) FORMAT_NAME(DXT5)
#undef FORMAT_NAME
    default:return "Unknown";}
}
const Util::C_String GetSurfaceFormatAsString(E_SurfaceFormat f){return GetTextureFormatAsString(E_TextureFormat(U32(f)));}
}
