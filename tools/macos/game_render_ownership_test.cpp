// [Blur2PC][macOS] Actual GameApp OnRender ownership spans real sampler-bound Metal output, including false boot flag and exceptions.
#include "../../code/amax/game/stdafx.h"
// Fixture-only inspection of the existing owner pointer; production layout/API is unchanged.
#define private public
#include <GameApp.h>
#undef private
#include <Game/SystemLayer.h>
#include <Game/AmaxCam/CameraSystem.h>
#include <system/artery/Artery.h>
#include <system/scheduler/jobqueue.h>
#define FX_ALLOW_INCLUDE
#include <display/context/Context.h>
#include <fx/effect.h>
#include <fx/System.h>
#include <fx/FxContext.h>
#include <fx/ShaderImpl.h>
#include <fx/Shared.h>
#include <fx/RenderMutex.h>
#include <fx/Manager.h>
#include <system/serialise.h>
#include <system/serialise/C_FileBlock.h>
#include <system/serialise/BinarySerialise.h>
#include <atomic>
#include <filesystem>
#include <chrono>
#include <thread>
#include <memory>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
static void require(bool value,const char* message){if(!value){std::cerr<<message<<std::endl;throw std::runtime_error(message);}}
static std::vector<U8> shader(bool vertex,bool texture=false){
    std::string metadata=vertex?R"({"parameters":{},"input_semantics":[[0,0,0]],"reflection":{"ubos":[]}})":R"({"parameters":{"colour":[0,1]},"input_semantics":[],"reflection":{"ubos":[{"name":"PixelConstants","binding":0,"block_size":16}]}})";
    std::string source=vertex?
        "#include <metal_stdlib>\nusing namespace metal;struct Input{float3 position[[attribute(0)]];};struct Output{float4 p[[position]];};vertex Output blur_main(Input v[[stage_in]]){return {float4(v.position,1)};}\n":
        "#include <metal_stdlib>\nusing namespace metal;fragment float4 blur_main(constant float4& colour[[buffer(0)]]){return colour;}\n";
    if(texture&&!vertex){metadata=R"({"parameters":{},"input_semantics":[],"reflection":{"ubos":[],"textures":[{"binding":0,"type":"sampler2D"}]}})";source="#include <metal_stdlib>\nusing namespace metal;fragment float4 blur_main(texture2d<float> image[[texture(0)]],sampler state[[sampler(0)]]){return image.sample(state,float2(.5));}\n";}
    const std::string entry="blur_main";std::vector<U8> bytes(40);std::memcpy(bytes.data(),"BMSL",4);
    auto put=[&](U32 p,U32 v){for(U32 i=0;i<4;++i)bytes[p+i]=U8(v>>(8*i));};put(4,1);put(8,vertex?0:1);put(16,40);put(20,metadata.size());put(24,40+metadata.size());put(28,source.size());put(32,40+metadata.size()+source.size());put(36,entry.size());
    bytes.insert(bytes.end(),metadata.begin(),metadata.end());bytes.insert(bytes.end(),source.begin(),source.end());bytes.insert(bytes.end(),entry.begin(),entry.end());put(12,bytes.size());return bytes;
}
static void function(FX::Function& f,bool vertex){auto data=shader(vertex);f.cMicrocode=data.size();f.Microcode=new U8[data.size()];memcpy(f.Microcode,data.data(),data.size());}
static void effect(FX::Effect& e){
    e.Name=SharedString::GetString("native-fixture");e.ParameterData.Resize(4);const F32 green[]={0,1,0,1};memcpy(&e.ParameterData[0],green,sizeof(green));
    e.Parameters.Resize(1);auto& p=e.Parameters[0];p.Name=SharedString::GetString("colour");p.Type=FX::NGLFX_PARAM_VECTOR;p.Data=&e.ParameterData[0];p.Size=4;p.bShared=p.bExported=0;
    e.Techniques.Resize(1);auto& t=e.Techniques[0];t.hTechnique=0;t.Passes.Resize(1);auto& pass=t.Passes[0];memset(&pass.RenderStates,0,sizeof(pass.RenderStates));
    pass.SharedBindingUsage.ClearBits();pass.SharedBindingMask.ClearBits();
    function(pass.VertexShader,true);function(pass.PixelShader,false);
    pass.PixelShader.Bindings.Resize(1);auto& b=pass.PixelShader.Bindings[0];b.Name="colour";b.ParameterIdx=0;b.Count=1;b.Handle=0;b.Type=FX::NGLFX_BIND_FLOAT;b.Source=FX::NGLFX_BIND_PARAMETER;b.Shared=b.Exported=b.Offset=0;
}
// Independent PCFX v9 field layout; the original writer supplies block framing only.
static void fileEffect(Sys::C_FileSystem& fs,const char* name,bool shared,bool texture=false,bool invalid=false,bool d3d9=false){
    std::vector<U8> payload;auto word=[&](U32 value){for(int i=0;i<4;++i)payload.push_back(U8(value>>(i*8)));};
    auto string=[&](const char* value){word(strlen(value));payload.insert(payload.end(),value,value+strlen(value));};
    word(9);string(name);word(1);string("default");word(1);string("pass");
    for(int i=0;i<35;++i)word(0); // 35 PC render-state words, no native ABI bytes.
    for(bool vertex:{true,false}){
        auto blob=shader(vertex,texture);
        if(d3d9){const std::vector<U32> tokens=vertex?std::vector<U32>{0xfffe0200,0x0200001f,0x80000000,0x900f0000,0x02000001,0xc00f0000,0x90e40000,0x0000ffff}:std::vector<U32>{0xffff0200,0x02000001,0x800f0800,0xa0e40000,0x0000ffff};blob.resize(tokens.size()*4);memcpy(blob.data(),tokens.data(),blob.size());}
        if(invalid&&!vertex)blob[0]='X';word(blob.size());word(vertex?0:1);
        if(!vertex){word(0);word(0);word(texture?FX::NGLFX_BIND_SAMPLER:FX::NGLFX_BIND_FLOAT);word(FX::NGLFX_BIND_PARAMETER);word(0);word(1);string(texture?"image":"colour");}
        payload.insert(payload.end(),blob.begin(),blob.end());
    }
    word(0);word(0); // pass and technique annotations.
    word(1);string(texture?"image":"colour");string("");word(texture?FX::NGLFX_PARAM_SAMPLER:FX::NGLFX_PARAM_VECTOR);word(texture?2:4);word(0);payload.push_back(shared);word(0);
    word(texture?2:4);if(texture){word(0);word((1<<0)|(1<<3)|(1<<6)|(1<<11)|(1<<13)|(1<<16));}else{word(0);word(0x3f800000);word(0);word(0x3f800000);}
    word(0);word(0); // vertex declaration and structs.
    std::unique_ptr<Sys::I_File> output(fs.Open(name,Sys::E_FileAccess_Write));require(bool(output),"Native FX fixture file could not open");
    Sys::BinarySerialiser<Ut::NoEndianSwap> stream(*output);stream<<U32(0x46584232);
    {Sys::C_FileBlock root(output.get(),"FXB");Sys::C_FileBlock pc(&root,"PCFX");require(pc.Write(payload.data(),payload.size()),"Native FX fixture file could not write");}
    output->Flush();
}

namespace {
struct DrawFailure:std::runtime_error{using std::runtime_error::runtime_error;};
class RenderApplication final:public C_GameApp {
    Display::C_Device& device;Display::C_Context* display;FX::Context* context;FX::Effect* effect;Display::C_Texture2D& texture;const Display::C_VertexDeclaration* declaration;int sampler;
public:
    bool throwInDraw=false;unsigned draws=0;std::atomic<bool> workerStarted{false},workerEntered{false};std::thread serialiser;
    RenderApplication(Display::C_Device& d,Display::C_Context* dc,FX::Context* fc,FX::Effect* e,Display::C_Texture2D& t,const Display::C_VertexDeclaration* v,int s):device(d),display(dc),context(fc),effect(e),texture(t),declaration(v),sampler(s){m_pSystemLayer=new Game::C_SystemLayer;m_pSystemLayer->m_UseRenderMutex=false;}
    ~RenderApplication()override{if(serialiser.joinable())serialiser.join();delete m_pSystemLayer;m_pSystemLayer=nullptr;}
    void CheckOwnership(){require(FX::g_RenderMutex.m_LockType==FX::RenderMutex::E_RenderMutex,"Actual OnRender failed to own FX through the disabled boot policy");FX::CheckRenderMutex();}
    bool BeginRender()override{CheckOwnership();return device.BeginScene();}
    void CommonRender()override{
        CheckOwnership();require(!m_pSystemLayer->m_UseRenderMutex,"Fixture lost the actual false boot flag");
        workerStarted=false;workerEntered=false;
        serialiser=std::thread([this]{workerStarted=true;FX::LockSerialiseMutex();workerEntered=true;FX::GetSamplerState(Display::E_TextureAddress_Wrap,Display::E_TextureAddress_Wrap,Display::E_TextureAddress_Wrap,Display::E_TextureFilter_None,Display::E_TextureFilter_Point,Display::E_TextureFilter_Point,1,false,0);FX::ReleaseSerialiseMutex();});
        const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(2);while(!workerStarted&&std::chrono::steady_clock::now()<until)std::this_thread::yield();
        require(workerStarted,"Actual serialisation worker did not start");std::this_thread::sleep_for(std::chrono::milliseconds(10));require(!workerEntered,"Serialisation mutated the actual sampler cache while GameApp rendered");
        device.Clear(Display::E_Clear_Colour,Maths::RGBA(0,0,0,255),1,0);display->SetCullMode(Display::E_CullMode_None);display->SetDepthEnable(Display::E_Bool_False);display->SetBlendMode(Display::E_BlendMode_Solid);
        context->InvalidateState();context->BeginPass(effect,0);context->SetTextureParameterImmediate(FX::GetImmediateParameter(effect,effect->GetParameter("image")),texture.GetGPUTexture(),sampler);
        auto* vertices=display->BeginVertices(Display::E_PrimitiveType_TriangleList,3,12,declaration);require(vertices,"Original FX transient vertices failed");const F32 positions[]={-1,-1,.5f,3,-1,.5f,-1,3,.5f};memcpy(vertices,positions,sizeof(positions));display->EndVertices();context->EndPass();++draws;CheckOwnership();
        if(throwInDraw){device.EndScene();throw DrawFailure("original required render failure");}
    }
    void EndRender()override{CheckOwnership();device.EndScene();}
    void JoinSerialiser(){serialiser.join();require(workerEntered&&FX::g_RenderMutex.m_LockType==FX::RenderMutex::E_None,"Original serialisation worker did not acquire/release after the real render owner exited");}
};
}
int main(int argc,char** argv){try{
    require(argc==2,"Expected authored FX fixture scratch directory");require(ArteryStartupEx(true,"Native Game Render Ownership",16),"Original Artery startup failed");struct ArteryScope{~ArteryScope(){ArteryCleanup();}} artery;
    std::filesystem::create_directories(argv[1]);
    Scheduler::Init();Scheduler::SetGroupPoolSize(128);Scheduler::Start();
    auto& fs=Sys::C_FileSystem::Instance();require(fs.SetWorkingDirectory(argv[1]),"Original filesystem could not open fixture directory");fileEffect(fs,"native-owner-texture.fxb",false,true);
    Display::C_Device device;Display::C_DeviceParams params;params.m_Width=params.m_Height=16;require(device.Create(params)&&device.CreateVolatile(params),"Real native device creation failed");FX::Initialise();
    {
        FX::LockSerialiseMutex();auto* loaded=static_cast<FX::Effect*>(FX::LoadEffect("native-owner-texture.fxb"));require(loaded,"Actual authored FX file failed to load");
        const int sampler=FX::GetSamplerState(Display::E_TextureAddress_Clamp,Display::E_TextureAddress_Clamp,Display::E_TextureAddress_Clamp,Display::E_TextureFilter_None,Display::E_TextureFilter_Point,Display::E_TextureFilter_Point,1,false,0);FX::ReleaseSerialiseMutex();
        Display::C_DeviceContext parent;parent.Initialise();parent.CloneState(nullptr);auto* display=Display::GetContext(&parent);auto* context=FX::GetContext(&parent);
        Display::C_VertexElement elements[]={Display::C_VertexElement(0,0,Display::E_VertexElementType_F32_3,Display::E_VertexElementUsage_Position,0),VERTEXELEMENT_END()};auto* declaration=device.CreateVertexDeclaration(elements);require(declaration,"Native vertex declaration failed");
        Display::C_Texture2D texture;require(device.CreateTexture2D(1,1,1,0,Display::E_Format_A8R8G8B8,&texture),"Native fixture texture allocation failed");const U32 yellow=0xffffff00;auto write=texture.LockRect2D(0);require(write.pBits,"Native fixture texture map failed");memcpy(write.pBits,&yellow,4);texture.UnlockRect2D(0);
        {
            RenderApplication app(device,display,context,loaded,texture,declaration,sampler);
            for(unsigned i=0;i<4;++i){app.throwInDraw=i==1;bool rejected=false;try{app.OnRender();}catch(const DrawFailure& error){rejected=std::string(error.what())=="original required render failure";}app.JoinSerialiser();require(rejected==app.throwInDraw,"Native render owner swallowed or replaced the original draw failure");
                auto read=device.GetNativeColourSurface()->LockRect(16);require(read.pBits,"Actual FX GPU readback failed");bool pixels=true;for(U32 y=0;y<16;++y)for(U32 x=0;x<16;++x){U32 pixel;memcpy(&pixel,read.pBits+y*read.pitch+x*4,4);pixels&=pixel==yellow;}device.GetNativeColourSurface()->UnlockRect();require(pixels,"Actual OnRender sampler-bound draw produced different pixels");}
            require(app.draws==4,"Actual render owner could not retry after failure");
            FX::RestoreDefaultData();device.DestroyTexture2D(&texture);FX::ReleaseEffect(loaded);
        }
        AmaxCam::CameraSystem().Exit();
    }
    FX::Shutdown();require(device.GetDisplay2().WaitIdle(),"Actual render-owner GPU completion failed");device.DestroyVolatile();device.Destroy();Scheduler::Shutdown();
    std::cout<<"Actual GameApp OnRender false boot flag, original sampler-bound FX GPU pixels, serialiser exclusion, original exception release/retry and complete teardown PASS (enabled game shadow/prepass validated separately)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
