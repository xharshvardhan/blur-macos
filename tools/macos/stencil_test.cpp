// [Blur2PC][macOS] Actual native depth/stencil operations inferred through GPU colour output, without inventing CPU packing.
#include <display2/device/MetalDevice.h>
#include <display2/context/Context.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace Display2;
static void require(bool v,const char* message){if(!v)throw std::runtime_error(message);}
static std::vector<U8> shader(bool vertex){
    const std::string metadata=R"({"parameters":{},"input_semantics":[],"reflection":{"ubos":[]}})";
    const std::string source=vertex?
        "#include <metal_stdlib>\nusing namespace metal;struct Output{float4 p[[position]];};vertex Output blur_main(uint i[[vertex_id]]){const float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};return {float4(p[i],.5,1)};}\n":
        "#include <metal_stdlib>\nusing namespace metal;fragment float4 blur_main(){return float4(0,1,0,1);}\n";
    const std::string entry="blur_main";std::vector<U8> bytes(40);std::memcpy(bytes.data(),"BMSL",4);
    auto put=[&](U32 p,U32 v){for(U32 i=0;i<4;++i)bytes[p+i]=U8(v>>(8*i));};put(4,1);put(8,vertex?0:1);put(16,40);put(20,metadata.size());put(24,40+metadata.size());put(28,source.size());put(32,40+metadata.size()+source.size());put(36,entry.size());
    bytes.insert(bytes.end(),metadata.begin(),metadata.end());bytes.insert(bytes.end(),source.begin(),source.end());bytes.insert(bytes.end(),entry.begin(),entry.end());put(12,bytes.size());return bytes;
}
int main(){try{
    C_MetalDevice device;require(device.Create(),"Stencil GPU device unavailable");C_Surface colour,depth;
    require(device.CreateSurface(16,16,E_Flags_Colour,E_SurfaceFormat_A8R8G8B8,E_Multisample_None,nullptr,&colour)&&device.CreateSurface(16,16,E_Flags_DepthStencil,E_SurfaceFormat_D24S8,E_Multisample_None,nullptr,&depth),"Native colour/depth/stencil attachments failed");
    require(!depth.LockRect(16).pBits&&GetSurfaceSize(16,16,0,E_SurfaceFormat_D24S8,E_Multisample_None)==1024,"Opaque depth/stencil exposed a fabricated lock or changed nominal PC size");
    C_VertexShader vertex;C_PixelShader pixel;auto v=shader(true),p=shader(false);std::string error;
    const bool programs=device.CreateVertexShader(v.size(),v.data(),0,&vertex,nullptr,&error)&&device.CreatePixelShader(p.size(),p.data(),0,&pixel,nullptr,&error);require(programs,error.c_str());
    C_VertexFormat format;format.AddElement(0,E_VertexElementType_F32_3,E_VertexElementUsage_Position,0);C_VertexDeclaration declaration;require(device.CreateVertexDeclaration(format,&declaration),"Stencil vertex declaration failed");
    auto& context=device.GetContext();context.SetColourSurface(0,&colour);context.SetDepthStencilSurface(&depth);context.SetVertexDeclaration(&declaration);context.SetVertexShader(&vertex);context.SetPixelShader(&pixel);context.SetCullMode(E_CullMode_None);context.SetDepthEnable(E_Bool_False);context.SetStencilEnable(E_Bool_True);
    auto begin=[&](U8 initial){context.SetViewport(C_Viewport(0,0,16,16));context.SetScissorEnable(E_Bool_False);context.BeginScene();context.Clear(E_Clear_Colour|E_Clear_Depth|E_Clear_Stencil,0xff000000,1,initial);};
    auto draw=[&](const C_StencilState& state,bool writeColour){context.SetStencilState(state);context.SetColourWriteEnable(writeColour?15:0);context.DrawPrimitive(E_PrimitiveType_TriangleList,0,3);};
    auto verify=[&](bool inside,U32 left=0,U32 top=0,U32 right=16,U32 bottom=16){context.EndScene();auto data=colour.LockRect(16);require(data.pBits,"Stencil colour readback failed");bool valid=true;for(U32 y=0;y<16;++y)for(U32 x=0;x<16;++x){U32 value;std::memcpy(&value,data.pBits+y*data.pitch+x*4,4);const bool green=inside&&x>=left&&x<right&&y>=top&&y<bottom;valid&=value==(green?0xff00ff00:0xff000000);}colour.UnlockRect();require(valid,"Native stencil comparison/operation/clear changed expected GPU pixels");};
    auto compare=[&](U32 expected){C_StencilState state;state.SetStencilFunc(E_CmpFunc_Equal);state.SetStencilRef(expected);draw(state,true);};
    struct Operation{E_StencilOp op;U8 initial,reference,expected;};
    for(const auto test:{Operation{E_StencilOp_Keep,5,7,5},Operation{E_StencilOp_Zero,5,7,0},Operation{E_StencilOp_Replace,5,7,7},Operation{E_StencilOp_IncrSat,254,0,255},Operation{E_StencilOp_IncrSat,255,0,255},Operation{E_StencilOp_DecrSat,1,0,0},Operation{E_StencilOp_DecrSat,0,0,0},Operation{E_StencilOp_Invert,0x35,0,0xca},Operation{E_StencilOp_Incr,255,0,0},Operation{E_StencilOp_Decr,0,0,255}}){
        begin(test.initial);C_StencilState state;state.SetStencilRef(test.reference);state.SetStencilDepthPass(test.op);draw(state,false);compare(test.expected);verify(true);
    }
    for(U32 function=E_CmpFunc_Never;function<=E_CmpFunc_Always;++function){begin(5);C_StencilState state;state.SetStencilFunc(E_CmpFunc(function));state.SetStencilRef(3);draw(state,true);const bool passed=function==E_CmpFunc_Less||function==E_CmpFunc_LessEqual||function==E_CmpFunc_NotEqual||function==E_CmpFunc_Always;verify(passed);}
    begin(0xa5);C_StencilState state;state.SetStencilRef(0x37);state.SetStencilWriteMask(0x0f);state.SetStencilDepthPass(E_StencilOp_Replace);draw(state,false);compare(0xa7);verify(true);
    begin(0x15);state=C_StencilState{};state.SetStencilFunc(E_CmpFunc_Equal);state.SetStencilMask(0x0f);state.SetStencilRef(0xa5);draw(state,true);verify(true);
    begin(3);state=C_StencilState{};state.SetStencilFunc(E_CmpFunc_Never);state.SetStencilFail(E_StencilOp_Replace);state.SetStencilRef(9);draw(state,false);compare(9);verify(true);
    begin(3);context.SetDepthEnable(E_Bool_True);context.SetDepthFunc(E_CmpFunc_Never);state=C_StencilState{};state.SetStencilDepthFail(E_StencilOp_Replace);state.SetStencilRef(11);draw(state,false);context.SetDepthEnable(E_Bool_False);compare(11);verify(true);
    begin(0);context.SetViewport(C_Viewport(4,4,8,8));context.SetScissorRect(C_ScissorRect{6,2,14,10});context.SetScissorEnable(E_Bool_True);context.Clear(E_Clear_Stencil,0,1,7);context.SetScissorEnable(E_Bool_False);context.SetViewport(C_Viewport(0,0,16,16));compare(7);verify(true,6,4,12,10);
    require(device.WaitIdle(),"Stencil GPU completion failed");device.Destroy(&declaration);device.Destroy(&pixel);device.Destroy(&vertex);device.Destroy(&depth);device.Destroy(&colour);
    std::cout<<"Native opaque depth/stencil: all operations/comparisons, masks, stencil/depth failure and partial stencil clear PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
