// [Blur2PC][macOS] Original photo strip accumulation on authored framebuffer pixels.
#include <core/core.h>
#include <core/maths.h>
#include <display.h>
#include <display/buffer/VertexDeclarations.h>
#include <render/photo/AccumulateStrip.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
static std::vector<U8> read(const std::filesystem::path& path){std::ifstream input(path,std::ios::binary);require(bool(input),"Missing authored photo dependency shader");return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};}
// Keep expected lower ownership failures observable through complete real-device teardown.
struct PhotoDeviceCleanup {
    Display::C_Device& device;
    ~PhotoDeviceCleanup(){if(device.InScene())device.EndScene();device.Destroy(&Display::PixelShader::m_PixelShaderTexture);device.Destroy(&Display::VertexShader_XY::m_VertexShader);device.DestroyVolatile();device.Destroy();}
};
static U32 pixel(U32 x,U32 y,U32 frame){return 0xff000000|((2*x+frame*128)<<16)|((2*y+frame*128)<<8)|(2*((x+y)%64)+frame*128);}
int main(int argc,char** argv){try{
    require(argc==2,"Expected authored shader directory");const std::filesystem::path root=argv[1];
    Display::C_Device device;Display::C_DeviceParams params;params.m_Width=34;params.m_Height=18;
    require(device.Create(params)&&device.CreateVolatile(params),"Native photo device creation failed");PhotoDeviceCleanup deviceCleanup{device};Display::CreateVertexDeclarations();
    auto vs=read(root/"display/vsDefault_XY.bmsl"),ps=read(root/"display/psDefaultTexture.bmsl");std::string error;
    require(device.GetDisplay2().CreateVertexShader(vs.size(),vs.data(),0,&Display::VertexShader_XY::m_VertexShader,nullptr,&error),error.c_str());
    require(device.GetDisplay2().CreatePixelShader(ps.size(),ps.data(),0,&Display::PixelShader::m_PixelShaderTexture,nullptr,&error),error.c_str());
    auto upload=[&](U32 frame){auto* source=device.GetNativeColourSurface();auto lock=source->LockRect();require(lock.pBits,"Photo framebuffer upload failed");for(U32 y=0;y<18;++y)for(U32 x=0;x<34;++x){const U32 value=pixel(x,y,frame);std::memcpy(lock.pBits+y*lock.pitch+x*4,&value,4);}source->UnlockRect();};
    auto setup=[&]{device.SetCullMode(Display::E_CullMode_None);device.SetBlendMode(Display::E_BlendMode_Solid);device.GetDisplay2().GetContext().SetScissorEnable(Display2::E_Bool_False);device.SetAlphaTestEnable(Display::E_Bool_False);device.SetColourWriteEnable(Display::E_ColourWriteEnable_All);};
    {
        Render::Photo::C_AccumulateStrip photo;U8* bits=reinterpret_cast<U8*>(1);S32 pitch=-1,width=-1,height=-1,format=-1;
        require(!photo.LockBitmap(bits,pitch,width,height,format)&&!bits&&!pitch&&!width&&!height&&!format,"Absent photo returned stale map outputs");
        require(photo.Create(),"Original photo shader creation failed");require(photo.CreateVolatile()&&!device.InScene(),"Original photo creation outside a scene did not retire its initial-clear command");require(photo.Width()==34&&photo.Height()==18,"Photo ignored arbitrary framebuffer dimensions");
        auto verify=[&](U32 mode){require(photo.LockBitmap(bits,pitch,width,height,format)&&bits&&pitch>=34*4&&width==34&&height==18&&format==21,"Photo native RW map metadata differs from PC BGRA8");
            bool valid=true;for(U32 y=0;y<18;++y)for(U32 x=0;x<34;++x){U32 actual=0;std::memcpy(&actual,bits+y*pitch+x*4,4);U32 expected=pixel(x,y,0);if(mode==1)expected=0xff000000|((expected&0xfefefe)>>1);if(mode==2)expected=0xff000000|(((pixel(x,y,0)&0xfefefe)>>1)+((pixel(x,y,1)&0xfefefe)>>1));if(actual!=expected){if(valid)std::cerr<<"photo "<<mode<<" x="<<x<<" y="<<y<<" actual="<<std::hex<<actual<<" expected="<<expected<<std::dec<<'\n';valid=false;}}
            photo.UnlockBitmap();require(valid,"Original photo strip GPU pixels differ from independent CPU accumulation");};
        // Actual OnAppRender -> RenderStart runs before RenderAddSample begins its scene.
        photo.Start(1);require(!device.InScene(),"Photo start retained its owned clear scene");
        upload(0);require(device.BeginScene(),"Single photo sample scene failed");setup();photo.AddSample();device.EndScene();verify(0);
        // Real CPU/GPU ownership rejection must close only the scene owned by the clear boundary.
        require(photo.LockBitmap(bits,pitch,width,height,format),"Photo clear-failure map failed");
        bool failed=false;try{photo.Start(1);}catch(const std::logic_error& error){failed=std::string(error.what()).find("owned by another CPU/GPU operation")!=std::string::npos;}
        require(failed&&!device.InScene(),"Failed outside-scene photo clear lost its owned scene or map rejection");photo.UnlockBitmap();
        photo.Start(1);require(!device.InScene(),"Photo clear retry retained an owned scene");upload(0);require(device.BeginScene(),"Photo outside-start retry scene failed");setup();photo.AddSample();device.EndScene();verify(0);
        require(photo.LockBitmap(bits,pitch,width,height,format)&&device.BeginScene(),"Borrowed photo clear-failure setup failed");
        failed=false;try{photo.Start(1);}catch(const std::logic_error& error){failed=std::string(error.what()).find("owned by another CPU/GPU operation")!=std::string::npos;}
        require(failed&&device.InScene(),"Failed photo clear closed the caller's borrowed scene");device.EndScene();photo.UnlockBitmap();
        // Successful borrowed starts remain legal and retain the caller's scene.
        upload(0);require(device.BeginScene(),"Borrowed photo start scene failed");setup();photo.Start(1);require(device.InScene(),"Photo start closed the caller's scene");photo.AddSample();device.EndScene();verify(0);
        require(photo.LockBitmap(bits,pitch,width,height,format),"Photo RW map failed");const U32 edited=0xff123456;std::memcpy(bits,&edited,4);U8* second=reinterpret_cast<U8*>(1);S32 p=1,w=1,h=1,f=1;
        require(!photo.LockBitmap(second,p,w,h,f)&&!second&&!p&&!w&&!h&&!f,"Photo permitted a second map ownership claim");photo.UnlockBitmap();photo.UnlockBitmap();require(photo.LockBitmap(bits,pitch,width,height,format),"Edited photo remap failed");U32 actual=0;std::memcpy(&actual,bits,4);require(actual==edited,"Native photo RW edits were not uploaded to GPU storage");photo.UnlockBitmap();
        photo.Start(2);require(!device.InScene(),"Two-sample photo start retained an owned scene");upload(0);require(device.BeginScene(),"Two-sample photo scene failed");setup();photo.AddSample();device.EndScene();verify(1);
        upload(1);require(device.BeginScene(),"Second photo sample scene failed");setup();photo.AddSample();device.EndScene();verify(2);
        require(photo.LockBitmap(bits,pitch,width,height,format),"Mapped photo teardown fixture failed");photo.DestroyVolatile();require(!photo.GetTexture()&&!photo.StripTexture()&&!photo.Width()&&!photo.Height(),"Photo teardown retained mapped texture owners");photo.UnlockBitmap();
        require(device.BeginScene(),"Photo recreation scene failed");require(photo.CreateVolatile(),"Photo volatile recreation failed");setup();photo.Start(0);photo.AddSample();device.EndScene();
        require(photo.LockBitmap(bits,pitch,width,height,format),"Recreated photo map failed");for(U32 y=0;y<18;++y)for(U32 x=0;x<34;++x){U32 value=0;std::memcpy(&value,bits+y*pitch+x*4,4);require(value==0xff000000,"Zero-sample photo changed cleared pixels");} // Destructor retires this map and all owned wrappers.
    }
    device.Destroy(&Display::PixelShader::m_PixelShaderTexture);device.Destroy(&Display::VertexShader_XY::m_VertexShader);
    require(device.GetDisplay2().WaitIdle(),"Photo GPU command failed");
#if TRACK_DISPLAY2_RESOURCES
    require(device.GetDisplay2().GetRenderTexture2DCount()==0,"Photo renderer leaked tracked render textures");
#endif
    std::cout<<"Original native photo: PC screen-strip GPU copies, 18-row partial strips, exact one/two-sample pixels, RW edits, map ownership and teardown/recreation passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
