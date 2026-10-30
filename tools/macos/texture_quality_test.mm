// [Blur2PC][macOS] Opt-in authored-anisotropic quality: real oblique GPU detail.
#include <display2/device/MetalDevice.h>
#include <display2/context/Context.h>
#include "../../code/mercury/library/graphics/display2/source/device/metal/MetalTextureQuality.h"
#include <array>
#include <cmath>
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
struct Env {
    const char* key;bool existed=false;std::string old;
    explicit Env(const char* name):key(name){if(const char* p=std::getenv(key)){existed=true;old=p;}}
    ~Env(){if(existed)setenv(key,old.c_str(),1);else unsetenv(key);}
};
struct Capture {
    FILE* file=std::tmpfile();int saved=-1;
    Capture(){require(file,"Quality diagnostic file failed");std::fflush(stderr);saved=dup(STDERR_FILENO);require(saved>=0&&dup2(fileno(file),STDERR_FILENO)>=0,"Quality diagnostic redirect failed");}
    std::string Text(){std::fflush(stderr);struct stat info{};require(fstat(fileno(file),&info)==0,"Quality diagnostic size failed");std::string text(size_t(info.st_size),'\0');for(size_t at=0;at<text.size();){auto n=pread(fileno(file),text.data()+at,text.size()-at,off_t(at));require(n>0,"Quality diagnostic read failed");at+=size_t(n);}return text;}
    ~Capture(){if(saved>=0){std::fflush(stderr);dup2(saved,STDERR_FILENO);close(saved);}if(file)std::fclose(file);}
};
static void policyChecks(){
    using Metal::TextureQualityPolicy;
    for(const char* value:{static_cast<const char*>(nullptr),"","authored"})require(TextureQualityPolicy::Parse(value)==0,"Authored quality parse changed");
    for(const char* value:{"4","8","16"})require(TextureQualityPolicy::Parse(value)==U32(std::stoi(value)),"Quality floor parse failed");
    for(const char* value:{"0","1","2","17","-1","16x","16 ","8junk"}){bool caught=false;try{TextureQualityPolicy::Parse(value);}catch(const std::invalid_argument&){caught=true;}require(caught,"Invalid quality option was silently accepted");}
    for(auto mag:{E_TextureFilter_Point,E_TextureFilter_Linear,E_TextureFilter_Anisotropic})for(auto min:{E_TextureFilter_Point,E_TextureFilter_Linear,E_TextureFilter_Anisotropic})for(auto mip:{E_TextureFilter_None,E_TextureFilter_Point,E_TextureFilter_Linear})for(U32 requested:{1u,4u,8u,16u}){
        C_SamplerState s;s.SetTextureFilter(mag,min,mip);s.SetTextureAddress(E_TextureAddress_Border,E_TextureAddress_Clamp,E_TextureAddress_Mirror);s.SetTextureBorderColour(0xffffffff);s.SetTextureLodBias(-8);s.SetTextureGamma(1);const auto before=s;
        TextureQualityPolicy original("authored",false),high("16",false);
        const bool eligible=mag!=E_TextureFilter_Point&&min!=E_TextureFilter_Point&&mip!=E_TextureFilter_None&&(mag==E_TextureFilter_Anisotropic||min==E_TextureFilter_Anisotropic);
        require(original.Select(s,requested)==requested&&high.Select(s,requested)==(eligible?16:requested),"Quality promotion changed filter eligibility or authored maxima");require(s.IsSame(before),"Quality selection mutated authored state");
    }
}
static std::vector<U8> blob(bool vertex,bool sampledVertex=false,bool explicitLevel=false){
    std::string uniforms="\"ubos\":[],\"textures\":[]";
    if(vertex&&sampledVertex)uniforms=R"("ubos":[{"binding":0,"block_size":16,"name":"Params"}],"textures":[{"binding":1,"type":"sampler2D"}])";
    if(!vertex&&!sampledVertex)uniforms=R"("ubos":[{"binding":0,"block_size":16,"name":"Params"}],"textures":[{"binding":2,"type":"sampler2D"}])";
    const std::string metadata="{\"parameters\":{},\"input_semantics\":"+std::string(vertex?"[[0,0,0]]":"[]")+",\"reflection\":{"+uniforms+"}}";
    const std::string prefix="#include <metal_stdlib>\nusing namespace metal;\nstruct V{float4 p[[position]];float4 c[[user(locn0)]];};\n";
    std::string source=prefix;
    if(vertex){source+="struct I{float2 p[[attribute(0)]];};vertex V blur_main(I i[[stage_in]]";if(sampledVertex)source+=",texture2d<float> t[[texture(1)]],sampler s[[sampler(1)]],constant float4& c[[buffer(0)]]";source+="){V o;o.p=float4(i.p,.5,1);o.c=";source+=sampledVertex?"t.sample(s,c.xy,gradient2d(float2(16.0/256,0),float2(0,1.0/256)))":"float4(1)";source+=";return o;}";}
    else if(sampledVertex)source+="fragment float4 blur_main(V i[[stage_in]]){return i.c;}";
    else {source+="fragment float4 blur_main(V i[[stage_in]],texture2d<float> t[[texture(2)]],sampler s[[sampler(2)]],constant float4& c[[buffer(0)]]){return ";source+=explicitLevel?"t.sample(s,c.xy,level(c.z))":"t.sample(s,float2(i.p.x*16.0/256,i.p.y/256),gradient2d(float2(16.0/256,0),float2(0,1.0/256)))";source+=";}";}
    const std::string entry="blur_main";std::vector<U8> data(40);std::memcpy(data.data(),"BMSL",4);auto word=[&](U32 at,U32 value){for(U32 j=0;j<4;++j)data[at+j]=U8(value>>(j*8));};word(4,1);word(8,vertex?0:1);word(16,40);word(20,U32(metadata.size()));word(24,U32(40+metadata.size()));word(28,U32(source.size()));word(32,U32(40+metadata.size()+source.size()));word(36,U32(entry.size()));data.insert(data.end(),metadata.begin(),metadata.end());data.insert(data.end(),source.begin(),source.end());data.insert(data.end(),entry.begin(),entry.end());word(12,U32(data.size()));return data;
}
static C_SamplerState anisotropic(){C_SamplerState s;s.SetTextureAddress(E_TextureAddress_Wrap);s.SetTextureFilter(E_TextureFilter_Linear,E_TextureFilter_Anisotropic,E_TextureFilter_Linear);return s;}
struct Request {C_SamplerState state=anisotropic();U32 maximum=1;bool vertex=false,explicitLevel=false,global=false;std::array<F32,4> params={.5f,12.5f/256,0,0};};
struct Evidence {std::vector<U32> pixels;unsigned long long requests=0,eligible=0,promoted=0;U32 requested=0,effective=0;std::string mode;};
struct Fixture {
    C_MetalDevice device;C_Surface target;C_Texture road;C_VertexShader vs;C_PixelShader ps;C_VertexBuffer vb;C_VertexDeclaration decl;bool active=false;
    explicit Fixture(const Request& request){require(device.Create(),"Quality device failed");require(device.CreateSurface(32,64,E_Flags_Colour,E_SurfaceFormat_A8R8G8B8,E_Multisample_None,nullptr,&target)&&device.CreateTexture2D(256,256,9,0,E_TextureFormat_A8R8G8B8,&road),"Quality targets failed");
        // Independent exact box-filtered mip chain: eight black/eight white rows.
        // At16:1 footprints isotropic mip4 loses the minor-axis stripe signal;
        // higher authored anisotropy can retain it without changing source data.
        for(U32 level=0;level<9;++level){auto map=road.LockRect2D(level);require(map.pBits,"Quality mip upload failed");for(U32 y=0;y<road.GetHeight(level);++y){U32 sum=0;for(U32 source=0;source<(1u<<level);++source)sum+=(((y<<level)+source)/8)%2?255:0;const U32 value=(sum+(1u<<level)/2)/(1u<<level),pixel=0xff000000|value*0x10101;for(U32 x=0;x<road.GetWidth(level);++x)std::memcpy(map.pBits+y*map.pitch+x*4,&pixel,4);}road.UnlockRect2D(level);}
        std::string error;auto data=blob(true,request.vertex,request.explicitLevel);require(device.CreateVertexShader(U32(data.size()),data.data(),0,&vs,nullptr,&error),error.c_str());data=blob(false,request.vertex,request.explicitLevel);require(device.CreatePixelShader(U32(data.size()),data.data(),0,&ps,nullptr,&error),error.c_str());C_VertexFormat format;format.AddElement(0,0,E_VertexElementType_F32_2,E_VertexElementUsage_Position,0);require(device.CreateVertexDeclaration(format,&decl)&&device.CreateVertexBuffer(32,0,&vb),"Quality geometry failed");const F32 xy[]={-1,1,1,1,-1,-1,1,-1};auto p=vb.Lock();require(p,"Quality geometry map failed");std::memcpy(p,xy,sizeof(xy));vb.Unlock();auto& c=device.GetContext();c.SetColourSurface(0,&target);c.SetDepthStencilSurface(nullptr);c.SetVertexShader(&vs);c.SetPixelShader(&ps);c.SetVertexBuffer(0,&vb,0,8,&decl);c.SetDepthEnable(E_Bool_False);c.SetStencilEnable(E_Bool_False);c.SetCullMode(E_CullMode_None);c.SetBlendEnable(E_Bool_False);c.SetAlphaTestEnable(E_Bool_False);c.SetAlphaToMaskEnable(E_Bool_False);c.SetColourWriteEnable(15);c.SetVertexShaderConstantF(0,request.params.data(),1);c.SetPixelShaderConstantF(0,request.params.data(),1);if(request.vertex){c.SetVertexTexture(1,&road);if(request.global){c.SetMaxAnisotropy(request.maximum);c.SetVertexSamplerState(1,request.state);}else c.SetVertexSamplerState(1,request.state,request.maximum);}else{c.SetTexture(2,&road);if(request.global){c.SetMaxAnisotropy(request.maximum);c.SetSamplerState(2,request.state);}else c.SetSamplerState(2,request.state,request.maximum);}}
    ~Fixture(){auto& c=device.GetContext();if(active){try{c.EndScene();}catch(...){}}c.SetColourSurface(0,nullptr);c.SetTexture(2,nullptr);c.SetVertexTexture(1,nullptr);c.SetVertexShader(nullptr);c.SetPixelShader(nullptr);c.SetVertexDeclaration(nullptr);c.SetVertexBuffer(0,nullptr,0,0);device.Destroy(&road);device.Destroy(&target);device.Destroy(&vs);device.Destroy(&ps);device.Destroy(&vb);device.Destroy(&decl);device.Destroy();}
    std::vector<U32> Render(){auto& c=device.GetContext();c.BeginScene();active=true;c.Clear(E_Clear_Colour,0xff102030);c.DrawPrimitive(E_PrimitiveType_TriangleStrip,0,4);c.EndScene();active=false;auto map=target.LockRect(16);require(map.pBits,"Quality GPU readback failed");std::vector<U32> result(32*64);for(U32 y=0;y<64;++y)std::memcpy(result.data()+y*32,map.pBits+y*map.pitch,32*4);target.UnlockRect();require(device.WaitIdle(),"Quality GPU completion failed");return result;}
};
static Evidence render(const char* mode,const Request& request,bool stats=true){if(mode)setenv("BLUR_MAC_TEXTURE_ANISOTROPY",mode,1);else unsetenv("BLUR_MAC_TEXTURE_ANISOTROPY");setenv("BLUR_MAC_TEXTURE_QUALITY_STATS",stats?"1":"0",1);Capture capture;Evidence out;{Fixture fixture(request);out.pixels=fixture.Render();}const auto text=capture.Text();const auto start=text.find("blur2pc.texture_quality ");if(!stats){require(start==std::string::npos,"Disabled texture quality evidence produced output");return out;}require(start!=std::string::npos&&text.find("blur2pc.texture_quality ",start+1)==std::string::npos,"Missing/repeated quality summary");char parsed[16]={};require(std::sscanf(text.c_str()+start,"blur2pc.texture_quality mode=%15s requests=%llu eligible=%llu promoted=%llu eligible_max_requested=%u eligible_max_effective=%u",parsed,&out.requests,&out.eligible,&out.promoted,&out.requested,&out.effective)==6,"Quality summary parse failed");out.mode=parsed;require(out.requests==1,"Quality fixture did not sample exactly its active stage");return out;}
static void identical(const Evidence& a,const Evidence& b,const char* why){require(a.pixels==b.pixels,why);}
static double error(const Evidence& image){double total=0;U32 n=0;for(U32 y=0;y<64;++y)if(y%8>=2&&y%8<=5)for(U32 x=2;x<30;++x){const double expected=(y/8)%2?255:0;total+=std::abs(double((image.pixels[y*32+x]>>16)&255)-expected);++n;}return total/n;}
static double contrast(const Evidence& image){double white=0,black=0;U32 wn=0,bn=0;for(U32 y=0;y<64;++y)if(y%8>=2&&y%8<=5)for(U32 x=2;x<30;++x){const double value=(image.pixels[y*32+x]>>16)&255;if((y/8)%2){white+=value;++wn;}else{black+=value;++bn;}}return white/wn-black/bn;}
int main(){try{Env quality("BLUR_MAC_TEXTURE_ANISOTROPY"),stats("BLUR_MAC_TEXTURE_QUALITY_STATS");policyChecks();Request road;const auto original=render(nullptr,road),high=render("16",road);require(original.mode=="authored"&&original.eligible==1&&!original.promoted&&original.requested==1&&original.effective==1,"Authored default changed");require(high.promoted==1&&high.requested==1&&high.effective==16,"Quality floor did not reach actual sampler selection");const double oldError=error(original),newError=error(high),oldContrast=contrast(original),newContrast=contrast(high);std::cout<<"Oblique16:1 stripe oracle: authored_mae="<<oldError<<" enhanced_mae="<<newError<<" authored_contrast="<<oldContrast<<" enhanced_contrast="<<newContrast<<'\n';require(oldError>60&&newError<oldError*.4&&newContrast>150&&oldContrast<40,"Anisotropic quality did not preserve the independent minor-axis detail signal");
    for(const char* mode:{"4","8","16"}){Request authored=road;authored.maximum=U32(std::stoi(mode));const auto native=render("authored",authored),selected=render(mode,road);identical(native,selected,"Selected quality differs from equivalent authored native maximum");require(selected.effective==authored.maximum,"Selected maximum evidence is wrong");}
    Request alreadyHigh=road;alreadyHigh.maximum=16;identical(render("authored",alreadyHigh),render("4",alreadyHigh),"Quality floor lowered a higher authored maximum");Request global=road;global.global=true;identical(high,render("16",global),"Global-default and per-slot quality selection differ");
    for(U32 control=0;control<4;++control){Request r=road;if(control==0)r.state.SetTextureFilter(E_TextureFilter_Point,E_TextureFilter_Point,E_TextureFilter_Linear);if(control==1)r.state.SetTextureFilter(E_TextureFilter_Linear,E_TextureFilter_Linear,E_TextureFilter_Linear);if(control==2)r.state.SetTextureFilter(E_TextureFilter_Linear,E_TextureFilter_Anisotropic,E_TextureFilter_None);if(control==3)r.state.SetTextureFilter(E_TextureFilter_Point,E_TextureFilter_Anisotropic,E_TextureFilter_Linear);const auto a=render("authored",r),b=render("16",r);identical(a,b,"Quality rewrote point/linear/no-mip/point-axis controls");require(!b.eligible&&!b.promoted,"Ineligible filter was promoted");}
    // Explicit mip/bias/address remain authored. Equivalent native max16 must
    // reproduce all pixels without a policy-driven LOD or address adjustment.
    for(U32 variant=0;variant<3;++variant){Request r=road;r.explicitLevel=true;r.params={.5f,12.5f/256,0,0};if(variant==0)r.state.SetTextureLodBias(8);if(variant==1){r.params[0]=-1;r.state.SetTextureAddress(E_TextureAddress_Border);r.state.SetTextureBorderColour(0xffffffff);}if(variant==2)r.params[2]=4;auto native=r;native.maximum=16;identical(render("authored",native),render("16",r),"Quality changed explicit mip/bias/address semantics");}
    Request vertex=road;vertex.vertex=true;const auto v=render("16",vertex);vertex.maximum=16;identical(v,render("authored",vertex),"Vertex quality and equivalent authored maximum differ");require(v.eligible==1&&v.promoted==1,"Actual vertex sampler was not promoted");identical(high,render("16",road,false),"Diagnostics changed quality pixels");std::cout<<"Texture quality: authored default,4/8/16 equivalent native sampling, oblique detail, point/linear/no-mip exclusion, higher maxima, per-slot/global, vertex/pixel, LOD/address, recreation and default-off evidence PASS\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
