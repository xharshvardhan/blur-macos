// [Blur2PC][macOS] PC missing depth/stencil tests pass; cached requests reactivate on rebind.
#include <display2/device/MetalDevice.h>
#include <display2/context/Context.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace Display2;
namespace {
void Require(bool value,const char* reason) { if(!value)throw std::runtime_error(reason); }
std::vector<U8> Shader(bool vertex) {
    const std::string metadata=R"({"parameters":{},"input_semantics":[],"reflection":{"ubos":[]}})";
    const std::string source=vertex?
        "#include <metal_stdlib>\nusing namespace metal;struct V{float4 p[[position]];};vertex V blur_main(uint i[[vertex_id]]){const float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};return {float4(p[i],.5,1)};}\n":
        "#include <metal_stdlib>\nusing namespace metal;fragment float4 blur_main(){return float4(0,1,0,1);}\n";
    const std::string entry="blur_main";
    std::vector<U8> bytes(40);std::memcpy(bytes.data(),"BMSL",4);
    auto word=[&](U32 offset,U32 value){for(U32 i=0;i<4;++i)bytes[offset+i]=U8(value>>(i*8));};
    word(4,1);word(8,vertex?0:1);word(16,40);word(20,metadata.size());
    word(24,40+metadata.size());word(28,source.size());
    word(32,40+metadata.size()+source.size());word(36,entry.size());
    bytes.insert(bytes.end(),metadata.begin(),metadata.end());
    bytes.insert(bytes.end(),source.begin(),source.end());
    bytes.insert(bytes.end(),entry.begin(),entry.end());word(12,bytes.size());return bytes;
}
void Colour(const C_Surface& surface,bool green,const char* reason) {
    auto& writable=const_cast<C_Surface&>(surface);auto image=writable.LockRect(16);
    Require(image.pBits,"Missing-attachment colour readback failed");bool valid=true;
    for(U32 y=0;y<16;++y)for(U32 x=0;x<16;++x){U32 value;std::memcpy(&value,image.pBits+y*image.pitch+x*4,4);valid&=value==(green?0xff00ff00:0xff000000);}
    writable.UnlockRect();Require(valid,reason);
}
std::vector<U16> DepthValues(C_Surface& surface) {
    auto image=surface.LockRect(16);Require(image.pBits,"Missing-attachment D16 readback failed");
    std::vector<U16> values(16*16);
    for(U32 y=0;y<16;++y)std::memcpy(values.data()+y*16,image.pBits+y*image.pitch,16*sizeof(U16));
    surface.UnlockRect();return values;
}
void DepthNear(const std::vector<U16>& values,U16 expected,const char* reason) {
    for(U16 value:values)Require(value+1>=expected&&value<=U32(expected)+1,reason);
}
}
int main() { try {
    C_MetalDevice device;Require(device.Create(),"Missing-attachment Metal device creation failed");
    C_Surface colour,depth16,depth24;
    Require(device.CreateSurface(16,16,E_Flags_Colour,E_SurfaceFormat_A8R8G8B8,E_Multisample_None,nullptr,&colour)&&
            device.CreateSurface(16,16,E_Flags_DepthStencil,E_SurfaceFormat_D16,E_Multisample_None,nullptr,&depth16)&&
            device.CreateSurface(16,16,E_Flags_DepthStencil,E_SurfaceFormat_D24S8,E_Multisample_None,nullptr,&depth24),
            "Missing-attachment real surfaces failed");
    C_VertexShader vertex;C_PixelShader pixel;C_VertexDeclaration declaration;
    auto v=Shader(true),p=Shader(false);std::string error;
    Require(device.CreateVertexShader(v.size(),v.data(),0,&vertex,nullptr,&error)&&
            device.CreatePixelShader(p.size(),p.data(),0,&pixel,nullptr,&error),error.c_str());
    C_VertexFormat format;format.AddElement(0,E_VertexElementType_F32_3,E_VertexElementUsage_Position,0);
    Require(device.CreateVertexDeclaration(format,&declaration),"Missing-attachment declaration failed");
    auto& c=device.GetContext();c.SetColourSurface(0,&colour);c.SetViewport(C_Viewport(0,0,16,16));
    c.SetVertexDeclaration(&declaration);c.SetVertexShader(&vertex);c.SetPixelShader(&pixel);
    c.SetCullMode(E_CullMode_None);c.SetColourWriteEnable(15);c.SetBlendEnable(E_Bool_False);
    auto draw=[&](const C_Surface* depth,bool green,const char* reason){
        c.SetDepthStencilSurface(depth);c.BeginScene();c.Clear(E_Clear_Colour,0xff000000);
        c.DrawPrimitive(E_PrimitiveType_TriangleList,0,3);c.EndScene();Colour(colour,green,reason);
    };
    for(U32 cycle=0;cycle<2;++cycle) {
        c.SetDepthStencilSurface(&depth16);c.BeginScene();c.Clear(E_Clear_Depth,0,.25f);c.EndScene();
        const auto originalDepth=DepthValues(depth16);DepthNear(originalDepth,16384,"D16 clear value is invalid");
        c.SetDepthStencilSurface(&depth24);c.BeginScene();c.Clear(E_Clear_Depth|E_Clear_Stencil,0,.25f,3);c.EndScene();
        c.SetDepthEnable(E_Bool_True);c.SetDepthWriteEnable(E_Bool_True);c.SetDepthFunc(E_CmpFunc_Never);
        C_StencilState stencil;stencil.SetStencilFunc(E_CmpFunc_Never);stencil.SetStencilRef(9);
        stencil.SetStencilMask(255);stencil.SetStencilWriteMask(255);stencil.SetStencilFail(E_StencilOp_Replace);
        c.SetStencilState(stencil);c.SetStencilEnable(E_Bool_True);
        draw(nullptr,true,"Absent depth/stencil did not pass requested Never tests");
        Require(DepthValues(depth16)==originalDepth,"Detached depth storage changed while no depth was bound");
        draw(&depth16,false,"Rebound D16 did not reactivate requested Never depth test");
        draw(nullptr,true,"Detaching D16 did not disable the effective depth test");
        draw(&depth24,false,"Rebound D24S8 did not reactivate requested Never stencil/depth tests");
        c.SetDepthFunc(E_CmpFunc_Always);
        draw(&depth16,true,"D16 incorrectly applied the requested Never stencil test without a stencil attachment");
        draw(&depth24,false,"Rebinding stencil did not restore its unchanged requested Never comparison");
        const auto writtenDepth=DepthValues(depth16);
        DepthNear(writtenDepth,32768,"Rebound D16 did not restore the requested depth-write enable");

        // Reset a real stencil value, then request an operation that would replace it
        // if a detached surface were accidentally kept active by the previous encoder.
        c.BeginScene();c.Clear(E_Clear_Stencil,0,1,3);c.EndScene();
        draw(nullptr,true,"Missing stencil failed after a previously attached stencil surface");
        stencil.SetStencilFunc(E_CmpFunc_Equal);stencil.SetStencilRef(3);
        stencil.SetStencilFail(E_StencilOp_Keep);stencil.SetStencilDepthPass(E_StencilOp_Keep);c.SetStencilState(stencil);
        draw(&depth24,true,"Detached draw changed the stored stencil value");
        stencil.SetStencilRef(9);c.SetStencilState(stencil);
        draw(&depth16,true,"Missing stencil did not pass requested mismatching Equal comparison");
        draw(&depth24,false,"Rebound stencil lost the requested mismatching Equal comparison");
        draw(nullptr,true,"Missing depth/stencil did not pass after repeated rebinds");
        Require(DepthValues(depth16)==writtenDepth,"Detached draw wrote to a formerly attached depth surface");
    }
    // Missing draw attachments are legal PC state; explicitly clearing a missing
    // aspect remains an error rather than silently accepting a missing resource.
    for(const auto flag:{E_Clear_Depth,E_Clear_Stencil}) {
        c.SetDepthStencilSurface(nullptr);c.BeginScene();bool rejected=false;
        try{c.Clear(flag,0);}catch(const std::logic_error&){rejected=true;}
        c.EndScene();Require(rejected,"An explicit clear of a missing attachment was accepted");
    }
    c.SetDepthStencilSurface(&depth16);c.BeginScene();bool rejected=false;
    try{c.Clear(E_Clear_Stencil,0);}catch(const std::logic_error&){rejected=true;}
    c.EndScene();Require(rejected,"D16 accepted an explicit stencil clear");
    c.SetDepthStencilSurface(nullptr);c.SetColourSurface(0,nullptr);
    Require(device.WaitIdle(),"Missing-attachment GPU completion failed");
    device.Destroy(&declaration);device.Destroy(&pixel);device.Destroy(&vertex);
    device.Destroy(&depth24);device.Destroy(&depth16);device.Destroy(&colour);
    std::cout<<"PC attachment-conditioned depth/stencil: absent tests pass, unchanged requests reactivate, detached storage preserved, rebound writes resume and explicit invalid clears reject PASS\n";return 0;
} catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;} }
