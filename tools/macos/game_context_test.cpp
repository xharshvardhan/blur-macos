// [Blur2PC][macOS] Actual registered upper contexts forward shared state and draws to native GPU resources.
#include <display/device/Device.h>
#include <display/context/Context.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>
using namespace Display;
static void require(bool v,const char* message){if(!v)throw std::runtime_error(message);}
template<class F>static void rejects(F call,const char* message){bool failed=false;try{call();}catch(const std::logic_error&){failed=true;}require(failed,message);}
template<class T>struct Resource{C_Device& device;T value;explicit Resource(C_Device& d):device(d){}~Resource(){device.Destroy(&value);}};
static std::vector<U8> shader(bool vertex){
    const std::string metadata=vertex?R"({"parameters":{},"input_semantics":[[0,0,0]],"reflection":{"ubos":[]}})":R"({"parameters":{"colour":[0,1]},"input_semantics":[],"reflection":{"ubos":[{"name":"PixelConstants","binding":0,"block_size":16}]}})";
    const std::string source=vertex?
        "#include <metal_stdlib>\nusing namespace metal;struct Input{float3 position[[attribute(0)]];};struct Output{float4 p[[position]];};vertex Output blur_main(Input v[[stage_in]]){return {float4(v.position,1)};}\n":
        "#include <metal_stdlib>\nusing namespace metal;fragment float4 blur_main(constant float4& colour[[buffer(0)]]){return colour;}\n";
    const std::string entry="blur_main";std::vector<U8> bytes(40);std::memcpy(bytes.data(),"BMSL",4);
    auto put=[&](U32 p,U32 v){for(U32 i=0;i<4;++i)bytes[p+i]=U8(v>>(8*i));};put(4,1);put(8,vertex?0:1);put(16,40);put(20,metadata.size());put(24,40+metadata.size());put(28,source.size());put(32,40+metadata.size()+source.size());put(36,entry.size());
    bytes.insert(bytes.end(),metadata.begin(),metadata.end());bytes.insert(bytes.end(),source.begin(),source.end());bytes.insert(bytes.end(),entry.begin(),entry.end());put(12,bytes.size());return bytes;
}

// [Blur2PC] Context_D3D11's geometry overrides delegate to Device; a later
// context VB/indexed call must preserve the current device declaration/index.
// Different complete left/right rectangles make stale replay visible on GPU.
static void mixedGeometryBindings(C_Device& device,C_Context& context,C_Context& clone,
                                  C_VertexShader& vertex,C_PixelShader& pixel){
    struct LowerDeclaration {
        Display2::C_Device& device;C_VertexDeclaration value;
        explicit LowerDeclaration(Display2::C_Device& d):device(d){}
        ~LowerDeclaration(){device.Destroy(&value);}
    };
    LowerDeclaration a(device.GetDisplay2()),b(device.GetDisplay2());
    Display2::C_VertexFormat fa,fb;
    fa.AddElement(0,0,Display2::E_VertexElementType_F32_3,Display2::E_VertexElementUsage_Position,0);
    fb.AddElement(0,12,Display2::E_VertexElementType_F32_3,Display2::E_VertexElementUsage_Position,0);
    require(device.GetDisplay2().CreateVertexDeclaration(fa,&a.value)&&device.GetDisplay2().CreateVertexDeclaration(fb,&b.value),"Mixed context declarations failed");
    const F32 left[][3]={{-1,-1,.5},{-1,1,.5},{0,-1,.5},{0,-1,.5},{-1,1,.5},{0,1,.5}};
    const F32 right[][3]={{0,-1,.5},{0,1,.5},{1,-1,.5},{1,-1,.5},{0,1,.5},{1,1,.5}};
    struct Pair {F32 a[3],b[3];};Pair pairs[6];
    for(U32 i=0;i<6;++i){std::memcpy(pairs[i].a,left[i],12);std::memcpy(pairs[i].b,right[i],12);}
    Resource<C_VertexBuffer> paired(device),sequential(device);
    Resource<C_IndexBuffer> oldIndices(device),currentIndices(device);
    require(device.CreateVertexBuffer(sizeof(pairs),0,&paired.value)&&device.CreateVertexBuffer(sizeof(left)+sizeof(right),0,&sequential.value)&&
            device.CreateIndexBuffer(12,0,E_Format_Index16,&oldIndices.value)&&device.CreateIndexBuffer(12,0,E_Format_Index16,&currentIndices.value),"Mixed context geometry resources failed");
    auto* positions=static_cast<U8*>(sequential.value.Lock());require(positions,"Mixed context sequential map failed");
    std::memcpy(positions,left,sizeof(left));std::memcpy(positions+sizeof(left),right,sizeof(right));sequential.value.Unlock();
    auto* packed=paired.value.Lock();require(packed,"Mixed context paired map failed");std::memcpy(packed,pairs,sizeof(pairs));paired.value.Unlock();
    const U16 ia[]={0,1,2,3,4,5},ib[]={6,7,8,9,10,11};
    auto upload=[&](C_IndexBuffer& buffer,const U16* values){void* memory=buffer.Lock();require(memory,"Mixed context index map failed");std::memcpy(memory,values,12);buffer.Unlock();};
    upload(oldIndices.value,ia);upload(currentIndices.value,ib);
    U32 failed=0;
    auto check=[&](const char* name,auto setup,auto draw){
        require(device.BeginScene(),"Mixed context scene failed");
        device.Clear(E_Clear_Colour,Maths::RGBA(0,0,0,255));
        device.SetVertexShader(&vertex);device.SetPixelShader(&pixel);
        device.SetCullMode(E_CullMode_None);device.SetDepthEnable(E_Bool_False);device.SetStencilEnable(E_Bool_False);device.SetBlendMode(E_BlendMode_Solid);
        const F32 green[]={0,1,0,1};device.SetPixelShaderConstantF(0,green,1);
        bool drew=true;std::string error;
        try{setup();draw();}catch(const std::logic_error& e){drew=false;error=e.what();}
        device.EndScene();
        bool correct=drew;U32 greenLeft=0,greenRight=0;
        auto* target=device.GetNativeColourSurface();auto data=target->LockRect(16);require(data.pBits,"Mixed context GPU readback failed");
        for(U32 y=0;y<16;++y)for(U32 x=0;x<16;++x){U32 actual;std::memcpy(&actual,data.pBits+y*data.pitch+x*4,4);correct&=actual==(x<8?0xff000000:0xff00ff00);if(actual==0xff00ff00){if(x<8)++greenLeft;else ++greenRight;}}
        target->UnlockRect();
        std::cout<<"Mixed geometry "<<name<<": "<<(correct?"PASS":"FAIL")<<" left="<<greenLeft<<" right="<<greenRight;
        if(!error.empty())std::cout<<" error="<<error;std::cout<<'\n';if(!correct)++failed;
    };
    auto nonindexed=[&]{context.DrawPrimitive(Display2::E_PrimitiveType_TriangleList,0,6);};
    auto indexed=[&]{context.DrawIndexedPrimitive(Display2::E_PrimitiveType_TriangleList,0,0,12,0,6);};
    check("direct Device control",[&]{device.SetVertexDeclaration(&b.value);device.SetVertexBuffer(0,&paired.value,0,sizeof(Pair));},nonindexed);
    check("Context A -> Device B -> Context VB",[&]{context.SetVertexDeclaration(&a.value);device.SetVertexDeclaration(&b.value);context.SetVertexBuffer(0,&paired.value,0,sizeof(Pair));},nonindexed);
    check("cloned Context A -> Device B -> Context VB",[&]{context.SetVertexDeclaration(&a.value);clone.CloneState(&context);device.SetVertexDeclaration(&b.value);clone.SetVertexBuffer(0,&paired.value,0,sizeof(Pair));},[&]{clone.DrawPrimitive(Display2::E_PrimitiveType_TriangleList,0,6);});
    check("null cached declaration -> Device B -> Context VB",[&]{context.CloneState(nullptr);device.SetVertexDeclaration(&b.value);context.SetVertexBuffer(0,&paired.value,0,sizeof(Pair));},nonindexed);
    check("Context declaration retired -> Device B -> Context VB",[&]{context.SetVertexDeclaration(&a.value);device.SetVertexDeclaration(&b.value);device.GetDisplay2().Destroy(&a.value);context.SetVertexBuffer(0,&paired.value,0,sizeof(Pair));},nonindexed);
    require(device.GetDisplay2().CreateVertexDeclaration(fa,&a.value),"Mixed retired declaration recreation failed");
    check("direct Device indexed control",[&]{device.SetVertexDeclaration(&a.value);device.SetVertexBuffer(0,&sequential.value,0,12);device.SetIndexBuffer(&currentIndices.value);},[&]{device.DrawIndexedPrimitive(E_PrimitiveType_TriangleList,0,0,12,0,6);});
    check("Context index A -> Device index B -> Context indexed",[&]{context.SetVertexDeclaration(&a.value);context.SetVertexBuffer(0,&sequential.value,0,12);
        context.SetIndexBuffer(&oldIndices.value);device.SetIndexBuffer(&currentIndices.value);},indexed);
    check("cloned Context index A -> Device index B -> Context indexed",[&]{context.SetIndexBuffer(&oldIndices.value);clone.CloneState(&context);device.SetVertexDeclaration(&a.value);device.SetVertexBuffer(0,&sequential.value,0,12);device.SetIndexBuffer(&currentIndices.value);},[&]{clone.DrawIndexedPrimitive(Display2::E_PrimitiveType_TriangleList,0,0,12,0,6);});
    check("Context index A retired -> Device index B -> Context indexed",[&]{device.SetVertexDeclaration(&a.value);device.SetVertexBuffer(0,&sequential.value,0,12);context.SetIndexBuffer(&oldIndices.value);device.SetIndexBuffer(&currentIndices.value);device.Destroy(&oldIndices.value);},indexed);
    // Explicit context rebinding is the valid recovery control, with a recreated owner.
    require(device.CreateIndexBuffer(12,0,E_Format_Index16,&oldIndices.value),"Mixed index retry allocation failed");upload(oldIndices.value,ia);
    check("explicit Context rebind recovery",[&]{context.SetVertexDeclaration(&a.value);context.SetVertexBuffer(0,&sequential.value,0,12);context.SetIndexBuffer(&currentIndices.value);},indexed);
    context.CloneState(nullptr);clone.CloneState(nullptr);
    require(!failed,"Original PC mixed Context/Device geometry binding contract failed");
}

int main(){try{
    C_Device device;C_DeviceParams params;params.m_Width=params.m_Height=16;params.m_BeginVerticesSize=256;params.m_FreeBeginVerticesSize=64;
    require(device.Create(params)&&device.CreateVolatile(params),"Native upper device unavailable");
    C_DeviceContext first,second;first.Initialise();second.Initialise();auto* context=GetContext(&first);auto* clone=GetContext(&second);
    require(context&&context->m_pParent==&first&&context->GetFlags()==context->GetId()&&context->m_Context.IsBound(),"Original registry/native context initialization failed");
    Resource<C_VertexShader> vertex(device);Resource<C_PixelShader> pixel(device);auto v=shader(true),p=shader(false);
    bool loaded=device.CreateVertexShader(v.size(),reinterpret_cast<const S32*>(v.data()),0,&vertex.value)&&device.CreatePixelShader(p.size(),reinterpret_cast<const S32*>(p.data()),0,&pixel.value);require(loaded,device.GetNativeError().c_str());
    C_VertexElement elements[]={C_VertexElement(0,0,E_VertexElementType_F32_3,E_VertexElementUsage_Position,0),VERTEXELEMENT_END()};auto* declaration=device.CreateVertexDeclaration(elements);require(declaration,"Upper context declaration failed");
    Resource<C_VertexBuffer> vertices(device);Resource<C_IndexBuffer> indices(device);
    require(device.CreateVertexBuffer(36,0,&vertices.value)&&device.CreateIndexBuffer(6,0,E_Format_Index16,&indices.value),"Upper context resources failed");
    const F32 positions[]={-1,-1,.5,3,-1,.5,-1,3,.5};std::memcpy(vertices.value.Lock(),positions,sizeof(positions));vertices.value.Unlock();const U16 order[]={0,1,2};std::memcpy(indices.value.Lock(),order,6);indices.value.Unlock();
    auto parameter=pixel.value.GetParamFromName("colour");require(parameter.m_RegisterCount==1,"Native pixel parameter reflection missing");
    auto verify=[&](U32 expected){auto* target=device.GetNativeColourSurface();auto data=target->LockRect(16);require(data.pBits,"Upper context GPU readback failed");bool valid=true;for(U32 y=0;y<16;++y)for(U32 x=0;x<16;++x){U32 value;std::memcpy(&value,data.pBits+y*data.pitch+x*4,4);valid&=value==expected;}target->UnlockRect();require(valid,"Original upper context state/draw changed expected GPU colour");};
    for(U32 frame=0;frame<4;++frame){
        require(device.BeginScene(),"Upper context scene failed");device.Clear(E_Clear_Colour|E_Clear_Depth|E_Clear_Stencil,Maths::RGBA(0,0,0,255),1,7);
        context->SetColourDepthStencilSurface(device.GetNativeColourSurface(),device.GetNativeDepthSurface());
        context->SetVertexDeclaration(declaration);context->SetVertexBuffer(0,&vertices.value,0,12);context->SetIndexBuffer(&indices.value);context->SetVertexShader(&vertex.value);context->SetPixelShader(&pixel.value);context->SetCullMode(E_CullMode_None);context->SetDepthEnable(E_Bool_False);context->SetBlendMode(E_BlendMode_Solid);
        C_StencilState stencil;stencil.m_StencilEnable=E_Bool_True;stencil.m_StencilRef=7;context->SetStencilState(stencil);
        const F32 green[]={0,1,0,1},blue[]={0,0,1,1};context->SetPixelShaderParameterArrayF(parameter,frame&1?blue:green);
        C_SamplerState sampler;sampler.SetDefaultStates();sampler.SetMaxAnisotropy(8);context->SetSamplerState(0,&sampler);sampler.SetMaxAnisotropy(2);context->SetVertexSamplerState(0,&sampler);
        context->m_pView->SetIndex(13);context->SetStencilTexture(nullptr);second.CloneState(&first);
        require(clone->m_pView->GetIndex()==13&&clone->m_pVertexDeclaration==declaration&&clone->m_pIndexBuffer==&indices.value,"Original registry/native context clone lost metadata");
        if(frame&1)clone->DrawIndexedPrimitive(Display2::E_PrimitiveType_TriangleList,0,0,3,0,3);else clone->DrawPrimitive(Display2::E_PrimitiveType_TriangleList,0,3);
        first.SetContextFlags(0);first.SetContextFlags(context->GetId());require(first.GetContextFlags()==context->GetId(),"Original registry context flag transition failed");
        device.EndScene();verify(frame&1?0xff0000ff:0xff00ff00);
    }
    // Different left/right vertex ranges and constants must survive later writes and both buffer rotations.
    auto dynamic=[&](F32 left,F32 right,const F32* colour,bool check=true){
        context->SetVertexShader(&vertex.value);context->SetPixelShader(&pixel.value);
        auto* data=context->BeginVertices(E_PrimitiveType_TriangleStrip,4,12,declaration);
        require(data&&device.BeginVerticesLocked(),"Original dynamic vertex map failed");
        require(!device.BeginVertices(E_PrimitiveType_TriangleStrip,4,12,declaration,check),"Nested original dynamic map succeeded");
        const F32 positions[]={left,-1,.5,left,1,.5,right,-1,.5,right,1,.5};std::memcpy(data,positions,sizeof(positions));context->SetPixelShaderParameterF(parameter,colour);context->EndVertices();require(!device.BeginVerticesLocked(),"Original dynamic vertex unlock failed");
    };
    const F32 red[]={1,0,0,1},blue[]={0,0,1,1};
    for(U32 frame=0;frame<6;++frame){
        device.Present(); // A headless presentation attempt still rotates original transient frame storage/budgets.
        require(device.BeginScene(),"Dynamic upper scene failed");device.Clear(E_Clear_Colour|E_Clear_Depth,Maths::RGBA(0,0,0,255),1,0);context->SetCullMode(E_CullMode_None);context->SetDepthEnable(E_Bool_False);context->SetBlendMode(E_BlendMode_Solid);
        dynamic(-1,0,red);dynamic(0,1,blue);
        auto* discard=device.BeginVertices(E_PrimitiveType_TriangleStrip,4,12,declaration,true);require(discard,"Dynamic fake-end fixture allocation failed");std::memset(discard,0,48);device.FakeEndVertices();
        require(!device.BeginVertices(E_PrimitiveType_TriangleStrip,~0U,12,declaration),"Dynamic vertex count multiplication overflow accepted");
        require(!device.BeginVertices(E_PrimitiveType_TriangleStrip,1,0,declaration),"Zero native stride accepted");
        device.EndScene();auto* target=device.GetNativeColourSurface();auto data=target->LockRect(16);require(data.pBits,"Dynamic GPU readback failed");bool correct=true;for(U32 y=0;y<16;++y)for(U32 x=0;x<16;++x){U32 pixel;std::memcpy(&pixel,data.pBits+y*data.pitch+x*4,4);correct&=pixel==(x<8?0xffff0000:0xff0000ff);}target->UnlockRect();require(correct,"Original dynamic ranges or immutable constants were overwritten");
    }
    device.Present();require(device.BeginScene(),"Dynamic budget scene failed");device.SetMaxBeginVerticesUsage(96);device.SetUserMaxBeginVerticesUsage(.5f);
    require(device.BeginVertices(E_PrimitiveType_TriangleStrip,4,12,declaration,true),"First budgeted vertex map failed");device.FakeEndVertices();require(!device.BeginVertices(E_PrimitiveType_TriangleStrip,4,12,declaration,true),"User dynamic usage budget ignored");
    for(U32 i=0;i<4;++i){require(device.BeginVertices(E_PrimitiveType_TriangleStrip,4,12,declaration,false),"Unchecked usage did not retain physical buffer capacity");device.FakeEndVertices();}
    require(!device.BeginVertices(E_PrimitiveType_TriangleStrip,4,12,declaration,false),"Dynamic physical exhaustion accepted");device.EndScene();device.Present();device.SetMaxBeginVerticesUsage(192);device.SetUserMaxBeginVerticesUsage(1);
    require(device.BeginScene()&&device.BeginVertices(E_PrimitiveType_TriangleStrip,4,12,declaration),"Mapped shutdown fixture failed");rejects([&]{device.EndScene();},"Ending scene while vertices mapped succeeded");device.DestroyVolatile();require(!device.BeginVerticesLocked()&&device.CreateVolatile(params),"Mapped volatile shutdown/recreation leaked dynamic storage");
    rejects([&]{context->SetAlphaToMaskEnable(E_Bool_Unknown);},"Invalid native coverage boolean accepted");context->SetAlphaToMaskEnable(E_Bool_True);rejects([&]{context->SetPointSpriteEnable(E_Bool_True);},"Unsupported native point sprites silently succeeded");rejects([&]{context->SetSamplerState(0,nullptr);},"Null native sampler accepted");
    context->SetPointSpriteEnable(E_Bool_False);context->SetAlphaToMaskEnable(E_Bool_False);context->BeginCommandBuffer(nullptr);context->EndCommandBuffer();
    mixedGeometryBindings(device,*context,*clone,vertex.value,pixel.value);
    device.Destroy(&indices.value);device.Destroy(&vertices.value);device.Destroy(&pixel.value);device.Destroy(&vertex.value);device.DestroyVolatile();device.Destroy();
    rejects([&]{clone->SetDepthEnable(E_Bool_True);},"Stale upper context retained a destroyed device");
    require(device.Create(params)&&device.CreateVolatile(params),"Native context device recreation failed");rejects([&]{clone->SetDepthEnable(E_Bool_True);},"Old native context silently rebound after recreation");clone->CloneState(nullptr);clone->SetDepthEnable(E_Bool_True);
    device.DestroyVolatile();device.Destroy();
    std::cout<<"Original context registry/state conversion/indexed and nonindexed GPU draws/parameter uploads/cloning/stale binding/dynamic GPU ranges/budgets/exhaustion/mapped teardown PASS (not game startup)\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
