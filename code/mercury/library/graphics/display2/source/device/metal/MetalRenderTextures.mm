// [Blur2PC][macOS] PC render-texture read/write slots, backed by actual mip/face aliases.
#include "MetalNative.h"
#include <cstdio>
namespace Display2 {
bool C_MetalDevice::CreateRenderTexture2D(U32 w,U32 h,U32 levels,U32 flags,E_TextureFormat textureFormat,E_SurfaceFormat surfaceFormat,E_Multisample samples,const C_SurfaceParams*,C_RenderTexture2D* target,void* memory,U32 size){
    auto fail=[&](const char* reason){std::fprintf(stderr,"Native render texture allocation failed: %s; %ux%u levels=%u flags=0x%x textureFormat=%u surfaceFormat=%u samples=%u externalMemory=%s budgetBytes=%u\n",reason,w,h,levels,flags,U32(textureFormat),U32(surfaceFormat),U32(samples),memory?"yes":"no",size);return false;};
    // [Blur2PC] Original PC postprocess passes its calculated allocation size
    // even without a console overlap address. The PC allocator owns real GPU
    // storage in that case; no external memory is being consumed or aliased.
    if(!m_State||!target||target->IsCreated()||!levels||(samples!=E_Multisample_None&&samples!=E_Multisample_2&&samples!=E_Multisample_4)||memory||U32(textureFormat)!=U32(surfaceFormat))return fail("invalid state, format/sample request or unsupported external memory alias");
    if(!(flags&(E_Flags_Colour|E_Flags_DepthStencil)))flags|=E_Flags_Colour;
    C_RenderTexture2D next;const U32 buffers=flags&E_Flags_ReadWrite?2:1;
    for(U32 index=0;index<buffers;++index){
        if(!CreateTexture2D(w,h,levels,flags,textureFormat,&next.m_Texture[index]))return fail("native texture format/dimensions/allocation rejected");
        next.m_Surfaces[index].Resize(levels);
        next.m_ResolveSurfaces[index].Resize(levels);
        for(U32 level=0;level<levels;++level){
            auto& surface=next.m_Surfaces[index][level];
            if(samples==E_Multisample_None){if(!GetTextureSurface(&next.m_Texture[index],0,level,&surface))return fail("native texture mip surface alias rejected");}
            else {
                auto& resolved=next.m_ResolveSurfaces[index][level];
                if(!GetTextureSurface(&next.m_Texture[index],0,level,&resolved)||
                   !CreateSurface(resolved.GetWidth(),resolved.GetHeight(),flags,surfaceFormat,samples,nullptr,&surface))return fail("native multisample render/resolve allocation rejected");
                surface.m_Surface->resolve=resolved.m_Surface;
            }
        }
    }
    next.m_Flags=flags;next.m_Multisample=samples;next.m_TextureIndex=buffers==2?1:0;next.m_SurfaceIndex=0;*target=next;return true;
}
void C_MetalDevice::DestroyRenderTexture2D(C_RenderTexture2D* target){if(target)*target=C_RenderTexture2D{};}
void C_RenderTexture2D::Resolve(U32 level)const{
    if(!IsCreated()||level>=m_Texture[0].GetLevels())throw std::logic_error("Render-texture resolve needs a created mip");
    if(m_Multisample!=E_Multisample_None){
        const auto& surface=GetSurface(level);
        std::shared_ptr<C_Context> context;
        {std::lock_guard<std::mutex> guard(surface.m_Surface->mutex);context=surface.m_Surface->context.lock();}
        if(context)context->ResolveSurface(&surface);
    }
    // As the existing PC Resolve: the completed write slot becomes the read
    // texture, and subsequent rendering uses the other slot. No CPU copy.
    if(m_Flags&E_Flags_ReadWrite){m_SurfaceIndex=(m_SurfaceIndex+1)&1;m_TextureIndex=(m_TextureIndex+1)&1;}
}
bool C_MetalDevice::CreateRenderTextureCube(U32 w,U32 levels,U32 flags,E_TextureFormat textureFormat,E_SurfaceFormat surfaceFormat,E_Multisample samples,const C_SurfaceParams*,C_RenderTextureCube* target,void* memory,U32 size){
    if(!m_State||!target||target->IsCreated()||!levels||samples!=E_Multisample_None||flags&E_Flags_ReadWrite||memory||size||U32(textureFormat)!=U32(surfaceFormat))return false;
    if(!(flags&(E_Flags_Colour|E_Flags_DepthStencil)))flags|=E_Flags_Colour;
    C_RenderTextureCube next;
    if(!CreateTextureCube(w,levels,flags,textureFormat,&next.m_Texture))return false;
    for(U32 face=0;face<6;++face){next.m_Surfaces[face].Resize(levels);
        for(U32 level=0;level<levels;++level)if(!GetTextureSurface(&next.m_Texture,face,level,&next.m_Surfaces[face][level]))return false;}
    next.m_Flags=flags;*target=next;return true;
}
void C_MetalDevice::DestroyRenderTextureCube(C_RenderTextureCube* target){if(target)*target=C_RenderTextureCube{};}
}
