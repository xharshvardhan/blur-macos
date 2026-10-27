// [Blur2PC][macOS] GPU-written opaque depth/stencil mips, sampling, read/write resolve, copy and feedback rejection.
#include <display2/device/MetalDevice.h>
#include <display2/context/Context.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <cmath>
using namespace Display2;
static void require(bool v,const char* message){if(!v)throw std::runtime_error(message);}
template<class F>static void rejects(F call,const char* message){bool failed=false;try{call();}catch(const std::logic_error&){failed=true;}require(failed,message);}
static std::vector<U8> shader(bool vertex){
    const std::string metadata=vertex?R"({"parameters":{},"input_semantics":[],"reflection":{"ubos":[]}})":R"({"parameters":{"lod":[0,1]},"input_semantics":[],"reflection":{"ubos":[{"name":"PixelConstants","binding":0,"block_size":16}],"separate_images":[{"binding":0,"type":"texture2D"}],"separate_samplers":[{"binding":0}]}})";
    const std::string source=vertex?
        "#include <metal_stdlib>\nusing namespace metal;struct Output{float4 p[[position]];};vertex Output blur_main(uint i[[vertex_id]]){const float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};return {float4(p[i],.5,1)};}\n":
        "#include <metal_stdlib>\nusing namespace metal;fragment float4 blur_main(texture2d<float> image[[texture(0)]],sampler sampleState[[sampler(0)]],constant float4& values[[buffer(0)]]){float z=image.sample(sampleState,float2(.5),level(values.x)).r;return float4(z,z,z,1);}\n";
    const std::string entry="blur_main";std::vector<U8> bytes(40);std::memcpy(bytes.data(),"BMSL",4);
    auto put=[&](U32 p,U32 v){for(U32 i=0;i<4;++i)bytes[p+i]=U8(v>>(8*i));};put(4,1);put(8,vertex?0:1);put(16,40);put(20,metadata.size());put(24,40+metadata.size());put(28,source.size());put(32,40+metadata.size()+source.size());put(36,entry.size());
    bytes.insert(bytes.end(),metadata.begin(),metadata.end());bytes.insert(bytes.end(),source.begin(),source.end());bytes.insert(bytes.end(),entry.begin(),entry.end());put(12,bytes.size());return bytes;
}

int main(){try{
    C_MetalDevice device;require(device.Create(),"Depth texture GPU unavailable");C_RenderTexture2D target,colour;C_Surface output;
    require(device.CreateRenderTexture2D(16,16,2,E_Flags_DepthStencil|E_Flags_ReadWrite,E_TextureFormat_D24S8,E_SurfaceFormat_D24S8,E_Multisample_None,nullptr,&target)&&device.CreateRenderTexture2D(16,16,2,E_Flags_Colour,E_TextureFormat_A8R8G8B8,E_SurfaceFormat_A8R8G8B8,E_Multisample_None,nullptr,&colour)&&device.CreateSurface(16,16,E_Flags_Colour,E_SurfaceFormat_A8R8G8B8,E_Multisample_None,nullptr,&output),"Native depth/colour mip attachments failed");
    require(GetTexture2DSize(16,16,2,0,E_TextureFormat_D24S8)==1280&&target.GetTexture().GetSize()>=1280,"Nominal PC depth texture size or native allocation is invalid");
    require(!target.GetSurface().LockRect(16).pBits&&!target.GetTexture().LockRect2D(0).pBits,"Opaque depth exposed an invented packed CPU layout");
    C_Texture rejected;require(!device.CreateTexture2D(16,16,1,E_Flags_Colour,E_TextureFormat_D24S8,&rejected)&&!device.CreateTexture3D(16,16,2,1,E_Flags_DepthStencil,E_TextureFormat_D24S8,&rejected)&&!device.CreateTextureCube(16,1,E_Flags_DepthStencil,E_TextureFormat_D24S8,&rejected),"Unintegrated colour/volume/cube depth format silently enabled");
    C_VertexShader vertex;C_PixelShader pixel;auto v=shader(true),p=shader(false);std::string error;
    bool programs=device.CreateVertexShader(v.size(),v.data(),0,&vertex,nullptr,&error)&&device.CreatePixelShader(p.size(),p.data(),0,&pixel,nullptr,&error);require(programs,error.c_str());
    C_VertexFormat format;format.AddElement(0,E_VertexElementType_F32_3,E_VertexElementUsage_Position,0);C_VertexDeclaration declaration;require(device.CreateVertexDeclaration(format,&declaration),"Depth texture declaration failed");
    auto& context=device.GetContext();context.SetVertexDeclaration(&declaration);context.SetVertexShader(&vertex);context.SetPixelShader(&pixel);context.SetCullMode(E_CullMode_None);context.SetDepthEnable(E_Bool_False);C_SamplerState sampler;sampler.SetTextureAddress(E_TextureAddress_Clamp);sampler.SetTextureFilter(E_TextureFilter_Point,E_TextureFilter_Point,E_TextureFilter_Point);context.SetSamplerState(0,sampler);
    auto write=[&](F32 first,F32 second){context.SetTexture(0,nullptr);for(U32 level=0;level<2;++level){context.SetColourSurface(0,&colour.GetSurface(level));context.SetDepthStencilSurface(&target.GetSurface(level));context.SetViewport(C_Viewport(0,0,16>>level,16>>level));context.BeginScene();context.Clear(E_Clear_Colour|E_Clear_Depth|E_Clear_Stencil,0xff000000,level?second:first,7);context.EndScene();}};
    auto read=[&](const C_Texture& texture,U32 level,F32 expected,bool retainWriteAttachment=false){context.SetColourSurface(0,&output);context.SetDepthStencilSurface(retainWriteAttachment?&target.GetSurface():nullptr);context.SetViewport(C_Viewport(0,0,16,16));context.SetTexture(0,&texture);const F32 values[]={F32(level),0,0,0};context.SetPixelShaderParamF(pixel.GetParamFromName("lod"),values);context.BeginScene();context.Clear(E_Clear_Colour,0xff000000);context.DrawPrimitive(E_PrimitiveType_TriangleList,0,3);context.EndScene();auto data=output.LockRect(16);require(data.pBits,"Depth-sampling GPU colour readback failed");bool valid=true;for(U32 y=0;y<16;++y)for(U32 x=0;x<16;++x){const U8* c=data.pBits+y*data.pitch+x*4;for(U32 i=0;i<3;++i)valid&=std::abs(int(c[i])-int(std::lround(expected*255)))<=1;valid&=c[3]==255;}output.UnlockRect();require(valid,"GPU depth texture sampling differs from rendered mip values");};
    write(.25f,.5f);target.Resolve();read(target.GetTexture(),0,.25f);read(target.GetTexture(),1,.5f);
    C_Texture copy;require(device.CreateTexture2D(16,16,2,0,E_TextureFormat_D24S8,&copy)&&device.CopyTexture(&copy,&target.GetTexture()),"Opaque depth GPU clone failed");read(copy,0,.25f);read(copy,1,.5f);
    write(.75f,.875f);read(target.GetTexture(),0,.25f,true);target.Resolve();read(target.GetTexture(),0,.75f,true);read(target.GetTexture(),1,.875f);read(copy,0,.25f);
    context.SetColourSurface(0,&colour.GetSurface());context.SetDepthStencilSurface(&target.GetSurface());context.SetViewport(C_Viewport(0,0,16,16));context.SetTexture(0,&copy);context.SetDepthEnable(E_Bool_True);context.SetDepthFunc(E_CmpFunc_Always);context.SetDepthWriteEnable(E_Bool_True);context.BeginScene();context.Clear(E_Clear_Colour|E_Clear_Depth|E_Clear_Stencil,0xff000000,1,0);context.DrawPrimitive(E_PrimitiveType_TriangleList,0,3);context.EndScene();context.SetDepthEnable(E_Bool_False);target.Resolve();read(target.GetTexture(),0,.5f);
    C_RenderTexture2D d16;require(device.CreateRenderTexture2D(16,16,1,E_Flags_DepthStencil,E_TextureFormat_D16,E_SurfaceFormat_D16,E_Multisample_None,nullptr,&d16),"Native D16 sampling fixture failed");context.SetTexture(0,nullptr);context.SetColourSurface(0,&colour.GetSurface());context.SetDepthStencilSurface(&d16.GetSurface());context.BeginScene();context.Clear(E_Clear_Depth,0,.375f);context.EndScene();read(d16.GetTexture(),0,.375f);device.DestroyRenderTexture2D(&d16);
    C_Surface feedback;require(device.GetTextureSurface(&target.GetTexture(),0,0,&feedback),"Depth feedback alias failed");context.SetTexture(0,&target.GetTexture());context.SetDepthStencilSurface(&feedback);context.BeginScene();rejects([&]{context.DrawPrimitive(E_PrimitiveType_TriangleList,0,3);},"Depth attachment sampling feedback was accepted");context.EndScene();context.SetDepthStencilSurface(nullptr);
    require(device.WaitIdle(),"Depth texture GPU completion failed");device.Destroy(&feedback);device.Destroy(&copy);device.Destroy(&declaration);device.Destroy(&pixel);device.Destroy(&vertex);device.Destroy(&output);device.DestroyRenderTexture2D(&colour);device.DestroyRenderTexture2D(&target);
    std::cout<<"Native opaque depth/stencil textures: GPU-written mip sampling/read-write resolve/copy/feedback rejection PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
