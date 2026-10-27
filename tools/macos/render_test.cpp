// [Blur2PC][macOS] Authored shader draws through engine resources, checked against CPU geometry.
#include <display2/device/MetalDevice.h>
#include <display2/context/Context.h>
#include <display2/device/BaseDevice.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <atomic>
#include <vector>
using namespace Display2;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F> static void rejects(F call,const char* message){bool failed=false;try{call();}catch(const std::logic_error&){failed=true;}require(failed,message);}
static std::vector<U8> file(const std::filesystem::path& path){std::ifstream stream(path,std::ios::binary);require(bool(stream),"Authored shader blob missing");return {std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};}
struct Vertex { U32 padding;F32 position[3];U32 colour;U32 tail[3]; };
static_assert(sizeof(Vertex)==32);
using Triangle=std::array<std::array<float,2>,3>;
static float edge(const std::array<float,2>& a,const std::array<float,2>& b,float x,float y){return (b[0]-a[0])*(y-a[1])-(b[1]-a[1])*(x-a[0]);}
static int inside(const Triangle& triangle,float x,float y){
    float nearest=100;bool in=true;const float direction=edge(triangle[0],triangle[1],triangle[2][0],triangle[2][1])>0?1:-1;
    for(int i=0;i<3;++i){const auto& a=triangle[i];const auto& b=triangle[(i+1)%3];const float distance=edge(a,b,x,y)*direction/std::hypot(b[0]-a[0],b[1]-a[1]);nearest=std::min(nearest,std::abs(distance));in&=distance>0;}
    if(nearest<0.035f)return -1;return in?1:0;
}
static constexpr U32 width=64,height=64;
static void pixels(C_Surface& colour,const Triangle& red,const Triangle& blue,bool useRed=true,bool useBlue=true,U32 blueValue=255,U32 alpha=255){
    const auto lock=colour.LockRect(16);require(lock.pBits&&lock.pitch>=width*4,"Native surface GPU readback failed");
    U32 tested=0,redCount=0,blueCount=0;
    for(U32 y=0;y<height;++y)for(U32 x=0;x<width;++x){
        const float px=2*(x+0.5f)/width-1,py=1-2*(y+0.5f)/height;
        const int r=useRed?inside(red,px,py):0,b=useBlue?inside(blue,px,py):0;if(r<0||b<0)continue;
        const auto* pixel=lock.pBits+y*lock.pitch+x*4;
        const int expectedRed=r&&!b?255:0,expectedBlue=b?int(blueValue):0,expectedAlpha=b?int(alpha):255;
        require(std::abs(int(pixel[2])-expectedRed)<=2&&pixel[1]<=1&&std::abs(int(pixel[0])-expectedBlue)<=2&&std::abs(int(pixel[3])-expectedAlpha)<=2,
                "Authored shader pixel differs from independent CPU geometry/color reference");
        ++tested;redCount+=r&&!b;blueCount+=b;
    }
    colour.UnlockRect();require(tested>3000&&(!useRed||redCount>50)&&(!useBlue||blueCount>20),"Raster reference did not cover enough pixels");
}
static void fill(C_VertexBuffer& buffer,const Triangle& triangle,F32 z,U32 colour,U32 flags=0){
    auto* bytes=static_cast<U8*>(buffer.Lock(0,buffer.GetSize(),flags));require(bytes,"Native vertex upload lock failed");
    std::memset(bytes,0,buffer.GetSize());
    for(int i=0;i<3;++i){Vertex v{};v.position[0]=triangle[i][0];v.position[1]=triangle[i][1];v.position[2]=z;v.colour=colour;
        std::memcpy(bytes+32+(i+1)*sizeof(Vertex),&v,sizeof(v));}
    buffer.Unlock();
}
static void textureDraws(C_MetalDevice& device,const std::filesystem::path& root,C_Surface& target,C_VertexBuffer& vertices){
    auto& context=device.GetContext();C_VertexShader vs;C_PixelShader ps;std::string error;
    auto v=file(root/"display/vsBasicRenderTextured3D.bmsl"),p=file(root/"display/psBasicRenderTextured.bmsl");
    require(device.CreateVertexShader(v.size(),v.data(),0,&vs,nullptr,&error),error.c_str());
    require(device.CreatePixelShader(p.size(),p.data(),0,&ps,nullptr,&error),error.c_str());
    const F32 identity[]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    context.SetVertexShader(&vs);context.SetPixelShader(&ps);
    context.SetVertexShaderParamF(vs.GetParamFromName("g_WorldMatrix"),identity);context.SetVertexShaderParamF(vs.GetParamFromName("g_ViewProjMatrix"),identity);
    C_VertexFormat f;f.AddElement(0,4,E_VertexElementType_F32_3,E_VertexElementUsage_Position,0);
    f.AddElement(0,16,E_VertexElementType_Colour,E_VertexElementUsage_Colour,0);f.AddElement(0,20,E_VertexElementType_F32_2,E_VertexElementUsage_TexCoord,0);
    C_VertexDeclaration decl;require(device.CreateVertexDeclaration(f,&decl),"Textured declaration failed");
    auto quad=[&](bool constant=false,F32 u=0,F32 v=0){
        auto* bytes=static_cast<U8*>(vertices.Lock());require(bytes,"Textured quad upload failed");
        for(U32 i=0;i<4;++i){Vertex vertex{};vertex.position[0]=i&1?1:-1;vertex.position[1]=i&2?-1:1;vertex.position[2]=0.5;vertex.colour=0xffffffff;
            std::memcpy(bytes+32+i*sizeof(Vertex),&vertex,sizeof(vertex));F32 uv[]={constant?u:F32(i&1),constant?v:F32((i>>1)&1)};std::memcpy(bytes+32+i*sizeof(Vertex)+20,uv,sizeof(uv));}
        vertices.Unlock();
    };
    quad();context.SetVertexBuffer(0,&vertices,32,sizeof(Vertex),&decl);context.SetBlendEnable(E_Bool_False);context.SetDepthEnable(E_Bool_False);context.SetCullMode(E_CullMode_None);context.SetDepthStencilSurface(nullptr);
    auto draw=[&]{context.BeginScene();context.Clear(E_Clear_Colour,0xff000000,1,0);context.DrawPrimitive(E_PrimitiveType_TriangleStrip,0,4);context.EndScene();};
    auto solid=[&](U32 expected){auto r=target.LockRect(16);require(r.pBits,"Texture draw readback failed");
        for(U32 y=0;y<height;++y)for(U32 x=0;x<width;++x)for(U32 c=0;c<4;++c)require(std::abs(int(r.pBits[y*r.pitch+x*4+c])-int((expected>>(c*8))&255))<=1,"Texture draw changed sampled colour");target.UnlockRect();};
    auto upload=[&](C_Texture& t,U32 level,U32 colour){auto r=t.LockRect2D(level);require(r.pBits,"Texture mip upload failed");
        for(U32 y=0;y<t.GetHeight(level);++y)for(U32 x=0;x<t.GetWidth(level);++x)std::memcpy(r.pBits+y*r.pitch+x*4,&colour,4);t.UnlockRect2D(level);};
    C_Texture texture;require(device.CreateTexture2D(2,2,2,0,E_TextureFormat_A8R8G8B8,&texture),"Native mip texture allocation failed");
    require(texture.GetSize()==20&&texture.GetWidth(1)==1&&texture.GetHeight(2)==0&&!texture.LockRect2D(2).pBits,"Texture dimensions/mip bounds differ from PC");
    const U32 colours[]={0xffff0000,0xff00ff00,0xff0000ff,0xffffffff};auto r=texture.LockRect2D(0);require(r.pBits,"Texture lock failed");
    std::memcpy(r.pBits,colours,8);std::memcpy(r.pBits+r.pitch,colours+2,8);texture.UnlockRect2D(0);upload(texture,1,0xff123456);
    context.SetTexture(0,texture.GetGPUTexture());C_SamplerState sample;sample.SetTextureAddress(E_TextureAddress_Clamp);context.SetSamplerState(0,sample);draw();
    r=target.LockRect(16);require(r.pBits,"Point texture readback failed");
    for(U32 y=0;y<height;++y)for(U32 x=0;x<width;++x){U32 actual;std::memcpy(&actual,r.pBits+y*r.pitch+x*4,4);require(actual==colours[(y>=height/2?2:0)+(x>=width/2?1:0)],"Texture orientation or point addressing changed");}target.UnlockRect();
    // Native opaque handles bind real storage, which remains owned by the context after wrapper retirement.
    C_Texture ownerProbe;require(device.CreateTexture2D(1,1,1,0,E_TextureFormat_A8R8G8B8,&ownerProbe),"Borrowed native texture allocation failed");upload(ownerProbe,0,0xff765432);
    context.SetTexture(0,ownerProbe.GetGPUTexture());device.DestroyTexture(&ownerProbe);require(!ownerProbe.GetGPUTexture(),"Destroyed native texture exposed a bindable handle");draw();solid(0xff765432);context.SetTexture(0,texture.GetGPUTexture());
    C_ScissorState scissor;scissor.SetScissorRect({16,8,48,40});scissor.SetScissorEnable(E_Bool_True);
    const auto& liveScissor=context.GetScissorState();
    context.BeginScene();context.Clear(E_Clear_Colour,0xff000000,1,0);context.SetScissorState(scissor);context.DrawPrimitive(E_PrimitiveType_TriangleStrip,0,4);context.EndScene();
    require(liveScissor.m_ScissorEnable&&liveScissor.m_ScissorRect.right==48,"State references did not reflect a native update");
    r=target.LockRect(16);require(r.pBits,"Scissored draw readback failed");
    for(U32 y=0;y<height;++y)for(U32 x=0;x<width;++x){U32 actual;std::memcpy(&actual,r.pBits+y*r.pitch+x*4,4);
        const U32 expected=x>=16&&x<48&&y>=8&&y<40?colours[(y>=32?2:0)+(x>=32?1:0)]:0xff000000;require(actual==expected,"Scissor draw changed outside its rectangle");}target.UnlockRect();
    context.BeginScene();context.SetScissorEnable(E_Bool_False);context.Clear(E_Clear_Colour,0xff000000,1,0);
    context.SetScissorState(scissor);context.SetViewport(C_Viewport(8,16,32,32));context.Clear(E_Clear_Colour,0xff112233,1,0);context.EndScene();
    r=target.LockRect(16);require(r.pBits,"Scissored clear readback failed");
    for(U32 y=0;y<height;++y)for(U32 x=0;x<width;++x){U32 actual;std::memcpy(&actual,r.pBits+y*r.pitch+x*4,4);
        require(actual==(x>=16&&x<40&&y>=16&&y<40?0xff112233:0xff000000),"Clear ignored the viewport/scissor intersection");}target.UnlockRect();
    context.SetColourSurface(0,&target);require(liveScissor.m_ScissorRect.right==S32(width)&&liveScissor.m_ScissorRect.bottom==S32(height),"Target bind did not reset the scissor rectangle");
    draw();r=target.LockRect(16);require(r.pBits,"Viewport reset readback failed");
    for(U32 y=0;y<height;++y)for(U32 x=0;x<width;++x){U32 actual;std::memcpy(&actual,r.pBits+y*r.pitch+x*4,4);require(actual==colours[(y>=32?2:0)+(x>=32?1:0)],"Target bind did not restore its full viewport");}target.UnlockRect();
    context.SetScissorEnable(E_Bool_False);C_FillState fillState;fillState.SetFillMode(E_FillMode_Wireframe);context.SetFillState(fillState);draw();
    r=target.LockRect(16);require(r.pBits,"Wireframe readback failed");U32 wirePixels=0;
    for(U32 y=0;y<height;++y)for(U32 x=0;x<width;++x){const auto* p=r.pBits+y*r.pitch+x*4;if(p[0]||p[1]||p[2]){++wirePixels;
        const float distance=std::min({F32(x),F32(y),F32(width-1-x),F32(height-1-y),std::abs(F32(x+y+1)-width)/std::sqrt(2.f)});
        require(distance<1.6f,"Wireframe shaded a triangle interior");}}
    target.UnlockRect();require(wirePixels>50&&wirePixels<500,"Wireframe did not draw actual edges");context.SetFillMode(E_FillMode_Solid);
    context.BeginScene();context.Clear(E_Clear_Colour,0xff000000,1,0);context.SetScissorRect({16,16,16,32});context.SetScissorEnable(E_Bool_True);context.DrawPrimitive(E_PrimitiveType_TriangleStrip,0,4);context.EndScene();solid(0xff000000);context.SetScissorEnable(E_Bool_False);
    C_ColourState colourState;colourState.SetColourWriteEnable(E_ColourWriteEnable_Blue);context.SetColourState(colourState);draw();
    r=target.LockRect(16);require(r.pBits,"Colour-mask readback failed");for(U32 y=0;y<height;++y)for(U32 x=0;x<width;++x){U32 actual;std::memcpy(&actual,r.pBits+y*r.pitch+x*4,4);require(actual==(y>=32?0xff0000ff:0xff000000),"Whole colour state changed masked channels");}target.UnlockRect();context.SetColourWriteEnable(E_ColourWriteEnable_All);
    const auto previousBlend=context.GetBlendState();C_BlendState preserve;preserve.SetBlendColourSrc(E_Blend_Zero);preserve.SetBlendColourDst(E_Blend_One);preserve.SetBlendAlphaSrc(E_Blend_Zero);preserve.SetBlendAlphaDst(E_Blend_One);
    context.SetBlendState(preserve);context.SetBlendEnable(E_Bool_True);context.BeginScene();context.Clear(E_Clear_Colour,0xff123456,1,0);context.DrawPrimitive(E_PrimitiveType_TriangleStrip,0,4);context.EndScene();solid(0xff123456);context.SetBlendState(previousBlend);context.SetBlendEnable(E_Bool_False);
    context.BeginScene();context.Clear(E_Clear_Colour,0xff000000,1,0);context.SetStencilEnable(E_Bool_True);context.DrawPrimitive(E_PrimitiveType_TriangleStrip,0,4);context.SetStencilEnable(E_Bool_False); // [Blur2PC] PC tests pass when the target has no stencil component.
    context.SetFillMode(E_FillMode_Point);rejects([&]{context.DrawPrimitive(E_PrimitiveType_TriangleStrip,0,4);},"Unported point-fill rendering reported success");context.SetFillMode(E_FillMode_Solid);
    context.SetDepthBias(0.125f);context.DrawPrimitive(E_PrimitiveType_TriangleStrip,0,4);context.SetDepthBias(0);context.EndScene(); // No depth attachment: bias has no depth effect.
    r=target.LockRect(16);require(r.pBits,"Absent-stencil draw readback failed");for(U32 y=0;y<height;++y)for(U32 x=0;x<width;++x){U32 actual;std::memcpy(&actual,r.pBits+y*r.pitch+x*4,4);require(actual==colours[(y>=height/2?2:0)+(x>=width/2?1:0)],"Absent stencil attachment changed the PC texture draw");}target.UnlockRect();
    for(U32 filtering=0;filtering<2;++filtering){
    sample.SetTextureFilter(filtering?E_TextureFilter_Anisotropic:E_TextureFilter_Linear,filtering?E_TextureFilter_Anisotropic:E_TextureFilter_Linear,E_TextureFilter_None);context.SetMaxAnisotropy(8);context.SetSamplerState(0,sample);draw();
    r=target.LockRect(16);require(r.pBits,"Linear texture readback failed");
    for(U32 y=0;y<height;++y)for(U32 x=0;x<width;++x){const F32 u=std::clamp((x+.5f)*2/width-.5f,0.f,1.f),v=std::clamp((y+.5f)*2/height-.5f,0.f,1.f);
        for(U32 c=0;c<4;++c){auto channel=[&](U32 i){return F32((colours[i]>>(c*8))&255);};F32 expected=(channel(0)*(1-u)+channel(1)*u)*(1-v)+(channel(2)*(1-u)+channel(3)*u)*v;
            require(std::abs(r.pBits[y*r.pitch+x*4+c]-expected)<1.6f,"Bilinear/anisotropic magnification differs from CPU reference");}}target.UnlockRect();}
    sample.SetTextureFilter(E_TextureFilter_Point,E_TextureFilter_Point,E_TextureFilter_None);sample.SetTextureAddress(E_TextureAddress_Wrap);context.SetSamplerState(0,sample);quad(true,1.25f,1.25f);draw();solid(colours[0]);
    sample.SetTextureAddress(E_TextureAddress_Mirror);context.SetSamplerState(0,sample);draw();solid(colours[3]);
    sample.SetTextureAddress(E_TextureAddress_Border);sample.SetTextureBorderColour(0xffffffff);context.SetSamplerState(0,sample);quad(true,-1,-1);draw();solid(0xffffffff);
    sample.SetTextureAddress(E_TextureAddress_Clamp);context.SetSamplerState(0,sample);quad();upload(texture,0,0x80808080);draw();solid(0x80808080);
    C_AlphaTestState alpha;alpha.SetAlphaTestCmpFunc(E_CmpFunc_Greater);alpha.SetAlphaTestRef(192);context.SetAlphaTestState(alpha);context.SetAlphaTestEnable(E_Bool_True);draw();solid(0xff000000);
    alpha.SetAlphaTestCmpFunc(E_CmpFunc_Less);context.SetAlphaTestState(alpha);draw();solid(0x80808080);context.SetAlphaTestEnable(E_Bool_False);
    sample.SetTextureGamma(1);context.SetSamplerState(0,sample);draw();solid(0x80373737);sample.SetTextureGamma(0);context.SetSamplerState(0,sample);
    texture.SetSRGB();context.SetTexture(0,&texture);draw();solid(0x80373737);texture.SetSRGB(false);context.SetTexture(0,&texture);
    context.BeginScene();context.DrawPrimitive(E_PrimitiveType_TriangleStrip,0,4);require(!texture.LockRect2D(0).pBits,"CPU modified a sampled unsubmitted texture");context.EndScene();
    C_Texture levels;require(device.CreateTexture2D(128,128,8,0,E_TextureFormat_A8R8G8B8,&levels),"Full mip chain creation failed");
    for(U32 l=0;l<8;++l)upload(levels,l,0xff000000|(l+1)*0x10101);
    context.SetTexture(0,&levels);sample.SetTextureFilter(E_TextureFilter_Point,E_TextureFilter_Point,E_TextureFilter_Point);context.SetSamplerState(0,sample);draw();solid(0xff020202);
    sample.SetTextureFilter(E_TextureFilter_Point,E_TextureFilter_Point,E_TextureFilter_None);context.SetSamplerState(0,sample);draw();solid(0xff010101);
    // A texture surface is the same storage: render into it, then sample it.
    C_Texture render;C_Surface surface;require(device.CreateTexture2D(64,64,1,E_Flags_Colour,E_TextureFormat_A8R8G8B8,&render)&&device.GetTextureSurface(&render,0,0,&surface),"Texture render-surface alias failed");
    context.SetColourSurface(0,&surface);context.BeginScene();context.Clear(E_Clear_Colour,0xff123456,1,0);context.EndScene();context.SetColourSurface(0,&target);context.SetTexture(0,&render);draw();solid(0xff123456);
    context.SetColourSurface(0,&surface);context.BeginScene();rejects([&]{context.DrawPrimitive(E_PrimitiveType_TriangleStrip,0,4);},"Texture render feedback loop accepted");context.EndScene();context.SetColourSurface(0,&target);
    device.Destroy(&render);draw();solid(0xff123456); // Context keeps a bound resource alive.
    C_RenderTexture2D history;
    require(device.CreateRenderTexture2D(width,height,3,E_Flags_ReadWrite,E_TextureFormat_A8R8G8B8,E_SurfaceFormat_A8R8G8B8,E_Multisample_None,nullptr,&history),"Read/write render texture allocation failed");
    require(history.IsCreated()&&history.m_SurfaceIndex==0&&history.m_TextureIndex==1&&history.GetSurface(2).GetWidth()==16,"PC render-texture initial slots/mips changed");
    {C_ResourceTracker tracking;tracking.InsertRenderTexture2D(&history);
        for(U32 i=0;i<2;++i){tracking.InsertTexture(&history.m_Texture[i]);for(S32 l=0;l<history.m_Surfaces[i].Size();++l)tracking.InsertSurface(&history.m_Surfaces[i][l]);}
        tracking.SetRenderTexture2DName(&history,"native-history");
        require(tracking.GetTextureResourceInfo(&history.m_Texture[1])->m_Name=="native-history"&&tracking.GetSurfaceResourceInfo(&history.m_Surfaces[1][2])->m_Name=="native-history","Read/write tracking used a stale usage flag or missed a slot");
        for(U32 i=0;i<2;++i){for(S32 l=0;l<history.m_Surfaces[i].Size();++l)tracking.RemoveSurface(&history.m_Surfaces[i][l]);tracking.RemoveTexture(&history.m_Texture[i]);}tracking.RemoveRenderTexture2D(&history);}
    require(!device.CreateRenderTexture2D(width,height,1,0,E_TextureFormat_A8R8G8B8,E_SurfaceFormat_A8R8G8B8,E_Multisample_None,nullptr,&history),"Existing render texture was overwritten");
    upload(history.GetTexture(),0,0xff0000ff);context.SetTexture(0,&history.GetTexture());draw();solid(0xff0000ff);
    for(U32 value:{0xffff0000U,0xff00ff00U}){context.SetColourSurface(0,&history.GetSurface());context.BeginScene();context.Clear(E_Clear_Colour,value,1,0);context.EndScene();history.Resolve();
        context.SetColourSurface(0,&target);context.SetTexture(0,&history.GetTexture());draw();solid(value);}
    require(history.m_TextureIndex==1&&history.m_SurfaceIndex==0,"Read/write resolve did not alternate both slots");
    context.SetColourSurface(0,&history.GetSurface(1));context.BeginScene();context.Clear(E_Clear_Colour,0xff123456,1,0);context.EndScene();history.Resolve(1);
    auto mip=history.GetTexture().LockRect2D(1,16);require(mip.pBits,"Rendered history mip readback failed");
    for(U32 y=0;y<32;++y)for(U32 x=0;x<32;++x){U32 value;std::memcpy(&value,mip.pBits+y*mip.pitch+x*4,4);require(value==0xff123456,"Render-texture mip alias wrote different storage");}history.GetTexture().UnlockRect2D(1);
    context.SetColourSurface(0,&target);context.SetTexture(0,&history.GetTexture());device.Destroy(&history);require(!history.IsCreated()&&!history.GetFlags(),"Render texture destroy kept its metadata/resources");draw();solid(0xffff0000);
    C_RenderTexture2D single;
    require(device.CreateRenderTexture2D(width,height,1,0,E_TextureFormat_A8R8G8B8,E_SurfaceFormat_A8R8G8B8,E_Multisample_2,nullptr,&single)&&single.GetSurface().GetMultisample()==E_Multisample_2,"Native multisample render target allocation failed");device.Destroy(&single);
    require(device.CreateRenderTexture2D(width,height,1,0,E_TextureFormat_A8R8G8B8,E_SurfaceFormat_A8R8G8B8,E_Multisample_None,nullptr,&single),"Single-slot render texture allocation failed");
    context.SetColourSurface(0,&single.GetSurface());context.BeginScene();context.Clear(E_Clear_Colour,0xff123456,1,0);context.EndScene();single.Resolve();require(single.m_TextureIndex==0&&single.m_SurfaceIndex==0,"Single-slot resolve switched to absent storage");
    context.SetColourSurface(0,&target);context.SetTexture(0,&single.GetTexture());draw();solid(0xff123456);device.Destroy(&single);
    for(E_TextureFormat format:{E_TextureFormat_DXT1,E_TextureFormat_DXT3,E_TextureFormat_DXT5}){
        C_Texture compressed;require(device.CreateTexture2D(8,8,4,0,format,&compressed),"BC texture allocation failed");
        for(U32 l=0;l<4;++l){auto lock=compressed.LockRect2D(l);require(lock.pBits,"BC mip staging lock failed");const U32 bytes=format==E_TextureFormat_DXT1?8:16;
            U8 block[16]={};if(format==E_TextureFormat_DXT3)std::memset(block,255,8);if(format==E_TextureFormat_DXT5)block[0]=255;
            const U16 red=0xf800;std::memcpy(block+(bytes==8?0:8),&red,2);
            for(U32 y=0;y<(compressed.GetHeight(l)+3)/4;++y)for(U32 x=0;x<(compressed.GetWidth(l)+3)/4;++x)std::memcpy(lock.pBits+y*lock.pitch+x*bytes,block,bytes);
            compressed.UnlockRect2D(l);}
        context.SetTexture(0,&compressed);draw();solid(0xffff0000);
    }
    C_Texture cube;require(device.CreateTextureCube(4,3,0,E_TextureFormat_A8R8G8B8,&cube),"Cube texture allocation failed");
    for(U32 face=0;face<6;++face)for(U32 l=0;l<3;++l){const U32 colour=0xff000000|(face+1)*0x10101+l;auto lock=cube.LockRectCube(face,l);require(lock.pBits,"Cube face/mip lock failed");
        for(U32 y=0;y<cube.GetHeight(l);++y)for(U32 x=0;x<cube.GetWidth(l);++x)std::memcpy(lock.pBits+y*lock.pitch+x*4,&colour,4);cube.UnlockRectCube(face,l);
        auto read=cube.LockRectCube(face,l,16);require(read.pBits,"Cube face readback failed");U32 actual;std::memcpy(&actual,read.pBits,4);require(actual==colour,"Cube faces/mips alias another subresource");cube.UnlockRectCube(face,l);}
    struct ChannelCase {E_TextureFormat format;U32 bytes,data,expected;};
    for(const auto& c:std::initializer_list<ChannelCase>{{E_TextureFormat_A8,1,128,0x80000000},{E_TextureFormat_L8,1,64,0xff404040},
        {E_TextureFormat_A8L8,2,0x8040,0x80404040},{E_TextureFormat_X8R8G8B8,4,0x00123456,0xff123456},
        {E_TextureFormat_R32F,4,0x3e800000,0xff40ffff},{E_TextureFormat_R16F,2,0x3400,0xff40ffff},
        {E_TextureFormat_V8U8,2,0x2040,0xff8040ff}}){
        const bool unsupportedGamma=c.format!=E_TextureFormat_X8R8G8B8;sample.SetTextureGamma(unsupportedGamma);context.SetSamplerState(0,sample);
        C_Texture channel;require(device.CreateTexture2D(2,2,1,unsupportedGamma?E_Flags_SRGB:0,c.format,&channel),"Texture channel format allocation failed");auto lock=channel.LockRect2D(0);require(lock.pBits,"Texture channel upload failed");
        for(U32 y=0;y<2;++y)for(U32 x=0;x<2;++x)std::memcpy(lock.pBits+y*lock.pitch+x*c.bytes,&c.data,c.bytes);channel.UnlockRect2D(0);
        context.SetTexture(0,&channel);draw();solid(c.expected);
    }
    sample.SetTextureGamma(0);context.SetSamplerState(0,sample);
    C_Texture volume;require(device.CreateTexture3D(4,8,4,4,0,E_TextureFormat_A8R8G8B8,&volume),"Volume mip allocation failed");
    require(volume.GetDepth(2)==1&&volume.GetWidth(3)==1&&volume.GetHeight(3)==1&&volume.GetSize()==588,"Volume mip dimensions/size changed");
    require(GetTexture3DSize(4,8,4,4,0,E_TextureFormat_A8R8G8B8)==volume.GetSize()&&GetTextureCubeSize(4,3,0,E_TextureFormat_A8R8G8B8)==cube.GetSize(),"Shared texture size helpers disagree with native subresources");
    require(GetTexturePitchFromWidth(5,0,E_TextureFormat_DXT1)==16&&GetTexture2DSize(5,7,3,0,E_TextureFormat_DXT1)==48,"BC disk row/mip sizing changed");
    for(U32 l=0;l<4;++l){auto lock=volume.LockBox3D(l);require(lock.pBits,"Volume mip lock failed");
        for(U32 z=0;z<volume.GetDepth(l);++z)for(U32 y=0;y<volume.GetHeight(l);++y)for(U32 x=0;x<volume.GetWidth(l);++x){U32 value=0xff000000|(z+1)*0x10000|(y+1)*0x100|(x+1+l);std::memcpy(lock.pBits+z*lock.slicePitch+y*lock.rowPitch+x*4,&value,4);}volume.UnlockBox3D(l);
        auto read=volume.LockBox3D(l,16);require(read.pBits,"Volume mip readback failed");
        for(U32 z=0;z<volume.GetDepth(l);++z)for(U32 y=0;y<volume.GetHeight(l);++y)for(U32 x=0;x<volume.GetWidth(l);++x){U32 value;std::memcpy(&value,read.pBits+z*read.slicePitch+y*read.rowPitch+x*4,4);require(value==(0xff000000|(z+1)*0x10000|(y+1)*0x100|(x+1+l)),"Volume slices/mips overlapped");}volume.UnlockBox3D(l);
    }
    C_Texture noise;require(device.CreateTexture3D(16,16,16,5,0,E_TextureFormat_A8R8G8B8,&noise),"Perlin noise volume allocation failed");
    const U32 reds[]={32,64,96,128,192};F32 reference=0,amplitude=0.5f;
    for(S32 l=4;l>=0;--l){reference+=(reds[l]/255.f*2-1)*amplitude;amplitude*=0.5f;auto lock=noise.LockBox3D(l);require(lock.pBits,"Noise volume mip upload failed");
        for(U32 z=0;z<noise.GetDepth(l);++z)for(U32 y=0;y<noise.GetHeight(l);++y)for(U32 x=0;x<noise.GetWidth(l);++x){U32 value=0xff000000|(reds[l]<<16);std::memcpy(lock.pBits+z*lock.slicePitch+y*lock.rowPitch+x*4,&value,4);}noise.UnlockBox3D(l);}
    C_PixelShader perlin;auto perlinBytes=file(root/"render/psPerlin.bmsl");require(device.CreatePixelShader(perlinBytes.size(),perlinBytes.data(),0,&perlin,nullptr,&error),error.c_str());
    context.SetPixelShader(&perlin);F32 settings[24]={0.5f};context.SetPixelShaderParamF(perlin.GetParamFromName("g_Settings"),settings);
    sample.SetTextureFilter(E_TextureFilter_Point,E_TextureFilter_Point,E_TextureFilter_Point);context.SetSamplerState(0,sample);context.SetTexture(0,&noise);draw();
    const U32 grey=U32(std::lround((reference*.5f+.5f)*255));solid(grey*0x1010101);
    C_PixelShader cubeShader;auto cubeBytes=file(root/"render/psTextureCubeColour.bmsl");require(device.CreatePixelShader(cubeBytes.size(),cubeBytes.data(),0,&cubeShader,nullptr,&error),error.c_str());
    context.SetPixelShader(&cubeShader);context.SetTexture(0,&cube);sample.SetTextureFilter(E_TextureFilter_Point,E_TextureFilter_Point,E_TextureFilter_None);context.SetSamplerState(0,sample);
    const F32 basis[4][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};const char* names[]={"g_vRed","g_vGreen","g_vBlue","g_vAlpha"};
    for(U32 c=0;c<4;++c)context.SetPixelShaderParamF(cubeShader.GetParamFromName(names[c]),basis[c]);
    const F32 zero[4]={};context.SetPixelShaderParamF(cubeShader.GetParamFromName("g_vBias"),zero);
    for(U32 face=0;face<6;++face){F32 direction[4]={};direction[face/2]=face&1?-1:1;
        for(const char* name:{"g_vTopLeft","g_vTopRight","g_vBottomLeft","g_vBottomRight"})context.SetPixelShaderParamF(cubeShader.GetParamFromName(name),direction);
        draw();solid(0xff000000|(face+1)*0x10101);}
    C_RenderTextureCube renderedCube;require(device.CreateRenderTextureCube(8,3,0,E_TextureFormat_A8R8G8B8,E_SurfaceFormat_A8R8G8B8,E_Multisample_None,nullptr,&renderedCube),"Render cube allocation failed");
    for(U32 face=0;face<6;++face)for(U32 level=0;level<3;++level){const U32 value=0xff000000|(face+1)*0x10101+level;context.SetColourSurface(0,&renderedCube.GetSurface(face,level));context.BeginScene();context.Clear(E_Clear_Colour,value,1,0);context.EndScene();
        auto lock=renderedCube.GetTexture().LockRectCube(face,level,16);require(lock.pBits,"Rendered cube mip readback failed");
        for(U32 y=0;y<renderedCube.GetTexture().GetHeight(level);++y)for(U32 x=0;x<renderedCube.GetTexture().GetWidth(level);++x){U32 actual;std::memcpy(&actual,lock.pBits+y*lock.pitch+x*4,4);require(actual==value,"Render cube face/mip alias wrote another subresource");}renderedCube.GetTexture().UnlockRectCube(face,level);}
    context.SetColourSurface(0,&target);context.SetTexture(0,&renderedCube.GetTexture());
    for(U32 face=0;face<6;++face){F32 direction[4]={};direction[face/2]=face&1?-1:1;
        for(const char* name:{"g_vTopLeft","g_vTopRight","g_vBottomLeft","g_vBottomRight"})context.SetPixelShaderParamF(cubeShader.GetParamFromName(name),direction);
        draw();solid(0xff000000|(face+1)*0x10101);}
    device.Destroy(&renderedCube);require(!renderedCube.IsCreated(),"Render cube destroy kept its storage");
    context.SetTexture(0,nullptr);
    C_Surface metadata;require(device.CreateSurface(width,height,E_Flags_Colour|E_Flags_SRGB,E_SurfaceFormat_A8R8G8B8,E_Multisample_None,nullptr,&metadata),"SRGB-metadata surface allocation failed");
    context.SetColourSurface(0,&metadata);context.BeginScene();context.Clear(E_Clear_Colour,0xff808080,1,0);context.EndScene();auto read=metadata.LockRect(16);require(read.pBits,"SRGB metadata surface readback failed");
    for(U32 y=0;y<height;++y)for(U32 x=0;x<width;++x){U32 value;std::memcpy(&value,read.pBits+y*read.pitch+x*4,4);require(value==0xff808080,"Texture gamma metadata changed PC linear render writes");}metadata.UnlockRect();context.SetColourSurface(0,&target);
}
int main(int argc,char** argv){
    try {
        require(argc==2,"Authored shader directory missing");
        const auto root=std::filesystem::path(argv[1]);C_MetalDevice device;require(device.Create(),"Native Metal device failed");
        auto& context=device.GetContext();
        Btl::Vector<S32> sorted;for(S32 i=0;i<513;++i)sorted.PushBack((i*173)%513);std::sort(sorted.Begin(),sorted.End());
        for(S32 i=0;i<513;++i)require(sorted[i]==i,"Engine checked iterators broke native resource ordering");
        C_ResourceTracker tracking;C_VertexBuffer tracked;
        require(device.CreateVertexBuffer(128,0,&tracked),"Tracked vertex allocation failed");tracking.InsertVertexBuffer(&tracked);
        require(tracking.GetVertexBufferCount()==1,"Shared resource tracker lost native buffer");
        tracking.GetVertexBufferResourceInfo(&tracked)->m_Name="native-tracked-buffer";
        void* found=nullptr;void* info=nullptr;E_ResourceType kind=E_ResourceType_Unknown;
        require(tracking.FindResourceFromAddress(tracked.GetAddress(),&found,&info,kind)&&found==&tracked&&kind==E_ResourceType_VertexBuffer,"Native buffer address lookup failed");
        require(!tracking.FindResourceFromAddress(static_cast<U8*>(tracked.GetAddress())+64,&found,&info,kind),"Shared tracker changed its exact-address lookup contract");
        tracking.DumpVertexBuffers();tracking.RemoveVertexBuffer(&tracked);device.Destroy(&tracked);
        require(tracking.GetVertexBufferCount()==0,"Shared resource tracker kept destroyed native buffer");
        C_VertexShader vertex;C_PixelShader pixel;
        const auto vs=file(root/"display/vsBasicRenderColoured3D.bmsl"),ps=file(root/"display/psBasicRenderColoured.bmsl");
        std::string error;
        require(device.CreateVertexShader(vs.size(),vs.data(),0,&vertex,nullptr,&error),error.c_str());
        require(device.CreatePixelShader(ps.size(),ps.data(),0,&pixel,nullptr,&error),error.c_str());
        const auto world=vertex.GetParamFromName("g_WorldMatrix"),view=vertex.GetParamFromName("g_ViewProjMatrix");
        C_VertexFormat format;format.AddElement(0,4,E_VertexElementType_F32_3,E_VertexElementUsage_Position,0);
        format.AddElement(0,16,E_VertexElementType_Colour,E_VertexElementUsage_Colour,0);
        require(format.GetStride(0)==20,"Explicit vertex padding was ignored");
        C_VertexDeclaration declaration;require(device.CreateVertexDeclaration(format,&declaration),"Native declaration creation failed");
        C_VertexElement element;SetVertexElementStream(&element,U16(1));SetVertexElementType(&element,U32(E_VertexElementType_F32_3));SetVertexElementOffset(&element,12);
        require(GetVertexElementStream(&element)==1&&GetVertexElementSize(&element)==12&&GetVertexElementOffset(&element)==12,"Shared engine context helpers do not use native resources");
        require(declaration.GetStride(0)==20&&declaration.GetElementCount()==2,"Native declaration lost padded layout");
        require(device.CreateVertexDeclaration(format)==device.CreateVertexDeclaration(format),"Declaration deduplication failed");
        C_VertexBuffer buffer;C_IndexBuffer indices;C_Surface colour,depth;
        require(device.CreateVertexBuffer(32+4*sizeof(Vertex),E_Flags_Dynamic,&buffer),"Vertex allocation failed");
        require(device.CreateIndexBuffer(10,0,E_IndexFormat_16,&indices),"Index allocation failed");
        const U16 indexData[]={65535,65535,0,1,2};auto* indexBytes=indices.Lock();require(indexBytes,"Index upload failed");std::memcpy(indexBytes,indexData,sizeof(indexData));indices.Unlock();
        require(device.CreateSurface(width,height,E_Flags_Colour,E_SurfaceFormat_A8R8G8B8,E_Multisample_None,nullptr,&colour),"Color target allocation failed");
        require(device.CreateSurface(width,height,E_Flags_DepthStencil,E_SurfaceFormat_D16,E_Multisample_None,nullptr,&depth),"Depth target allocation failed");
        const Triangle source={{{-0.75f,-0.75f},{0.75f,-0.75f},{0,0.75f}}};
        const Triangle blue={{{-0.9f,0.6f},{-0.3f,0.6f},{-0.6f,0.95f}}};
        Triangle transformed=source;
        for(auto& p:transformed){p[0]=p[0]*0.5f+0.2f;p[1]=p[1]*0.75f-0.1f;p[0]+=p[1]*0.15f;}
        fill(buffer,source,0.6f,0xffff0000);
        context.SetColourSurface(0,&colour);context.SetDepthStencilSurface(&depth);context.SetViewport(C_Viewport(0,0,width,height));
        context.SetVertexBuffer(0,&buffer,32,sizeof(Vertex),&declaration);context.SetIndexBuffer(&indices);
        context.SetVertexShader(&vertex);context.SetPixelShader(&pixel);context.SetDepthEnable(E_Bool_True);context.SetCullMode(E_CullMode_None);
        const F32 worldMatrix[]={.5f,0,0,0,0,.75f,0,0,0,0,1,0,.2f,-.1f,0,1};
        const F32 viewMatrix[]={1,0,0,0,.15f,1,0,0,0,0,1,0,0,0,0,1};
        const F32 identity[]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        context.SetVertexShaderParamF(world,worldMatrix);context.SetVertexShaderParamF(view,viewMatrix);
        rejects([&]{context.DrawPrimitive(E_PrimitiveType_TriangleList,0,3);},"Draw outside a scene was accepted");
        context.BeginScene();context.Clear(E_Clear_Colour|E_Clear_Depth,0xff000000,1,0);
        std::atomic<bool> foreignRejected{false};std::thread foreign([&]{try{context.EndScene();}catch(const std::logic_error&){foreignRejected=true;}});foreign.join();require(foreignRejected,"Foreign thread submitted another thread's scene");
        rejects([&]{context.DrawIndexedPrimitive(E_PrimitiveType_TriangleList,1,0,3,0,3);},"Invalid index vertex range was accepted");
        context.DrawIndexedPrimitive(E_PrimitiveType_TriangleList,1,0,3,2,3,nullptr);
        require(!buffer.Lock(),"CPU wrote a buffer used by an unsubmitted draw");
        require(!colour.LockRect(16).pBits,"CPU read an unsubmitted render target");
        auto* unused=buffer.Lock(0,16,0x1000);require(unused,"NOOVERWRITE rejected a disjoint dynamic range");std::memset(unused,0x7b,16);buffer.Unlock();
        require(!buffer.Lock(64,16,0x1000),"NOOVERWRITE changed a recorded vertex range");
        fill(buffer,blue,0.2f,0xff0000ff,0x2000); // Discard renames storage; the first draw keeps its old bytes.
        context.SetVertexShaderParamF(world,identity);context.SetVertexShaderParamF(view,identity);
        context.DrawIndexedPrimitive(E_PrimitiveType_TriangleList,1,0,3,2,3);
        auto alias=buffer;device.Destroy(&buffer);device.Destroy(&vertex);device.Destroy(&pixel);device.Destroy(&declaration);
        context.EndScene();require(device.WaitIdle(),"Authored shader rendering reported a GPU error");
        pixels(colour,transformed,blue);
        auto z=depth.LockRect(16);require(z.pBits,"Depth target readback failed");
        U16 centre=0;std::memcpy(&centre,z.pBits+40*z.pitch+36*2,2);require(std::abs(int(centre)-39321)<3,"Depth buffer did not receive transformed vertex depth");depth.UnlockRect();
        context.SetDepthEnable(E_Bool_False);context.SetAlphaTestEnable(E_Bool_True);context.SetAlphaTestFunc(E_CmpFunc_Greater);context.SetAlphaTestRef(128);
        fill(alias,blue,0.2f,0x400000ff);context.BeginScene();context.Clear(E_Clear_Colour|E_Clear_Depth,0xff000000,1,0);context.DrawIndexedPrimitive(E_PrimitiveType_TriangleList,1,0,3,2,3);context.EndScene();
        pixels(colour,transformed,blue,false,false);
        fill(alias,blue,0.2f,0xc00000ff);context.BeginScene();context.Clear(E_Clear_Colour,0xff000000,1,0);context.DrawIndexedPrimitive(E_PrimitiveType_TriangleList,1,0,3,2,3);context.EndScene();
        pixels(colour,transformed,blue,false,true,255,192);
        context.SetAlphaTestEnable(E_Bool_False);context.SetBlendEnable(E_Bool_True);context.SetBlendSrc(E_Blend_SrcAlpha);context.SetBlendDst(E_Blend_InvSrcAlpha);context.SetBlendDstAlpha(E_Blend_Zero);
        fill(alias,blue,0.2f,0x800000ff);context.BeginScene();context.Clear(E_Clear_Colour,0xff000000,1,0);context.DrawIndexedPrimitive(E_PrimitiveType_TriangleList,1,0,3,2,3);context.EndScene();
        pixels(colour,transformed,blue,false,true,128,128);
        context.BeginScene();context.Clear(E_Clear_Colour|E_Clear_Depth,0xff000000,1,0);
        context.SetViewport(C_Viewport(30,30,16,16));context.Clear(E_Clear_Colour|E_Clear_Depth,0xff112233,0.25f,0);
        context.SetViewport(C_Viewport(0,0,width,height));context.DrawIndexedPrimitive(E_PrimitiveType_TriangleList,1,0,3,2,3);context.EndScene();
        auto rectangle=colour.LockRect(16);require(rectangle.pBits,"Viewport-clear readback failed");
        for(U32 y=0;y<height;++y)for(U32 x=0;x<width;++x){
            const int b=inside(blue,2*(x+0.5f)/width-1,1-2*(y+0.5f)/height);if(b<0)continue;
            const bool region=x>=30&&x<46&&y>=30&&y<46;
            const auto* p=rectangle.pBits+y*rectangle.pitch+x*4;
            require(std::abs(int(p[0])-(b?128:region?51:0))<=2&&p[1]==(region?34:0)&&p[2]==(region?17:0)&&std::abs(int(p[3])-(b?128:255))<=2,
                    "Viewport clear changed outside its rectangle or leaked draw scissor state");
        }
        colour.UnlockRect();auto rectangleDepth=depth.LockRect(16);require(rectangleDepth.pBits,"Viewport depth clear readback failed");
        for(U32 y=0;y<height;++y)for(U32 x=0;x<width;++x){U16 value;std::memcpy(&value,rectangleDepth.pBits+y*rectangleDepth.pitch+x*2,2);
            require(std::abs(int(value)-(x>=30&&x<46&&y>=30&&y<46?16384:65535))<=1,"Viewport depth clear exceeded its rectangle");}
        depth.UnlockRect();
        C_VertexFormat split;split.AddElement(0,4,E_VertexElementType_F32_3,E_VertexElementUsage_Position,0);
        split.AddElement(1,16,E_VertexElementType_Colour,E_VertexElementUsage_Colour,0);
        C_VertexDeclaration splitDeclaration;require(device.CreateVertexDeclaration(split,&splitDeclaration),"Multistream declaration failed");
        context.SetVertexDeclaration(&splitDeclaration);context.SetVertexBuffer(1,&alias,32,sizeof(Vertex));
        C_IndexBuffer wide;require(device.CreateIndexBuffer(20,0,E_IndexFormat_32,&wide),"32-bit index allocation failed");
        U32 wideData[]={~0U,~0U,0,1,2};auto* wideBytes=wide.Lock();require(wideBytes,"32-bit index upload failed");std::memcpy(wideBytes,wideData,sizeof(wideData));wide.Unlock();
        context.SetIndexBuffer(&wide);context.BeginScene();context.Clear(E_Clear_Colour,0xff000000,1,0);
        context.DrawIndexedPrimitive(E_PrimitiveType_TriangleFan,1,0,3,2,3);context.EndScene();pixels(colour,transformed,blue,false,true,128,128);
        context.BeginScene();context.Clear(E_Clear_Colour,0xff000000,1,0);context.DrawPrimitive(E_PrimitiveType_TriangleFan,1,3);context.EndScene();
        pixels(colour,transformed,blue,false,true,128,128);
        auto* cached=device.CreateVertexDeclaration(format);device.DestroyVertexDeclaration(cached);
        require(!cached->IsCreated()&&device.CreateVertexDeclaration(format,cached),"Owned declaration could not be recreated after destroy");
        for(bool signedType:{false,true}){
            C_VertexFormat packed;packed.AddElement(0,4,signedType?E_VertexElementType_S10_3N:E_VertexElementType_U10_3,E_VertexElementUsage_Position,0);
            packed.AddElement(0,16,E_VertexElementType_Colour,E_VertexElementUsage_Colour,0);
            C_VertexDeclaration packedDeclaration;require(device.CreateVertexDeclaration(packed,&packedDeclaration),"Packed declaration failed");
            const S32 coordinates[2][3][3]={{{128,128,600},{895,128,600},{512,895,600}},{{-384,-384,256},{384,-384,256},{0,384,256}}};
            const auto& positions=coordinates[signedType?1:0];Triangle reference;
            auto* bytes=static_cast<U8*>(alias.Lock());require(bytes,"Packed vertex upload failed");
            for(U32 i=0;i<3;++i){const U32 value=(U32(positions[i][0])&1023)|((U32(positions[i][1])&1023)<<10)|((U32(positions[i][2])&1023)<<20)|0xc0000000;
                const U32 tint=0xff0000ff;std::memcpy(bytes+32+(i+1)*sizeof(Vertex)+4,&value,4);std::memcpy(bytes+32+(i+1)*sizeof(Vertex)+16,&tint,4);
                for(U32 c=0;c<2;++c)reference[i][c]=signedType?positions[i][c]/511.f:positions[i][c]*2/1023.f-1;}
            alias.Unlock();
            const F32 unpackMatrix[]={2/1023.f,0,0,0,0,2/1023.f,0,0,0,0,1/1023.f,0,-1,-1,0,1};
            context.SetVertexShaderParamF(world,signedType?identity:unpackMatrix);context.SetVertexShaderParamF(view,identity);
            context.SetVertexBuffer(0,&alias,32,sizeof(Vertex),&packedDeclaration);context.SetBlendEnable(E_Bool_False);
            context.BeginScene();context.Clear(E_Clear_Colour,0xff000000,1,0);context.DrawIndexedPrimitive(E_PrimitiveType_TriangleList,1,0,3,2,3);context.EndScene();
            pixels(colour,source,reference,false,true);
        }
        textureDraws(device,root,colour,alias);
        auto write=colour.LockRect();require(write.pBits,"Surface staging write lock failed");write.pBits[0]=23;write.pBits[1]=45;write.pBits[2]=67;write.pBits[3]=89;colour.UnlockRect();
        auto read=colour.LockRect(16);require(read.pBits&&read.pBits[0]==23&&read.pBits[1]==45&&read.pBits[2]==67&&read.pBits[3]==89,"Surface staging round trip changed bytes");colour.UnlockRect();
        require(device.WaitIdle(),"Final render submission failed");
        std::cout<<"Authored shader engine draw: offsets/matrices, dynamic renaming, depth/alpha/blend, clears, packed vertices, 2D/3D/cube textures, BC, samplers/gamma/mips and staging PASS\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
