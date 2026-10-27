// [Blur2PC][macOS] Exercise the original FX loader/cache/context using real Metal output.
#include <core/core.h>
#include <core/maths.h>
#include <display.h>
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
#include <system/memory/MemoryTracker.h>
#include <memory>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace FX {
extern Display::C_Texture2D* g_pFXDefaultTexture;
extern Display::C_Texture3D* g_pFXDefaultVertexTexture;
extern Display::C_TextureCube* g_pFXPC2DToCubeTexture;
const void* GetPC2DToCubeTexture();
const void* GetDefaultVertexTextureFor(const void* texture);
}
static void require(bool value,const char* message){if(!value){std::cerr<<message<<"\n";throw std::runtime_error(message);}}
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
int main(int argc,char** argv){try{
    require(argc==2,"Native FX fixture directory missing");auto& fs=Sys::C_FileSystem::Instance();require(fs.SetWorkingDirectory(argv[1]),"Native FX filesystem setup failed");
    fileEffect(fs,"native-fx-local.fxb",false);fileEffect(fs,"native-fx-shared.fxb",true);fileEffect(fs,"native-fx-texture.fxb",false,true);fileEffect(fs,"native-fx-invalid.fxb",false,false,true);fileEffect(fs,"native-fx-d3d9.fxb",false,false,false,true);
    Display::C_Device device;Display::C_DeviceParams params;params.m_Width=params.m_Height=16;
    // A real upper owner with an uncreated Metal device rejects actual resource
    // allocation. Failure must publish neither default and must unwind tracking.
    const auto trackerBefore=MEMORY_TRACKER_CURRENT_CONTEXT();
    FX::Shutdown();FX::Shutdown();bool initRejected=false;
    try{FX::Initialise();}catch(const std::runtime_error& e){initRejected=std::string(e.what()).find("default colour texture")!=std::string::npos;}
    require(initRejected&&!FX::g_pFXDefaultTexture&&!FX::g_pFXDefaultVertexTexture&&!FX::g_System.GetDefaultTexture()&&!FX::GetPC2DToCubeTexture(),"Failed native FX init published incomplete required defaults");
    require(MEMORY_TRACKER_CURRENT_CONTEXT()==trackerBefore,"Failed native FX init retained its tracker context");
    FX::Shutdown();
    require(device.Create(params)&&device.CreateVolatile(params),"Native FX device initialization failed");
#if TRACK_DISPLAY2_RESOURCES
    const auto defaultTextureBaseline=Display2::Device()->GetTextureCount();
#endif
    auto verifyDefaults=[&]{
        require(FX::g_pFXDefaultTexture&&FX::g_pFXDefaultVertexTexture&&FX::g_pFXDefaultTexture->IsCreated()&&FX::g_pFXDefaultVertexTexture->IsCreated()&&FX::g_pFXPC2DToCubeTexture&&FX::g_pFXPC2DToCubeTexture->IsCreated(),"Native required defaults were not fully published");
        auto white=FX::g_pFXDefaultTexture->LockRect2D(0);require(white.pBits,"Native white default readback failed");U32 pixel=0;memcpy(&pixel,white.pBits,4);FX::g_pFXDefaultTexture->UnlockRect2D(0);require(pixel==0xffffffff,"Native FX colour default was not initialized white");
        auto zero=FX::g_pFXDefaultVertexTexture->LockBox3D(0);require(zero.pBits,"Native vertex default readback failed");std::array<U32,4> values;memcpy(values.data(),zero.pBits,16);FX::g_pFXDefaultVertexTexture->UnlockBox3D(0);for(U32 value:values)require(value==0,"Native FX vertex volume was not initialized to no damage");
        require(FX::GetPC2DToCubeTexture()==FX::g_pFXPC2DToCubeTexture->GetGPUTexture()&&FX::g_pFXPC2DToCubeTexture->GetType()==Display2::E_TextureType_Cube,"Native PC compatibility owner did not publish an actual cube");
        for(U32 face=0;face<6;++face){auto texel=FX::g_pFXPC2DToCubeTexture->LockRectCube(face,0);require(texel.pBits,"Native zero cube face readback failed");U32 rgba=~0U;memcpy(&rgba,texel.pBits,sizeof(rgba));FX::g_pFXPC2DToCubeTexture->UnlockRectCube(face,0);require(rgba==0,"PC 2D-to-cube compatibility face is not exact RGBA zero");}
        require(FX::GetDefaultVertexTextureFor(FX::g_System.GetDefaultTexture())==FX::g_pFXDefaultVertexTexture->GetGPUTexture(),"Native FX fallback did not publish the owned 3D volume");
#if TRACK_DISPLAY2_RESOURCES
        require(Display2::Device()->GetTextureCount()==defaultTextureBaseline+3,"Native FX defaults have incorrect tracked ownership");
#endif
    };
    FX::Initialise();verifyDefaults();initRejected=false;
    try{FX::Initialise();}catch(const std::logic_error&){initRejected=true;}
    require(initRejected,"Repeated live native FX init replaced required owners");verifyDefaults();
    {
        FX::Effect first,second;effect(first);effect(second);FX::OnEffectLoad(&first);FX::OnEffectLoad(&second);
        auto& pass=first.Techniques[0].Passes[0];auto& other=second.Techniques[0].Passes[0];
        require(pass.VertexShader.pShader==other.VertexShader.pShader&&pass.PixelShader.pShader==other.PixelShader.pShader,"Original FX shader cache did not share payloads");
        require(pass.VertexShader.pShader->m_iRefCount==2&&pass.PixelShader.pShader->m_iRefCount==2,"Original FX shader reference counts differ");
        FX::Effect invalid;effect(invalid);auto& bad=invalid.Techniques[0].Passes[0];static_cast<U8*>(bad.PixelShader.Microcode)[0]='X';bool rejected=false;
        try{FX::OnEffectLoad(&invalid);}catch(const std::runtime_error&){rejected=true;}
        require(rejected&&!bad.VertexShader.pShader&&!bad.PixelShader.pShader&&pass.VertexShader.pShader->m_iRefCount==2,"Failed native FX load retained shaders or changed the cache");
        Display::C_DeviceContext parent;parent.Initialise();parent.CloneState(nullptr);auto* display=Display::GetContext(&parent);auto* context=FX::GetContext(&parent);require(context,"Original FX context was not registered");
        Display::C_VertexElement elements[]={Display::C_VertexElement(0,0,Display::E_VertexElementType_F32_3,Display::E_VertexElementUsage_Position,0),VERTEXELEMENT_END()};auto* declaration=device.CreateVertexDeclaration(elements);require(declaration,"Native FX declaration failed");
        auto verify=[&](U32 expected){auto* target=device.GetNativeColourSurface();auto data=target->LockRect(16);require(data.pBits,"FX GPU readback failed");bool valid=true;for(U32 y=0;y<16;++y)for(U32 x=0;x<16;++x){U32 pixel;memcpy(&pixel,data.pBits+y*data.pitch+x*4,4);valid&=pixel==expected;}target->UnlockRect();require(valid,"Original FX bindings produced incorrect GPU pixels");};
        for(U32 frame=0;frame<4;++frame){
            require(device.BeginScene(),"Native FX scene failed");device.Clear(Display::E_Clear_Colour,Maths::RGBA(0,0,0,255),1,0);
            display->SetCullMode(Display::E_CullMode_None);display->SetDepthEnable(Display::E_Bool_False);display->SetBlendMode(Display::E_BlendMode_Solid);
            context->InvalidateState();context->BeginPass(frame&1?&second:&first,0);
            if(frame&1){const F32 blue[]={0,0,1,1};context->SetParameterImmediate(FX::GetImmediateParameter(&second,&second.Parameters[0]),blue);}
            auto* vertices=display->BeginVertices(Display::E_PrimitiveType_TriangleList,3,12,declaration);require(vertices,"Native FX transient vertices failed");const F32 positions[]={-1,-1,.5f,3,-1,.5f,-1,3,.5f};memcpy(vertices,positions,sizeof(positions));display->EndVertices();context->EndPass();device.EndScene();verify(frame&1?0xff0000ff:0xff00ff00);device.Present();
        }
        FX::OnEffectRelease(&first);require(other.VertexShader.pShader->m_iRefCount==1&&other.PixelShader.pShader->m_iRefCount==1,"Original FX release discarded a shared shader");FX::OnEffectRelease(&second);
    }
    {
        FX::LockSerialiseMutex();
        auto* local=static_cast<FX::Effect*>(FX::LoadEffect("native-fx-local.fxb"));auto* shared=static_cast<FX::Effect*>(FX::LoadEffect("native-fx-shared.fxb"));auto* textured=static_cast<FX::Effect*>(FX::LoadEffect("native-fx-texture.fxb"));auto* d3d9=static_cast<FX::Effect*>(FX::LoadEffect("native-fx-d3d9.fxb"));
        require(local&&shared&&textured&&d3d9&&FX::LoadEffect("native-fx-local.fxb")==local,"Original FX file manager load/reuse failed");FX::ReleaseEffect(local);
        require(FX::g_SharedEffect.GetNumSharedBindings()==1,"Original FX file loader failed to register the shared constant");
        bool rejected=false;try{FX::LoadEffect("native-fx-invalid.fxb");}catch(const std::runtime_error&){rejected=true;}require(rejected,"Foreign payload in PCFX was accepted");
        Display::C_DeviceContext parent;parent.Initialise();parent.CloneState(nullptr);auto* display=Display::GetContext(&parent);auto* context=FX::GetContext(&parent);
        Display::C_VertexElement elements[]={Display::C_VertexElement(0,0,Display::E_VertexElementType_F32_3,Display::E_VertexElementUsage_Position,0),VERTEXELEMENT_END()};auto* declaration=device.CreateVertexDeclaration(elements);
        Display::C_Texture2D texture;require(device.CreateTexture2D(1,1,1,0,Display::E_Format_A8R8G8B8,&texture),"FX texture fixture allocation failed");auto lock=texture.LockRect2D(0);require(lock.pBits,"FX texture fixture map failed");U32 yellow=0xffffff00;memcpy(lock.pBits,&yellow,4);texture.UnlockRect2D(0);
        auto sampler=FX::GetSamplerState(Display::E_TextureAddress_Clamp,Display::E_TextureAddress_Clamp,Display::E_TextureAddress_Clamp,Display::E_TextureFilter_None,Display::E_TextureFilter_Point,Display::E_TextureFilter_Point,1,false,0);FX::ReleaseSerialiseMutex();FX::LockRenderMutex();
        for(int frame=0;frame<8;++frame){
            auto* active=frame%4==0?local:frame%4==1?shared:frame%4==2?textured:d3d9;require(device.BeginScene(),"File-backed FX scene failed");device.Clear(Display::E_Clear_Colour,Maths::RGBA(0,0,0,255),1,0);display->SetCullMode(Display::E_CullMode_None);display->SetDepthEnable(Display::E_Bool_False);display->SetBlendMode(Display::E_BlendMode_Solid);context->BeginPass(active,0);
            if(frame%4==1){const F32 blue[]={0,0,1,1};context->SetParameterImmediate(FX::GetImmediateParameter(shared,shared->GetParameter("colour")),blue);}
            if(frame%4==2)context->SetTextureParameterImmediate(FX::GetImmediateParameter(textured,textured->GetParameter("image")),frame==2?static_cast<const void*>(FX::g_System.GetDefaultTexture()):static_cast<const void*>(texture.GetGPUTexture()),sampler);
            auto* vertices=display->BeginVertices(Display::E_PrimitiveType_TriangleList,3,12,declaration);require(vertices,"File-backed FX vertices failed");const F32 positions[]={-1,-1,.5f,3,-1,.5f,-1,3,.5f};memcpy(vertices,positions,sizeof(positions));display->EndVertices();context->EndPass();device.EndScene();
            auto* target=device.GetNativeColourSurface();auto read=target->LockRect(16);require(read.pBits,"File-backed FX readback failed");bool valid=true;const U32 expected=frame%4==0||frame%4==3?0xff00ff00:frame%4==1?0xff0000ff:frame==2?0xffffffff:yellow;for(int y=0;y<16;++y)for(int x=0;x<16;++x){U32 pixel;memcpy(&pixel,read.pBits+y*read.pitch+x*4,4);if(pixel!=expected&&x==0&&y==0)std::cerr<<"frame="<<frame<<" actual="<<std::hex<<pixel<<" expected="<<expected<<std::dec<<"\n";valid&=pixel==expected;}target->UnlockRect();require(valid,"File-backed FX shared/local/texture binding output differs");device.Present();
        }
        device.DestroyTexture2D(&texture);FX::RestoreDefaultData();FX::ReleaseRenderMutex();FX::ReleaseEffect(local);FX::ReleaseEffect(shared);FX::ReleaseEffect(textured);FX::ReleaseEffect(d3d9);
    }
    // [Blur2PC] Original preload ownership is distinct from live effect clients.
#if TRACK_DISPLAY2_RESOURCES
    const auto vertexBaseline=Display2::Device()->GetVertexShaderCount();
    const auto pixelBaseline=Display2::Device()->GetPixelShaderCount();
#endif
    for(int lifetime=0;lifetime<2;++lifetime){
        Btl::Vector<const char*> preload;preload.PushBack(static_cast<const char*>("native-fx-local.fxb"));
        Btl::Vector<FX::I_Effect*> pins;FX::g_Manager.LoadEffects(preload,pins);
        require(pins.Size()==1&&!pins[0],"Original new preload unexpectedly transferred its manager ownership");
        auto* client=static_cast<FX::Effect*>(FX::LoadEffect("native-fx-local.fxb"));
        require(client&&client->Techniques[0].Passes[0].VertexShader.pShader,"Native preload created no original shader");
#if TRACK_DISPLAY2_RESOURCES
        require(Display2::Device()->GetVertexShaderCount()>vertexBaseline,"Native preload created no tracked shader");
#endif
        require(!FX::g_Manager.NativeShutdownCache(),"FX shutdown consumed a live effect client's reference");
        require(client->Techniques[0].Passes[0].VertexShader.pShader,"FX shutdown invalidated a surviving client");
        FX::ReleaseEffect(client);
        require(FX::g_Manager.NativeShutdownCache()&&FX::g_Manager.NativeShutdownCache(),"FX cache shutdown/repeat did not empty the original manager");
#if TRACK_DISPLAY2_RESOURCES
        require(Display2::Device()->GetVertexShaderCount()==vertexBaseline&&Display2::Device()->GetPixelShaderCount()==pixelBaseline,"FX cache retirement retained original tracked shaders");
#endif
    }
    FX::Shutdown();FX::Shutdown();
    require(!FX::g_pFXDefaultTexture&&!FX::g_pFXDefaultVertexTexture&&!FX::g_System.GetDefaultTexture()&&!FX::GetPC2DToCubeTexture(),"Native FX shutdown retained default publication");
#if TRACK_DISPLAY2_RESOURCES
    require(Display2::Device()->GetTextureCount()==defaultTextureBaseline,"Native FX shutdown retained required textures");
#endif
    FX::Initialise();verifyDefaults();FX::Shutdown();
    device.DestroyVolatile();device.Destroy();
    std::cout<<"Original FX loading/cache/reference counts/register bindings/immediate constants/GPU draws/rejected-payload rollback PASS (authored fixture, not game startup)\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
