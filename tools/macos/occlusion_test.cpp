// [Blur2PC][macOS] Real Metal visibility counts checked against exact pixel areas.
#include <display2/device/MetalDevice.h>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace Display2;
static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
template<class F>static void rejects(F function,const char* message){bool failed=false;try{function();}catch(const std::logic_error&){failed=true;}require(failed,message);}
static std::vector<U8> load(const std::filesystem::path& path){std::ifstream input(path,std::ios::binary);require(bool(input),"Missing authored shader");return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};}
int main(int argc,char** argv){try{
    require(argc==2,"Expected authored shader directory");const std::filesystem::path root=argv[1];
    C_MetalDevice device;require(device.Create(),"Metal device creation failed");auto& context=device.GetContext();
    C_Surface colour,other,depth;C_VertexBuffer vertices;C_VertexDeclaration declaration;C_VertexShader vs;C_PixelShader ps;
    for(auto* surface:{&colour,&other})require(device.CreateSurface(64,64,E_Flags_Colour,E_SurfaceFormat_A8R8G8B8,E_Multisample_None,nullptr,surface),"Colour surface allocation failed");
    require(device.CreateSurface(64,64,E_Flags_DepthStencil,E_SurfaceFormat_D16,E_Multisample_None,nullptr,&depth),"Depth surface allocation failed");
    struct Vertex{F32 position[3];U32 colour;};const Vertex quad[]={{{-1,1,.5f},0xffffffff},{{1,1,.5f},0xffffffff},{{-1,-1,.5f},0xffffffff},{{1,-1,.5f},0xffffffff}};
    require(device.CreateVertexBuffer(sizeof(quad),0,&vertices),"Vertex buffer allocation failed");auto* upload=vertices.Lock();require(upload,"Vertex upload lock failed");std::memcpy(upload,quad,sizeof(quad));vertices.Unlock();
    C_VertexFormat format;format.AddElement(0,0,E_VertexElementType_F32_3,E_VertexElementUsage_Position,0);format.AddElement(0,12,E_VertexElementType_Colour,E_VertexElementUsage_Colour,0);
    require(device.CreateVertexDeclaration(format,&declaration),"Vertex declaration failed");
    auto vertex=load(root/"display/vsBasicRenderColoured3D.bmsl"),pixel=load(root/"display/psBasicRenderColoured.bmsl");std::string error;
    require(device.CreateVertexShader(vertex.size(),vertex.data(),0,&vs,nullptr,&error),error.c_str());require(device.CreatePixelShader(pixel.size(),pixel.data(),0,&ps,nullptr,&error),error.c_str());
    context.SetColourSurface(0,&colour);context.SetDepthStencilSurface(&depth);context.SetVertexShader(&vs);context.SetPixelShader(&ps);context.SetVertexBuffer(0,&vertices,0,sizeof(Vertex),&declaration);
    const F32 identity[]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};context.SetVertexShaderParamF(vs.GetParamFromName("g_WorldMatrix"),identity);context.SetVertexShaderParamF(vs.GetParamFromName("g_ViewProjMatrix"),identity);
    context.SetCullMode(E_CullMode_None);context.SetDepthEnable(E_Bool_True);context.SetDepthWriteEnable(E_Bool_False);context.SetDepthFunc(E_CmpFunc_Less);context.SetColourWriteEnable(0);
    auto draw=[&]{context.DrawPrimitive(E_PrimitiveType_TriangleStrip,0,4);};
    C_OcclusionQuery query,second;U64 count=123;require(query.Create(context)&&query.IsCreated()&&second.Create(context),"GPU counting query creation failed");
    {C_MetalDevice unrelated;require(unrelated.Create()&&!query.Create(unrelated.GetContext()),"Query was rebound to another live device");}
    require(!query.GetResult(count)&&count==0,"Unissued query invented a result");rejects([&]{query.Begin();},"Query began outside a scene");
    context.BeginScene();context.Clear(E_Clear_Colour|E_Clear_Depth,0xff000000,1);query.Begin();
    require(!query.GetResult(count),"Active query reported completed");rejects([&]{second.Begin();},"Nested query was accepted");rejects([&]{second.End();},"Mismatched query End was accepted");rejects([&]{context.EndScene();},"Unended query was silently submitted");
    draw();query.End();require(query.GetResult(count)&&count==64*64,"Visible sample count differs from full pixel area");
    // Reading before EndScene must flush and then permit more native scene work.
    context.Clear(E_Clear_Depth,0,.25f);second.Begin();draw();second.End();context.EndScene();
    require(second.GetResult(count)&&count==0,"Depth-rejected geometry reported visible samples");
    context.BeginScene();context.Clear(E_Clear_Depth,0,1);context.SetScissorRect({8,4,40,20});context.SetScissorEnable(E_Bool_True);query.Begin();draw();query.End();context.EndScene();
    require(query.GetResult(count)&&count==32*16,"Scissored sample count differs from exact independent pixel area");
    // An active query can cross render passes, API clears and actual blit encoders.
    context.BeginScene();context.SetScissorEnable(E_Bool_False);query.Begin();draw();context.CopySurface(&other,&colour);context.SetColourSurface(0,&other);draw();
    context.SetViewport(C_Viewport(8,8,16,16));context.Clear(E_Clear_Colour,0xff112233);context.SetViewport(C_Viewport(0,0,64,64));query.End();context.EndScene();
    require(query.GetResult(count)&&count==2*64*64,"Query lost an encoder segment or counted an API clear");
    context.BeginScene();query.Begin();context.SetViewport(C_Viewport(8,8,16,16));context.Clear(E_Clear_Colour,0);query.End();context.EndScene();
    require(query.GetResult(count)&&count==0,"Clear-only query invented geometry visibility");context.SetViewport(C_Viewport(0,0,64,64));
    // A pending read can flush another query's first segment, which then continues.
    context.BeginScene();query.Begin();draw();query.End();second.Begin();draw();require(query.GetResult(count)&&count==4096,"Prior query read failed during another active query");draw();second.End();context.EndScene();
    require(second.GetResult(count)&&count==8192,"Active query did not survive a prior-query flush");
    context.BeginScene();query.Begin();draw();query.Destroy();require(!query.IsCreated(),"Retired query remained created");second.Begin();draw();second.End();context.EndScene();
    require(second.GetResult(count)&&count==4096,"Active query retirement damaged following GPU query");require(query.Create(context),"Retired query could not be recreated");
    context.BeginScene();query.Begin();draw();std::thread retire([&]{query.Destroy();});retire.join();context.EndScene();require(!query.IsCreated()&&query.Create(context),"Wrong-thread wrapper retirement damaged context cleanup");
    context.BeginScene();bool wrongThread=false;std::thread intruder([&]{try{query.Begin();}catch(const std::logic_error&){wrongThread=true;}});intruder.join();require(wrongThread,"Query bypassed context thread ownership");query.Begin();draw();query.End();context.EndScene();
    require(query.GetResult(count)&&count==4096,"Recreated query retained old counts");
    // Copy a nonuniform subrectangle between differently offset rows; untouched
    // destination pixels must survive and blit work must not count as geometry.
    auto source=colour.LockRect();require(source.pBits,"Region source upload failed");
    for(U32 y=0;y<64;++y)for(U32 x=0;x<64;++x){const U32 value=0xff000000|(x<<16)|(y<<8)|(x^y);std::memcpy(source.pBits+y*source.pitch+x*4,&value,4);}colour.UnlockRect();
    auto destination=other.LockRect();require(destination.pBits,"Region destination upload failed");
    for(U32 y=0;y<64;++y)for(U32 x=0;x<64;++x){const U32 value=0xffabcdef;std::memcpy(destination.pBits+y*destination.pitch+x*4,&value,4);}other.UnlockRect();
    rejects([&]{context.CopySurfaceRegion(&other,&colour,7,11,17,7,13,19);},"Region copy ran outside a scene");
    context.BeginScene();query.Begin();
    rejects([&]{context.CopySurfaceRegion(&other,&colour,63,0,2,1);},"Source region bounds were ignored");
    rejects([&]{context.CopySurfaceRegion(&other,&colour,0,0,2,1,63,0);},"Destination region bounds were ignored");
    rejects([&]{context.CopySurfaceRegion(&other,&colour,~U32(0),0,2,1);},"Overflowing source bounds were accepted");
    rejects([&]{context.CopySurfaceRegion(&other,&colour,0,0,0,1);},"Empty copy was accepted");
    rejects([&]{context.CopySurfaceRegion(&colour,&colour,0,0,1,1);},"Aliased region copy was accepted");
    context.CopySurfaceRegion(&other,&colour,7,11,17,7,13,19);query.End();context.EndScene();require(query.GetResult(count)&&count==0,"Region blit contributed visibility samples");
    destination=other.LockRect(16);require(destination.pBits,"Region GPU readback failed");
    for(U32 y=0;y<64;++y)for(U32 x=0;x<64;++x){U32 actual=0;std::memcpy(&actual,destination.pBits+y*destination.pitch+x*4,4);const U32 sx=x-13+7,sy=y-19+11;const U32 expected=x>=13&&x<30&&y>=19&&y<26?0xff000000|(sx<<16)|(sy<<8)|(sx^sy):0xffabcdef;require(actual==expected,"Region GPU copy changed stride, offsets or untouched pixels");}other.UnlockRect();
    // Ended and still-active commands abandoned during device destruction must
    // fail cleanly instead of waiting forever on an uncommitted command buffer.
    C_OcclusionQuery abandoned,unended;require(abandoned.Create(context)&&unended.Create(context),"Abandonment query allocation failed");
    context.BeginScene();abandoned.Begin();draw();abandoned.End();unended.Begin();draw();
    device.Destroy(&vs);device.Destroy(&ps);device.Destroy(&vertices);device.Destroy(&declaration);device.Destroy(&colour);device.Destroy(&other);device.Destroy(&depth);require(device.WaitIdle(),"Visibility GPU command failed");device.Destroy();
    require(!query.IsCreated()&&query.GetResult(count)&&count==0,"Completed query storage was not retained across device retirement");
    require(second.GetResult(count)&&count==4096,"Completed nonzero GPU query lost storage during device retirement");
    require(!abandoned.GetResult(count)&&!unended.GetResult(count),"Abandoned device commands invented completed results");query.Destroy();require(!query.GetResult(count)&&count==0,"Destroyed query returned stale data");
    std::cout<<"Native GPU occlusion: exact visible/hidden/scissor counts, encoder segmentation, clear exclusion, scene flush, bounded offset pixel copies, retirement, abandonment and ownership passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
