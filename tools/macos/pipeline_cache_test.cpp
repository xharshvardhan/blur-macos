// [Blur2PC][macOS] Cache-hit ownership and real shader/layout/render-state alternation.
#include <display2/device/MetalDevice.h>
#include <display2/context/Context.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace Display2;
static void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
static std::vector<U8> shader(bool vertex,U32 variant){
    const std::string meta=vertex?R"({"parameters":{},"input_semantics":[[0,0,0]],"reflection":{"ubos":[]}})":R"({"parameters":{},"input_semantics":[],"reflection":{"ubos":[{"binding":0,"block_size":32,"name":"PixelConstants"}]}})";
    const std::string common="#include <metal_stdlib>\nusing namespace metal;struct V{float4 p[[position]];float r[[user(locn0)]];};\n";
    const std::string body=vertex?"struct I{float2 p[[attribute(0)]];};vertex V blur_main(I i[[stage_in]]){return {float4(i.p,.5,1),"+std::string(variant?".375":".125")+"};}":variant==2?"struct O{float4 c[[color(0)]];float d[[color(1)]];};fragment O blur_main(V i[[stage_in]],constant float4* v[[buffer(0)]]){return {float4(v[0].r+i.r,v[0].g,.25,v[0].a),v[1].r};}":"fragment float4 blur_main(V i[[stage_in]],constant float4* v[[buffer(0)]]){return float4(v[0].r+i.r,v[0].g,"+std::string(variant?".75":".25")+",v[0].a);}";
    const std::string source=common+body,entry="blur_main";std::vector<U8> result(40);std::memcpy(result.data(),"BMSL",4);auto word=[&](U32 p,U32 n){for(U32 i=0;i<4;++i)result[p+i]=U8(n>>(8*i));};word(4,1);word(8,vertex?0:1);word(16,40);word(20,U32(meta.size()));word(24,U32(40+meta.size()));word(28,U32(source.size()));word(32,U32(40+meta.size()+source.size()));word(36,U32(entry.size()));result.insert(result.end(),meta.begin(),meta.end());result.insert(result.end(),source.begin(),source.end());result.insert(result.end(),entry.begin(),entry.end());word(12,U32(result.size()));return result;
}
struct State{U32 vertex=0,pixel=0,layout=0,blend=0,mask=15,mrtMask=1;E_SurfaceFormat format=E_SurfaceFormat_A8R8G8B8,depth=E_SurfaceFormat_D16;E_Multisample samples=E_Multisample_None;bool coverage=false;F32 alpha=1;};
struct Fixture{
    C_MetalDevice device;std::array<C_VertexShader,2> vs;std::array<C_PixelShader,3> ps;std::array<C_VertexDeclaration,2> declarations;std::array<C_VertexBuffer,3> geometry;
    Fixture(){require(device.Create(),"Pipeline cache device failed");std::string error;for(U32 i=0;i<2;++i){const auto code=shader(true,i);require(device.CreateVertexShader(U32(code.size()),code.data(),0,&vs[i],nullptr,&error),error.c_str());}for(U32 i=0;i<3;++i){const auto code=shader(false,i);require(device.CreatePixelShader(U32(code.size()),code.data(),0,&ps[i],nullptr,&error),error.c_str());}
        C_VertexFormat a,b;a.AddElement(0,0,E_VertexElementType_F32_2,E_VertexElementUsage_Position,0);b.AddElement(1,8,E_VertexElementType_F32_2,E_VertexElementUsage_Position,0);require(device.CreateVertexDeclaration(a,&declarations[0])&&device.CreateVertexDeclaration(b,&declarations[1]),"Pipeline declarations failed");
        const F32 xy[]={-1,1,1,1,-1,-1,1,-1};for(U32 layout=0;layout<3;++layout){const U32 stride=layout==0?8:layout==1?20:16,offset=layout==1?8:0;require(device.CreateVertexBuffer(stride*4,0,&geometry[layout]),"Pipeline geometry allocation failed");auto* data=static_cast<U8*>(geometry[layout].Lock());require(data,"Pipeline geometry lock failed");std::memset(data,0,stride*4);for(U32 vertex=0;vertex<4;++vertex)std::memcpy(data+vertex*stride+offset,xy+vertex*2,8);geometry[layout].Unlock();}
    }
    ~Fixture(){auto& c=device.GetContext();c.SetColourSurface(0,nullptr);c.SetColourSurface(1,nullptr);c.SetDepthStencilSurface(nullptr);c.SetVertexShader(nullptr);c.SetPixelShader(nullptr);c.SetVertexDeclaration(nullptr);for(U32 i=0;i<8;++i)c.SetVertexBuffer(i,nullptr,0,0);for(auto& shader:vs)device.Destroy(&shader);for(auto& shader:ps)device.Destroy(&shader);for(auto& buffer:geometry)device.Destroy(&buffer);for(auto& declaration:declarations)device.Destroy(&declaration);}
    void bind(const State& s){auto& c=device.GetContext();c.SetVertexShader(&vs[s.vertex]);c.SetPixelShader(&ps[s.pixel]);for(U32 i=0;i<8;++i)c.SetVertexBuffer(i,nullptr,0,0);c.SetVertexBuffer(s.layout==1?1:0,&geometry[s.layout],0,s.layout==0?8:s.layout==1?20:16,&declarations[s.layout==1?1:0]);c.SetCullMode(E_CullMode_None);c.SetDepthEnable(E_Bool_True);c.SetDepthWriteEnable(E_Bool_False);c.SetDepthFunc(E_CmpFunc_Less);c.SetStencilEnable(E_Bool_False);c.SetAlphaTestEnable(E_Bool_False);c.SetAlphaToMaskEnable(s.coverage?E_Bool_True:E_Bool_False);c.SetBlendEnable(s.blend?E_Bool_True:E_Bool_False);
        c.SetBlendSrc(s.blend==2?E_Blend_SrcAlpha:E_Blend_One);c.SetBlendDst(s.blend==2?E_Blend_InvSrcAlpha:E_Blend_One);c.SetBlendOp(s.blend==2?E_BlendOp_Subtract:E_BlendOp_Add);c.SetBlendSrcAlpha(E_Blend_One);c.SetBlendDstAlpha(s.blend==2?E_Blend_One:E_Blend_Zero);c.SetBlendOpAlpha(s.blend==2?E_BlendOp_RevSubtract:E_BlendOp_Add);c.SetColourWriteEnable(0,s.mask);c.SetColourWriteEnable(1,s.mrtMask);const F32 values[]={.125f,.5f,0,s.alpha,.625f,0,0,0};c.SetPixelShaderConstantF(0,values,2);
    }
    void check(const State& s,bool benchmark=false){
        auto& c=device.GetContext();C_Surface colour,depth,mrt;require(device.CreateSurface(16,8,E_Flags_Colour,s.format,s.samples,nullptr,&colour)&&device.CreateSurface(16,8,E_Flags_DepthStencil,s.depth,s.samples,nullptr,&depth),"Pipeline attachments failed");if(s.pixel==2)require(device.CreateSurface(16,8,E_Flags_Colour,E_SurfaceFormat_R32F,s.samples,nullptr,&mrt),"Pipeline float MRT failed");
        c.SetColourSurface(0,&colour);c.SetColourSurface(1,s.pixel==2?&mrt:nullptr);c.SetDepthStencilSurface(&depth);c.SetViewport(C_Viewport(0,0,16,8));c.BeginScene();c.Clear(E_Clear_Colour|E_Clear_Depth|(s.depth==E_SurfaceFormat_D24S8?E_Clear_Stencil:0),0xff204060,.75f,7);bind(s);c.DrawPrimitive(E_PrimitiveType_TriangleStrip,0,4);
        if(benchmark){std::vector<double> times;for(U32 trial=0;trial<5;++trial){const auto begin=std::chrono::steady_clock::now();for(U32 draw=0;draw<2000;++draw)c.DrawPrimitive(E_PrimitiveType_TriangleStrip,0,4);times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());}std::sort(times.begin(),times.end());std::cout<<"hot_pipeline_encode_median_ms="<<times[2]<<" draws=2000 trials=5\n";}
        c.EndScene();auto map=colour.LockRect(16);require(map.pBits,"Pipeline colour resolve/readback failed");
        const std::array<F32,4> background={32.f/255,64.f/255,96.f/255,1},source={s.vertex?.5f:.25f,.5f,s.pixel==1?.75f:.25f,s.alpha};auto expected=background;
        if(!s.coverage||s.alpha!=0)for(U32 channel=0;channel<4;++channel)if(s.mask&(1u<<channel)){F32 value=source[channel];if(s.blend==1&&channel<3)value+=background[channel];if(s.blend==2)value=channel==3?background[channel]-source[channel]:source[channel]*s.alpha-background[channel]*(1-s.alpha);expected[channel]=std::min(1.f,std::max(0.f,value));}
        bool valid=true;for(U32 y=0;y<8;++y)for(U32 x=0;x<16;++x)for(U32 channel=0;channel<4;++channel){const U32 component=s.format==E_SurfaceFormat_A8R8G8B8&&channel!=3?2-channel:channel;valid&=std::abs(S32(map.pBits[y*map.pitch+x*4+component])-S32(std::lround(expected[channel]*255)))<=1;}colour.UnlockRect();require(valid,"Cached shader/layout/blend/mask/depth/sample state produced incorrect GPU pixels");
        if(s.pixel==2){map=mrt.LockRect(16);require(map.pBits,"Pipeline MRT readback failed");valid=true;for(U32 y=0;y<8;++y)for(U32 x=0;x<16;++x){F32 value;std::memcpy(&value,map.pBits+y*map.pitch+x*4,4);valid&=std::abs(value-(s.mrtMask?.625f:32.f/255))<1e-6f;}mrt.UnlockRect();require(valid,"Cached float MRT format or independent mask changed values");}
        c.SetColourSurface(0,nullptr);c.SetColourSurface(1,nullptr);c.SetDepthStencilSurface(nullptr);device.Destroy(&colour);device.Destroy(&depth);device.Destroy(&mrt);
    }
};
int main(int argc,char** argv){try{
    const bool benchmark=argc==2&&std::string(argv[1])=="--benchmark";require(argc==1||benchmark,"Usage: pipeline_cache_test [--benchmark]");Fixture f;State a;std::vector<State> states={a};
    State b=a;b.vertex=1;states.push_back(b);b.pixel=1;states.push_back(b);b.layout=1;states.push_back(b);b.layout=2;states.push_back(b);b.format=E_SurfaceFormat_A8B8G8R8;states.push_back(b);b.depth=E_SurfaceFormat_D24S8;states.push_back(b);b.blend=1;states.push_back(b);b.blend=2;b.alpha=.5f;states.push_back(b);b.blend=0;b.mask=2;states.push_back(b);
    for(auto samples:{E_Multisample_None,E_Multisample_2,E_Multisample_4}){b=a;b.samples=samples;b.coverage=true;b.alpha=0;states.push_back(b);b.coverage=false;states.push_back(b);b.coverage=true;b.alpha=1;states.push_back(b);b.pixel=2;b.coverage=false;b.mrtMask=1;states.push_back(b);b.mrtMask=0;states.push_back(b);}
    // Every distinct B key mutates the same scratch storage, then returns to A.
    // Revisit the entire set so these are cache hits with independent owned keys.
    for(U32 repeat=0;repeat<2;++repeat)for(const auto& state:states){f.check(state);f.check(a);}
    // Retire/recreate both shader wrappers after cached pipelines exist. New
    // immutable identities must select the new programs rather than reuse an
    // allocation address, while old cached native states remain safe to retire.
    if(benchmark)f.check(a,true);
    auto replacement=shader(true,1);f.device.Destroy(&f.vs[0]);std::string error;require(f.device.CreateVertexShader(U32(replacement.size()),replacement.data(),0,&f.vs[0],nullptr,&error),error.c_str());State changed=a;changed.vertex=0; // shader0 now has shader1's actual code
    // Preserve independent expected semantics by swapping handles for this check.
    std::swap(f.vs[0],f.vs[1]);changed.vertex=1;
    replacement=shader(false,1);f.device.Destroy(&f.ps[0]);require(f.device.CreatePixelShader(U32(replacement.size()),replacement.data(),0,&f.ps[0],nullptr,&error),error.c_str());std::swap(f.ps[0],f.ps[1]);changed.pixel=1;f.check(changed);std::swap(f.ps[0],f.ps[1]);std::swap(f.vs[0],f.vs[1]);
    require(f.device.WaitIdle(),"Pipeline cache GPU completion failed");std::cout<<"Pipeline owned-key A/B/A, VS/PS lifetime, declaration/stride, blend/ops, RGBA/BGRA, MRT/masks, depth/stencil formats,1x/2x/4x and coverage GPU checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
