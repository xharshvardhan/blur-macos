// [Blur2PC] Actual native sampler state, both shader paths, real mip GPU pixels.
#include <display2/device/MetalDevice.h>
#include <display2/context/Context.h>
#include <display2/device/BaseDevice.h>
#include <display2/context/MetalLegacyShader.h>
#include <display2/context/MetalSamplerLod.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace Display2;
static void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
static std::vector<U8> blob(const std::string& source,bool vertex,const std::string& textures="",const std::string& uniforms=""){
    const std::string metadata="{\"parameters\":{},\"input_semantics\":"+std::string(vertex?"[[0,0,0]]":"[]")+",\"reflection\":{\"textures\":["+textures+"],\"ubos\":["+uniforms+"]}}";
    const std::string entry="blur_main";const U32 start=40+metadata.size(),end=start+source.size();
    std::vector<U8> out={'B','M','S','L'};for(U32 word:{1u,vertex?0u:1u,U32(end+entry.size()),40u,U32(metadata.size()),start,U32(source.size()),end,U32(entry.size())})for(U32 i=0;i<4;++i)out.push_back(U8(word>>(8*i)));
    out.insert(out.end(),metadata.begin(),metadata.end());out.insert(out.end(),source.begin(),source.end());out.insert(out.end(),entry.begin(),entry.end());return out;
}
static const std::string prefix="#include <metal_stdlib>\nusing namespace metal;\nstruct Out { float4 p [[position]]; float2 uv [[user(locn0)]]; float4 colour [[user(locn1)]]; };\n";
static const std::string vertexSource=prefix+"struct In { float4 p [[attribute(0)]]; }; vertex Out blur_main(In v [[stage_in]]) { Out o; o.p=v.p; o.uv=v.p.xy*0.5+0.5; o.colour=1; return o; }";
static const std::string texture2="{\"binding\":2,\"type\":\"sampler2D\"}";
int main(){try{
    C_MetalDevice device;require(device.Create(),"Metal device failed");auto& context=device.GetContext();
    C_Surface target;C_VertexBuffer vertices;C_VertexDeclaration declaration;C_Texture texture;
    require(device.CreateSurface(16,16,E_Flags_Colour,E_SurfaceFormat_A8R8G8B8,E_Multisample_None,nullptr,&target),"LOD target failed");
    require(device.CreateVertexBuffer(16*sizeof(F32),0,&vertices),"LOD vertex buffer failed");const F32 positions[]={-1,1,.5,1, 1,1,.5,1, -1,-1,.5,1, 1,-1,.5,1};auto* p=vertices.Lock();require(p,"LOD vertex upload failed");std::memcpy(p,positions,sizeof(positions));vertices.Unlock();
    C_VertexFormat format;format.AddElement(0,0,E_VertexElementType_F32_4,E_VertexElementUsage_Position,0);require(device.CreateVertexDeclaration(format,&declaration),"LOD declaration failed");
    require(device.CreateTexture2D(64,64,7,0,E_TextureFormat_A8R8G8B8,&texture),"LOD mip texture failed");
    for(U32 level=0;level<7;++level){auto lock=texture.LockRect2D(level);require(lock.pBits,"LOD mip upload failed");U32 colour=0xff000000|((level+1)*32<<16);for(U32 y=0;y<texture.GetHeight(level);++y)for(U32 x=0;x<texture.GetWidth(level);++x)std::memcpy(lock.pBits+y*lock.pitch+x*4,&colour,4);texture.UnlockRect2D(level);}
    context.SetColourSurface(0,&target);context.SetDepthStencilSurface(nullptr);context.SetVertexBuffer(0,&vertices,0,16,&declaration);context.SetCullMode(E_CullMode_None);context.SetDepthEnable(E_Bool_False);context.SetBlendEnable(E_Bool_False);
    context.SetTexture(2,texture.GetGPUTexture());context.SetTexture(7,texture.GetGPUTexture());context.SetVertexTexture(1,texture.GetGPUTexture());
    C_VertexShader vs;C_PixelShader ps;std::string error;
    auto loadVS=[&](const std::vector<U8>& b){device.DestroyVertexShader(&vs);require(device.CreateVertexShader(b.size(),b.data(),0,&vs,nullptr,&error),error.c_str());context.SetVertexShader(&vs);};
    auto loadPS=[&](const std::vector<U8>& b){device.DestroyPixelShader(&ps);require(device.CreatePixelShader(b.size(),b.data(),0,&ps,nullptr,&error),error.c_str());context.SetPixelShader(&ps);};
    auto state=[&](S8 bias,E_TextureFilter mip=E_TextureFilter_Linear){C_SamplerState s;s.SetTextureAddress(E_TextureAddress_Clamp);s.SetTextureFilter(E_TextureFilter_Linear,E_TextureFilter_Linear,mip);s.SetTextureLodBias(bias);return s;};
    auto pixel=[&](const std::string& expression){return blob(prefix+"fragment float4 blur_main(Out i [[stage_in]], texture2d<float> t [[texture(2)]], sampler s [[sampler(2)]]) { return "+expression+"; }",false,texture2);};
    auto draw=[&](int red,int green=0){context.BeginScene();context.Clear(E_Clear_Colour,0xff030405,1,0);context.DrawPrimitive(E_PrimitiveType_TriangleStrip,0,4);context.EndScene();auto lock=target.LockRect(16);require(lock.pBits,"LOD GPU readback failed");bool okay=true;for(U32 y=1;y<15;++y)for(U32 x=1;x<15;++x){const auto* c=lock.pBits+y*lock.pitch+x*4;okay&=std::abs(int(c[2])-red)<=2&&std::abs(int(c[1])-green)<=2&&c[0]<=1&&c[3]==255;}target.UnlockRect();if(!okay)throw std::runtime_error("Native biased mip GPU pixels differ, expected R="+std::to_string(red)+" G="+std::to_string(green));};
    loadVS(blob(vertexSource,true));loadPS(pixel("t.sample(s, i.uv)"));
    for(S8 bias:{S8(0),S8(16),S8(-16),S8(8),S8(-8),S8(127),S8(-128)}){context.SetSamplerState(2,state(bias));draw(int(32*(1+std::max(0.f,std::min(6.f,2+bias/16.f)))));}
    // Instruction bias and PC sampler state add; nested coordinates and comments
    // exercise balanced integration rather than textual comma splitting.
    loadPS(pixel("t.sample(s, float2(i.uv.x, i.uv.y), bias(0.5f))"));context.SetSamplerState(2,state(8));draw(128);
    loadPS(pixel("t.sample(s, float2(0.5f), level(1.0f))"));
    for(S8 bias:{S8(16),S8(-48),S8(8)}){context.SetSamplerState(2,state(bias));draw(bias==-48?32:64+bias*2);}
    context.SetSamplerState(2,state(48,E_TextureFilter_None));draw(32);
    // Independent slots and updates must remain independent of sampler cache.
    const std::string two=prefix+"fragment float4 blur_main(Out i [[stage_in]], texture2d<float> a [[texture(2)]], sampler sa [[sampler(2)]], texture2d<float> b [[texture(7)]], sampler sb [[sampler(7)]]) { return float4(a.sample(sa,i.uv).r,b.sample(sb,i.uv,level(1)).r,0,1); }";
    loadPS(blob(two,false,texture2+",{\"binding\":7,\"type\":\"sampler2D\"}"));context.SetSamplerState(2,state(16));context.SetSamplerState(7,state(-16));draw(128,32);context.SetSamplerState(2,state(-16));context.SetSamplerState(7,state(16));draw(64,96);
    loadPS(pixel("t.sample(s,i.uv,gradient2d(float2(0.0625f,0),float2(0,0.0625f)))"));context.SetSamplerState(2,state(16));draw(128);context.SetSamplerState(2,state(-16));draw(64);
    auto anisotropic=state(16);anisotropic.SetTextureFilter(E_TextureFilter_Anisotropic,E_TextureFilter_Anisotropic,E_TextureFilter_Linear);context.SetSamplerState(2,anisotropic,4);bool rejected=false;context.BeginScene();try{context.DrawPrimitive(E_PrimitiveType_TriangleStrip,0,4);}catch(const std::logic_error& e){rejected=std::string(e.what()).find("Anisotropic explicit-gradient")!=std::string::npos;}context.EndScene();require(rejected,"Unproven anisotropic gradient bias reported success");
    context.SetSamplerState(2,state(0));draw(96);
    loadPS(pixel("t.sample(s,float2(t.sample(s,i.uv).r,0.5f),level(1))"));context.SetSamplerState(2,state(16));draw(96);
    // Existing authored constant binding15 must not be overwritten by private
    // state; the loader selects14, and pipeline reflection observes both.
    const auto occupied=blob(prefix+"fragment float4 blur_main(Out i [[stage_in]],texture2d<float> t [[texture(2)]],sampler s [[sampler(2)]],constant float4& k [[buffer(15)]]) { return t.sample(s,i.uv)+k; }",false,texture2,"{\"name\":\"Authored\",\"binding\":15,\"block_size\":16}");
    loadPS(occupied);const F32 zero[]={0,0,0,0};context.SetPixelShaderConstantF(0,zero,1);draw(128);
    // A vertex-stage explicit sample reads its own sampler bank, never pixel2.
    const std::string vertexSample=prefix+"struct In { float4 p [[attribute(0)]]; }; vertex Out blur_main(In v [[stage_in]], texture2d<float> t [[texture(1)]], sampler s [[sampler(1)]]) { Out o; o.p=v.p; o.uv=0; o.colour=t.sample(s,float2(0.5),level(1)); return o; }";
    loadVS(blob(vertexSample,true,"{\"binding\":1,\"type\":\"sampler2D\"}"));loadPS(blob(prefix+"fragment float4 blur_main(Out i [[stage_in]]) { return i.colour; }",false));context.SetVertexSamplerState(1,state(16));draw(96);context.SetVertexSamplerState(1,state(-48));draw(32);context.SetVertexSamplerState(1,state(48,E_TextureFilter_None));draw(32);
    const std::string implicitVertex=prefix+"struct In { float4 p [[attribute(0)]]; }; vertex Out blur_main(In v [[stage_in]], texture2d<float> t [[texture(1)]], sampler s [[sampler(1)]]) { Out o; o.p=v.p; o.uv=0; o.colour=t.sample(s,float2(0.5)); return o; }";
    loadVS(blob(implicitVertex,true,"{\"binding\":1,\"type\":\"sampler2D\"}"));context.SetVertexSamplerState(1,state(16));draw(64);
    // Real authored D3D9 TEXLDL tokens, upstream decoder and corrected emitter.
    const std::vector<U32> legacyVS={0xfffe0300,0x0200001f,0x80000000,0x900f0000,0x0200001f,0x80000000,0xe00f0000,0x0200001f,0x80000005,0xe00f0001,0x02000001,0xe00f0000,0x90e40000,0x02000001,0xe00f0001,0xa0e40007,0xffff};
    const std::vector<U32> legacyPS={0xffff0300,0x0200001f,0x80000005,0x900f0000,0x0200001f,0x90000000,0xa00f0802,0x0300005f,0x800f0000,0x90e40000,0xa0e40802,0x02000001,0x800f0800,0x80e40000,0xffff};
    BlurMac::LegacyShader::Translation lv,lp;require(BlurMac::LegacyShader::Translate(legacyVS.data(),legacyVS.size()*4,BlurMac::LegacyShader::Stage::Vertex,lv),lv.error.c_str());require(BlurMac::LegacyShader::Translate(legacyPS.data(),legacyPS.size()*4,BlurMac::LegacyShader::Stage::Pixel,lp),lp.error.c_str());require(lp.source.find("level(")!=std::string::npos,"Legacy TEXLDL lost explicit LOD");loadVS(lv.blob);loadPS(lp.blob);const F32 uv[]={.5,.5,0,1};context.SetVertexShaderConstantF(7,uv,1);context.SetSamplerState(2,state(16));draw(96);context.SetSamplerState(2,state(-48));draw(32);context.SetSamplerState(2,state(48,E_TextureFilter_None));draw(32);
    const std::vector<U32> legacyVertexLod={0xfffe0300,0x0200001f,0x80000000,0x900f0000,0x0200001f,0x80000000,0xe00f0000,0x0200001f,0x80010005,0xe00f0002,0x0200001f,0x90000000,0xa00f0801,0x02000001,0xe00f0000,0x90e40000,0x0300005f,0x800f0000,0xa0e40007,0xa0e40801,0x02000001,0xe00f0002,0x80e40000,0xffff};
    require(BlurMac::LegacyShader::Translate(legacyVertexLod.data(),legacyVertexLod.size()*4,BlurMac::LegacyShader::Stage::Vertex,lv),lv.error.c_str());require(lv.source.find("level(")!=std::string::npos,"Legacy vertex TEXLDL lost explicit LOD");loadVS(lv.blob);loadPS(blob(prefix+"fragment float4 blur_main(Out i [[stage_in]]) { return i.colour; }",false));context.SetVertexShaderConstantF(7,uv,1);context.SetVertexSamplerState(1,state(16));draw(96);context.SetVertexSamplerState(1,state(-48));draw(32);
    // Buffer15 authored ownership forces another free private binding. Reject
    // unsupported helper sampler provenance only when nonzero state is used.
    const auto integrated=BlurMac::SamplerLod::Integrate(prefix+"float4 helper(texture2d<float> t,sampler x,float2 uv){return t.sample(x,uv,level(1));}\nfragment float4 blur_main(Out i [[stage_in]],texture2d<float> t [[texture(2)]],sampler s [[sampler(2)]]){return helper(t,s,i.uv);}","blur_main",false,1u<<2,0);
    require(integrated.unsupportedSamplers&(1u<<2),"Opaque sampler provenance was silently accepted");
    loadVS(blob(vertexSource,true));loadPS(blob(integrated.source,false,texture2));context.SetSamplerState(2,state(0));draw(64);
    rejected=false;context.SetSamplerState(2,state(16));context.BeginScene();try{context.DrawPrimitive(E_PrimitiveType_TriangleStrip,0,4);}catch(const std::logic_error& e){rejected=std::string(e.what()).find("provenance")!=std::string::npos;}context.EndScene();require(rejected,"Opaque sampler provenance ignored nonzero bias");
    rejected=false;context.SetSamplerState(2,state(0,E_TextureFilter_None));context.BeginScene();try{context.DrawPrimitive(E_PrimitiveType_TriangleStrip,0,4);}catch(const std::logic_error& e){rejected=std::string(e.what()).find("provenance")!=std::string::npos;}context.EndScene();
    require(rejected,"Opaque explicit sampler ignored zero-bias PC MaxLOD0");
    loadPS(pixel("t.sample(s,i.uv,level(1))"));draw(32);context.SetSamplerState(2,state(16));loadPS(pixel("t.sample(s,i.uv)"));draw(128);
    context.SetTexture(2,nullptr);context.SetTexture(7,nullptr);context.SetVertexTexture(1,nullptr);device.DestroyPixelShader(&ps);device.DestroyVertexShader(&vs);device.DestroyTexture(&texture);device.DestroyVertexDeclaration(&declaration);device.DestroyVertexBuffer(&vertices);device.DestroySurface(&target);
    std::cout<<"PC sampler S8/16 bias: real implicit/instruction/explicit/clamped/vertex/two-slot/rebind/isotropic-gradient mip GPU pixels, legacy TEXLDL and truthful unsupported-gradient boundary PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
