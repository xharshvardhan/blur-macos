// [Blur2PC][macOS] Exact GPU sampler pixels and context-owned lookup capacity.
#include <display2/device/MetalDevice.h>
#include <display2/context/Context.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/stat.h>
using namespace Display2;
static void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
struct Capture {
    FILE* file=std::tmpfile();int saved=-1;
    Capture(){require(file,"Sampler diagnostic file failed");std::fflush(stderr);saved=dup(STDERR_FILENO);require(saved>=0&&dup2(fileno(file),STDERR_FILENO)>=0,"Sampler diagnostic redirect failed");}
    std::string Text(){std::fflush(stderr);struct stat info{};require(fstat(fileno(file),&info)==0,"Sampler diagnostic size failed");std::string result(size_t(info.st_size),'\0');for(size_t at=0;at<result.size();){const auto n=pread(fileno(file),result.data()+at,result.size()-at,off_t(at));require(n>0,"Sampler diagnostic read failed");at+=size_t(n);}return result;}
    ~Capture(){if(saved>=0){std::fflush(stderr);dup2(saved,STDERR_FILENO);close(saved);}if(file)std::fclose(file);}
};
struct Env {bool existed=false;std::string old;Env(){if(const char* p=std::getenv("BLUR_MAC_SAMPLER_KEY_STATS")){existed=true;old=p;}}~Env(){if(existed)setenv("BLUR_MAC_SAMPLER_KEY_STATS",old.c_str(),1);else unsetenv("BLUR_MAC_SAMPLER_KEY_STATS");}};
static std::vector<U8> shader(bool vertex){
    const std::string bindings=vertex?R"("textures":[{"binding":1,"type":"sampler2D"}],"ubos":[{"binding":0,"block_size":16,"name":"VertexConstants"}])":R"("textures":[{"binding":2,"type":"sampler2D"},{"binding":7,"type":"sampler3D"}],"ubos":[{"binding":0,"block_size":32,"name":"PixelConstants"}])";
    const std::string meta="{\"parameters\":{},\"input_semantics\":"+std::string(vertex?"[[0,0,0]]":"[]")+",\"reflection\":{"+bindings+"}}";
    const std::string prefix="#include <metal_stdlib>\nusing namespace metal;\nstruct Out{float4 p[[position]];float value[[user(locn0)]];};\n";
    const std::string source=prefix+(vertex?"struct In{float2 p[[attribute(0)]];};vertex Out blur_main(In i[[stage_in]],constant float4& c[[buffer(0)]],texture2d<float> t[[texture(1)]],sampler s[[sampler(1)]]){return {float4(i.p,.5,1),t.sample(s,c.xy,level(c.z)).r};}":"struct C{float4 a;float4 b;};fragment float4 blur_main(Out i[[stage_in]],constant C& c[[buffer(0)]],texture2d<float> a[[texture(2)]],sampler sa[[sampler(2)]],texture3d<float> b[[texture(7)]],sampler sb[[sampler(7)]]){float4 va=a.sample(sa,c.a.xy,level(c.a.z));return float4(va.r,b.sample(sb,c.b.xyz,level(c.b.w)).r,i.value,va.a);}");
    const std::string entry="blur_main";std::vector<U8> bytes(40);std::memcpy(bytes.data(),"BMSL",4);auto word=[&](U32 offset,U32 value){for(U32 i=0;i<4;++i)bytes[offset+i]=U8(value>>(i*8));};word(4,1);word(8,vertex?0:1);word(16,40);word(20,U32(meta.size()));word(24,U32(40+meta.size()));word(28,U32(source.size()));word(32,U32(40+meta.size()+source.size()));word(36,U32(entry.size()));bytes.insert(bytes.end(),meta.begin(),meta.end());bytes.insert(bytes.end(),source.begin(),source.end());bytes.insert(bytes.end(),entry.begin(),entry.end());word(12,U32(bytes.size()));return bytes;
}
static C_SamplerState point(){C_SamplerState s;s.SetTextureAddress(E_TextureAddress_Wrap);s.SetTextureFilter(E_TextureFilter_Point,E_TextureFilter_Point,E_TextureFilter_Point);s.SetTextureBorderColour(0);return s;}
struct Case {
    C_SamplerState a=point(),b=point(),v=point();U32 anisotropy=1;
    std::array<F32,4> ca={.25f,.25f,0,0},cb={.25f,.25f,.25f,0},cv={.25f,.25f,0,0};
    U32 expected=0xff202020;
};
static U32 rgb(U32 a=32,U32 b=32,U32 v=32,U32 alpha=255){return alpha<<24|a<<16|b<<8|v;}
static std::vector<Case> cases(){
    std::vector<Case> out;Case base;out.push_back(base);
    // Independent manually known samples from a2x2 red ramp (32,96,160,224),
    // its mip64, and a3D texture with slices32/224 and mip128.
    for(auto axis:{0,1})for(auto mode:{E_TextureAddress_Wrap,E_TextureAddress_Mirror,E_TextureAddress_Clamp,E_TextureAddress_Border}){
        Case c;c.ca[axis]=1.25f;if(axis)c.a.SetTextureAddressV(mode);else c.a.SetTextureAddressU(mode);c.a.SetTextureBorderColour(0xffffffff);const U32 sample=mode==E_TextureAddress_Wrap?32:mode==E_TextureAddress_Border?255:axis?160:96;c.expected=rgb(sample);out.push_back(c);out.push_back(base);
    }
    for(U32 border:{0u,0xff000000u,0xffffffffu}){Case c;c.ca[0]=1.25f;c.a.SetTextureAddressU(E_TextureAddress_Border);c.a.SetTextureBorderColour(border);c.expected=rgb(border==0xffffffffu?255:0,32,32,border?255:0);out.push_back(c);}
    {Case c;c.ca={.5f,.5f,0,0};c.a.SetTextureFilter(E_TextureFilter_Linear,E_TextureFilter_Linear,E_TextureFilter_Point);c.expected=rgb(128);out.push_back(c);}
    for(auto mip:{E_TextureFilter_None,E_TextureFilter_Point,E_TextureFilter_Linear}){Case c;c.ca[2]=1;c.a.SetTextureFilter(E_TextureFilter_Point,E_TextureFilter_Point,mip);c.expected=rgb(mip==E_TextureFilter_None?32:64);out.push_back(c);}
    for(S8 bias:{S8(16),S8(-16),S8(8)}){Case c;c.ca[2]=bias<0?1:0;c.a.SetTextureLodBias(bias);c.a.SetTextureFilter(E_TextureFilter_Point,E_TextureFilter_Point,E_TextureFilter_Linear);c.expected=rgb(bias<0?32:bias==8?48:64);out.push_back(c);}
    {Case c;c.ca[2]=1;c.a.SetTextureLodBias(32);c.a.SetTextureFilter(E_TextureFilter_Point,E_TextureFilter_Point,E_TextureFilter_None);c.expected=rgb();out.push_back(c);}
    for(auto mode:{E_TextureAddress_Wrap,E_TextureAddress_Mirror,E_TextureAddress_Clamp,E_TextureAddress_Border}){Case c;c.cb[2]=1.25f;c.b.SetTextureAddressW(mode);c.b.SetTextureBorderColour(0xffffffff);c.expected=rgb(32,mode==E_TextureAddress_Wrap?32:mode==E_TextureAddress_Border?255:224);out.push_back(c);out.push_back(base);}
    {Case c;c.cb[2]=.5f;c.b.SetTextureFilter(E_TextureFilter_Linear,E_TextureFilter_Linear,E_TextureFilter_Point);c.expected=rgb(32,128);out.push_back(c);}
    for(auto mip:{E_TextureFilter_None,E_TextureFilter_Point}){Case c;c.cb[3]=1;c.b.SetTextureFilter(E_TextureFilter_Point,E_TextureFilter_Point,mip);c.expected=rgb(32,mip==E_TextureFilter_None?32:128);out.push_back(c);}
    {Case c;c.b.SetTextureLodBias(16);c.expected=rgb(32,128);out.push_back(c);}
    for(auto mode:{E_TextureAddress_Wrap,E_TextureAddress_Clamp}){Case c;c.ca[0]=c.cv[0]=1.25f;c.a.SetTextureAddressU(mode);c.v.SetTextureAddressU(mode==E_TextureAddress_Wrap?E_TextureAddress_Clamp:E_TextureAddress_Wrap);c.expected=rgb(mode==E_TextureAddress_Wrap?32:96,32,mode==E_TextureAddress_Wrap?96:32);out.push_back(c);}
    for(auto mip:{E_TextureFilter_None,E_TextureFilter_Point}){Case c;c.cv[2]=1;c.v.SetTextureFilter(E_TextureFilter_Point,E_TextureFilter_Point,mip);c.expected=rgb(32,32,mip==E_TextureFilter_None?32:64);out.push_back(c);}
    {Case c;c.v.SetTextureLodBias(16);c.expected=rgb(32,32,64);out.push_back(c);}
    // Exercise each exact descriptor-key field and many independently owned map
    // keys. At texel centers all these known address/filter variants still read32.
    // Return A after every B so moving the reusable lookup string into the map,
    // stale keys, or rehash lifetime mistakes cannot hide behind a warm last key.
    for(U32 i=0;i<96;++i){Case c;c.a.SetTextureAddressU(E_TextureAddress(1+i%4));c.a.SetTextureAddressV(E_TextureAddress(1+(i/4)%4));c.a.SetTextureAddressW(E_TextureAddress(1+(i/16)%4));c.a.SetTextureFilter(i&64?E_TextureFilter_Linear:E_TextureFilter_Point,i&32?E_TextureFilter_Linear:E_TextureFilter_Point,E_TextureFilter((i/8)%3));c.a.SetTextureBorderColour(i%3==0?0:i%3==1?0xff000000:0xffffffff);c.anisotropy=1u<<(i%5);out.push_back(c);out.push_back(base);}
    return out;
}
struct Fixture {
    C_MetalDevice device;C_Surface target;C_Texture plane,volume;C_VertexShader vs;C_PixelShader ps;C_VertexDeclaration declaration;C_VertexBuffer vertices;bool active=false;
    explicit Fixture(U32 rows){require(device.Create(),"Sampler key device failed");require(device.CreateSurface(4,rows,E_Flags_Colour,E_SurfaceFormat_A8R8G8B8,E_Multisample_None,nullptr,&target),"Sampler key target failed");require(device.CreateTexture2D(2,2,2,0,E_TextureFormat_A8R8G8B8,&plane)&&device.CreateTexture3D(2,2,2,2,0,E_TextureFormat_A8R8G8B8,&volume),"Sampler key textures failed");
        auto r=plane.LockRect2D(0);require(r.pBits,"Sampler plane upload failed");const U32 values[]={0xff200000,0xff600000,0xffa00000,0xffe00000};std::memcpy(r.pBits,values,8);std::memcpy(r.pBits+r.pitch,values+2,8);plane.UnlockRect2D(0);r=plane.LockRect2D(1);require(r.pBits,"Sampler plane mip upload failed");U32 pixel=0xff400000;std::memcpy(r.pBits,&pixel,4);plane.UnlockRect2D(1);
        auto box=volume.LockBox3D(0);require(box.pBits,"Sampler volume upload failed");for(U32 z=0;z<2;++z)for(U32 y=0;y<2;++y)for(U32 x=0;x<2;++x){pixel=z?0xffe00000:0xff200000;std::memcpy(box.pBits+z*box.slicePitch+y*box.rowPitch+x*4,&pixel,4);}volume.UnlockBox3D(0);box=volume.LockBox3D(1);require(box.pBits,"Sampler volume mip upload failed");pixel=0xff800000;std::memcpy(box.pBits,&pixel,4);volume.UnlockBox3D(1);
        std::string error;auto bytes=shader(true);require(device.CreateVertexShader(U32(bytes.size()),bytes.data(),0,&vs,nullptr,&error),error.c_str());bytes=shader(false);require(device.CreatePixelShader(U32(bytes.size()),bytes.data(),0,&ps,nullptr,&error),error.c_str());C_VertexFormat format;format.AddElement(0,0,E_VertexElementType_F32_2,E_VertexElementUsage_Position,0);require(device.CreateVertexDeclaration(format,&declaration)&&device.CreateVertexBuffer(32,0,&vertices),"Sampler key geometry failed");const F32 xy[]={-1,1,1,1,-1,-1,1,-1};auto p=vertices.Lock();require(p,"Sampler key geometry map failed");std::memcpy(p,xy,sizeof(xy));vertices.Unlock();auto& c=device.GetContext();c.SetColourSurface(0,&target);c.SetDepthStencilSurface(nullptr);c.SetVertexShader(&vs);c.SetPixelShader(&ps);c.SetVertexBuffer(0,&vertices,0,8,&declaration);c.SetTexture(2,&plane);c.SetTexture(7,&volume);c.SetVertexTexture(1,&plane);c.SetCullMode(E_CullMode_None);c.SetDepthEnable(E_Bool_False);c.SetStencilEnable(E_Bool_False);c.SetBlendEnable(E_Bool_False);c.SetAlphaTestEnable(E_Bool_False);c.SetAlphaToMaskEnable(E_Bool_False);c.SetColourWriteEnable(15);}
    ~Fixture(){auto& c=device.GetContext();if(active){try{End();}catch(...){}}c.SetColourSurface(0,nullptr);c.SetTexture(2,nullptr);c.SetTexture(7,nullptr);c.SetVertexTexture(1,nullptr);c.SetVertexShader(nullptr);c.SetPixelShader(nullptr);c.SetVertexDeclaration(nullptr);c.SetVertexBuffer(0,nullptr,0,0);device.Destroy(&plane);device.Destroy(&volume);device.Destroy(&vertices);device.Destroy(&declaration);device.Destroy(&vs);device.Destroy(&ps);device.Destroy(&target);device.Destroy();}
    void Begin(){device.GetContext().BeginScene();active=true;}
    void End(){device.GetContext().EndScene();active=false;}
    void Draw(const Case& t){auto& c=device.GetContext();c.SetSamplerState(2,t.a,t.anisotropy);c.SetSamplerState(7,t.b);c.SetVertexSamplerState(1,t.v);c.SetVertexShaderConstantF(0,t.cv.data(),1);c.SetPixelShaderConstantF(0,t.ca.data(),1);c.SetPixelShaderConstantF(1,t.cb.data(),1);c.DrawPrimitive(E_PrimitiveType_TriangleStrip,0,4);}
    void Pixels(const std::vector<U32>& expected){auto map=target.LockRect(16);require(map.pBits,"Sampler key GPU readback failed");bool good=true;for(U32 y=0;y<expected.size();++y)for(U32 x=0;x<4;++x){U32 actual=0;std::memcpy(&actual,map.pBits+y*map.pitch+x*4,4);for(U32 shift=0;shift<32;shift+=8){const int diff=int((actual>>shift)&255)-int((expected[y]>>shift)&255);good&=diff>=-2&&diff<=2;}}target.UnlockRect();require(good,"Sampler alternating-key2D/3D/vertex/LOD GPU pixels differ");}
};
struct Counts{unsigned long long requests=0,growth=0,capacity=0,hits=0,created=0;size_t entries=0;};
static Counts run(U32 repeats,bool enabled){setenv("BLUR_MAC_SAMPLER_KEY_STATS",enabled?"1":"0",1);Capture capture;const auto sequence=cases();std::vector<U32> expected;{Fixture f(U32(sequence.size())*repeats);auto& c=f.device.GetContext();f.Begin();c.Clear(E_Clear_Colour,0xff030405);c.SetScissorEnable(E_Bool_True);U32 y=0;for(U32 repeat=0;repeat<repeats;++repeat)for(const auto& t:sequence){c.SetScissorRect({0,S32(y),4,S32(y+1)});f.Draw(t);expected.push_back(t.expected);++y;}f.End();f.Pixels(expected);
    // Reject a real invalid address at draw time, then retry the valid state;
    // lookup scratch must not corrupt earlier owned keys on the failed miss.
    Case invalid;invalid.a.SetTextureAddressU(E_TextureAddress(0));bool rejected=false;f.Begin();try{f.Draw(invalid);}catch(const std::logic_error&){rejected=true;}f.End();require(rejected,"Invalid sampler address was accepted");f.Begin();c.SetScissorEnable(E_Bool_False);f.Draw(Case{});f.End();f.Pixels(std::vector<U32>(expected.size(),rgb()));require(f.device.WaitIdle(),"Sampler key final completion failed");}
    const auto text=capture.Text();const auto start=text.find("blur2pc.sampler_key_stats ");if(!enabled){require(start==std::string::npos,"Disabled sampler key diagnostics produced output");return {};}
    require(start!=std::string::npos&&text.find("blur2pc.sampler_key_stats ",start+1)==std::string::npos,"Missing or repeated sampler key summary");Counts n;require(std::sscanf(text.c_str()+start,"blur2pc.sampler_key_stats requests=%llu capacity_growth_calls=%llu max_capacity=%llu hits=%llu created=%llu entries=%zu",&n.requests,&n.growth,&n.capacity,&n.hits,&n.created,&n.entries)==6,"Sampler key summary parse failed");return n;
}
int main(){try{Env env;const auto once=run(1,true),repeated=run(3,true);std::cout<<"GPU sampler pixel/invalid-retry oracle PASS; once_requests="<<once.requests<<" once_growth="<<once.growth<<" once_created="<<once.created<<" repeated_requests="<<repeated.requests<<" repeated_growth="<<repeated.growth<<" repeated_created="<<repeated.created<<'\n';require(once.created>=64&&once.hits&&once.created==once.entries&&repeated.created==once.created&&repeated.entries==once.entries,"Alternating sampler map ownership or native state reuse failed");require(repeated.requests>once.requests&&repeated.hits>once.hits,"Repeated real shader sampler bindings were not observed");require(once.capacity>=32&&repeated.capacity==once.capacity&&once.growth<=1&&repeated.growth==once.growth,"Warm sampler lookup repeatedly grew key storage");run(1,false);std::cout<<"Sampler exact eight-word scratch key:2D/3D/both-stage GPU pixels, filters/address/border/LOD/clamp, A/B/A map growth, retry/recreation and default-off capacity evidence PASS\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
