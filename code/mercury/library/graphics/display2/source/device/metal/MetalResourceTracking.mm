// [Blur2PC][macOS] Keep original engine resource metadata around actual native allocations.
#include <display2/device/MetalEngineDevice.h>
namespace Display2 {
#if TRACK_DISPLAY2_RESOURCES
#define TRACK_RESOURCE(Method,Resource,Flags) do{if(!((Flags)&E_Flags_DoNotTrack))Method(Resource);}while(false)
#define UNTRACK_RESOURCE(Method,Resource) Method(Resource)
#else
#define TRACK_RESOURCE(Method,Resource,Flags) ((void)0)
#define UNTRACK_RESOURCE(Method,Resource) ((void)0)
#endif
bool C_Device::CreateVertexBuffer(U32 size,U32 flags,C_VertexBuffer* b,void* memory){if(!C_MetalDevice::CreateVertexBuffer(size,flags,b,memory))return false;TRACK_RESOURCE(InsertVertexBuffer,b,flags);return true;}
void C_Device::DestroyVertexBuffer(C_VertexBuffer* b){if(b){UNTRACK_RESOURCE(RemoveVertexBuffer,b);C_MetalDevice::DestroyVertexBuffer(b);}}
bool C_Device::CreateIndexBuffer(U32 size,U32 flags,E_IndexFormat f,C_IndexBuffer* b,void* memory){if(!C_MetalDevice::CreateIndexBuffer(size,flags,f,b,memory))return false;TRACK_RESOURCE(InsertIndexBuffer,b,flags);return true;}
void C_Device::DestroyIndexBuffer(C_IndexBuffer* b){if(b){UNTRACK_RESOURCE(RemoveIndexBuffer,b);C_MetalDevice::DestroyIndexBuffer(b);}}
bool C_Device::CreateVertexShader(U32 size,const U8* data,U32 flags,C_VertexShader* s,void* memory,std::string* error){if(!C_MetalDevice::CreateVertexShader(size,data,flags,s,memory,error))return false;TRACK_RESOURCE(InsertVertexShader,s,flags);return true;}
void C_Device::DestroyVertexShader(C_VertexShader* s){if(s){UNTRACK_RESOURCE(RemoveVertexShader,s);C_MetalDevice::DestroyVertexShader(s);}}
bool C_Device::CreatePixelShader(U32 size,const U8* data,U32 flags,C_PixelShader* s,void* memory,std::string* error){if(!C_MetalDevice::CreatePixelShader(size,data,flags,s,memory,error))return false;TRACK_RESOURCE(InsertPixelShader,s,flags);return true;}
void C_Device::DestroyPixelShader(C_PixelShader* s){if(s){UNTRACK_RESOURCE(RemovePixelShader,s);C_MetalDevice::DestroyPixelShader(s);}}
bool C_Device::CreateSurface(U32 w,U32 h,U32 flags,E_SurfaceFormat f,E_Multisample samples,const C_SurfaceParams* p,C_Surface* s,void* memory,U32 size){if(!C_MetalDevice::CreateSurface(w,h,flags,f,samples,p,s,memory,size))return false;TRACK_RESOURCE(InsertSurface,s,flags);return true;}
void C_Device::DestroySurface(C_Surface* s){if(s){UNTRACK_RESOURCE(RemoveSurface,s);C_MetalDevice::DestroySurface(s);}}
bool C_Device::CreateTexture2D(U32 w,U32 h,U32 levels,U32 flags,E_TextureFormat f,C_Texture* t,void* memory,U32 size){if(!C_MetalDevice::CreateTexture2D(w,h,levels,flags,f,t,memory,size))return false;TRACK_RESOURCE(InsertTexture,t,flags);return true;}
bool C_Device::CreateTexture3D(U32 w,U32 h,U32 d,U32 levels,U32 flags,E_TextureFormat f,C_Texture* t,void* memory,U32 size){if(!C_MetalDevice::CreateTexture3D(w,h,d,levels,flags,f,t,memory,size))return false;TRACK_RESOURCE(InsertTexture,t,flags);return true;}
bool C_Device::CreateTextureCube(U32 w,U32 levels,U32 flags,E_TextureFormat f,C_Texture* t,void* memory,U32 size){if(!C_MetalDevice::CreateTextureCube(w,levels,flags,f,t,memory,size))return false;TRACK_RESOURCE(InsertTexture,t,flags);return true;}
void C_Device::DestroyTexture(C_Texture* t){if(t){UNTRACK_RESOURCE(RemoveTexture,t);C_MetalDevice::DestroyTexture(t);}}
bool C_Device::GetTextureSurface(const C_Texture* t,U32 face,U32 level,C_Surface* s){if(!C_MetalDevice::GetTextureSurface(t,face,level,s))return false;TRACK_RESOURCE(InsertSurface,s,s->GetFlags());return true;}
bool C_Device::CreateRenderTexture2D(U32 w,U32 h,U32 levels,U32 flags,E_TextureFormat tf,E_SurfaceFormat sf,E_Multisample samples,const C_SurfaceParams* p,C_RenderTexture2D* t,void* memory,U32 size){
    if(!C_MetalDevice::CreateRenderTexture2D(w,h,levels,flags,tf,sf,samples,p,t,memory,size))return false;
    TRACK_RESOURCE(InsertRenderTexture2D,t,t->GetFlags());for(U32 i=0;i<2;++i)if(t->m_Texture[i].IsCreated()){
        TRACK_RESOURCE(InsertTexture,&t->m_Texture[i],t->m_Texture[i].GetFlags());for(S32 l=0;l<t->m_Surfaces[i].Size();++l)TRACK_RESOURCE(InsertSurface,&t->m_Surfaces[i][l],t->m_Surfaces[i][l].GetFlags());}return true;
}
void C_Device::DestroyRenderTexture2D(C_RenderTexture2D* t){if(!t)return;UNTRACK_RESOURCE(RemoveRenderTexture2D,t);for(U32 i=0;i<2;++i){
    UNTRACK_RESOURCE(RemoveTexture,&t->m_Texture[i]);for(S32 l=0;l<t->m_Surfaces[i].Size();++l)UNTRACK_RESOURCE(RemoveSurface,&t->m_Surfaces[i][l]);}C_MetalDevice::DestroyRenderTexture2D(t);}
bool C_Device::CreateRenderTextureCube(U32 w,U32 levels,U32 flags,E_TextureFormat tf,E_SurfaceFormat sf,E_Multisample samples,const C_SurfaceParams* p,C_RenderTextureCube* t,void* memory,U32 size){
    if(!C_MetalDevice::CreateRenderTextureCube(w,levels,flags,tf,sf,samples,p,t,memory,size))return false;
    TRACK_RESOURCE(InsertRenderTextureCube,t,t->GetFlags());TRACK_RESOURCE(InsertTexture,&t->m_Texture,t->m_Texture.GetFlags());
    for(U32 f=0;f<6;++f)for(S32 l=0;l<t->m_Surfaces[f].Size();++l)TRACK_RESOURCE(InsertSurface,&t->m_Surfaces[f][l],t->m_Surfaces[f][l].GetFlags());return true;
}
void C_Device::DestroyRenderTextureCube(C_RenderTextureCube* t){if(!t)return;UNTRACK_RESOURCE(RemoveRenderTextureCube,t);UNTRACK_RESOURCE(RemoveTexture,&t->m_Texture);
    for(U32 f=0;f<6;++f)for(S32 l=0;l<t->m_Surfaces[f].Size();++l)UNTRACK_RESOURCE(RemoveSurface,&t->m_Surfaces[f][l]);C_MetalDevice::DestroyRenderTextureCube(t);}
#undef TRACK_RESOURCE
#undef UNTRACK_RESOURCE
}
