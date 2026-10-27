// [Blur2PC][macOS] Exact binding-time GPU snapshots using the original luminance-adaptation shader.
#include <display.h>
#include <display/context/Context.h>
#include <display/buffer/VertexDeclarations.h>
#include <render/shaders/Constants.h>
#include <system/serialise.h>
#include <system/serialise/macos/NativeFile.h>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>
static void require(bool value,const char* why){if(!value){std::cerr<<why<<std::endl;throw std::runtime_error(why);}}
static F32 half(U16 value){const U32 e=(value>>10)&31,m=value&1023;const F32 f=e?std::ldexp(F32(1024+m),S32(e)-25):std::ldexp(F32(m),-24);return value&0x8000?-f:f;}
static F32 read(const Display::C_RenderTexture2D& texture,Display::E_Format format){
    auto pixels=texture.GetTexture().LockRect2D(0,16);require(pixels.pBits,"Snapshot GPU pixels unavailable");F32 first=0;bool good=true;
    for(U32 y=0;y<texture.GetTexture().GetHeight();++y)for(U32 x=0;x<texture.GetTexture().GetWidth();++x){
        const U8* p=pixels.pBits+y*pixels.pitch+x*(format==Display::E_Format_A16B16G16R16F?8:4);F32 value;
        if(format==Display::E_Format_R32F)std::memcpy(&value,p,4);else if(format==Display::E_Format_A16B16G16R16F){U16 bits;std::memcpy(&bits,p,2);value=half(bits);}else value=p[2]/255.f;
        if(!x&&!y)first=value;good&=std::isfinite(value)&&std::abs(value-first)<1.e-6f;
    }
    texture.GetTexture().UnlockRect2D(0);require(good,"Snapshot draw did not cover exact non-square extent");return first;
}
static std::vector<U8> cubeShader(){
    const std::string meta=R"({"parameters":{},"input_semantics":[],"reflection":{"ubos":[{"binding":0,"block_size":16,"name":"SampleDirection"}],"textures":[{"binding":1,"type":"textureCube"}]}})";
    const std::string source="#include <metal_stdlib>\nusing namespace metal;fragment float4 blur_main(constant float4& d[[buffer(0)]],texturecube<float> t[[texture(1)]],sampler s[[sampler(1)]]){return t.sample(s,d.xyz,level(d.w));}";
    const std::string entry="blur_main";std::vector<U8> bytes(40);
    auto word=[&](U32 at,U32 value){for(U32 i=0;i<4;++i)bytes[at+i]=U8(value>>(i*8));};
    std::memcpy(bytes.data(),"BMSL",4);word(4,1);word(8,1);word(16,40);word(20,meta.size());word(24,40+meta.size());word(28,source.size());word(32,40+meta.size()+source.size());word(36,entry.size());
    bytes.insert(bytes.end(),meta.begin(),meta.end());bytes.insert(bytes.end(),source.begin(),source.end());bytes.insert(bytes.end(),entry.begin(),entry.end());word(12,bytes.size());return bytes;
}
static void cube(Display::C_Device& device,bool contextRoute){
    using namespace Display;C_RenderTextureCube original;require(device.CreateRenderTextureCube(8,3,E_Usage_Colour,E_Format_A8R8G8B8,E_Multisample_None,nullptr,&original),"Cube snapshot allocation failed");
    for(U32 face=0;face<6;++face)for(U32 level=0;level<3;++level){auto p=original.GetTexture().LockRectCube(face,level);require(p.pBits,"Cube snapshot mip map failed");for(U32 y=0;y<original.GetTexture().GetHeight(level);++y)for(U32 x=0;x<original.GetTexture().GetWidth(level);++x){U8* pixel=p.pBits+y*p.pitch+x*4;pixel[0]=10+face;pixel[1]=20+level*40;pixel[2]=40+face*20+level*3;pixel[3]=255;}original.GetTexture().UnlockRectCube(face,level);}
    device.SetColourDepthStencilSurface(&original.GetSurface(3,2),nullptr);
    // No scene exists: this must create/submit its own real copy command, retain
    // all faces/mips, and remain valid after the source wrapper is destroyed.
    Display::C_Context upper;upper.m_Context.Bind(device.GetDisplay2());if(contextRoute)upper.SetTexture(1,&original.GetTexture());else device.SetTexture(1,&original.GetTexture());device.SetColourDepthStencilSurface();device.Destroy(&original);
    Display2::C_PixelShader shader;auto bytes=cubeShader();std::string error;require(device.GetDisplay2().CreatePixelShader(bytes.size(),bytes.data(),0,&shader,nullptr,&error),error.c_str());
    const F32 direction[6][3]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    auto& context=device.GetDisplay2().GetContext();device.SetTexture(0,nullptr);device.SetTextureAddress(1,E_TextureAddress_Clamp);device.SetTextureFilter(1,E_TextureFilter_Point);device.SetDepthEnable(E_Bool_False);device.SetDepthWriteEnable(E_Bool_False);device.SetAlphaTestEnable(E_Bool_False);device.SetStencilEnable(E_Bool_False);device.SetCullMode(E_CullMode_None);device.SetBlendMode(E_BlendMode_Solid);device.SetColourWriteEnable(E_ColourWriteEnable_All);
    Display2::C_VertexFormat format;format.AddElement(0,0,Display2::E_VertexElementType_F32_2,Display2::E_VertexElementUsage_Position,0);format.AddElement(0,8,Display2::E_VertexElementType_F32_2,Display2::E_VertexElementUsage_TexCoord,0);
    Display2::C_VertexDeclaration declaration;Display2::C_VertexBuffer vertices;require(device.GetDisplay2().CreateVertexDeclaration(format,&declaration)&&device.GetDisplay2().CreateVertexBuffer(64,0,&vertices),"Cube snapshot geometry allocation failed");
    const F32 quad[]={-1,1,0,0,1,1,1,0,-1,-1,0,1,1,-1,1,1};void* mapped=vertices.Lock();require(mapped,"Cube snapshot geometry map failed");std::memcpy(mapped,quad,sizeof(quad));vertices.Unlock();context.SetVertexBuffer(0,&vertices,0,16,&declaration);
    for(bool gamma:{false,true})for(U32 face=0;face<6;++face)for(U32 level=0;level<3;++level){
        // The lower BeginScene preserves explicit bindings; the upper game BeginScene intentionally resets them.
        context.BeginScene();device.SetColourDepthStencilSurface(device.GetNativeColourSurface(),nullptr);C_Viewport viewport;viewport.m_X=viewport.m_Y=0;viewport.m_W=32;viewport.m_H=16;device.SetViewport(viewport);device.SetVertexShader(&VertexShader_XY::m_VertexShader);device.SetPixelShader(&shader);device.SetTextureGamma(1,gamma?E_Bool_True:E_Bool_False);
        const F32 sample[]={direction[face][0],direction[face][1],direction[face][2],F32(level)};context.SetPixelShaderConstantF(0,sample,1);context.DrawPrimitive(Display2::E_PrimitiveType_TriangleStrip,0,4);context.EndScene();
        auto p=device.GetNativeColourSurface()->LockRect(16);require(p.pBits,"Cube snapshot sampled pixels unavailable");U8 expected[]={U8(10+face),U8(20+level*40),U8(40+face*20+level*3),255};
        if(gamma)for(U32 c=0;c<3;++c){const F32 v=expected[c]/255.f;expected[c]=U8(std::lround((v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f))*255));}
        bool good=true;for(U32 y=0;y<16;++y)for(U32 x=0;x<32;++x)for(U32 c=0;c<4;++c)good&=std::abs(S32(p.pBits[y*p.pitch+x*4+c])-S32(expected[c]))<=1;device.GetNativeColourSurface()->UnlockRect();require(good,"Cube snapshot lost a face/mip, source lifetime, or sRGB view");
    }
    device.SetTexture(1,nullptr);device.GetDisplay2().Destroy(&vertices);device.GetDisplay2().Destroy(&declaration);device.GetDisplay2().Destroy(&shader);
}
static void elapsed(Display::C_Device& device,Display::C_PixelShader& shader,Display::C_PixelShaderParameter parameter,const Display::C_Texture2D& current){
    using namespace Display;F32 results[2]={};U32 run=0;
    for(U32 frames:{60U,144U}){
        C_RenderTexture2D target;require(device.CreateRenderTexture2D(1,1,1,E_Usage_Colour,E_Format_R32F,E_Multisample_None,nullptr,&target),"Elapsed-time target failed");
        for(U32 frame=0;frame<frames;++frame){
            require(device.BeginScene(),"Elapsed-time scene failed");device.SetColourDepthStencilSurface(&target.GetSurface(),nullptr);C_Viewport viewport;viewport.m_W=viewport.m_H=1;device.SetViewport(viewport);device.SetDepthEnable(E_Bool_False);device.SetAlphaTestEnable(E_Bool_False);device.SetStencilEnable(E_Bool_False);device.SetBlendMode(E_BlendMode_Solid);
            if(!frame)device.Clear(E_Clear_Colour,Maths::RGBA(0xff404040));
            device.SetVertexShader(&VertexShader_XY::m_VertexShader);device.SetPixelShader(&shader);const F32 settings[]={5.f/frames,0,0,0};device.SetPixelShaderParameterF(parameter,settings);
            device.SetTexture(0,&current);device.SetTexture(1,&target.GetTexture());device.SetTextureGamma(0,E_Bool_False);device.SetTextureGamma(1,E_Bool_False);device.SetTextureAddress(0,E_TextureAddress_Clamp);device.SetTextureAddress(1,E_TextureAddress_Clamp);device.SetTextureFilter(0,E_TextureFilter_Point);device.SetTextureFilter(1,E_TextureFilter_Point);Quad::DrawFullScreen();device.EndScene();
        }
        results[run++]=read(target,E_Format_R32F);device.SetTexture(1,nullptr);device.SetColourDepthStencilSurface();device.Destroy(&target);
    }
    const F32 expected=.875f+(64.f/255-.875f)*std::pow(.98f,150.f);
    require(std::abs(results[0]-expected)<3.e-5f&&std::abs(results[1]-expected)<3.e-5f&&std::abs(results[0]-results[1])<3.e-5f,"Original HDR adaptation changed with60/144Hz for identical elapsed time");
}
// [Blur2PC] The same upper pixel contract must cover any MRT slot and explicit target/binding order.
static void contextTargets(Display::C_Device& device,Display::C_PixelShader& shader,Display::C_PixelShaderParameter parameter,const Display::C_Texture2D& current){
    using namespace Display;C_Context upper;upper.m_Context.Bind(device.GetDisplay2());C_RenderTexture2D output,feedback,depth;
    require(device.CreateRenderTexture2D(8,4,1,E_Usage_Colour,E_Format_R32F,E_Multisample_None,nullptr,&output)&&device.CreateRenderTexture2D(8,4,1,E_Usage_Colour,E_Format_R32F,E_Multisample_None,nullptr,&feedback),"Context MRT feedback allocation failed");
    const F32 settings[]={1,0,0,0},initial=64.f/255,rate=1-std::pow(.98f,30.f),expected=initial+(.875f-initial)*rate;
    auto setup=[&]{device.SetColourDepthStencilSurface(&output.GetSurface(),nullptr);C_Viewport viewport;viewport.m_W=8;viewport.m_H=4;device.SetViewport(viewport);device.SetDepthEnable(E_Bool_False);device.SetDepthWriteEnable(E_Bool_False);device.SetAlphaTestEnable(E_Bool_False);device.SetStencilEnable(E_Bool_False);device.SetCullMode(E_CullMode_None);device.SetBlendMode(E_BlendMode_Solid);device.SetColourWriteEnable(E_ColourWriteEnable_All);device.SetVertexShader(&VertexShader_XY::m_VertexShader);device.SetPixelShader(&shader);device.SetPixelShaderParameterF(parameter,settings);upper.SetTexture(0,&current);device.SetTextureGamma(0,E_Bool_False);device.SetTextureGamma(1,E_Bool_False);device.SetTextureAddress(0,E_TextureAddress_Clamp);device.SetTextureAddress(1,E_TextureAddress_Clamp);device.SetTextureFilter(0,E_TextureFilter_Point);device.SetTextureFilter(1,E_TextureFilter_Point);};
    for(U32 target=1;target<4;++target){require(device.BeginScene(),"Context MRT feedback scene failed");setup();device.SetColourSurface(target,&feedback.GetSurface());device.Clear(E_Clear_Colour,Maths::RGBA(0xff404040));upper.SetTexture(1,&feedback.GetTexture());Quad::DrawFullScreen();device.EndScene();require(std::abs(read(output,E_Format_R32F)-expected)<2.e-6f,"Upper Context did not snapshot the actual nonzero MRT colour attachment");}
    require(device.BeginScene(),"Context late-target scene failed");setup();upper.SetTexture(1,&feedback.GetTexture());device.SetColourDepthStencilSurface(&feedback.GetSurface(),nullptr);C_Viewport viewport;viewport.m_W=8;viewport.m_H=4;device.SetViewport(viewport);device.Clear(E_Clear_Colour,Maths::RGBA(0xff404040));bool lateRejected=false;try{Quad::DrawFullScreen();}catch(const std::logic_error& error){lateRejected=std::string(error.what()).find("simultaneously be sampled")!=std::string::npos;}require(lateRejected,"Target change silently resnapshotted an older nonfeedback binding");upper.SetTexture(1,&feedback.GetTexture());Quad::DrawFullScreen();device.EndScene();require(std::abs(read(feedback,E_Format_R32F)-expected)<2.e-6f,"Explicit upper Context rebind failed to snapshot newest target contents after strict rejection");
    require(device.CreateRenderTexture2D(8,4,1,E_Usage_DepthStencil,E_Format_D24S8,E_Multisample_None,nullptr,&depth),"Context strict depth feedback allocation failed");
    require(device.BeginScene(),"Context depth feedback scene failed");setup();device.SetDepthStencilSurface(&depth.GetSurface());upper.SetTexture(1,&depth.GetTexture());bool depthRejected=false;try{Quad::DrawFullScreen();}catch(const std::logic_error& error){depthRejected=std::string(error.what()).find("simultaneously be sampled")!=std::string::npos;}require(depthRejected,"Upper colour adapter incorrectly substituted/copied an active depth attachment");upper.SetTexture(1,nullptr);device.SetColourDepthStencilSurface();device.EndScene();device.Destroy(&depth);device.Destroy(&feedback);device.Destroy(&output);
}
int main(int argc,char**argv){try{
    using namespace Display;require(argc==2,"Expected source-built shader resource root");auto& fs=Sys::C_FileSystem::Instance();Sys::SetDefaultFileSystem(&fs);require(fs.SetWorkingDirectory(argv[1]),"Snapshot shader filesystem unavailable");
    C_Device device;C_DeviceParams params;params.m_Width=32;params.m_Height=16;require(device.Create(params)&&device.CreateVolatile(params),"Snapshot native device creation failed");
    CreateVertexDeclarations();CreateVertexShaders();CreatePixelShaders();C_PixelShader shader;require(Display::CreatePixelShaderFromFile("Shaders/psLuminanceAdapted",0,&shader),"Original luminance-adaptation shader unavailable");auto parameter=shader.GetParameter(CP_RENDER_POSTPROCESS_PARAMETERS_0,"g_vSettings");
    C_Texture2D current;require(device.CreateTexture2D(1,1,1,E_Usage_Managed,E_Format_R32F,&current),"Current luminance allocation failed");auto bits=current.Lock(0);require(bits.pBits,"Current luminance map failed");const F32 currentValue=.875f;std::memcpy(bits.pBits,&currentValue,4);current.Unlock(0);
    auto& context=device.GetDisplay2().GetContext();device.SetCullMode(E_CullMode_None);device.SetDepthEnable(E_Bool_False);device.SetAlphaTestEnable(E_Bool_False);device.SetStencilEnable(E_Bool_False);device.SetBlendMode(E_BlendMode_Solid);
    const F32 settings[]={1,0,0,0},rate=1-std::pow(.98f,30.f);
    Display::C_Context upper;upper.m_Context.Bind(device.GetDisplay2());
    for(bool contextRoute:{false,true})for(auto format:{E_Format_R32F,E_Format_A16B16G16R16F,E_Format_A8R8G8B8})for(auto samples:{E_Multisample_None,E_Multisample_2,E_Multisample_4}){
        C_RenderTexture2D target;require(device.CreateRenderTexture2D(8,4,1,E_Usage_Colour,format,samples,nullptr,&target),"Snapshot colour format/sample allocation failed");
        auto bind=[&]{device.SetColourDepthStencilSurface(&target.GetSurface(),nullptr);C_Viewport viewport;viewport.m_X=viewport.m_Y=0;viewport.m_W=8;viewport.m_H=4;device.SetViewport(viewport);device.SetVertexShader(&VertexShader_XY::m_VertexShader);device.SetPixelShader(&shader);device.SetPixelShaderParameterF(parameter,settings);device.SetTexture(0,&current);device.SetTextureGamma(0,E_Bool_False);device.SetTextureGamma(1,E_Bool_False);device.SetTextureAddress(0,E_TextureAddress_Clamp);device.SetTextureAddress(1,E_TextureAddress_Clamp);device.SetTextureFilter(0,E_TextureFilter_Point);device.SetTextureFilter(1,E_TextureFilter_Point);};
        require(device.BeginScene(),"Snapshot clear scene failed");bind();device.Clear(E_Clear_Colour,Maths::RGBA(0xff404040));device.EndScene();const F32 initial=read(target,format);
        require(device.BeginScene(),"Snapshot recurrence scene failed");bind();if(contextRoute)upper.SetTexture(1,&target.GetTexture());else device.SetTexture(1,&target.GetTexture());Quad::DrawFullScreen();Quad::DrawFullScreen();device.EndScene();const F32 first=read(target,format);const F32 tolerance=format==E_Format_A8R8G8B8?1.1f/255:format==E_Format_A16B16G16R16F?.001f:2.e-6f;
        require(std::abs(first-(initial+(currentValue-initial)*rate))<=tolerance,"Snapshot was not frozen at binding time across repeated writes");
        require(device.BeginScene(),"Snapshot refreshed recurrence scene failed");bind();if(contextRoute)upper.SetTexture(1,&target.GetTexture());else device.SetTexture(1,&target.GetTexture());Quad::DrawFullScreen();device.EndScene();const F32 next=read(target,format);
        require(std::abs(next-(first+(currentValue-first)*rate))<=tolerance,"Rebinding did not snapshot the newest actual prior contents");
        require(device.BeginScene(),"Raw feedback rejection scene failed");bind();context.SetTexture(1,&target.GetTexture());bool rejected=false;try{Quad::DrawFullScreen();}catch(const std::logic_error&){rejected=true;}require(rejected,"Raw lower feedback guard was disabled");device.SetTexture(1,nullptr);device.SetColourDepthStencilSurface();device.EndScene();device.Destroy(&target);
    }
    contextTargets(device,shader,parameter,current);elapsed(device,shader,parameter,current);cube(device,false);cube(device,true);device.SetTexture(0,nullptr);device.SetTexture(1,nullptr);device.Destroy(&current);device.Destroy(&shader);DestroyPixelShaders();DestroyVertexShaders();require(device.GetDisplay2().WaitIdle(),"Snapshot GPU completion failed");device.DestroyVolatile();device.Destroy();Sys::SetDefaultFileSystem(nullptr);
    std::cout<<"Original adaptation shader exact prior-content binding snapshots across repeated/refreshed writes, R32F/RGBA16F/BGRA8 8x4 with1x/2x/4x MSAA resolves,60/144Hz elapsed-time equivalence, all6cube faces/3mips with linear+sRGB views/source destroyed/no-scene copies, both upper Device/Context routes, MRT1..3, late-target strict rejection/explicit-rebind retry, depth rejection, unchanged lower feedback rejection and ownership teardown PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
