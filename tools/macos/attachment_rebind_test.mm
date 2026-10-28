// [Blur2PC][macOS] Real render-pass attachment identity, pixels and boundary evidence.
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
#include <thread>
#include <unistd.h>
#include <sys/stat.h>
using namespace Display2;
static void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<class F>static void rejects(F fn,const char* why){bool caught=false;try{fn();}catch(const std::logic_error&){caught=true;}require(caught,why);}
struct Capture {
    FILE* file=std::tmpfile();int saved=-1;
    Capture(){require(file,"Diagnostic file failed");std::fflush(stderr);saved=dup(STDERR_FILENO);require(saved>=0&&dup2(fileno(file),STDERR_FILENO)>=0,"Diagnostic redirect failed");}
    std::string Text(){std::fflush(stderr);struct stat info{};require(fstat(fileno(file),&info)==0,"Diagnostic size failed");std::string result(size_t(info.st_size),'\0');for(size_t at=0;at<result.size();){const auto n=pread(fileno(file),result.data()+at,result.size()-at,off_t(at));require(n>0,"Diagnostic read failed");at+=size_t(n);}return result;}
    ~Capture(){if(saved>=0){std::fflush(stderr);dup2(saved,STDERR_FILENO);close(saved);}if(file)std::fclose(file);}
};
struct Env {
    bool existed=false,tileExisted=false;std::string old,tileOld;
    Env(){if(const char* p=std::getenv("BLUR_MAC_RENDER_PASS_STATS")){existed=true;old=p;}if(const char* p=std::getenv("BLUR_MAC_TILE_PASS_REUSE")){tileExisted=true;tileOld=p;}}
    ~Env(){if(existed)setenv("BLUR_MAC_RENDER_PASS_STATS",old.c_str(),1);else unsetenv("BLUR_MAC_RENDER_PASS_STATS");if(tileExisted)setenv("BLUR_MAC_TILE_PASS_REUSE",tileOld.c_str(),1);else unsetenv("BLUR_MAC_TILE_PASS_REUSE");}
};
static std::vector<U8> shader(bool vertex,bool mrt=false){
    const std::string meta=vertex?R"({"parameters":{},"input_semantics":[[0,0,0]],"reflection":{"ubos":[{"binding":0,"block_size":16,"name":"VertexConstants"}]}})":R"({"parameters":{},"input_semantics":[],"reflection":{"ubos":[{"binding":0,"block_size":16,"name":"PixelConstants"}]}})";
    std::string source="#include <metal_stdlib>\nusing namespace metal;\n";
    source+=vertex?"struct I{float2 p[[attribute(0)]];};vertex float4 blur_main(I i[[stage_in]],constant float4& v[[buffer(0)]]){return float4(i.p,v.x,1);}":mrt?"struct O{float4 a[[color(0)]];float4 b[[color(1)]];};fragment O blur_main(constant float4& c[[buffer(0)]]){return {c,float4(1-c.rgb,1)};}":"fragment float4 blur_main(constant float4& c[[buffer(0)]]){return c;}";
    const std::string entry="blur_main";std::vector<U8> bytes(40);std::memcpy(bytes.data(),"BMSL",4);auto word=[&](U32 offset,U32 value){for(U32 i=0;i<4;++i)bytes[offset+i]=U8(value>>(i*8));};word(4,1);word(8,vertex?0:1);word(16,40);word(20,U32(meta.size()));word(24,U32(40+meta.size()));word(28,U32(source.size()));word(32,U32(40+meta.size()+source.size()));word(36,U32(entry.size()));bytes.insert(bytes.end(),meta.begin(),meta.end());bytes.insert(bytes.end(),source.begin(),source.end());bytes.insert(bytes.end(),entry.begin(),entry.end());word(12,U32(bytes.size()));return bytes;
}
struct Fixture {
    C_MetalDevice device;C_VertexShader vs;C_PixelShader ps,mrt;C_VertexBuffer vertices;C_VertexDeclaration declaration;bool active=false;
    Fixture(){require(device.Create(),"Rebind device failed");std::string error;auto bytes=shader(true);require(device.CreateVertexShader(U32(bytes.size()),bytes.data(),0,&vs,nullptr,&error),error.c_str());bytes=shader(false);require(device.CreatePixelShader(U32(bytes.size()),bytes.data(),0,&ps,nullptr,&error),error.c_str());bytes=shader(false,true);require(device.CreatePixelShader(U32(bytes.size()),bytes.data(),0,&mrt,nullptr,&error),error.c_str());C_VertexFormat format;format.AddElement(0,0,E_VertexElementType_F32_2,E_VertexElementUsage_Position,0);require(device.CreateVertexDeclaration(format,&declaration)&&device.CreateVertexBuffer(32,0,&vertices),"Rebind geometry failed");const F32 xy[]={-1,1,1,1,-1,-1,1,-1};auto p=vertices.Lock();require(p,"Rebind geometry map failed");std::memcpy(p,xy,sizeof(xy));vertices.Unlock();}
    ~Fixture(){auto& c=device.GetContext();if(active){try{End();}catch(...){}}for(U32 i=0;i<4;++i)c.SetColourSurface(i,nullptr);c.SetDepthStencilSurface(nullptr);c.SetVertexShader(nullptr);c.SetPixelShader(nullptr);c.SetVertexDeclaration(nullptr);c.SetVertexBuffer(0,nullptr,0,0);device.Destroy(&vertices);device.Destroy(&declaration);device.Destroy(&vs);device.Destroy(&ps);device.Destroy(&mrt);device.Destroy();}
    void Begin(){device.GetContext().BeginScene();active=true;}
    void End(){device.GetContext().EndScene();active=false;}
    void Draw(U32 colour,bool depth=false,F32 z=.5f,bool write=false,bool multiple=false){auto& c=device.GetContext();c.SetVertexShader(&vs);c.SetPixelShader(multiple?&mrt:&ps);c.SetVertexBuffer(0,&vertices,0,8,&declaration);c.SetCullMode(E_CullMode_None);c.SetBlendEnable(E_Bool_False);c.SetColourWriteEnable(15);c.SetAlphaTestEnable(E_Bool_False);c.SetAlphaToMaskEnable(E_Bool_False);c.SetDepthEnable(depth?E_Bool_True:E_Bool_False);c.SetDepthWriteEnable(write?E_Bool_True:E_Bool_False);c.SetDepthFunc(E_CmpFunc_Less);c.SetStencilEnable(E_Bool_False);const F32 position[]={z,0,0,0},rgba[]={F32((colour>>16)&255)/255,F32((colour>>8)&255)/255,F32(colour&255)/255,F32(colour>>24)/255};c.SetVertexShaderConstantF(0,position,1);c.SetPixelShaderConstantF(0,rgba,1);c.DrawPrimitive(E_PrimitiveType_TriangleStrip,0,4);}
    template<class F>void Pixels(C_Surface& surface,F expected){auto map=surface.LockRect(16);require(map.pBits,"Rebind GPU readback failed");bool valid=true;for(U32 y=0;y<8;++y)for(U32 x=0;x<16;++x){U32 actual=0;std::memcpy(&actual,map.pBits+y*map.pitch+x*4,4);const U32 wanted=expected(x,y);for(U32 shift=0;shift<32;shift+=8){const int d=int((actual>>shift)&255)-int((wanted>>shift)&255);valid&=d>=-1&&d<=1;}}surface.UnlockRect();require(valid,"Attachment rebinding changed colour/depth/partial-clear/resolve pixels");}
    void Same(C_Surface& colour,C_Surface* depth){auto& c=device.GetContext();C_Surface alias=colour,depthAlias;if(depth)depthAlias=*depth;c.SetColourSurface(0,&alias);for(U32 i=1;i<4;++i)c.SetColourSurface(i,nullptr);c.SetDepthStencilSurface(depth?&depthAlias:nullptr);}
    void Check(E_Multisample samples,E_SurfaceFormat depthFormat,bool repeat){
        auto& c=device.GetContext();C_Surface a,b,depth,copied;require(device.CreateSurface(16,8,E_Flags_Colour,E_SurfaceFormat_A8R8G8B8,samples,nullptr,&a)&&device.CreateSurface(16,8,E_Flags_Colour,E_SurfaceFormat_A8R8G8B8,samples,nullptr,&b),"Rebind colour surfaces failed");require(device.CreateSurface(16,8,E_Flags_Colour,E_SurfaceFormat_A8R8G8B8,E_Multisample_None,nullptr,&copied),"Copy destination failed");C_Surface* ds=nullptr;if(depthFormat!=E_SurfaceFormat_Unknown){require(device.CreateSurface(16,8,E_Flags_DepthStencil,depthFormat,samples,nullptr,&depth),"Rebind depth failed");ds=&depth;}
        Same(a,ds);c.SetScissorEnable(E_Bool_False);Begin();U32 clear=E_Clear_Colour;if(ds)clear|=E_Clear_Depth;if(depthFormat==E_SurfaceFormat_D24S8)clear|=E_Clear_Stencil;c.Clear(clear,0xff000000,.75f,0);Draw(0xffff0000,bool(ds),.25f,true);
        for(U32 i=0;i<4;++i){if(repeat)Same(a,ds);Draw(0xffff0000);}
        // Validation must still run for a live pass, before it is disturbed.
        C_Surface absent;rejects([&]{c.SetColourSurface(4,&a);},"Invalid MRT slot accepted");rejects([&]{c.SetColourSurface(0,&absent);},"Uncreated colour accepted");rejects([&]{c.SetDepthStencilSurface(&absent);},"Uncreated depth accepted");bool wrongThread=false;std::thread worker([&]{try{c.SetColourSurface(0,&a);}catch(const std::logic_error&){wrongThread=true;}});worker.join();require(wrongThread,"Identical rebind bypassed thread ownership");
        // Colour0's original viewport/scissor reset remains observable even when
        // its attachment identity is unchanged and an encoder is still live.
        c.SetViewport(C_Viewport(0,0,4,8));c.SetScissorEnable(E_Bool_True);c.SetScissorRect({0,0,4,8});c.SetColourSurface(0,&a);Draw(0xff00ff00);c.SetScissorEnable(E_Bool_False);
        c.SetViewport(C_Viewport(4,2,8,4));c.Clear(E_Clear_Colour,0xffffff00);c.SetViewport(C_Viewport(0,0,16,8));End();Pixels(a,[](U32 x,U32 y){return x>=4&&x<12&&y>=2&&y<6?0xffffff00u:0xff00ff00u;});
        // Real target changes retire the pass. A later read must keep A's old pixels.
        Same(b,ds);Begin();c.Clear(clear,0xff0000ff,.75f,0);Draw(0xff0000ff);for(U32 i=0;i<4;++i){if(repeat)Same(b,ds);Draw(0xff0000ff);}End();Pixels(b,[](U32,U32){return 0xff0000ffu;});Pixels(a,[](U32 x,U32 y){return x>=4&&x<12&&y>=2&&y<6?0xffffff00u:0xff00ff00u;});
        if(ds){Same(a,ds);Begin();c.Clear(E_Clear_Depth,0,.25f);c.SetDepthStencilSurface(nullptr);Draw(0xff0000ff,true,.5f);if(repeat)c.SetDepthStencilSurface(nullptr);c.SetDepthStencilSurface(ds);Draw(0xffff0000,true,.5f);End();Pixels(a,[](U32,U32){return 0xff0000ffu;});}
        Same(a,ds);Begin();Draw(0xffff8000);c.ResolveSurface(&a);if(repeat)Same(a,ds);c.SetScissorEnable(E_Bool_True);c.SetScissorRect({0,0,8,8});Draw(0xffff0000);c.SetScissorEnable(E_Bool_False);End();Pixels(a,[](U32 x,U32){return x<8?0xffff0000u:0xffff8000u;});
        Begin();c.CopySurface(&copied,&a);if(repeat)Same(a,ds);End();Pixels(copied,[](U32 x,U32){return x<8?0xffff0000u:0xffff8000u;});
        if(samples==E_Multisample_None){Same(a,ds);C_OcclusionQuery query;require(query.Create(c),"Rebind query failed");Begin();query.Begin();Draw(0xff808080);if(repeat)Same(a,ds);Draw(0xff808080);query.End();End();U64 count=0;require(query.GetResult(count)&&count==2*16*8,"Rebinding changed exact query samples");query.Destroy();
            // A genuine MRT bind/detach remains a pass boundary; earlier output
            // in the detached slot must survive later single-output rendering.
            c.SetColourSurface(1,&b);Begin();c.Clear(E_Clear_Colour,0xff000000);Draw(0xffff0000,false,.5f,false,true);if(repeat){c.SetColourSurface(1,&b);c.SetDepthStencilSurface(ds);}Draw(0xffff0000,false,.5f,false,true);c.SetColourSurface(1,nullptr);Draw(0xff00ff00);End();Pixels(a,[](U32,U32){return 0xff00ff00u;});Pixels(b,[](U32,U32){return 0xff00ffffu;});}
        Same(a,ds);c.SetColourSurface(0,nullptr);c.SetDepthStencilSurface(nullptr);device.Destroy(&a);device.Destroy(&b);device.Destroy(&depth);device.Destroy(&copied);
    }
};
struct Counts {unsigned long long passes=0,colour=0,depth=0,sameColour=0,sameDepth=0,avoidedColour=0,avoidedDepth=0;};
static Counts run(bool repeat,bool enabled,bool tile){if(tile)setenv("BLUR_MAC_TILE_PASS_REUSE","1",1);else unsetenv("BLUR_MAC_TILE_PASS_REUSE");setenv("BLUR_MAC_RENDER_PASS_STATS",enabled?"1":"0",1);Capture capture;{Fixture fixture;for(auto samples:{E_Multisample_None,E_Multisample_2,E_Multisample_4})for(auto depth:{E_SurfaceFormat_Unknown,E_SurfaceFormat_D16,E_SurfaceFormat_D24S8})fixture.Check(samples,depth,repeat);require(fixture.device.WaitIdle(),"Rebind final GPU completion failed");}const auto text=capture.Text();const auto start=text.find("blur2pc.render_pass_stats ");if(!enabled){require(start==std::string::npos,"Disabled render-pass diagnostics produced output");return {};}
    require(start!=std::string::npos&&text.find("blur2pc.render_pass_stats ",start+1)==std::string::npos,"Missing or repeated render-pass summary");Counts c;const int n=std::sscanf(text.c_str()+start,"blur2pc.render_pass_stats passes=%llu colour_setters=%llu depth_setters=%llu identical_colour=%llu identical_depth=%llu avoided_colour_end=%llu avoided_depth_end=%llu",&c.passes,&c.colour,&c.depth,&c.sameColour,&c.sameDepth,&c.avoidedColour,&c.avoidedDepth);require(n==7,"Render-pass summary parse failed");return c;
}
int main(){try{Env env;
    const auto ordinary=run(false,true,false),ordinaryRebound=run(true,true,false);
    require(ordinaryRebound.passes>ordinary.passes,"Default-off tile policy failed to preserve original setter boundaries");
    require(!ordinary.avoidedColour&&!ordinary.avoidedDepth&&!ordinaryRebound.avoidedColour&&!ordinaryRebound.avoidedDepth,"Default-off tile policy reported avoided encoder endings");
    const auto baseline=run(false,true,true),rebound=run(true,true,true);
    std::cout<<"GPU pixel/query/resolve/MRT/viewport oracle PASS; default_passes="<<ordinary.passes<<" default_rebound_passes="<<ordinaryRebound.passes<<" experimental_baseline_passes="<<baseline.passes<<" experimental_rebound_passes="<<rebound.passes<<" avoided_colour="<<rebound.avoidedColour<<" avoided_depth="<<rebound.avoidedDepth<<'\n';
    require(baseline.passes&&rebound.passes==baseline.passes,"Identical setters introduced additional experimental render passes");
    require(rebound.colour>baseline.colour&&rebound.depth>baseline.depth&&rebound.sameColour>baseline.sameColour&&rebound.sameDepth>baseline.sameDepth,"Actual repeated colour/depth setters were not observed");
    require(rebound.avoidedColour&&rebound.avoidedDepth,"Experimental live same-attachment encoder preservation was not observed");
    run(true,false,true);run(true,false,false);
    std::cout<<"Exact attachment rebinding: default-off original boundaries + explicit experimental reuse, unchanged pixels/query samples,1x/2x/4x resolves, depth detach, MRT, viewport/scissor resets, validation and disabled counters PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
