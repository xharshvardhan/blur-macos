// [Blur2PC][macOS] Original BasicRender lifecycle/shaders/dynamic geometry through actual native GPU state.
#include <display/render/BasicRender.h>
#include <display/view/View.h>
#include <display/shaders/Constants.h>
#include <system/serialise.h>
#include <system/serialise/macos/NativeFile.h>
#include <filesystem>
#include <cstring>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace Display;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){try{
    require(argc==2,"Native BasicRender shader directory missing");auto root=std::filesystem::path(argv[1]);auto& fs=Sys::C_FileSystem::Instance();Sys::SetDefaultFileSystem(&fs);require(fs.SetWorkingDirectory(root.string().c_str()),"Native BasicRender filesystem setup failed");
    C_Device device;C_DeviceParams params;params.m_Width=64;params.m_Height=32;require(device.Create(params)&&device.CreateVolatile(params),"Native BasicRender device failed");
    require(GPU_RGBA(0x12,0xab,0x34,0x56)==0x5612ab34&&GPU_RGBA(Maths::RGBA(0x12,0xab,0x34,0x56))==0x5612ab34,"Native packed colour overloads differ from actual PC vertex layout");
    C_View view;C_ViewSetup setup;setup.m_ViewSetup2D.m_ViewExtents.SetBounds(0,0,64,32);view.Setup(setup,64,32);
    {
        C_BasicRender render;
        const auto missing=root/"Shaders/psBasicRenderTextured.bmsl",saved=root/"Shaders/psBasicRenderTextured.saved";std::filesystem::rename(missing,saved);
        bool created=render.Create();std::filesystem::rename(saved,missing);require(!created&&!render.IsCreated(),"Original BasicRender partial creation did not fail cleanly");
        require(render.Create()&&render.CreateVolatile()&&render.Initialise()&&render.IsCreated()&&BasicRender()==&render,"Original BasicRender shader creation/lifecycle failed");
        auto begin=[&]{require(device.BeginScene(),"Native BasicRender scene failed");device.SetView(&view);device.GetDisplay2().GetContext().SetCullMode(Display2::E_CullMode_None);device.GetDisplay2().GetContext().SetDepthEnable(Display2::E_Bool_False);device.Clear(E_Clear_Colour,Maths::RGBA(0,0,0,255));};
        auto verify=[&](U32 left,U32 right){device.EndScene();auto* target=device.GetNativeColourSurface();auto data=target->LockRect(16);require(data.pBits,"BasicRender GPU pixels unavailable");bool correct=true;for(U32 y=0;y<32;++y)for(U32 x=0;x<64;++x)for(U32 c=0;c<4;++c){const U32 expected=x<32?left:right;correct&=std::abs(int(data.pBits[y*data.pitch+x*4+c])-int((expected>>(c*8))&255))<=1;}target->UnlockRect();require(correct,"Original BasicRender shader/geometry/view/blend changed expected GPU pixels");device.Present();};
        begin();render.DrawColoured2DRect(0,0,32,32,Maths::RGBA(255,0,0,255),E_BlendMode_Solid);device.GetDisplay2().GetContext().SetBlendEnable(Display2::E_Bool_True);device.GetDisplay2().GetContext().SetBlendSrc(Display2::E_Blend_Zero);device.GetDisplay2().GetContext().SetBlendDst(Display2::E_Blend_Zero);render.DrawColoured2DRect(32,0,32,32,Maths::RGBA(0,0,255,255),E_BlendMode_Solid);verify(0xffff0000,0xff0000ff);
        begin();device.SetView(nullptr);render.DrawColoured2DRect(0,0,512,768,Maths::RGBA(255,0,0,255),E_BlendMode_Solid);render.DrawColoured2DRect(512,0,512,768,Maths::RGBA(0,0,255,255),E_BlendMode_Solid);verify(0xffff0000,0xff0000ff);
        begin();device.SetColourWriteEnable(E_ColourWriteEnable_All);render.DrawColoured2DRect(0,0,32,32,Maths::RGBA(255,0,0,255),E_BlendMode_Solid);render.DrawColoured2DRect(0,0,32,32,Maths::RGBA(0,255,0,128),E_BlendMode_Alpha);verify(0xbf7f8000,0xff000000);
        struct Texture{C_Device& device;C_Texture2D value;~Texture(){device.Destroy(&value);}} owner{device,{}};auto& texture=owner.value;require(device.CreateTexture2D(2,2,1,0,E_Format_A8R8G8B8,&texture),"BasicRender texture creation failed");auto data=texture.LockRect2D(0);require(data.pBits,"BasicRender texture upload failed");const U32 colour=0xff1b72cd;for(U32 y=0;y<2;++y)for(U32 x=0;x<2;++x)std::memcpy(data.pBits+y*data.pitch+x*4,&colour,4);texture.UnlockRect2D(0);
        begin();render.DrawTextured2DRect(0,0,64,32,Maths::RGBA(255,255,255,255),&texture);verify(colour,colour);
        begin();const F32 identity[]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};device.SetVertexShaderConstantF(CV_DISPLAY_VIEWPROJ_MATRIX,identity,4);const C_Float2 uv[]={C_Float2(0,0),C_Float2(1,0),C_Float2(0,1)};render.DrawTextured3DTriangle(Maths::HVec3(-1,-1,.5),Maths::HVec3(3,-1,.5),Maths::HVec3(-1,3,.5),uv,Maths::C_RGBA_F32(1,1,1,1),&texture);verify(colour,colour);
        device.Destroy(&texture);render.DestroyRenderer();require(!render.IsCreated(),"Original BasicRender destroy retained shaders");
    }
    require(!BasicRenderExists(),"Original BasicRender destructor retained singleton");device.SetView(nullptr,false);device.DestroyVolatile();device.Destroy();Sys::SetDefaultFileSystem(nullptr);
    std::cout<<"Original BasicRender creation rollback/2D colours/alpha blending/2D and 3D textures/native view constants/transient geometry PASS (not game startup)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
