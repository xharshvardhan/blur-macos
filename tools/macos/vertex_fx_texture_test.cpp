// [Blur2PC][macOS] Real mixed-dimension vertex samplers through original FX bindings.
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
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

static void require(bool ok, const char* message) { if(!ok) { std::cerr << message << std::endl; throw std::runtime_error(message); } }
static std::vector<U8> shader(bool vertex, bool volume = true) {
    std::string metadata = vertex ? R"({"parameters":{},"input_semantics":[[0,0,0]],"reflection":{"ubos":[],"textures":[{"binding":0,"type":"sampler3D"},{"binding":1,"type":"sampler2D"}]}})"
        : R"({"parameters":{},"input_semantics":[],"reflection":{"ubos":[]}})";
    std::string source = vertex ? R"(#include <metal_stdlib>
using namespace metal;
struct Input{float3 position[[attribute(0)]];};
struct Output{float4 p[[position]];float4 colour;};
vertex Output blur_main(Input v[[stage_in]],texture3d<float> volume[[texture(0)]],sampler volumeState[[sampler(0)]],texture2d<float> image[[texture(1)]],sampler imageState[[sampler(1)]]){
    float3 a=volume.sample(volumeState,float3(.5),level(0)).rgb;
    float3 b=image.sample(imageState,float2(.5),level(0)).rgb;
    return {float4(v.position,1),float4(a+b,1)};
})" : R"(#include <metal_stdlib>
using namespace metal;
struct Output{float4 p[[position]];float4 colour;};
fragment float4 blur_main(Output v[[stage_in]]){return v.colour;}
)";
    if(vertex&&!volume){
        auto replace=[](std::string& text,const char* from,const char* to){auto at=text.find(from);require(at!=std::string::npos,"Authored preceding shader token missing");text.replace(at,std::strlen(from),to);};
        replace(metadata,"sampler3D","sampler2D");replace(source,"texture3d<float>","texture2d<float>");replace(source,"float3(.5)","float2(.5)");
    }
    const std::string entry="blur_main";std::vector<U8> bytes(40);std::memcpy(bytes.data(),"BMSL",4);
    auto word=[&](U32 pos,U32 value){for(U32 i=0;i<4;++i)bytes[pos+i]=U8(value>>(i*8));};
    word(4,1);word(8,vertex?0:1);word(16,40);word(20,metadata.size());word(24,40+metadata.size());word(28,source.size());word(32,40+metadata.size()+source.size());word(36,entry.size());
    bytes.insert(bytes.end(),metadata.begin(),metadata.end());bytes.insert(bytes.end(),source.begin(),source.end());bytes.insert(bytes.end(),entry.begin(),entry.end());word(12,bytes.size());return bytes;
}
static void effect(FX::Effect& e, bool shared = false, bool volume = true, const char* prefix = "") {
    e.Name=SharedString::GetString("native-mixed-vertex-textures");e.ParameterData.Resize(4);e.Parameters.Resize(2);
    e.Techniques.Resize(1);auto& technique=e.Techniques[0];technique.hTechnique=0;technique.Passes.Resize(1);auto& pass=technique.Passes[0];
    std::memset(&pass.RenderStates,0,sizeof(pass.RenderStates));pass.SharedBindingUsage.ClearBits();pass.SharedBindingMask.ClearBits();
    for(bool vertex:{true,false}){auto data=shader(vertex,volume);auto& function=vertex?pass.VertexShader:pass.PixelShader;function.cMicrocode=data.size();function.Microcode=new U8[data.size()];std::memcpy(function.Microcode,data.data(),data.size());}
    pass.VertexShader.Bindings.Resize(2);
    for(U32 i=0;i<2;++i){
        const std::string name=std::string(prefix)+(i?"image":"volume");auto& p=e.Parameters[i];p.Name=SharedString::GetString(name.c_str());p.Type=FX::NGLFX_PARAM_SAMPLER;p.Data=&e.ParameterData[i*2];p.Size=2;p.bShared=shared;p.bExported=0;
        p.Sampler->Texture=0;p.Sampler->iSamplerState=(1<<0)|(1<<3)|(1<<6)|(1<<11)|(1<<13)|(1<<16);
        auto& b=pass.VertexShader.Bindings[i];b.Name=p.Name.Chars();b.ParameterIdx=i;b.Count=1;b.Handle=i;b.Type=FX::NGLFX_BIND_SAMPLER;b.Source=FX::NGLFX_BIND_PARAMETER;b.Shared=b.Exported=0;b.Offset=0; // Original OnEffectLoad derives byte offsets from each parameter data pointer.
    }
}
// [Blur2PC] Same original FX adapter at both stages; PC cube/2D sampling is zero RGBA.
static std::vector<U8> cubeShader(bool vertex, bool vertexSampler, bool cube) {
    const bool samples=vertex==vertexSampler;const U32 slot=vertexSampler?0:8;
    const std::string kind=cube?"samplerCube":"sampler2D";
    std::string metadata=std::string("{\"parameters\":{},\"input_semantics\":")+(vertex?"[[0,0,0],[3,0,1]]":"[]")+",\"reflection\":{\"ubos\":[],\"textures\":["+
        (samples?"{\"binding\":"+std::to_string(slot)+",\"type\":\""+kind+"\"}":"")+"]}}";
    std::string source="#include <metal_stdlib>\nusing namespace metal;\nstruct In{float3 p[[attribute(0)]];float3 d[[attribute(1)]];};\nstruct Out{float4 p[[position]];float3 d;float4 c;};\n";
    const std::string args=samples?std::string(",texture")+(cube?"cube":"2d")+"<float> image[[texture("+std::to_string(slot)+")]],sampler state[[sampler("+std::to_string(slot)+")]]":"";
    const std::string sample="image.sample(state,"+std::string(cube?(vertex?"v.d":"v.d"):"float2(.5)")+",level(0))";
    source+=vertex?"vertex Out blur_main(In v[[stage_in]]"+args+"){return {float4(v.p,1),v.d,"+(samples?sample:"float4(0)")+"};}\n"
                  :"fragment float4 blur_main(Out v[[stage_in]]"+args+"){return "+(samples?sample:"v.c")+";}\n";
    const std::string entry="blur_main";std::vector<U8> bytes(40);std::memcpy(bytes.data(),"BMSL",4);
    auto word=[&](U32 pos,U32 value){for(U32 i=0;i<4;++i)bytes[pos+i]=U8(value>>(i*8));};
    word(4,1);word(8,vertex?0:1);word(16,40);word(20,metadata.size());word(24,40+metadata.size());word(28,source.size());word(32,40+metadata.size()+source.size());word(36,entry.size());
    bytes.insert(bytes.end(),metadata.begin(),metadata.end());bytes.insert(bytes.end(),source.begin(),source.end());bytes.insert(bytes.end(),entry.begin(),entry.end());word(12,bytes.size());return bytes;
}
static void cubeEffect(FX::Effect& e,bool vertexSampler,bool cube,bool shared,const char* name){
    e.Name=SharedString::GetString(name);e.ParameterData.Resize(2);e.Parameters.Resize(1);e.Techniques.Resize(1);
    auto& technique=e.Techniques[0];technique.hTechnique=0;technique.Passes.Resize(1);auto& pass=technique.Passes[0];
    std::memset(&pass.RenderStates,0,sizeof(pass.RenderStates));pass.SharedBindingUsage.ClearBits();pass.SharedBindingMask.ClearBits();
    for(bool vertex:{true,false}){auto data=cubeShader(vertex,vertexSampler,cube);auto& f=vertex?pass.VertexShader:pass.PixelShader;f.cMicrocode=data.size();f.Microcode=new U8[data.size()];std::memcpy(f.Microcode,data.data(),data.size());}
    auto& p=e.Parameters[0];p.Name=SharedString::GetString(name);p.Type=FX::NGLFX_PARAM_SAMPLER;p.Data=&e.ParameterData[0];p.Size=2;p.bShared=shared;p.bExported=0;
    p.Sampler->Texture=0;p.Sampler->iSamplerState=(1<<0)|(1<<3)|(1<<6)|(1<<11)|(1<<13)|(1<<16);
    auto& f=vertexSampler?pass.VertexShader:pass.PixelShader;f.Bindings.Resize(1);auto& binding=f.Bindings[0];
    binding.Name=p.Name.Chars();binding.ParameterIdx=0;binding.Count=1;binding.Handle=vertexSampler?0:8;binding.Type=FX::NGLFX_BIND_SAMPLER;binding.Source=FX::NGLFX_BIND_PARAMETER;binding.Shared=binding.Exported=0;binding.Offset=0;
}
static void cubeSampling(Display::C_Device& device){
    Display::C_Texture2D image;Display::C_TextureCube cube;Display::C_Texture3D volume;Display::C_RenderTexture2D feedback;
    require(device.CreateTexture2D(1,1,1,0,Display::E_Format_A8R8G8B8,&image)&&device.CreateTextureCube(1,1,0,Display::E_Format_A8R8G8B8,&cube)&&
            device.CreateTexture3D(1,1,1,1,Display::E_Format_Linear,Display::E_Format_A32B32G32R32F,&volume),"Cube FX explicit resources failed");
    require(device.CreateRenderTexture2D(16,16,1,Display::E_Usage_Colour,Display::E_Format_A8R8G8B8,Display::E_Multisample_None,nullptr,&feedback),"FX snapshot target failed");
    const U32 imageColour=0x7f3f7fbf;{auto map=image.LockRect2D(0);require(map.pBits,"Cube FX2D map failed");std::memcpy(map.pBits,&imageColour,4);image.UnlockRect2D(0);}
    const std::array<U32,6> faces={0x11223344,0x22334455,0x33445566,0x44556677,0x55667788,0x66778899};
    for(U32 face=0;face<6;++face){auto map=cube.LockRectCube(face,0);require(map.pBits,"Cube FX face map failed");std::memcpy(map.pBits,&faces[face],4);cube.UnlockRectCube(face,0);}
    {auto map=volume.LockBox3D(0);require(map.pBits,"Cube FX3D map failed");std::memset(map.pBits,0,16);volume.UnlockBox3D(0);}
    const F32 directions[6][3]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    for(bool vertexSampler:{false,true}){
        FX::Effect local,shared,preceding,foreign,snapshot;
        const char* own=vertexSampler?"native_cube_vertex":"native_cube_pixel";
        const char* other=vertexSampler?"foreign_cube_vertex":"foreign_cube_pixel";
        cubeEffect(local,vertexSampler,true,false,"local_cube");cubeEffect(shared,vertexSampler,true,true,own);
        cubeEffect(preceding,vertexSampler,false,false,"preceding_2d");cubeEffect(foreign,vertexSampler,false,true,other);
        std::vector<FX::Effect*> owned={&local,&shared,&preceding,&foreign};
        if(!vertexSampler){cubeEffect(snapshot,false,false,true,"snapshot_shared_pixel");owned.push_back(&snapshot);}
        FX::LockSerialiseMutex();for(auto* e:owned)FX::OnEffectLoad(e);FX::ReleaseSerialiseMutex();
        auto& pass=shared.Techniques[0].Passes[0];auto& foreignPass=foreign.Techniques[0].Passes[0];
        FX::Parameter* sharedParam=FX::g_SharedEffect.GetSharedParameterByIdx(FX::g_SharedEffect.GetSharedParameterIdxByName(own));require(sharedParam,"Cube shared parameter missing");
        for(int i=0;i<FX::g_SharedEffect.GetNumSharedBindings();++i){
            auto* binding=FX::g_SharedEffect.GetSharedBinding(i);const char* name=FX::g_SharedEffect.GetSharedParameterByIdx(binding->Idx)->Name.Chars();
            if(std::strcmp(name,own)==0){require(binding->Handle==(vertexSampler?0:8)&&binding->Type==(vertexSampler?FX::E_SPGResource_VertexTexture:FX::E_SPGResource_Texture),"Cube binding stage/register mismatch");pass.SharedBindingUsage.SetBit(i);foreignPass.SharedBindingMask.SetBit(i);}
            if(std::strcmp(name,other)==0){foreignPass.SharedBindingUsage.SetBit(i);pass.SharedBindingMask.SetBit(i);local.Techniques[0].Passes[0].SharedBindingMask.SetBit(i);}
            if(!vertexSampler&&std::strcmp(name,"snapshot_shared_pixel")==0)snapshot.Techniques[0].Passes[0].SharedBindingUsage.SetBit(i);
        }
        auto* shaderV=static_cast<FX::VertexShader*>(local.Techniques[0].Passes[0].VertexShader.pShader);
        auto* shaderP=static_cast<FX::PixelShader*>(local.Techniques[0].Passes[0].PixelShader.pShader);
        require((vertexSampler?shaderV->GetTextureType(0):shaderP->GetTextureType(8))==Display2::E_TextureType_Cube,"Both-stage cube reflection missing");
        Display::C_DeviceContext parent;parent.Initialise();parent.CloneState(nullptr);auto* display=Display::GetContext(&parent);auto* context=FX::GetContext(&parent);require(context,"Cube FX context missing");
        Display::C_VertexElement elements[]={Display::C_VertexElement(0,0,Display::E_VertexElementType_F32_3,Display::E_VertexElementUsage_Position,0),Display::C_VertexElement(0,12,Display::E_VertexElementType_F32_3,Display::E_VertexElementUsage_Normal,0),VERTEXELEMENT_END()};
        auto* declaration=device.CreateVertexDeclaration(elements);require(declaration,"Cube FX declaration failed");
        FX::LockRenderMutex();
        auto begin=[&]{require(device.BeginScene(),"Cube FX scene failed");device.Clear(Display::E_Clear_Colour,Maths::RGBA(255,255,255,255),1,0);display->SetCullMode(Display::E_CullMode_None);display->SetDepthEnable(Display::E_Bool_False);display->SetBlendMode(Display::E_BlendMode_Solid);context->InvalidateState();};
        auto immediate=[&](FX::Effect& e,const void* texture){auto* p=e.Parameters[0].bShared?FX::g_SharedEffect.GetSharedParameterByIdx(FX::g_SharedEffect.GetSharedParameterIdxByName(e.Parameters[0].Name.Chars())):&e.Parameters[0];context->SetTextureParameterImmediate(FX::GetImmediateParameter(&e,p),texture,p->Sampler->iSamplerState);};
        auto draw=[&](FX::Effect& e,const void* texture,bool update,U32 face,const char* rejection=nullptr){
            context->BeginPass(&e,0);if(update)immediate(e,texture);display->SetColourWriteEnable(Display::E_ColourWriteEnable_All);
            auto* vertices=static_cast<F32*>(display->BeginVertices(Display::E_PrimitiveType_TriangleList,3,24,declaration));require(vertices,"Cube FX upload failed");
            const F32 positions[3][3]={{-1,-1,.5f},{3,-1,.5f},{-1,3,.5f}};
            for(U32 i=0;i<3;++i){std::memcpy(vertices+i*6,positions[i],12);std::memcpy(vertices+i*6+3,directions[face],12);}
            bool rejected=false;try{display->EndVertices();}catch(const std::logic_error& error){if(!rejection){std::cerr<<error.what()<<std::endl;throw;}rejected=std::string(error.what()).find(rejection)!=std::string::npos;}
            context->EndPass();require(rejected==bool(rejection),"Cube FX mismatch/null did not retain strict rejection");
        };
        auto end=[&](U32 expected){device.EndScene();auto* target=device.GetNativeColourSurface();auto map=target->LockRect(16);require(map.pBits,"Cube FX readback failed");bool correct=true;
            for(U32 y=0;y<16;++y)for(U32 x=0;x<16;++x){U32 pixel;std::memcpy(&pixel,map.pBits+y*map.pitch+x*4,4);correct&=pixel==expected;}U32 first=0;std::memcpy(&first,map.pBits,4);target->UnlockRect();if(!correct)std::cerr<<"Cube stage="<<(vertexSampler?"vertex":"pixel")<<" expected="<<std::hex<<expected<<" actual="<<first<<std::dec<<std::endl;require(correct,"Both-stage PC cube/2D zero RGBA or valid face texel differs");device.Present();};
        const void* white=FX::g_System.GetDefaultTexture();
        for(U32 face=0;face<6;++face){
            begin();draw(local,white,true,face);end(0); // Actual original default2D.
            begin();draw(local,image.GetGPUTexture(),true,face);end(0); // Nonzero explicit2D also returns zero, including alpha.
            begin();draw(local,cube.GetGPUTexture(),true,face);end(faces[face]); // Valid cube/each direction preserved.
            begin();draw(preceding,image.GetGPUTexture(),true,face);end(imageColour); // Returning to2D restores its actual texels.
        }
        begin();context->BindSharedParameters(&pass.SharedBindingUsage,&pass.SharedBindingMask);draw(shared,nullptr,false,0);end(0); // Before first shader selection.
        begin();context->BeginPass(&preceding,0);context->BindSharedParameters(&pass.SharedBindingUsage,&pass.SharedBindingMask);draw(shared,nullptr,false,0);end(0); // Previous shader expected2D.
        begin();context->BeginPass(&shared,0);immediate(shared,cube.GetGPUTexture());draw(shared,nullptr,false,2);end(faces[2]); // Same-pass shared actualCube survives.
        begin();context->BeginPass(&shared,0);context->BindSharedParameters(&foreignPass.SharedBindingUsage,&foreignPass.SharedBindingMask);draw(shared,nullptr,false,3);end(faces[3]); // Foreign node clobber repaired.
        begin();context->BeginPass(&local,0);immediate(local,cube.GetGPUTexture());draw(local,nullptr,false,4);end(faces[4]); // Cached local immediate preserved.
        begin();context->BeginPass(&local,0);immediate(local,cube.GetGPUTexture());context->BindSharedParameters(&foreignPass.SharedBindingUsage,&foreignPass.SharedBindingMask);draw(local,nullptr,false,0);end(0); // Actual conflict restores logical default2D->zeroCube.
        const std::string stage=vertexSampler?"vertex":"pixel";const std::string slot=vertexSampler?"0":"8";
        begin();draw(local,volume.GetGPUTexture(),true,0,("stage="+stage+" slot="+slot+" expectedType=5 actualType=4").c_str());device.EndScene();
        begin();draw(preceding,cube.GetGPUTexture(),true,0,("stage="+stage+" slot="+slot+" expectedType=3 actualType=5").c_str());device.EndScene();
        begin();draw(local,nullptr,true,0,("stage="+stage+" slot="+slot+" expectedType=5 actualType=unbound").c_str());device.EndScene(); // PC nullCube differs: retain rejection, never silently convert.
        begin();draw(local,image.GetGPUTexture(),true,0);end(0); // Retry after rejected bindings.
        if(!vertexSampler){
            // The original unchanged cached FX pass does not rebind a pixel sampler.
            // Native colour feedback therefore keeps its original binding-time
            // snapshot, even when the attachment changes before the next draw.
            auto verifyFeedback=[&](U32 expected){device.EndScene();auto map=feedback.GetTexture().LockRect2D(0,16);require(map.pBits,"FX cached snapshot readback failed");bool correct=true;
                for(U32 y=0;y<16;++y)for(U32 x=0;x<16;++x){U32 pixel;std::memcpy(&pixel,map.pBits+y*map.pitch+x*4,4);correct&=pixel==expected;}
                feedback.GetTexture().UnlockRect2D(0);require(correct,"Cached shared pixel pass refreshed an unchanged binding-time snapshot");device.SetColourDepthStencilSurface();};
            begin();device.SetColourDepthStencilSurface(&feedback.GetSurface(),nullptr);device.Clear(Display::E_Clear_Colour,Maths::RGBA(0,0,0,0),1,0);
            context->BeginPass(&snapshot,0);immediate(snapshot,feedback.GetTexture().GetGPUTexture());
            device.Clear(Display::E_Clear_Colour,Maths::RGBA(255,255,255,255),1,0);
            draw(snapshot,nullptr,false,0);verifyFeedback(0);
            begin();device.SetColourDepthStencilSurface(&feedback.GetSurface(),nullptr);device.Clear(Display::E_Clear_Colour,Maths::RGBA(255,255,255,255),1,0);
            context->BeginPass(&snapshot,0);immediate(snapshot,feedback.GetTexture().GetGPUTexture());draw(snapshot,nullptr,false,0);verifyFeedback(0xffffffff); // An explicit new bind captures new contents.
        }
        FX::ReleaseRenderMutex();
        for(auto it=owned.rbegin();it!=owned.rend();++it)FX::OnEffectRelease(*it);
    }
    device.Destroy(&feedback);device.DestroyTexture3D(&volume);device.DestroyTextureCube(&cube);device.DestroyTexture2D(&image);
}

int main(){try{
    Display::C_Device device;Display::C_DeviceParams params;params.m_Width=params.m_Height=16;
    require(device.Create(params)&&device.CreateVolatile(params),"Mixed vertex FX device initialization failed");FX::Initialise();
    {
        FX::Effect mixed;effect(mixed);FX::LockSerialiseMutex();FX::OnEffectLoad(&mixed);FX::ReleaseSerialiseMutex();
        auto* vertex=static_cast<FX::VertexShader*>(mixed.Techniques[0].Passes[0].VertexShader.pShader);
        require(vertex->GetTextureType(0)==Display2::E_TextureType_3D&&vertex->GetTextureType(1)==Display2::E_TextureType_2D&&vertex->GetTextureType(2)==~0U,"Vertex texture reflection lost original sampler dimensions");
        for(U32 i=0;i<2;++i)require(Core::Slot32ToPointer(mixed.Parameters[i].Sampler->Texture)==FX::g_System.GetDefaultTexture(),"Original FX loader did not initialize generic sampler defaults");
        Display::C_DeviceContext parent;parent.Initialise();parent.CloneState(nullptr);auto* display=Display::GetContext(&parent);auto* context=FX::GetContext(&parent);require(context,"Original FX context missing");
        Display::C_VertexElement elements[]={Display::C_VertexElement(0,0,Display::E_VertexElementType_F32_3,Display::E_VertexElementUsage_Position,0),VERTEXELEMENT_END()};auto* declaration=device.CreateVertexDeclaration(elements);require(declaration,"Mixed vertex declaration failed");
        Display::C_Texture2D black,green;Display::C_Texture3D blue;
        require(device.CreateTexture2D(1,1,1,0,Display::E_Format_A8R8G8B8,&black)&&device.CreateTexture2D(1,1,1,0,Display::E_Format_A8R8G8B8,&green)&&device.CreateTexture3D(1,1,1,1,Display::E_Format_Linear,Display::E_Format_A32B32G32R32F,&blue),"Explicit vertex texture allocation failed");
        for(auto pair:{std::make_pair(&black,U32(0xff000000)),std::make_pair(&green,U32(0xff00ff00))}){auto data=pair.first->LockRect2D(0);require(data.pBits,"Explicit 2D map failed");std::memcpy(data.pBits,&pair.second,4);pair.first->UnlockRect2D(0);}
        {auto data=blue.LockBox3D(0);require(data.pBits,"Explicit 3D map failed");const F32 colour[]={0,0,1,0};std::memcpy(data.pBits,colour,sizeof(colour));blue.UnlockBox3D(0);}
        auto draw=[&](const void* volume,const void* image,bool immediate,const char* rejection){
            if(!immediate){mixed.Parameters[0].Sampler->Texture=Core::PointerToSlot32(volume);mixed.Parameters[1].Sampler->Texture=Core::PointerToSlot32(image);}
            require(device.BeginScene(),"Mixed vertex scene failed");device.Clear(Display::E_Clear_Colour,Maths::RGBA(0,0,0,255),1,0);
            display->SetCullMode(Display::E_CullMode_None);display->SetDepthEnable(Display::E_Bool_False);display->SetBlendMode(Display::E_BlendMode_Solid);context->InvalidateState();context->BeginPass(&mixed,0);
            if(immediate){for(U32 i=0;i<2;++i)context->SetTextureParameterImmediate(FX::GetImmediateParameter(&mixed,&mixed.Parameters[i]),i?image:volume,mixed.Parameters[i].Sampler->iSamplerState);}
            auto* vertices=display->BeginVertices(Display::E_PrimitiveType_TriangleList,3,12,declaration);require(vertices,"Mixed vertex upload failed");const F32 positions[]={-1,-1,.5f,3,-1,.5f,-1,3,.5f};std::memcpy(vertices,positions,sizeof(positions));
            bool rejected=false;try{display->EndVertices();}catch(const std::logic_error& error){if(!rejection)throw;rejected=std::string(error.what()).find(rejection)!=std::string::npos;}
            context->EndPass();device.EndScene();require(rejected==bool(rejection),"Explicit incompatible vertex texture was not truthfully rejected");
        };
        auto verify=[&](U32 expected){auto* target=device.GetNativeColourSurface();auto data=target->LockRect(16);require(data.pBits,"Mixed vertex GPU readback failed");bool matches=true;for(U32 y=0;y<16;++y)for(U32 x=0;x<16;++x){U32 colour;std::memcpy(&colour,data.pBits+y*data.pitch+x*4,4);matches&=colour==expected;}target->UnlockRect();require(matches,"Mixed vertex default/explicit GPU sample differs");device.Present();};
        FX::LockRenderMutex();
        const void* white=FX::g_System.GetDefaultTexture();
        draw(white,white,false,nullptr);verify(0xffffffff); // Actual load defaults: white 2D plus zero 3D.
        draw(white,black.GetGPUTexture(),false,nullptr);verify(0xff000000);
        draw(blue.GetGPUTexture(),black.GetGPUTexture(),false,nullptr);verify(0xff0000ff);
        draw(white,green.GetGPUTexture(),true,nullptr);verify(0xff00ff00);
        draw(blue.GetGPUTexture(),blue.GetGPUTexture(),true,"stage=vertex slot=1 expectedType=3 actualType=4");
        draw(green.GetGPUTexture(),green.GetGPUTexture(),false,"stage=vertex slot=0 expectedType=4 actualType=3");
        draw(white,white,true,nullptr);verify(0xffffffff); // Rejected explicit inputs did not poison the next binding.
        FX::ReleaseRenderMutex();
        device.DestroyTexture3D(&blue);device.DestroyTexture2D(&green);device.DestroyTexture2D(&black);FX::OnEffectRelease(&mixed);
    }
    {
        // Original SharedEffect registration supplies the actual binding records.
        // These authored usage/mask fields state which records this fixture's
        // passes use/conflict with, as the normal manager computes for FX files.
        FX::Effect shared,foreign,previous,local;
        effect(shared,true);effect(foreign,true,true,"foreign_");effect(previous,false,false);effect(local);
        FX::LockSerialiseMutex();for(auto* e:{&shared,&foreign,&previous,&local})FX::OnEffectLoad(e);FX::ReleaseSerialiseMutex();
        auto& sharedPass=shared.Techniques[0].Passes[0];auto& foreignPass=foreign.Techniques[0].Passes[0];
        for(int i=0;i<FX::g_SharedEffect.GetNumSharedBindings();++i){
            auto* binding=FX::g_SharedEffect.GetSharedBinding(i);require(binding->Type==FX::E_SPGResource_VertexTexture,"Authored shared record has wrong stage/type");
            const char* name=FX::g_SharedEffect.GetSharedParameterByIdx(binding->Idx)->Name.Chars();
            if(std::strncmp(name,"foreign_",8)==0){foreignPass.SharedBindingUsage.SetBit(i);sharedPass.SharedBindingMask.SetBit(i);}
            else{sharedPass.SharedBindingUsage.SetBit(i);foreignPass.SharedBindingMask.SetBit(i);}
            previous.Techniques[0].Passes[0].SharedBindingMask.SetBit(i);local.Techniques[0].Passes[0].SharedBindingMask.SetBit(i);
        }
        require(FX::g_SharedEffect.GetNumSharedBindings()==4,"Authored mixed shared registrations missing");
        auto* sharedVolume=FX::g_SharedEffect.GetSharedParameterByIdx(FX::g_SharedEffect.GetSharedParameterIdxByName("volume"));auto* sharedImage=FX::g_SharedEffect.GetSharedParameterByIdx(FX::g_SharedEffect.GetSharedParameterIdxByName("image"));
        require(sharedVolume&&sharedImage,"Original shared parameter lookup failed");
        Display::C_DeviceContext parent;parent.Initialise();parent.CloneState(nullptr);auto* display=Display::GetContext(&parent);auto* context=FX::GetContext(&parent);require(context,"Original shared FX context missing");
        Display::C_VertexElement elements[]={Display::C_VertexElement(0,0,Display::E_VertexElementType_F32_3,Display::E_VertexElementUsage_Position,0),VERTEXELEMENT_END()};auto* declaration=device.CreateVertexDeclaration(elements);require(declaration,"Shared vertex declaration failed");
        Display::C_Texture2D black,green;Display::C_Texture3D blue;
        require(device.CreateTexture2D(1,1,1,0,Display::E_Format_A8R8G8B8,&black)&&device.CreateTexture2D(1,1,1,0,Display::E_Format_A8R8G8B8,&green)&&device.CreateTexture3D(1,1,1,1,Display::E_Format_Linear,Display::E_Format_A32B32G32R32F,&blue),"Shared explicit textures failed");
        for(auto pair:{std::make_pair(&black,U32(0xff000000)),std::make_pair(&green,U32(0xff00ff00))}){auto data=pair.first->LockRect2D(0);require(data.pBits,"Shared2D map failed");std::memcpy(data.pBits,&pair.second,4);pair.first->UnlockRect2D(0);}
        {auto data=blue.LockBox3D(0);require(data.pBits,"Shared3D map failed");const F32 colour[]={0,0,1,0};std::memcpy(data.pBits,colour,sizeof(colour));blue.UnlockBox3D(0);}
        FX::LockRenderMutex();
        auto node=[&](FX::Pass& pass){context->BindSharedParameters(&pass.SharedBindingUsage,&pass.SharedBindingMask);};
        auto begin=[&]{require(device.BeginScene(),"Shared vertex scene failed");device.Clear(Display::E_Clear_Colour,Maths::RGBA(0,0,0,255),1,0);display->SetCullMode(Display::E_CullMode_None);display->SetDepthEnable(Display::E_Bool_False);display->SetBlendMode(Display::E_BlendMode_Solid);context->InvalidateState();};
        auto immediate=[&](FX::Effect& active,const void* volume,const void* image){for(U32 i=0;i<2;++i){auto* parameter=active.Parameters[i].bShared?(i?sharedImage:sharedVolume):&active.Parameters[i];context->SetTextureParameterImmediate(FX::GetImmediateParameter(&active,parameter),i?image:volume,sharedVolume->Sampler->iSamplerState);}};
        auto draw=[&](FX::Effect& active,const void* volume,const void* image,bool update,const char* rejection){
            context->BeginPass(&active,0);
            if(update)immediate(active,volume,image);
            auto* vertices=display->BeginVertices(Display::E_PrimitiveType_TriangleList,3,12,declaration);require(vertices,"Shared vertex upload failed");const F32 positions[]={-1,-1,.5f,3,-1,.5f,-1,3,.5f};std::memcpy(vertices,positions,sizeof(positions));
            bool rejected=false;try{display->EndVertices();}catch(const std::logic_error& error){if(!rejection){std::cerr<<error.what()<<std::endl;throw;}rejected=std::string(error.what()).find(rejection)!=std::string::npos;}
            context->EndPass();device.EndScene();require(rejected==bool(rejection),"Shared explicit incompatible texture was not rejected");
        };
        auto verify=[&](U32 expected){auto* target=device.GetNativeColourSurface();auto data=target->LockRect(16);require(data.pBits,"Shared GPU readback failed");bool matches=true;for(U32 y=0;y<16;++y)for(U32 x=0;x<16;++x){U32 pixel;std::memcpy(&pixel,data.pBits+y*data.pitch+x*4,4);matches&=pixel==expected;}target->UnlockRect();require(matches,"Shared/cached/local materialization GPU pixels differ");device.Present();};
        const void* white=FX::g_System.GetDefaultTexture();
        begin();node(sharedPass);draw(shared,nullptr,nullptr,false,nullptr);verify(0xffffffff); // Before ANY selected shader.
        begin();context->BeginPass(&previous,0);node(sharedPass);draw(shared,nullptr,nullptr,false,nullptr);verify(0xffffffff); // Previous shader declares2D at0.
        begin();draw(shared,blue.GetGPUTexture(),black.GetGPUTexture(),true,nullptr);verify(0xff0000ff);
        begin();context->BeginPass(&shared,0);draw(shared,nullptr,nullptr,false,nullptr);verify(0xff0000ff); // Same-pass shared values survive cached entry.
        begin();context->BeginPass(&shared,0);node(foreignPass);draw(shared,nullptr,nullptr,false,nullptr);verify(0xff0000ff); // Node conflict repairs cached shared pass.
        begin();context->BeginPass(&local,0);immediate(local,blue.GetGPUTexture(),black.GetGPUTexture());draw(local,nullptr,nullptr,false,nullptr);verify(0xff0000ff); // Do not reset a valid local immediate.
        begin();context->BeginPass(&local,0);immediate(local,blue.GetGPUTexture(),black.GetGPUTexture());node(foreignPass);draw(local,nullptr,nullptr,false,nullptr);verify(0xffffffff); // Actual node clobber requires local rebind.
        begin();draw(local,blue.GetGPUTexture(),green.GetGPUTexture(),true,nullptr);verify(0xff00ffff);
        begin();draw(shared,blue.GetGPUTexture(),blue.GetGPUTexture(),true,"stage=vertex slot=1 expectedType=3 actualType=4");
        begin();draw(shared,blue.GetGPUTexture(),black.GetGPUTexture(),true,nullptr);verify(0xff0000ff);
        begin();context->InvalidateState();node(sharedPass);draw(shared,nullptr,nullptr,false,nullptr);verify(0xff0000ff); // Invalidation retains logical values without stale shader.
        FX::ReleaseRenderMutex();device.DestroyTexture3D(&blue);device.DestroyTexture2D(&green);device.DestroyTexture2D(&black);
        for(auto* e:{&local,&previous,&foreign,&shared})FX::OnEffectRelease(e);
    }
    cubeSampling(device);
    FX::Shutdown();device.DestroyVolatile();device.Destroy();
    std::cout<<"Original FX mixed vertex 2D-white/3D-zero defaults, explicit textures, immediate bindings, strict mismatch rejection/retry, original shared/node/cache lifecycle plus both-stage cube/2D zero-RGBA transitions/allfaces GPU PASS (authored data)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
