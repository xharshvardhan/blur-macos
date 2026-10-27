// [Blur2PC][macOS] Actual original packed blob PreRender and depth/stencil screen shaders; authored caster geometry, real PAK resource owner.
#include "../../code/mercury/library/gamesys/blobshadows/stdafx.h"
#include <blobshadows/BlobShadows.h>
#include <scenegraph/Context.h>
#define SCENEGRAPH_ALLOW_INCLUDE
#include <scenegraph/SceneGraphImpl.h>
#include <fx/Fx.h>
#include <fx/RenderMutex.h>
#include <display/buffer/VertexDeclarations.h>
#include <display/window/Window.h>
#include <display2/utils/Utils.h>
#include <resource/Resource.h>
#include <system/artery/Artery.h>
#include <system/io/ASync.h>
#include <system/scheduler/jobqueue.h>
#include <system/serialise/C_FileSystem.h>
#include <system/serialise/FileSystemGlobals.h>
#include <pakfs/PakFs.h>
#include <pakfs/PakGlobals.h>
#include <overlayfs/OverlayFs.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <array>
#include <cmath>
#include <vector>
#include <string>
namespace {
void Require(bool ok,const char* text){if(!ok){std::cerr<<text<<std::endl;throw std::runtime_error(text);}}
static std::vector<U8> shader(bool vertex){
    const std::string metadata=vertex?R"({"parameters":{},"input_semantics":[[0,0,0]],"reflection":{"ubos":[]}})":R"({"parameters":{"colour":[0,1]},"input_semantics":[],"reflection":{"ubos":[{"name":"PixelConstants","binding":0,"block_size":16}]}})";
    const std::string source=vertex?
        "#include <metal_stdlib>\nusing namespace metal;struct Input{float3 position[[attribute(0)]];};struct Output{float4 p[[position]];};vertex Output blur_main(Input v[[stage_in]]){return {float4(v.position,1)};}\n":
        "#include <metal_stdlib>\nusing namespace metal;fragment float4 blur_main(constant float4& colour[[buffer(0)]]){return colour;}\n";
    const std::string entry="blur_main";std::vector<U8> bytes(40);std::memcpy(bytes.data(),"BMSL",4);
    auto put=[&](U32 p,U32 v){for(U32 i=0;i<4;++i)bytes[p+i]=U8(v>>(8*i));};put(4,1);put(8,vertex?0:1);put(16,40);put(20,metadata.size());put(24,40+metadata.size());put(28,source.size());put(32,40+metadata.size()+source.size());put(36,entry.size());
    bytes.insert(bytes.end(),metadata.begin(),metadata.end());bytes.insert(bytes.end(),source.begin(),source.end());bytes.insert(bytes.end(),entry.begin(),entry.end());put(12,bytes.size());return bytes;
}

struct IoScheduler {
    IoScheduler(){Require(IoInitialise(),"Original I/O initialization failed");static const Scheduler::Policy workers[8]={5,5,5,5,5,5,5,5};Pak::SetPacketCompressionPolicy(Scheduler::CreatePolicy(workers));Scheduler::Init();Scheduler::SetGroupPoolSize(128);Scheduler::jqEnableWorkers(Scheduler::JQ_CORE_1|Scheduler::JQ_CORE_3);Scheduler::Start();}
    ~IoScheduler(){Scheduler::Shutdown();Pak::SetPacketCompressionPolicy(-1);IoShutdown();}
};
}
int main(int argc,char** argv){try {
    Require(argc==3,"Expected original data root and source-built shader root");const auto data=std::filesystem::absolute(argv[1]),shaders=std::filesystem::absolute(argv[2]);
    Require(ArteryStartupEx(true,"Native Packed Blob Fixture",16),"Original Artery startup failed");struct ArteryScope{~ArteryScope(){ArteryCleanup();}} artery;IoScheduler io;
    auto& physical=Sys::C_FileSystem::Instance();Require(physical.SetWorkingDirectory("/"),"Physical filesystem root unavailable");LogicalFs::C_PakFs pak;Require(pak.OpenPak(&physical,(data/"cache/root.pak").c_str(),nullptr),"Original root.pak unavailable");LogicalFs::C_OverlayFs fs;LogicalFs::C_OverlayLayer::C_Params layer;layer.flags.layerType=LogicalFs::C_OverlayLayer::E_Type_Overlay;
    fs.AddLayer("/",&pak,"/",layer,LogicalFs::E_MediaLayer_0,"Original root PAK");fs.AddLayer("/",&physical,"/",layer,LogicalFs::E_MediaLayer_0,"Physical source shaders");Require(fs.SetWorkingDirectory("/"),"Original overlay filesystem root unavailable");Sys::SetDefaultFileSystem(&fs);Display2::SetNativeShaderResourceRoot(shaders.string().c_str());
    Display::C_Device device;device.AddWindow(Maths::C_Rect(Maths::HVec2(0,0),Maths::HVec2(1,1)));Display::C_DeviceParams params;params.m_Width=params.m_Height=64;Require(device.Create(params)&&device.CreateVolatile(params),"Actual native blob device failed");Require(device.GetDepthFlip()&&!device.GetDepthViewportFlip()&&device.GetDepthClear()==0,"Native device lost the original PC reversed comparison/clear policy");
    {
        Display::C_View view;Display::C_ViewSetup setup;setup.m_ViewSetup3D.m_NearClip=1;setup.m_ViewSetup3D.m_FarClip=100;
        for(bool ortho:{false,true}){auto config=setup.m_ViewSetup3D;if(ortho)config.m_PerspDist=0;Maths::HMat44 projection;view.CalculateProjectionMatrix(config,projection);const auto nearPoint=Maths::HVec4(0,0,1,1)*projection,farPoint=Maths::HVec4(0,0,100,1)*projection;Require(std::abs(F32(nearPoint.Z()/nearPoint.W())-1)<1e-5f&&std::abs(F32(farPoint.Z()/farPoint.W()))<1e-5f,"Actual native perspective/orthographic projection is not PC near1/far0");}
        Display::C_VertexShader vertex;Display::C_PixelShader pixel;auto v=shader(true),p=shader(false);Require(device.CreateVertexShader(v.size(),reinterpret_cast<const S32*>(v.data()),0,&vertex)&&device.CreatePixelShader(p.size(),reinterpret_cast<const S32*>(p.data()),0,&pixel),"Authored depth-order GPU fixture shaders failed");
        Display::C_VertexElement elements[]={Display::C_VertexElement(0,0,Display::E_VertexElementType_F32_3,Display::E_VertexElementUsage_Position,0),VERTEXELEMENT_END()};auto* declaration=device.CreateVertexDeclaration(elements);Require(declaration,"Authored depth-order declaration failed");const auto colour=pixel.GetParamFromName("colour");
        for(U32 order=0;order<2;++order){Require(device.BeginScene(),"Actual default depth-order scene failed");device.Clear(Display::E_Clear_Colour|Display::E_Clear_Depth,Maths::RGBA(0,0,0,255));device.SetCullMode(Display::E_CullMode_None);device.SetBlendMode(Display::E_BlendMode_Solid);device.SetColourWriteEnable(Display::E_ColourWriteEnable_All);device.SetVertexShader(&vertex);device.SetPixelShader(&pixel);device.SetPixelShaderParameters(&pixel);
            for(U32 draw=0;draw<2;++draw){const bool near=(draw!=order);const F32 tint[]={near?0.f:1.f,near?1.f:0.f,0,1};device.SetPixelShaderParameterF(colour,tint);auto* positions=static_cast<F32*>(device.BeginVertices(Display::E_PrimitiveType_TriangleList,3,12,declaration));Require(positions,"Authored depth-order vertices failed");const F32 z=near?.8f:.2f;const F32 triangle[]={-1,-1,z,3,-1,z,-1,3,z};memcpy(positions,triangle,sizeof(triangle));device.EndVertices();}device.EndScene();auto image=device.GetNativeColourSurface()->LockRect(16);Require(image.pBits,"Actual default depth-order readback failed");bool good=true;for(U32 y=0;y<64;++y)for(U32 x=0;x<64;++x){U32 value;memcpy(&value,image.pBits+y*image.pitch+x*4,4);good&=value==0xff00ff00;}device.GetNativeColourSurface()->UnlockRect();Require(good,"Native PC default clear/GreaterEqual failed near-over-far depth ordering in one draw order");}
        device.Destroy(&pixel);device.Destroy(&vertex);
    }
    Display::CreateVertexDeclarations();Display::CreateVertexShaders();Display::CreatePixelShaders();FX::Initialise();
    {
        Resource::C_Manager resources;resources.SetFileSystems(&fs,nullptr);
        // [Blur2PC] CloneState(nullptr) reads the owning graph flags; use the original graph-owned context.
        SceneGraph::C_SceneGraph scene;auto& context=*scene.GetContext();context.CloneState(nullptr);
        {
            BlobShadows::C_BlobShadows blob;BlobShadows::C_BlobShadows::C_BlobShadowsInitSettings settings;settings.m_Num256Textures=0;settings.m_Num128Textures=0;settings.m_Num64Textures=1;settings.m_Num32Textures=0;settings.m_MinBlurResolution=128;
            blob.Create(settings,&context);Require(blob.Display::I_Renderer::Create()&&blob.CreateVolatile(),"Actual original blob shaders/targets failed required initialization");
            resources.WaitForAllResourceData();resources.PartialUpdate();Require(!blob.m_resourcesPending,"Original PAK imposter resource did not publish real readiness");Require(blob.m_BlobRenderTexture.Size()==1&&blob.m_ScreenBlobChannelPixelShader[3].IsCreated(),"Native did not own original packed PC channel resources");
            Display::C_VertexBuffer vertices[4];Display::C_IndexBuffer indices;
            const U16 quad[]={0,1,2,0,2,3,2,1,0,3,2,0};Require(device.CreateIndexBuffer(sizeof(quad),0,Display::E_Format_Index16,&indices),"Actual caster index buffer failed");auto* ix=indices.Lock();Require(ix,"Actual caster index map failed");memcpy(ix,quad,sizeof(quad));indices.Unlock();
            BlobShadows::C_DynamicBlobShadowInstance* instances[4]{};
            for(U32 i=0;i<4;++i){const F32 size=3.f-F32(i)*.6f;const Display::Vertex_XYZ geometry[]={{-size,0,-size},{size,0,-size},{size,0,size},{-size,0,size}};Require(device.CreateVertexBuffer(sizeof(geometry),0,&vertices[i]),"Actual caster vertex buffer failed");auto* v=vertices[i].Lock();Require(v,"Actual caster vertex map failed");memcpy(v,geometry,sizeof(geometry));vertices[i].Unlock();instances[i]=blob.CreateInstance(&vertices[i],4,&indices,12);instances[i]->SetVisibilityFlags(1u<<i);Maths::HMat43 transform;transform.SetIdentity();transform.AW(Maths::HVec3(0,1,0));instances[i]->UpdatePosition(transform,Maths::HVec3(0,0,0),Maths::HVec3(0,1,0),F32(i+1),1.f,1.f);}
            blob.SetDropShadowIntensity(.4f);
            Require(device.BeginScene(),"Actual blob PreRender scene failed");FX::LockRenderMutex();blob.PreRender();FX::ReleaseRenderMutex();device.SetColourDepthStencilSurface();device.EndScene();
            Require(blob.m_ProcessShadows.Size()==4&&blob.m_BlobRenderTexture[0].m_Used==15,"Original allocator did not pack four casters into four channels");
            for(U32 i=0;i<4;++i)Require(blob.m_ProcessShadows[i].m_RenderTextureIndex==i,"Original channel assignment did not preserve sorted caster ownership");
            auto& packed=blob.m_BlobRenderTexture[0].m_Texture;packed.Resolve();auto read=packed.GetTexture().LockRect2D(0,16);Require(read.pBits,"Actual packed shadow GPU readback unavailable");U32 channelPixels[4]{};for(U32 y=0;y<64;++y)for(U32 x=0;x<64;++x){const U8* p=read.pBits+y*read.pitch+x*4;for(U32 c=0;c<4;++c)channelPixels[c]+=p[c==0?2:c==2?0:c]>0;}packed.GetTexture().UnlockRect2D(0);std::cerr<<"Actual RGBA caster coverage "<<channelPixels[0]<<' '<<channelPixels[1]<<' '<<channelPixels[2]<<' '<<channelPixels[3]<<std::endl;Require(channelPixels[0]>channelPixels[1]&&channelPixels[1]>channelPixels[2]&&channelPixels[2]>channelPixels[3]&&channelPixels[3]>0,"Original PC per-channel projection did not preserve four differently sized caster silhouettes");
            Display::C_Texture2D depth;Require(device.CreateTexture2D(64,64,1,Display::E_Usage_Managed,Display::E_Format_R32F,&depth),"Actual authored receiver depth failed");auto d=depth.Lock(0);const F32 z=.9f;Require(d.pBits,"Actual receiver depth map failed");for(U32 y=0;y<64;++y)for(U32 x=0;x<64;++x)memcpy(d.pBits+y*d.pitch+x*4,&z,4);depth.Unlock(0);auto* window=device.GetWindow(0);window->m_pDepthTexture=&depth;
            Display::C_View view;Display::C_ViewSetup viewSetup;viewSetup.m_ViewSetup3D.m_NearClip=1;viewSetup.m_ViewSetup3D.m_FarClip=100;device.SetupView(&view,viewSetup);
            Require(device.BeginScene(),"Actual window stencil-mask scene failed");device.SetWindow(0);device.SetView(&view);auto& lower=device.GetDisplay2().GetContext();lower.SetViewport(Display2::C_Viewport(0,0,64,64));lower.Clear(Display2::E_Clear_Stencil,0,1,0);lower.SetViewport(Display2::C_Viewport(0,0,32,64));lower.Clear(Display2::E_Clear_Stencil,0,1,0xff);lower.SetViewport(Display2::C_Viewport(0,0,64,64));device.RenderWindowMask();device.EndWindow();device.SetColourDepthStencilSurface();device.EndScene();
            auto maskRead=window->GetMaskTexture()->LockRect2D(0,16);Require(maskRead.pBits,"Actual window mask readback failed");U32 maskLeft=0,maskRight=0;for(U32 y=0;y<64;++y)for(U32 x=0;x<64;++x){const auto* pixel=maskRead.pBits+y*maskRead.pitch+x*4;if(pixel[0]>250)(x<32?maskLeft:maskRight)++;}window->GetMaskTexture()->UnlockRect2D(0);std::cerr<<"Actual blue mask coverage "<<maskLeft<<' '<<maskRight<<std::endl;Require(maskLeft==2048&&maskRight==0,"Actual fixture window mask does not contain the GPU half-stencil");
            auto* display=Display::GetContext(&context);display->SetStencilTexture(window->GetMaskTexture());blob.SetDepthTexture(&depth);
            U32 changed[4]{};
            for(U32 i=0;i<4;++i){for(U32 j=0;j<4;++j)instances[j]->SetVisibilityFlags(i==j?1:0);Require(device.BeginScene(),"Actual original screen-blob scene failed");device.SetColourDepthStencilSurface();device.SetView(&view);Display::C_Viewport viewport;viewport.m_W=viewport.m_H=64;device.SetViewport(viewport);device.SetColourWriteEnable(Display::E_ColourWriteEnable_All);device.Clear(Display::E_Clear_Colour,Maths::RGBA(255,255,255,255),1,0);blob.ScreenRender();device.EndScene();auto image=device.GetNativeColourSurface()->LockRect(16);Require(image.pBits,"Actual screen-blob output readback unavailable");bool maskGood=true;U32 maskedChanged=0;for(U32 y=0;y<64;++y)for(U32 x=0;x<64;++x){const U8* p=image.pBits+y*image.pitch+x*4;if(x<30){if(p[0]!=255||p[1]!=255||p[2]!=255){if(!maskedChanged)std::cerr<<"Masked pixel "<<x<<','<<y<<" RGB "<<U32(p[2])<<' '<<U32(p[1])<<' '<<U32(p[0])<<std::endl;++maskedChanged;maskGood=false;}}else if(x>33)changed[i]+=p[0]<254;}device.GetNativeColourSurface()->UnlockRect();std::cerr<<"Actual masked changed "<<maskedChanged<<std::endl;Require(maskGood,"Original screen blob failed to honor actual blue stencil car mask");std::cerr<<"Actual channel "<<i<<" receiver changed "<<changed[i]<<std::endl;Require(changed[i]>0,"Original selected blob shader did not darken any unmasked receiver pixels");}
            display->SetStencilTexture(nullptr);display->SetColourDepthStencilTexture(nullptr,nullptr);window->m_pDepthTexture=nullptr;device.SetView(nullptr);device.Destroy(&depth);
            for(U32 i=0;i<4;++i){blob.DestroyInstance(instances[i]);device.Destroy(&vertices[i]);}device.Destroy(&indices);blob.DestroyVolatile();blob.Display::I_Renderer::Destroy();
        }
        Require(resources.NativeDrainUnreferencedResources(),"Original blob resource owner failed to drain");
    }
    FX::Shutdown();Display::DestroyPixelShaders();Display::DestroyVertexShaders();Require(device.GetDisplay2().WaitIdle(),"Actual blob GPU completion failed");device.DestroyVolatile();device.Destroy();Display2::SetNativeShaderResourceRoot("");Sys::SetDefaultFileSystem(nullptr);pak.ClosePak();
    std::cout<<"Original PC blob shader ownership, actual RGBA caster projection/packing, real PAK readiness, real depth/stencil screen-mask sampling and complete GPU/resource teardown PASS (authored geometry, not BMW gameplay)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
