// [Blur2PC][macOS] Original PC particle writer/count/map contract and actual GPU triangles.
#include "../../code/mercury/library/graphics/dynamic/stdafx.h"
#include <dynamic/Renderer.h>
#include <dynamic/DynamicVertexBuffer.h>
#include <dynamic/particle/Manager.h>
#include <dynamic/particle/Generic/SpriteRotating.h>
#include <dynamic/particle/Shape/All.h>
#include <dynamic/particle/Shape/Decal.h>
#include <dynamic/TextureCoords.h>
#include <dynamic/light/Generic/Shape3dQuad.h>
#include <dynamic/light/Shape/Quad2d.h>
#include <dynamic/light/Shape/Beam2d.h>
#include <dynamic/light/Shape/Beam3d.h>
#include <dynamic/light/Shape/Cross2d.h>
#include <dynamic/light/Shape/Decal3d.h>
#include <display/context/Context.h>
#include <system/scheduler/jobqueue.h>
#include <dynamic/shaders/Constants.h>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace DynamicEffect;
using namespace DynamicEffect::Particle;
namespace {
void Require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
RenderSettings Settings(){
    RenderSettings s;s.vertexShader=E_VertexShader_UV;s.pixelShader=E_PixelShader_Default;s.blendMode=E_BlendMode_Alpha;s.primitiveType=Display::E_PrimitiveType_TriangleList;s.pVertexDeclaration=nullptr;
    s.uvScale=MathsStorage::Float4(1,1,0,0);s.vertices=0;s.vertexSize=0;s.lodFade=MathsStorage::Float2(0,0);s.cullDistance=100;s.soft=false;s.additive=false;s.blendModeTransparent=true;
    auto& v=s.vertexBuildSettings;v.eVertexBuild=E_VertexBuild_SpriteRotating;v.eParticleShape=E_ParticleShape_SpriteRotating;v.eParticleSort=E_ParticleSort_PerBlock;v.eLimitSize=E_ParticleLimitSize_NoLimit;v.limitSize=MathsStorage::Float4A16(0,0,0,0);v.pivot=MathsStorage::Float4A16(0,0,0,0);v.eAtlasType=E_AtlasType_Random;v.fAtlasAnimPeriod=0;v.iAtlasSize=1;v.iAtlasOffset=0;v.iElementSize=sizeof(C_Particle3D_R1D);v.iParticleLibraryIndex=-1;v.pUserShape3DVariants=nullptr;
    v.sizeTable.vSize=MathsStorage::Float4A16(0,0,0,0);v.colourTable.vSize=MathsStorage::Float4A16(0,0,0,0);return s;
}

static std::vector<U8> blob(bool vertex){
    const std::string metadata=std::string("{\"parameters\":{},\"input_semantics\":")+(vertex?"[[0,0,0]]":"[]")+",\"reflection\":{\"ubos\":"+(vertex?"[]":"[{\"binding\":13,\"block_size\":16,\"name\":\"BlurPCAlphaTestState\"}]")+"}}";
    const std::string source=std::string("#include <metal_stdlib>\nusing namespace metal;\n")+(vertex?"struct I{float3 p[[attribute(0)]];};vertex float4 blur_main(I i[[stage_in]]){return float4(i.p,1);}":"struct Alpha{uint enabled,function;float reference;uint format;};fragment float4 blur_main(constant Alpha& a[[buffer(13)]]){if(a.enabled&&(a.function==1||(a.function==5&&.5<=a.reference)))discard_fragment();return float4(.25,.5,.75,.5);}");
    const std::string entry="blur_main";std::vector<U8> b(40);std::memcpy(b.data(),"BMSL",4);auto word=[&](U32 p,U32 n){for(U32 i=0;i<4;++i)b[p+i]=U8(n>>(8*i));};word(4,1);word(8,vertex?0:1);word(16,40);word(20,metadata.size());word(24,40+metadata.size());word(28,source.size());word(32,40+metadata.size()+source.size());word(36,entry.size());b.insert(b.end(),metadata.begin(),metadata.end());b.insert(b.end(),source.begin(),source.end());b.insert(b.end(),entry.begin(),entry.end());word(12,b.size());return b;
}
template<class Shape>void Metadata(U32 triangles){Require(Shape::GetPrimitiveType(nullptr)==Display::E_PrimitiveType_TriangleList&&Shape::GetNumPrimitives(nullptr)==triangles&&Shape::GetNumVertices(nullptr)==3,"Particle shape count/topology did not select original PC triangle contract");}
using ParticleVertex=ShapePolicy::SpriteRotatingDefault::ParticleVertex;
void CheckVertices(const ParticleVertex* v,const C_Particle3D_R1D* particles,C_TextureCoords& tex){
    constexpr U32 corner[]={0,1,2,2,3,0};
    for(U32 p=0;p<2;++p)for(U32 i=0;i<6;++i){const auto& out=v[p*6+i];const auto k=corner[i];const F32 sx=k==0||k==3?-.35f:.35f,sy=k<2?.35f:-.35f;
        const auto& uv=tex.GetTextureCoordinates<F32>(0);const auto& map=tex.GetTextureCoordinatesMapping(particles[p].iUV);const U32 mapped[]={map.m0,map.m1,map.m2,map.m3};
        Require(std::abs(out.x-(particles[p].vPosition.x+sx))<1e-5f&&std::abs(out.y-sy)<1e-5f&&std::abs(out.z-.5f)<1e-5f,"Original CPU particle writer lost duplicated triangle positions");
        Require(std::abs(out.u-uv.u[mapped[k]])<1e-5f&&std::abs(out.v-uv.v[mapped[k]])<1e-5f&&std::abs(out.w-.25f)<1e-5f,"Original CPU particle writer lost UV mapping or animation");
        Require(std::abs(out.r-.25f)<1e-5f&&std::abs(out.g-.5f)<1e-5f&&std::abs(out.b-.75f)<1e-5f&&std::abs(out.a-1.f)<1e-5f,"Original CPU particle writer lost colour data");
    }
}
template<class Policy>void LightMetadata(U32 triangles){Policy p;Require(p.GetPrimitiveType()==Display::E_PrimitiveType_TriangleList&&p.GetNumPrimitives()==triangles&&p.GetNumVertices()==3,"Dynamic light shape did not retain paired PC triangle count/topology");}
void ActualLight(C_Renderer& renderer){
    LightMetadata<Light::Quad2dPolicy<Light::Instance2d>>(2);LightMetadata<Light::Quad3dPolicy<Light::Instance3d_P>>(2);LightMetadata<Light::Beam2dPolicy<Light::Instance2d>>(2);LightMetadata<Light::Beam3dPolicy<Light::Instance3d_P2>>(2);LightMetadata<Light::Cross2dPolicy<Light::Instance2d>>(4);LightMetadata<Light::Decal3dPolicy<Light::Instance3d_P_R>>(2);
    Light::C_Manager manager(&renderer);Light::Shape3dQuad effect(&renderer,&manager,2,0);Light::RenderSettings settings{};settings.nAtlasWidth=settings.nAtlasHeight=1;settings.fRcpCullDistance=1;settings.viewTable.vSize=settings.distanceTable.vSize=MathsStorage::Float4A16(0,0,0,0);settings.viewTable.table.Resize(2);settings.distanceTable.table.Resize(2);for(U32 i=0;i<2;++i)settings.viewTable.table[i]=settings.distanceTable.table[i]=MathsStorage::Float4A16(1,1,1,1);effect.Create(settings);
    Light::Light light;light.m_iColour=Maths::RGBA(11,22,33,44);light.m_fIntensity=light.m_fView=light.m_fDistance=1;
    Light::Instance3d_P instance;instance.m_hLight=Light::LightHandle(&light);instance.m_vPosition=MathsStorage::Float3(0,0,.5f);instance.m_vDimensions=MathsStorage::Float2(.3f,.2f);instance.m_fIntensity=1;instance.m_iTexture=0;RenderNodePtrs blocks;blocks.PushBack(&instance);
    Light::Quad3dPolicy<Light::Instance3d_P> policy;policy.m_pEffect=&effect;using V=decltype(policy)::ShapeVertex;std::array<V,7> vertices;std::memset(vertices.data(),0xcd,sizeof vertices);Require(policy.Build(blocks,vertices.data())==vertices.data()+6,"Actual light writer did not emit complete original PC triangle pair");Require(std::memcmp(&vertices[0],&vertices[5],sizeof(V))==0&&std::memcmp(&vertices[2],&vertices[3],sizeof(V))==0,"Actual light writer lost repeated diagonal endpoints/UV/colour");Require(std::abs(vertices[0].x-vertices[1].x)>.1f&&std::abs(vertices[1].y-vertices[2].y)>.1f,"Actual light CPU triangles collapsed a required rectangle dimension");for(U32 i=0;i<sizeof(V);++i)Require(reinterpret_cast<U8*>(&vertices[6])[i]==0xcd,"Actual light writer exceeded paired PC triangle allocation");effect.Destroy();
}
void Pixels(Display2::C_Surface& surface,U32 colour=0x804080bf){auto map=surface.LockRect(16);Require(map.pBits,"Particle triangle readback failed");bool okay=true;for(U32 y=0;y<surface.GetHeight();++y)for(U32 x=0;x<surface.GetWidth();++x){const float px=(x+.5f)*2/surface.GetWidth()-1,py=1-(y+.5f)*2/surface.GetHeight();if(std::abs(std::abs(px)-.15f)<.04f||std::abs(std::abs(px)-.85f)<.04f||std::abs(std::abs(py)-.35f)<.04f)continue;const bool covered=std::abs(py)<.35f&&std::abs(px)>.15f&&std::abs(px)<.85f;U32 actual;std::memcpy(&actual,map.pBits+y*map.pitch+x*4,4);const U32 expected=covered?colour:0xff000000;for(U32 b=0;b<4;++b)okay&=std::abs(int((actual>>(8*b))&255)-int((expected>>(8*b))&255))<=1;}surface.UnlockRect();Require(okay,"Actual dynamic particle GPU triangles lost a corner, extent or cleared gap");}
}
int main(){try{
    Metadata<ShapePolicy::Beam>(2);Metadata<ShapePolicy::Cube>(12);Metadata<ShapePolicy::Decal>(2);Metadata<ShapePolicy::Quad>(2);Metadata<ShapePolicy::Sprite>(2);Metadata<ShapePolicy::SpriteDirection>(2);Metadata<ShapePolicy::SpriteLimitSize>(2);Metadata<ShapePolicy::SpriteRotatingDefault>(2);Metadata<ShapePolicy::SpriteRotatingLimitSize>(2);Metadata<ShapePolicy::SpriteRotatingSoft>(2);Metadata<ShapePolicy::SpriteRotatingSoftLimitSize>(2);
    Display::C_Device device;Display::C_DeviceParams params;params.m_Width=80;params.m_Height=40;
    Scheduler::Init();Scheduler::SetGroupPoolSize(32);Scheduler::Start();
    {C_Renderer renderer(nullptr);C_ParticleManager manager(&renderer);Require(!manager.UseSoft(),"Native particle manager did not retain original PC soft-particle policy");
    Display2::Device(&device.GetDisplay2());bool failed=false;try{failed=!manager.Create();}catch(const std::exception&){failed=true;}Require(failed,"Required particle shader initialization falsely succeeded on an uncreated real device");manager.Destroy();Display2::Device(nullptr);
    Require(device.Create(params)&&device.CreateVolatile(params),"Particle geometry actual display creation failed");Display::CreateVertexDeclarations();
    auto& backend=device.GetDisplay2();
#if TRACK_DISPLAY2_RESOURCES
    const auto vertexBaseline=backend.GetVertexShaderCount(),pixelBaseline=backend.GetPixelShaderCount();
    auto shaderCounts=[&]{Require(backend.GetVertexShaderCount()==vertexBaseline+16&&backend.GetPixelShaderCount()==pixelBaseline+7,"Particle initialization did not own its16 actual PC vertex variants and7 selected pixel shaders");};
#else
    auto shaderCounts=[]{}; // Optimized builds have no resource tracker; production readiness and actual draw still run.
#endif
    Require(manager.Create(),"Actual complete original particle shader initialization failed");shaderCounts();manager.RecreateShaders();shaderCounts();
    Display::C_View cameraView;Display::C_ViewSetup cameraSetup;cameraView.Setup(cameraSetup,80,40);device.SetView(&cameraView);ActualLight(renderer);
    SpriteRotating effect(&renderer,&manager,1);auto settings=Settings();UpdateSettings update;PhysicsSettings physics;physics.vAcceleration=MathsStorage::Float4A16(0,0,0,0);RenderStates states{};effect.Create(update,states,settings,physics);
    Require(effect.GetRenderSettings().primitiveType==Display::E_PrimitiveType_TriangleList&&effect.GetRenderSettings().vertices==6,"Actual effect initialization lost PC primitive/vertex product");
    auto& build=settings.vertexBuildSettings;build.colourTable.table.Resize(2);build.sizeTable.table.Resize(2);for(U32 i=0;i<2;++i){build.colourTable.table[i]=MathsStorage::Float4A16(.25,.5,.75,1);build.sizeTable.table[i]=MathsStorage::Float4A16(.35,.35,1,.25);}
    C_TextureCoords& tex=C_TextureCoords::Instance();VertexBuildConstants constants{};constants.vForward=MathsStorage::Float4A16(0,0,1,0);constants.vRight=MathsStorage::Float4A16(1,0,0,0);constants.vUp=MathsStorage::Float4A16(0,1,0,0);constants.pTexCoords=&tex;
    C_Particle3D_R1D particles[2]{};ParticleArray sorted;ParticleBlock block;block.m_Element.Reserve(sizeof(C_Particle3D_R1D),2);RenderNodePtrs blocks;blocks.PushBack(&block);
    for(U32 i=0;i<2;++i){auto& p=particles[i];p.vPosition=MathsStorage::Float3(i?.5f:-.5f,0,.5f);p.vColour=MathsStorage::Float4A16(1,1,1,1);p.vForceSizeLife=MathsStorage::Float4A16(0,0,1,1);p.iUV=i;sorted.PushBack(ParticleSortInfo(&p,i));std::memcpy(block.m_Element.PushBack(),&p,sizeof p);}
    std::array<ParticleVertex,13> output;std::memset(output.data(),0xcd,sizeof output);void* end=ShapePolicy::SpriteRotatingDefault::Build(&build,&constants,sorted,output.data());Require(end==output.data()+12,"Sorted particle writer byte count disagrees with initialized effect");CheckVertices(output.data(),particles,tex);for(U32 i=0;i<sizeof(ParticleVertex);++i)Require(reinterpret_cast<U8*>(&output[12])[i]==0xcd,"Sorted particle writer exceeded reserved PC vertex count");
    std::memset(output.data(),0xcd,sizeof output);end=ShapePolicy::SpriteRotatingDefault::Build(&build,&constants,blocks,output.data());Require(end==output.data()+12,"Block particle writer byte count disagrees with initialized effect");CheckVertices(output.data(),particles,tex);for(U32 i=0;i<sizeof(ParticleVertex);++i)Require(reinterpret_cast<U8*>(&output[12])[i]==0xcd,"Block particle writer exceeded reserved PC vertex count");
    auto& native=device.GetDisplay2();auto& c=native.GetContext();Display2::C_VertexShader vs;Display2::C_PixelShader ps;auto vb=blob(true),pb=blob(false);std::string error;Require(native.CreateVertexShader(vb.size(),vb.data(),0,&vs,nullptr,&error),error.c_str());Require(native.CreatePixelShader(pb.size(),pb.data(),0,&ps,nullptr,&error),error.c_str());Display2::C_VertexFormat format;format.AddElement(0,0,Display2::E_VertexElementType_F32_3,Display2::E_VertexElementUsage_Position,0);Display2::C_VertexDeclaration declaration;Require(native.CreateVertexDeclaration(format,&declaration),"Particle geometry declaration failed");
    {C_DynamicVertexBuffer dynamic;dynamic.SetVertexBufferSize(4096,6);dynamic.ReserveBlocks(8);Require(dynamic.OnCreateVolatile(),"Original particle dynamic buffers failed");dynamic.Begin();Display::C_Context context;context.m_Context.Bind(native);
        for(U32 frame=0;frame<3;++frame){dynamic.Begin();auto target=dynamic.Begin("native-particle-triangle-fixture",sizeof(ParticleVertex),12);Require(target.pVertices,"Original PC dynamic particle map unavailable");end=ShapePolicy::SpriteRotatingDefault::Build(&build,&constants,blocks,target.pVertices);Require(static_cast<U8*>(end)==static_cast<U8*>(target.pVertices)+12*sizeof(ParticleVertex),"Actual dynamic particle map has wrong byte count");CheckVertices(static_cast<const ParticleVertex*>(target.pVertices),particles,tex);dynamic.End(target);
            Require(device.BeginScene(),"Particle geometry scene failed");device.SetDepthEnable(Display::E_Bool_False);device.SetCullMode(Display::E_CullMode_None);device.SetBlendEnable(Display::E_Bool_False);device.SetAlphaTestEnable(Display::E_Bool_False);device.SetColourWriteEnable(Display::E_ColourWriteEnable_All);c.SetVertexShader(&vs);c.SetPixelShader(&ps);context.SetVertexDeclaration(&declaration);c.Clear(Display2::E_Clear_Colour,0xff000000);dynamic.DrawPrimitiveBlock(&context,effect.GetRenderSettings().primitiveType,target);device.EndScene();Pixels(*device.GetNativeColourSurface());
        }
        // [Blur2PC] Draw through the real Manager shader arrays that were NULL in MAC161.
        Display2::C_Texture white;Require(native.CreateTexture2D(1,1,1,0,Display2::E_TextureFormat_A8R8G8B8,&white),"Actual particle input texture allocation failed");auto whiteMap=white.LockRect2D(0,0);Require(whiteMap.pBits,"Actual particle input texture map failed");const U32 whitePixel=0xffffffff;std::memcpy(whiteMap.pBits,&whitePixel,4);white.UnlockRect2D(0);
        dynamic.Begin();auto actual=dynamic.Begin("actual-manager-particle-shader",sizeof(ParticleVertex),12);Require(actual.pVertices,"Actual particle shader geometry map failed");ShapePolicy::SpriteRotatingDefault::Build(&build,&constants,blocks,actual.pVertices);dynamic.End(actual);
        Require(device.BeginScene(),"Actual particle shader scene failed");device.SetDepthEnable(Display::E_Bool_False);device.SetCullMode(Display::E_CullMode_None);device.SetBlendEnable(Display::E_Bool_False);device.SetAlphaTestEnable(Display::E_Bool_False);device.SetColourWriteEnable(Display::E_ColourWriteEnable_All);
        manager.SetVertexShader(&context,E_VertexShader_UVW,E_VertexShaderVariation_Default);manager.SetPixelShader(&context,E_PixelShader_Default);context.SetVertexDeclaration(ParticleVertex::m_pDeclaration);c.SetTexture(0,&white);Display2::C_SamplerState sampler;sampler.SetTextureFilter(Display2::E_TextureFilter_Point);c.SetSamplerState(0,sampler);
        Display::C_Matrix identity;Display::MatrixSetIdentity(&identity);const F32 uvScale[]={1,1,1,1},lod[]={0,1,0,0},gain[]={1,1,0,0},fog[]={0,0,0,0};context.SetVertexShaderConstantF(CV_DISPLAY_VIEWPROJ_MATRIX,reinterpret_cast<const F32*>(&identity),4);context.SetVertexShaderConstantF(CV_RENDER_SETTINGS,uvScale,1);context.SetVertexShaderConstantF(CV_RENDER_SETTINGS_1,lod,1);context.SetPixelShaderConstantF(CP_RENDER_POSTPROCESS_LUMINANCE,gain,1);context.SetPixelShaderConstantF(CP_RENDER_FOG_DEPTH_SETTINGS,fog,1);c.Clear(Display2::E_Clear_Colour,0xff000000);dynamic.DrawPrimitiveBlock(&context,effect.GetRenderSettings().primitiveType,actual);device.EndScene();Pixels(*device.GetNativeColourSurface(),0x404080bf);native.Destroy(&white);
        dynamic.OnDestroyVolatile();}
    effect.Reset();manager.Destroy();
#if TRACK_DISPLAY2_RESOURCES
    Require(native.GetVertexShaderCount()==vertexBaseline+1&&native.GetPixelShaderCount()==pixelBaseline+1,"Particle shader retirement retained owned resources");
#endif
    native.Destroy(&declaration);native.Destroy(&vs);native.Destroy(&ps);Require(native.WaitIdle(),"Particle geometry GPU completion failed");}
    Scheduler::Shutdown();device.SetView(nullptr);device.DestroyVolatile();device.Destroy();std::cout<<"Original PC particle counts/topology, actual effect initialization, sorted/block six-vertex position/colour/UV writers, exact map byte bounds, actual dynamic Lock/Unlock and repeated GPU triangle coverage; real required particle shader failure/retry/recreate,16VS/7PS ownership and original Manager shader GPU draw PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
