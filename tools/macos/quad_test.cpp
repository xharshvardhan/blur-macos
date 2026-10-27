// [Blur2PC][macOS] Original fullscreen/partial/split-viewport drawing with source-built default shader registries.
#include <display/device/Device.h>
#include <display/render/Render.h>
#include <display/buffer/VertexDeclarations.h>
#include <display/shader/VertexShaders.h>
#include <display/shader/PixelShaders.h>
#include <system/serialise.h>
#include <system/serialise/macos/NativeFile.h>
#include <filesystem>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <cmath>
using namespace Display;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct ShaderScope{~ShaderScope(){Display::DestroyPixelShaders();Display::DestroyVertexShaders();}};
struct TextureScope{C_Device& device;C_Texture2D value;~TextureScope(){device.Destroy(&value);}};
int main(int argc,char** argv){try{
    require(argc==2,"Original Quad shader fixture directory missing");const auto root=std::filesystem::path(argv[1]);auto& fs=Sys::C_FileSystem::Instance();Sys::SetDefaultFileSystem(&fs);require(fs.SetWorkingDirectory(root.string().c_str()),"Original Quad filesystem setup failed");
    C_Device device;C_DeviceParams params;params.m_Width=64;params.m_Height=32;require(device.Create(params)&&device.CreateVolatile(params),"Original Quad native GPU setup failed");device.SetCullMode(E_CullMode_Default);
    {
        ShaderScope shaders;Display::CreateVertexDeclarations();Display::CreateVertexShaders();
        const auto missing=root/"Shaders/psDefaultTextureD.bmsl",saved=root/"Shaders/psDefaultTextureD.saved";std::filesystem::rename(missing,saved);bool failed=false;try{Display::CreatePixelShaders();}catch(const std::runtime_error&){failed=true;}std::filesystem::rename(saved,missing);require(failed&&!PixelShader::m_PixelShaderColour.IsCreated()&&!PixelShader::m_PixelShaderTexture.IsCreated(),"Native default pixel factory did not roll back failed creation");Display::CreatePixelShaders();
        const C_Viewport full=device.GetViewport();require(full.GetW()==64&&full.GetH()==32,"Original CPU viewport did not match native framebuffer");
        auto begin=[&]{require(device.BeginScene(),"Original Quad scene failed");device.SetViewport(full);device.SetBlendMode(E_BlendMode_Solid);device.Clear(E_Clear_Colour|E_Clear_Depth,Maths::RGBA(0,0,0,255),1,0);};
        auto verify=[&](auto expected){device.EndScene();auto* target=device.GetNativeColourSurface();auto data=target->LockRect(16);require(data.pBits,"Original Quad GPU readback failed");bool valid=true;for(U32 y=0;y<32;++y)for(U32 x=0;x<64;++x){const U32 colour=expected(x,y);for(U32 c=0;c<4;++c)valid&=std::abs(int(data.pBits[y*data.pitch+x*4+c])-int((colour>>(8*c))&255))<=1;}target->UnlockRect();require(valid,"Original Quad shader/UV/viewport/colour output differs from independent reference");device.Present();};
        begin();Quad::DrawFullScreenColour(Maths::C_RGBA_F32(.125f,.5f,.875f,1));verify([](U32,U32){return 0xff2080df;});
        begin();device.SetCullMode(E_CullMode_None);device.SetDepthEnable(E_Bool_False);Quad::DrawVertexColour(Maths::RGBA(17,101,219,255));verify([](U32,U32){return 0xff1165db;});
        TextureScope texture{device,{}};require(device.CreateTexture2D(2,2,1,0,E_Format_A8R8G8B8,&texture.value),"Quad texture allocation failed");const U32 quadrants[]={0xffff0000,0xff00ff00,0xff0000ff,0xffffffff};auto upload=texture.value.LockRect2D(0);require(upload.pBits,"Quad texture upload failed");for(U32 y=0;y<2;++y)for(U32 x=0;x<2;++x)std::memcpy(upload.pBits+y*upload.pitch+x*4,&quadrants[y*2+x],4);texture.value.UnlockRect2D(0);
        Quad::TextureFilter(E_TextureFilter_Point);
        for(U32 flip=0;flip<4;++flip){begin();Quad::SetQuadSettings(MathsStorage::UInt2(1,1),1,1,flip&1,flip&2);Quad::DrawFullScreen(&texture.value);verify([&](U32 x,U32 y){return quadrants[(((y>=16)^bool(flip&2))*2)+((x>=32)^bool(flip&1))];});}
        Quad::SetQuadSettings();
        begin();C_Viewport left=full;left.m_W=32;device.SetViewport(left);Quad::DrawFullScreenColour(Maths::C_RGBA_F32(1,0,0,1));C_Viewport right=left;right.m_X=32;device.SetViewport(right);Quad::DrawFullScreenColour(Maths::C_RGBA_F32(0,0,1,1));verify([](U32 x,U32){return x<32?0xffff0000:0xff0000ff;});
        begin();const F32 green[]={0,1,0,1,0,1,0,1,0,1,0,1};device.SetPixelShaderParameterF(PixelShader::m_PixelShaderColour_Colour,green);Quad::DrawPartScreen(-1,1,0,-1,nullptr,&PixelShader::m_PixelShaderColour);verify([](U32 x,U32){return x<32?0xff00ff00:0xff000000;});
    }
    device.DestroyVolatile();device.Destroy();Sys::SetDefaultFileSystem(nullptr);
    std::cout<<"Original default shader factory rollback/vertex declarations/Quad colours/textures/flips/split viewports/partial drawing PASS (not game startup)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
