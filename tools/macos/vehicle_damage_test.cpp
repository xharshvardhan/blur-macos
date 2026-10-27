// [Blur2PC][macOS] Original damage-node owner, static GPU pool, updates and failure boundaries.
#include "../../code/mercury/library/graphics/vehicle/VehiclePCH.h"
#include <vehicle/damage/DamageLayer.h>
#include <vehicle/VehicleModel.h>
#include <vehicle/Setup.h>
#include <vehicle/db/VehicleXt.h>
#include <display/buffer/VertexDeclarations.h>
#include <vector>
#include <string>
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace Vehicle {extern Debug::Int g_debugDamageTextureWidth,g_debugDamageTextureHeight,g_debugDamageTextureDepth;}
static void require(bool value,const char* why){if(!value){std::cerr<<why<<std::endl;throw std::runtime_error(why);}}
static void volume(Vehicle::C_DamageLayer& layer,bool changed=false){
    auto* texture=layer.GetDamageVolumeTexture();require(texture&&texture->IsCreated(),"Original damage-volume slot was not ready");
    require(texture->GetFormat()==Display2::E_TextureFormat_A32B32G32R32F,"Actual retail FXB requires raw float damage displacement");
    auto map=texture->LockBox3D(0,16);require(map.pBits,"Actual damage volume GPU readback failed");bool valid=true;
    for(U32 z=0;z<texture->GetDepth();++z)for(U32 y=0;y<texture->GetHeight();++y)for(U32 x=0;x<texture->GetWidth();++x){
        F32 value[4];std::memcpy(value,map.pBits+z*map.slicePitch+y*map.rowPitch+x*16,16);
        const F32 expected[4]={.25f,-.5f,.75f,.25f};
        for(U32 c=0;c<4;++c)valid&=std::isfinite(value[c])&&std::abs(value[c]-((changed&&!x&&!y&&!z)?expected[c]:0.f))<1.e-7f;
    }
    texture->UnlockBox3D(0);require(valid,"Actual retail float damage reset/update bytes disagreed across rows/slices");
}
static void filteredVolume(Display::C_Device& device,Vehicle::C_DamageLayer& layer){
    const std::string metadata=R"({"parameters":{},"input_semantics":[],"reflection":{"ubos":[],"textures":[{"binding":0,"type":"texture3D"}]}})";
    const std::string source="#include <metal_stdlib>\nusing namespace metal;fragment float4 blur_main(texture3d<float> t[[texture(0)]],sampler s[[sampler(0)]]){return t.sample(s,float3(.125,.125,1.0/3.0),level(0));}";
    const std::string entry="blur_main";std::vector<U8> bytes(40);std::memcpy(bytes.data(),"BMSL",4);auto word=[&](U32 at,U32 value){for(U32 i=0;i<4;++i)bytes[at+i]=U8(value>>(8*i));};word(4,1);word(8,1);word(16,40);word(20,metadata.size());word(24,40+metadata.size());word(28,source.size());word(32,40+metadata.size()+source.size());word(36,entry.size());bytes.insert(bytes.end(),metadata.begin(),metadata.end());bytes.insert(bytes.end(),source.begin(),source.end());bytes.insert(bytes.end(),entry.begin(),entry.end());word(12,bytes.size());
    Display::C_PixelShader shader;require(device.GetDisplay2().CreatePixelShader(bytes.size(),bytes.data(),0,&shader),"Damage linear-sampling probe shader failed");
    Display::C_RenderTexture2D output;require(device.CreateRenderTexture2D(2,2,1,Display::E_Usage_Colour,Display::E_Format_A32B32G32R32F,Display::E_Multisample_None,nullptr,&output),"Damage filtered sample target failed");
    const std::string vertexMetadata=R"({"parameters":{},"input_semantics":[[0,0,0]],"reflection":{"ubos":[]}})";
    const std::string vertexSource="#include <metal_stdlib>\nusing namespace metal;struct I{float2 position[[attribute(0)]];};vertex float4 blur_main(I input[[stage_in]]){return float4(input.position,0,1);}";
    bytes.resize(40);word(8,0);word(20,vertexMetadata.size());word(24,40+vertexMetadata.size());word(28,vertexSource.size());word(32,40+vertexMetadata.size()+vertexSource.size());bytes.insert(bytes.end(),vertexMetadata.begin(),vertexMetadata.end());bytes.insert(bytes.end(),vertexSource.begin(),vertexSource.end());bytes.insert(bytes.end(),entry.begin(),entry.end());word(12,bytes.size());
    Display::C_VertexShader vertex;Display::C_VertexDeclaration declaration;Display::C_VertexBuffer vertices;Display::C_VertexFormat format;format.AddElement(0,0,Display::E_VertexElementType_F32_2,Display::E_VertexElementUsage_Position,0);
    std::string shaderError;require(device.GetDisplay2().CreateVertexShader(bytes.size(),bytes.data(),0,&vertex,nullptr,&shaderError),shaderError.c_str());require(device.GetDisplay2().CreateVertexDeclaration(format,&declaration)&&device.GetDisplay2().CreateVertexBuffer(32,0,&vertices),"Damage filtered sample geometry failed");const F32 quad[8]={-1,1,1,1,-1,-1,1,-1};auto* mapped=vertices.Lock();require(mapped,"Damage filtered geometry map failed");std::memcpy(mapped,quad,sizeof(quad));vertices.Unlock();
    require(device.BeginScene(),"Damage filtered sample scene failed");device.SetColourDepthStencilSurface(&output.GetSurface(),nullptr);Display::C_Viewport viewport;viewport.m_W=viewport.m_H=2;device.SetViewport(viewport);device.SetDepthEnable(Display::E_Bool_False);device.SetAlphaTestEnable(Display::E_Bool_False);device.SetStencilEnable(Display::E_Bool_False);device.SetCullMode(Display::E_CullMode_None);device.SetBlendMode(Display::E_BlendMode_Solid);device.SetColourWriteEnable(Display::E_ColourWriteEnable_All);device.SetVertexShader(&vertex);device.SetPixelShader(&shader);device.SetTexture(0,layer.GetDamageVolumeTexture());device.SetTextureAddress(0,Display::E_TextureAddress_Clamp);device.SetTextureFilter(0,Display::E_TextureFilter_Linear);device.GetDisplay2().GetContext().SetVertexBuffer(0,&vertices,0,8,&declaration);device.GetDisplay2().GetContext().DrawPrimitive(Display2::E_PrimitiveType_TriangleStrip,0,4);device.EndScene();
    auto pixels=output.GetTexture().LockRect2D(0,16);require(pixels.pBits,"Damage filtered GPU readback failed");const F32 expected[4]={.03125f,-.0625f,.09375f,.03125f};bool valid=true;for(U32 y=0;y<2;++y)for(U32 x=0;x<2;++x){F32 value[4];std::memcpy(value,pixels.pBits+y*pixels.pitch+x*16,16);for(U32 c=0;c<4;++c){if(std::abs(value[c]-expected[c])>=1.e-6f)std::cerr<<"Filtered ["<<x<<","<<y<<"] component"<<c<<" actual="<<value[c]<<" expected="<<expected[c]<<std::endl;valid&=std::abs(value[c]-expected[c])<1.e-6f;}}output.GetTexture().UnlockRect2D(0);device.SetTexture(0,nullptr);device.SetColourDepthStencilSurface();device.Destroy(&output);device.Destroy(&shader);device.Destroy(&vertex);device.GetDisplay2().Destroy(&vertices);device.GetDisplay2().Destroy(&declaration);require(valid,"Actual32-bit-float 3D linear filtering did not interpolate all8 corners");
}
int main(){try{
    Display::C_Device device;Display::C_DeviceParams params;params.m_Width=params.m_Height=16;
    require(device.Create(params)&&device.CreateVolatile(params),"Damage fixture native device initialization failed");
#if TRACK_DISPLAY2_RESOURCES
    const auto baseline=device.GetDisplay2().GetTextureCount();
#endif
    using Vehicle::C_DamageLayer;
    require(!C_DamageLayer::CreateTextureResources(21),"Fixed twenty-slot damage pool accepted an invalid vehicle count");
    Vehicle::g_debugDamageTextureWidth=9;
    require(!C_DamageLayer::CreateTextureResources(1),"Damage lattice exceeded its original64-node fixed array");
    Vehicle::g_debugDamageTextureWidth=8;Vehicle::g_debugDamageTextureDepth=20000;
    require(!C_DamageLayer::CreateTextureResources(1),"Unsupported real GPU volume allocation falsely succeeded");
    Vehicle::g_debugDamageTextureDepth=3;
#if TRACK_DISPLAY2_RESOURCES
    require(device.GetDisplay2().GetTextureCount()==baseline,"Failed pool acquisition retained tracked GPU textures");
#endif
    for(U32 lifetime=0;lifetime<2;++lifetime){
        require(C_DamageLayer::CreateTextureResources(1),"Actual damage texture pool acquisition failed");
        require(!C_DamageLayer::CreateTextureResources(1),"Live pool ownership was overwritten");
#if TRACK_DISPLAY2_RESOURCES
        require(device.GetDisplay2().GetTextureCount()==baseline+4,"Original double-buffered volume/info pool was incomplete");
#endif
        C_DamageLayer layer(nullptr);require(layer.GetNumLayers()==0,"Native node pointer did not refer to its empty owned lattice");
        layer.GetVehicleInfo()->m_Centre=Maths::HVec3(0,0,0);layer.GetVehicleInfo()->m_Extents=Maths::HVec3(1,1,2);
        require(!layer.Create(1),"Out-of-range borrowed damage slot was published");
        require(layer.Create(0),"Original damage-layer Create failed");require(layer.GetNumLayers()==3&&layer.GetNumWidth(0)==8&&layer.GetNumLength(0)==8,"Original node lattice was not created");
        for(S32 z=0;z<3;++z)for(S32 y=0;y<8;++y)for(S32 x=0;x<8;++x){auto* node=layer.GetNode(z,y,x);require(node&&std::isfinite(node->m_Position.X())&&std::isfinite(node->m_Position.Y())&&std::isfinite(node->m_Position.Z()),"Initial empty-curve lattice contains uninitialized coordinates");}
        volume(layer);layer.SwapTextures();volume(layer);
        auto* node=layer.GetNode(0,0,0);node->m_Position=node->m_OriginalPosition+Maths::HVec3(.25f,-.5f,.75f);node->m_ScratchLevel=.5f;node->m_DeformLevel=.25f;layer.UpdateVolumeTexture();volume(layer,true);filteredVolume(device,layer);
        auto* info=layer.GetDamageInfoTexture();auto infoMap=info->LockRect2D(0,16);require(infoMap.pBits,"Actual damage information GPU readback failed");bool infoZero=true;for(U32 y=0;y<info->GetHeight();++y)for(U32 x=0;x<info->GetWidth()*4;++x)infoZero&=infoMap.pBits[y*infoMap.pitch+x]==0;info->UnlockRect2D(0);require(infoZero,"Original information reset pixels were not initialized");
        // Holding the next real volume map exercises the backend's incompatible-map failure.
        layer.SwapTextures();auto* nextVolume=layer.GetDamageVolumeTexture();layer.SwapTextures();auto held=nextVolume->LockBox3D(0);require(held.pBits,"Damage map-failure fixture could not hold actual texture");bool mapRejected=false;try{layer.UpdateVolumeTexture();}catch(const std::runtime_error&){mapRejected=true;}nextVolume->UnlockBox3D(0);require(mapRejected,"Required failed GPU map was accepted");
        layer.Destroy();layer.Destroy();require(layer.GetNumLayers()==0,"Retired damage layer retained node borrowers/lattice state");
        C_DamageLayer::DestroyTextureResources();C_DamageLayer::DestroyTextureResources();
#if TRACK_DISPLAY2_RESOURCES
        require(device.GetDisplay2().GetTextureCount()==baseline,"Original damage owners survived pool teardown");
#endif
    }
    {
        Vehicle::C_VehicleModel model;Vehicle::C_VehicleModelSetup setup;setup.m_Index=0;
        require(model.Create(&setup),"Actual vehicle-model layer acquisition failed");
        bool rejected=false;try{model.Initialise(nullptr);}catch(const std::runtime_error&){rejected=true;}
        require(rejected&&!model.GetDamageLayer(),"Missing authored damage setup did not retire partial model owners");
        require(model.Destroy(),"Repeated failed-model retirement failed");
        require(model.Create(&setup),"Vehicle-model acquisition did not recover after failure");
        Vehicle::XtVehicle vehicle;Vehicle::XtDamageCurveSetup curve{};XtRecord curveRecord{};curveRecord.object=&curve;vehicle.m_DamageCurveSetup=XtHandle<Vehicle::XtDamageCurveSetup>(&curveRecord,0,0);
        rejected=false;try{model.Initialise(&vehicle);}catch(const std::runtime_error&){rejected=true;}
        require(rejected&&!model.GetDamageLayer(),"Missing damage pool falsely initialized a vehicle model");model.Destroy();
    }
    require(device.GetDisplay2().WaitIdle(),"Damage GPU completion failed");device.DestroyVolatile();device.Destroy();
    std::cout<<"Original native single-buffer damage lattice,64-node/pool bounds, real GPU allocation failure rollback, two lifetimes/four textures, exact reset+deformed GPU volume rows/slices and vehicle-model required initialization rollback/retry PASS"<<std::endl;return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
