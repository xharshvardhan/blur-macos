// [Blur2PC][macOS] Exact pixels through real context draws/clears, plus opt-in
// native creation evidence. No original assets or production test hooks.
#include <display2/device/MetalDevice.h>
#include <display2/context/Context.h>
#include <algorithm>
#include <array>
#include <chrono>
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
struct Capture {
    FILE* file=std::tmpfile();int saved=-1;
    Capture(){require(file,"Cannot create diagnostic capture");std::fflush(stderr);saved=dup(STDERR_FILENO);require(saved>=0&&dup2(fileno(file),STDERR_FILENO)>=0,"Cannot redirect diagnostics");}
    std::string Text(){std::fflush(stderr);struct stat info{};require(fstat(fileno(file),&info)==0,"Cannot size diagnostic capture");std::string text(size_t(info.st_size),'\0');size_t at=0;while(at<text.size()){auto n=pread(fileno(file),text.data()+at,text.size()-at,off_t(at));require(n>0,"Cannot read diagnostic capture");at+=size_t(n);}return text;}
    ~Capture(){if(saved>=0){std::fflush(stderr);dup2(saved,STDERR_FILENO);close(saved);}if(file)std::fclose(file);}
};
struct Env {
    bool existed=false;std::string old;
    Env(){if(const char* value=std::getenv("BLUR_MAC_DEPTH_STATE_STATS")){existed=true;old=value;}}
    ~Env(){if(existed)setenv("BLUR_MAC_DEPTH_STATE_STATS",old.c_str(),1);else unsetenv("BLUR_MAC_DEPTH_STATE_STATS");}
};
static std::vector<U8> shader(bool vertex){
    const std::string meta=vertex?R"({"parameters":{},"input_semantics":[[0,0,0]],"reflection":{"ubos":[{"binding":0,"block_size":16,"name":"VertexConstants"}]}})":R"({"parameters":{},"input_semantics":[],"reflection":{"ubos":[{"binding":0,"block_size":16,"name":"PixelConstants"}]}})";
    const std::string common="#include <metal_stdlib>\nusing namespace metal;\n";
    const std::string body=vertex?"struct I{float2 p[[attribute(0)]];};vertex float4 blur_main(I i[[stage_in]],constant float4& v[[buffer(0)]]){return float4(i.p,v.x,1);}":"fragment float4 blur_main(constant float4& c[[buffer(0)]]){return c;}";
    const std::string source=common+body,entry="blur_main";std::vector<U8> bytes(40);std::memcpy(bytes.data(),"BMSL",4);auto word=[&](U32 p,U32 n){for(U32 i=0;i<4;++i)bytes[p+i]=U8(n>>(i*8));};word(4,1);word(8,vertex?0:1);word(16,40);word(20,U32(meta.size()));word(24,U32(40+meta.size()));word(28,U32(source.size()));word(32,U32(40+meta.size()+source.size()));word(36,U32(entry.size()));bytes.insert(bytes.end(),meta.begin(),meta.end());bytes.insert(bytes.end(),source.begin(),source.end());bytes.insert(bytes.end(),entry.begin(),entry.end());word(12,U32(bytes.size()));return bytes;
}
static bool compare(U32 function,F32 reference,F32 stored){switch(function){case E_CmpFunc_Never:return false;case E_CmpFunc_Less:return reference<stored;case E_CmpFunc_Equal:return reference==stored;case E_CmpFunc_LessEqual:return reference<=stored;case E_CmpFunc_Greater:return reference>stored;case E_CmpFunc_NotEqual:return reference!=stored;case E_CmpFunc_GreaterEqual:return reference>=stored;case E_CmpFunc_Always:return true;default:throw std::runtime_error("Oracle comparison invalid");}}
static U8 operation(U32 op,U8 stored,U32 reference){switch(op){case E_StencilOp_Keep:return stored;case E_StencilOp_Zero:return 0;case E_StencilOp_Replace:return U8(reference);case E_StencilOp_IncrSat:return U8(std::min(255u,U32(stored)+1));case E_StencilOp_DecrSat:return stored?U8(stored-1):0;case E_StencilOp_Invert:return U8(~stored);case E_StencilOp_Incr:return U8(stored+1);case E_StencilOp_Decr:return U8(stored-1);default:throw std::runtime_error("Oracle stencil operation invalid");}}
struct State {bool depth=true,write=true,stencil=false;E_CmpFunc depthFunc=E_CmpFunc_Less,stencilFunc=E_CmpFunc_Always;E_StencilOp fail=E_StencilOp_Keep,depthFail=E_StencilOp_Keep,pass=E_StencilOp_Keep;U32 reference=0,readMask=255,writeMask=255;};
struct Pixel {F32 depth=.75f;U8 stencil=0x35;U32 colour=0xff203040;};
static void oracle(Pixel& pixel,const State& state,E_SurfaceFormat attachment,F32 z,U32 colour){
    const bool stencil=attachment==E_SurfaceFormat_D24S8&&state.stencil,depth=attachment!=E_SurfaceFormat_Unknown&&state.depth;
    const bool stencilPass=!stencil||compare(state.stencilFunc,F32(state.reference&state.readMask&255),F32(pixel.stencil&state.readMask));
    const bool depthPass=!depth||compare(state.depthFunc,z,pixel.depth);
    if(stencil){const U8 result=operation(!stencilPass?state.fail:!depthPass?state.depthFail:state.pass,pixel.stencil,state.reference);pixel.stencil=U8((pixel.stencil&~state.writeMask)|(result&state.writeMask));}
    if(stencilPass&&depthPass){pixel.colour=colour;if(depth&&state.write)pixel.depth=z;}
}
struct Fixture {
    C_MetalDevice device;C_VertexShader vs;C_PixelShader ps;C_VertexBuffer vertices;C_VertexDeclaration declaration;bool inScene=false;
    Fixture(){require(device.Create(),"Depth cache GPU device failed");std::string error;auto bytes=shader(true);require(device.CreateVertexShader(U32(bytes.size()),bytes.data(),0,&vs,nullptr,&error),error.c_str());bytes=shader(false);require(device.CreatePixelShader(U32(bytes.size()),bytes.data(),0,&ps,nullptr,&error),error.c_str());C_VertexFormat format;format.AddElement(0,0,E_VertexElementType_F32_2,E_VertexElementUsage_Position,0);require(device.CreateVertexDeclaration(format,&declaration)&&device.CreateVertexBuffer(32,0,&vertices),"Depth cache geometry failed");const F32 xy[]={-1,1,1,1,-1,-1,1,-1};auto map=vertices.Lock();require(map,"Depth cache geometry lock failed");std::memcpy(map,xy,sizeof(xy));vertices.Unlock();}
    ~Fixture(){auto& c=device.GetContext();if(inScene){try{End();}catch(...){}}c.SetColourSurface(0,nullptr);c.SetDepthStencilSurface(nullptr);c.SetVertexShader(nullptr);c.SetPixelShader(nullptr);c.SetVertexDeclaration(nullptr);c.SetVertexBuffer(0,nullptr,0,0);device.Destroy(&vertices);device.Destroy(&declaration);device.Destroy(&vs);device.Destroy(&ps);device.Destroy();}
    void Begin(){device.GetContext().BeginScene();inScene=true;}
    void End(){device.GetContext().EndScene();inScene=false;}
    void CheckPixels(C_Surface& colour,const std::array<Pixel,16*8>& expected){auto map=colour.LockRect(16);require(map.pBits,"Depth cache GPU readback failed");bool valid=true;for(U32 y=0;y<8;++y)for(U32 x=0;x<16;++x){U32 value=0;std::memcpy(&value,map.pBits+y*map.pitch+x*4,4);const U32 wanted=expected[y*16+x].colour;for(U32 shift=0;shift<32;shift+=8)valid&=std::abs(S32((value>>shift)&255)-S32((wanted>>shift)&255))<=1;}colour.UnlockRect();require(valid,"Alternating depth/stencil/absence/clear/MSAA states changed actual pixels or stencil results");}
    void Draw(const State& s,F32 z,U32 colour){auto& c=device.GetContext();c.SetVertexShader(&vs);c.SetPixelShader(&ps);c.SetVertexBuffer(0,&vertices,0,8,&declaration);c.SetCullMode(E_CullMode_None);c.SetBlendEnable(E_Bool_False);c.SetColourWriteEnable(15);c.SetAlphaTestEnable(E_Bool_False);c.SetAlphaToMaskEnable(E_Bool_False);c.SetDepthEnable(s.depth?E_Bool_True:E_Bool_False);c.SetDepthWriteEnable(s.write?E_Bool_True:E_Bool_False);c.SetDepthFunc(s.depthFunc);c.SetStencilEnable(s.stencil?E_Bool_True:E_Bool_False);C_StencilState stencil;stencil.SetStencilFunc(s.stencilFunc);stencil.SetStencilFail(s.fail);stencil.SetStencilDepthFail(s.depthFail);stencil.SetStencilDepthPass(s.pass);stencil.SetStencilMask(s.readMask);stencil.SetStencilWriteMask(s.writeMask);stencil.SetStencilRef(s.reference);c.SetStencilState(stencil);const F32 position[]={z,0,0,0},rgba[]={F32((colour>>16)&255)/255,F32((colour>>8)&255)/255,F32(colour&255)/255,F32(colour>>24)/255};c.SetVertexShaderConstantF(0,position,1);c.SetPixelShaderConstantF(0,rgba,1);c.DrawPrimitive(E_PrimitiveType_TriangleStrip,0,4);}
    void Check(E_SurfaceFormat attachment,E_Multisample samples,const State& state,bool partial){
        auto& c=device.GetContext();C_Surface colour,depth;require(device.CreateSurface(16,8,E_Flags_Colour,E_SurfaceFormat_A8R8G8B8,samples,nullptr,&colour),"Depth cache colour target failed");if(attachment!=E_SurfaceFormat_Unknown)require(device.CreateSurface(16,8,E_Flags_DepthStencil,attachment,samples,nullptr,&depth),"Depth cache depth target failed");
        c.SetColourSurface(0,&colour);c.SetDepthStencilSurface(attachment==E_SurfaceFormat_Unknown?nullptr:&depth);c.SetViewport(C_Viewport(0,0,16,8));Begin();U32 flags=E_Clear_Colour;if(attachment!=E_SurfaceFormat_Unknown)flags|=E_Clear_Depth;if(attachment==E_SurfaceFormat_D24S8)flags|=E_Clear_Stencil;const U8 initialStencil=state.pass==E_StencilOp_Incr||state.pass==E_StencilOp_IncrSat?255:state.pass==E_StencilOp_Decr||state.pass==E_StencilOp_DecrSat?0:0x35;c.Clear(flags,0xff203040,.75f,initialStencil);std::array<Pixel,16*8> expected{};for(auto& pixel:expected)pixel.stencil=initialStencil;
        Draw(state,.25f,0xffe04020);for(auto& pixel:expected)oracle(pixel,state,attachment,.25f,0xffe04020);
        if(partial){c.SetViewport(C_Viewport(0,0,8,8));c.Clear(flags,0xff102030,.5f,0xa6);c.SetViewport(C_Viewport(0,0,16,8));for(U32 y=0;y<8;++y)for(U32 x=0;x<8;++x){auto& pixel=expected[y*16+x];pixel.colour=0xff102030;if(attachment!=E_SurfaceFormat_Unknown)pixel.depth=.5f;if(attachment==E_SurfaceFormat_D24S8)pixel.stencil=0xa6;}}
        // Reference is dynamic, not an immutable cache property. Exercise both
        // reference-only alternation and all operation/mask settings below.
        State changed=state;changed.reference^=1;Draw(changed,.625f,0xff2040e0);for(auto& pixel:expected)oracle(pixel,changed,attachment,.625f,0xff2040e0);
        if(attachment==E_SurfaceFormat_D24S8){
            End();CheckPixels(colour,expected);Begin();
            // Probe the actual resulting stencil, independently of final color.
            // Each half has one uniform expected value after the full-screen draws.
            for(U32 half=0;half<2;++half){State probe;probe.depth=false;probe.write=false;probe.stencil=true;probe.stencilFunc=E_CmpFunc_Equal;probe.reference=expected[half*8].stencil;c.SetScissorEnable(E_Bool_True);c.SetScissorRect(C_ScissorRect{S32(half*8),0,S32(half*8+8),8});Draw(probe,.5f,0xff40d080);for(U32 y=0;y<8;++y)for(U32 x=half*8;x<half*8+8;++x)oracle(expected[y*16+x],probe,attachment,.5f,0xff40d080);}
            c.SetScissorEnable(E_Bool_False);
        }
        End();CheckPixels(colour,expected);c.SetColourSurface(0,nullptr);c.SetDepthStencilSurface(nullptr);device.Destroy(&colour);device.Destroy(&depth);
    }
    void Benchmark(){auto& c=device.GetContext();C_Surface colour,depth;require(device.CreateSurface(16,8,E_Flags_Colour,E_SurfaceFormat_A8R8G8B8,E_Multisample_None,nullptr,&colour)&&device.CreateSurface(16,8,E_Flags_DepthStencil,E_SurfaceFormat_D24S8,E_Multisample_None,nullptr,&depth),"Benchmark targets failed");c.SetColourSurface(0,&colour);c.SetDepthStencilSurface(&depth);Begin();c.Clear(E_Clear_Colour|E_Clear_Depth|E_Clear_Stencil,0xff000000,.75f,0);State state;state.write=false;state.depthFunc=E_CmpFunc_Greater;Draw(state,.875f,0xff4080c0);std::vector<double> times;for(U32 trial=0;trial<5;++trial){auto start=std::chrono::steady_clock::now();for(U32 draw=0;draw<2000;++draw)Draw(state,.875f,0xff4080c0);times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());}End();require(device.WaitIdle(),"Depth cache benchmark GPU completion failed");std::sort(times.begin(),times.end());std::cout<<"depth_state_encode_median_ms="<<times[2]<<" draws=2000 trials=5 (CPU encoding only)\n";c.SetColourSurface(0,nullptr);c.SetDepthStencilSurface(nullptr);device.Destroy(&colour);device.Destroy(&depth);}
};
struct Counts{unsigned long long draw=0,clear=0,hits=0,attempts=0,created=0,failures=0,last=0,map=0;size_t entries=0;};
static Counts run(U32 repeats,bool benchmark){Capture capture;{
    Fixture f;std::vector<State> states(1);State state;state.depthFunc=E_CmpFunc_Greater;states.push_back(state);state.write=false;states.push_back(state);state.depth=false;states.push_back(state);state.stencil=true;state.stencilFunc=E_CmpFunc_Never;state.fail=E_StencilOp_Replace;state.reference=0xa5;states.push_back(state);state.stencilFunc=E_CmpFunc_Equal;state.readMask=0x0f;state.writeMask=0xf0;state.fail=E_StencilOp_Invert;state.depth=true;state.depthFunc=E_CmpFunc_Never;state.depthFail=E_StencilOp_Incr;states.push_back(state);state.depthFunc=E_CmpFunc_Always;state.pass=E_StencilOp_Decr;states.push_back(state);
    for(auto op:{E_StencilOp_Keep,E_StencilOp_Zero,E_StencilOp_Replace,E_StencilOp_IncrSat,E_StencilOp_DecrSat,E_StencilOp_Invert,E_StencilOp_Incr,E_StencilOp_Decr}){state.stencilFunc=E_CmpFunc_Always;state.pass=op;state.writeMask=255;states.push_back(state);}
    for(U32 repeat=0;repeat<repeats;++repeat)for(auto samples:{E_Multisample_None,E_Multisample_2,E_Multisample_4})for(auto attachment:{E_SurfaceFormat_Unknown,E_SurfaceFormat_D16,E_SurfaceFormat_D24S8})for(U32 i=0;i<states.size();++i)f.Check(attachment,samples,states[i],i%2);
    // Return A after each newly inserted B; enough exact mask combinations grow
    // the map repeatedly, then revisit A and B after that growth. Pixel oracles
    // remain the actual context path, not direct cache helper lookups.
    for(U32 repeat=0;repeat<repeats;++repeat)for(U32 mask=0;mask<40;++mask){State alternate;alternate.write=false;alternate.stencil=true;alternate.stencilFunc=E_CmpFunc_Always;alternate.readMask=mask;alternate.writeMask=255-mask;f.Check(E_SurfaceFormat_D24S8,E_Multisample_None,alternate,false);f.Check(E_SurfaceFormat_D24S8,E_Multisample_None,states[0],false);}
    if(benchmark)f.Benchmark();require(f.device.WaitIdle(),"Depth cache suite GPU completion failed");
    }const auto text=capture.Text();const auto start=text.find("blur2pc.depth_state_cache ");require(start!=std::string::npos,"Missing opt-in native depth-state summary");Counts c;const int fields=std::sscanf(text.c_str()+start,"blur2pc.depth_state_cache draw_requests=%llu clear_requests=%llu hits=%llu create_attempts=%llu created=%llu failures=%llu entries=%zu last_hits=%llu map_hits=%llu",&c.draw,&c.clear,&c.hits,&c.attempts,&c.created,&c.failures,&c.entries,&c.last,&c.map);require(fields==9,"Native depth-state summary could not be parsed");require(text.find("blur2pc.depth_state_cache ",start+1)==std::string::npos,"One context emitted multiple summaries");return c;}
int main(int argc,char** argv){try{const bool benchmark=argc==2&&std::string(argv[1])=="--benchmark";require(argc==1||benchmark,"Usage: depth_state_cache_test [--benchmark]");Env env;setenv("BLUR_MAC_DEPTH_STATE_STATS","1",1);const auto once=run(1,false),repeated=run(3,benchmark);std::cout<<"GPU state oracle PASS; once(draw="<<once.draw<<",clear="<<once.clear<<",created="<<once.created<<",entries="<<once.entries<<",hits="<<once.hits<<",last="<<once.last<<",map="<<once.map<<") repeated(draw="<<repeated.draw<<",clear="<<repeated.clear<<",created="<<repeated.created<<",entries="<<repeated.entries<<",hits="<<repeated.hits<<",last="<<repeated.last<<",map="<<repeated.map<<")\n";require(once.draw&&once.clear&&once.hits&&once.created&&once.failures==0&&once.created==once.entries&&once.created==once.attempts,"Native state creation/hit invariants failed");require(repeated.created==once.created&&repeated.entries==once.entries,"Repeated draw/clear/reference/attachment/MSAA sequence recreated immutable native states");require(repeated.draw>=once.draw*3&&repeated.clear==once.clear*3&&repeated.hits>once.hits,"Repeated original draws/clears were not observed by real cache diagnostics");require(repeated.hits+repeated.created==repeated.draw+repeated.clear,"Diagnostic requests did not partition into hits and successful creations");require(once.last&&once.map&&repeated.last&&repeated.map&&once.last+once.map==once.hits&&repeated.last+repeated.map==repeated.hits,"Real context last-entry/map A/B/A hit evidence is missing or inconsistent");setenv("BLUR_MAC_DEPTH_STATE_STATS","0",1);{Capture capture;{Fixture fixture;State state;fixture.Check(E_SurfaceFormat_D24S8,E_Multisample_None,state,true);require(fixture.device.WaitIdle(),"Disabled-statistics draw completion failed");}require(capture.Text().find("blur2pc.depth_state_cache")==std::string::npos,"Disabled state statistics produced output");}std::cout<<"Depth/stencil native cache: exact GPU pixels, absence, masks/ops, reference-only state, partial clears,1x/2x/4x, repeat/recreate and creation counts PASS; unique="<<once.created<<" hits="<<repeated.hits<<'\n';return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
