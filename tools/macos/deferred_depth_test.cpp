// [Blur2PC] Actual PC deferred normal->lighting borrowed depth/stencil ownership.
#include <core/core.h>
#include <core/maths.h>
#include <display.h>
#include <display2/utils/Utils.h>
#include <deferred/Manager.h>
#include <deferred/prepass/Light.h>
#include <display/buffer/VertexDeclarations.h>
#include <system/serialise/C_FileSystem.h>
#include <filesystem>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
void Require(bool value,const char* why){if(!value){std::cerr<<why<<std::endl;throw std::runtime_error(why);}}
S32 Resources(){
#if TRACK_DISPLAY2_RESOURCES
    auto* d=Display2::Device();return d->GetSurfaceCount()+d->GetTextureCount()+d->GetRenderTexture2DCount()+d->GetVertexShaderCount()+d->GetPixelShaderCount()+d->GetVertexBufferCount()+d->GetIndexBufferCount();
#else
    return 0;
#endif
}
std::vector<U8> ProbeShader(bool vertex){
    const std::string metadata=R"({"parameters":{},"input_semantics":[],"reflection":{"ubos":[]}})";
    const std::string source=vertex?
        "#include <metal_stdlib>\nusing namespace metal;struct V{float4 p[[position]];};vertex V blur_main(uint i[[vertex_id]]){const float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};return {float4(p[i],.5,1)};}\n":
        "#include <metal_stdlib>\nusing namespace metal;fragment float4 blur_main(){return float4(0,1,0,1);}\n";
    const std::string entry="blur_main";std::vector<U8> bytes(40);std::memcpy(bytes.data(),"BMSL",4);
    auto word=[&](U32 at,U32 value){for(U32 i=0;i<4;++i)bytes[at+i]=U8(value>>(i*8));};
    word(4,1);word(8,vertex?0:1);word(16,40);word(20,metadata.size());word(24,40+metadata.size());word(28,source.size());word(32,40+metadata.size()+source.size());word(36,entry.size());
    bytes.insert(bytes.end(),metadata.begin(),metadata.end());bytes.insert(bytes.end(),source.begin(),source.end());bytes.insert(bytes.end(),entry.begin(),entry.end());word(12,bytes.size());return bytes;
}
struct Scene{Display::C_Device& d;explicit Scene(Display::C_Device& v):d(v){Require(d.BeginScene(),"Deferred scene could not begin");}~Scene(){d.EndScene();}};
struct Pass{Deferred::C_Manager& m;Pass(Deferred::C_Manager& v,S32 index):m(v){m.Begin(index);}~Pass(){m.End();}};
struct DeviceOwner{Display::C_Device& d;~DeviceOwner(){d.SetView(nullptr);d.SetColourDepthStencilSurface();d.DestroyVolatile();d.Destroy();Display2::SetNativeShaderResourceRoot("");Sys::SetDefaultFileSystem(nullptr);}};
struct Probe{
    Display::C_Device& d;Display2::C_VertexShader vs;Display2::C_PixelShader ps;Display2::C_VertexDeclaration decl;
    explicit Probe(Display::C_Device& device):d(device){try{auto v=ProbeShader(true),p=ProbeShader(false);std::string error;auto& g=d.GetDisplay2();Require(g.CreateVertexShader(v.size(),v.data(),0,&vs,nullptr,&error),error.c_str());Require(g.CreatePixelShader(p.size(),p.data(),0,&ps,nullptr,&error),error.c_str());Display2::C_VertexFormat format;format.AddElement(0,0,Display2::E_VertexElementType_F32_3,Display2::E_VertexElementUsage_Position,0);Require(g.CreateVertexDeclaration(format,&decl),"Deferred probe declaration failed");}catch(...){d.GetDisplay2().Destroy(&decl);d.GetDisplay2().Destroy(&ps);d.GetDisplay2().Destroy(&vs);throw;}}
    ~Probe(){auto& g=d.GetDisplay2();g.Destroy(&decl);g.Destroy(&ps);g.Destroy(&vs);}
    void Draw(bool stencil,U32 reference,bool depth=false){
        auto& c=d.GetDisplay2().GetContext();c.Clear(Display2::E_Clear_Colour,0xff000000);
        c.SetVertexShader(&vs);c.SetPixelShader(&ps);c.SetVertexDeclaration(&decl);c.SetColourWriteEnable(15);c.SetCullMode(Display2::E_CullMode_None);c.SetBlendEnable(Display2::E_Bool_False);
        c.SetDepthEnable(depth?Display2::E_Bool_True:Display2::E_Bool_False);c.SetDepthWriteEnable(Display2::E_Bool_False);c.SetDepthFunc(Display2::E_CmpFunc_Less);
        Display2::C_StencilState state;state.SetStencilFunc(Display2::E_CmpFunc_Equal);state.SetStencilRef(reference);state.SetStencilMask(255);state.SetStencilWriteMask(0);c.SetStencilState(state);c.SetStencilEnable(stencil?Display2::E_Bool_True:Display2::E_Bool_False);c.DrawPrimitive(Display2::E_PrimitiveType_TriangleList,0,3);
    }
};
U32 Green(const Display::C_Texture2D* t){auto image=t->LockRect2D(0,16);Require(image.pBits,"Deferred actual GPU readback failed");U32 green=0;bool valid=true;for(U32 y=0;y<t->GetHeight();++y)for(U32 x=0;x<t->GetWidth();++x){U32 p;std::memcpy(&p,image.pBits+y*image.pitch+x*4,4);green+=p==0xff00ff00;valid&=p==0xff00ff00||p==0xff000000;}t->UnlockRect2D(0);Require(valid,"Unexpected stencil/depth probe GPU colour");return green;}
Deferred::C_RenderTargetSetup Setup(S32 id,bool depth,Display::E_Multisample samples=Display::E_Multisample_None){Deferred::C_RenderTargetSetup s{};for(auto& f:s.eColourFormat)f=Display::E_Format_Unknown;s.eColourFormat[0]=Display::E_Format_A8R8G8B8;s.eColourFormat[1]=depth?Display::E_Format_R32F:Display::E_Format_Unknown;s.eDepthStencilFormat=depth?Display::E_Format_D24S8:Display::E_Format_Unknown;s.eMultisample=samples;s.iIndex=id;return s;}
void Run(Display::C_Device& device,Probe& probe,Display::C_View& view){
    const auto count=Resources();
    {
        Deferred::C_Manager manager;manager.AddRenderTargetSetup(Setup(0,true));manager.AddRenderTargetSetup(Setup(1,false));manager.AddRenderTargetSetup(Setup(2,false,Display::E_Multisample_2));
        manager.AddView(32,32,1,0);manager.AddView(32,32,-1,1);manager.AddView(16,16,2,0);manager.AddView(16,16,-2,1);manager.AddView(64,64,-3,1);manager.AddView(32,32,-4,2);
        Require(manager.Create()&&manager.CreateVolatile(),"Actual deferred required targets failed");
        Deferred::C_Light light;Require(light.Create()&&light.CreateVolatile(),"Actual large-light shaders failed");
        Display::C_Texture3D volume;Require(device.CreateTexture3D(2,2,2,1,0,Display::E_Format_A8R8G8B8,&volume),"Actual spotlight volume texture failed");
        struct VolumeOwner{Display::C_Device& d;Display::C_Texture3D& v;~VolumeOwner(){d.Destroy(&v);}} volumeOwner{device,volume};
        auto bytes=volume.LockBox3D(0);Require(bytes.pBits,"Actual spotlight volume map failed");for(U32 z=0;z<2;++z)for(U32 y=0;y<2;++y)std::memset(bytes.pBits+z*bytes.slicePitch+y*bytes.rowPitch,255,8);volume.UnlockBox3D(0);
        const auto* owned=manager.GetDepthStencilSurface(1);Require(owned&&manager.GetDepthStencilSurface(-1)==nullptr,"Lighting unexpectedly owns a fabricated depth surface");
        light.SetNormalTexture(manager.GetColourTexture(1,0));light.SetDepthTexture(manager.GetColourTexture(1,1));
        {
            Scene scene(device);device.DefaultStates();device.SetView(&view);
            {Pass normal(manager,1);device.Clear(Display::E_Clear_Colour|Display::E_Clear_Depth|Display::E_Clear_Stencil,Maths::RGBA(128,128,255,255),1,34);}
            {Pass lighting(manager,-1);
                Require(device.GetDisplay2().GetContext().GetDepthState().m_DepthEnable==Display2::E_Bool_False,"Missing-owned-depth pass changed original requested depth-disable policy");
                Display::C_Matrix matrix;Maths::HMat44 transform;transform.SetIdentity();transform.AW(Maths::HVec4(0,0,1,1));Maths::Store(transform,reinterpret_cast<MathsStorage::MatrixRow44*>(&matrix));
                light.RenderSpotlightVolume(matrix,2,.15f,.15f,Display::C_Float3(1,1,1),&volume);
                Require(device.GetRenderTarget().GetDepthStencilSurface()==owned,"Actual large light changed borrowed target identity");
                probe.Draw(true,255);
            }
            device.SetColourDepthStencilSurface();
        }
        const U32 coverage=Green(manager.GetColourTexture(-1,0));std::cout<<"Actual large-light stencil coverage="<<coverage<<"/1024\n";Require(coverage>0&&coverage<1024,"Actual large-volume stencil did not bound real GPU pixels");
        // Stencil passes must not overwrite the previously populated depth plane.
        {Scene scene(device);Pass lighting(manager,-1);device.SetDepthStencilSurface(owned);probe.Draw(false,0,true);}
        Require(Green(manager.GetColourTexture(-1,0))==1024,"Actual large-volume pass overwrote borrowed depth");
        {Scene scene(device);Pass lighting(manager,-1);device.Clear(Display::E_Clear_Depth,Maths::RGBA(0),.25f,0);probe.Draw(false,0,true);}
        Require(Green(manager.GetColourTexture(-1,0))==0,"Deferred GPU probe did not test the actual bound depth plane");
        // An explicitly detached target stays detached: Begin does not invent a
        // manager/default attachment. Existing strict missing-stencil rejection remains.
        {Scene scene(device);device.SetDepthStencilSurface(nullptr);Pass lighting(manager,-1);bool rejected=false;try{device.Clear(Display::E_Clear_Stencil);}catch(const std::logic_error&){rejected=true;}Require(rejected,"Explicitly detached stencil clear was accepted");}
        // A larger borrowed depth target is legal; smaller or mismatched samples
        // remain rejected by actual Metal attachment validation.
        {Scene scene(device);{Pass normal(manager,1);device.Clear(Display::E_Clear_Depth|Display::E_Clear_Stencil,Maths::RGBA(0),1,3);}{Pass small(manager,-2);Require(device.GetRenderTarget().GetDepthStencilSurface()==owned,"Smaller lighting target replaced borrowed depth");device.Clear(Display::E_Clear_Colour|Display::E_Clear_Stencil,Maths::RGBA(0),1,3);}}
        for(S32 invalid:{-3,-4}){Scene scene(device);device.SetDepthStencilSurface(owned);Pass lighting(manager,invalid);bool rejected=false;try{device.Clear(Display::E_Clear_Colour);}catch(const std::logic_error&){rejected=true;}Require(rejected,"Invalid borrowed depth extent/sample pairing bypassed Metal validation");}
        {Scene scene(device);device.SetDepthStencilSurface(owned);{Pass other(manager,2);Require(device.GetRenderTarget().GetDepthStencilSurface()==manager.GetDepthStencilSurface(2),"Owned normal pass failed explicit depth replacement");}{Pass lighting(manager,-2);Require(device.GetRenderTarget().GetDepthStencilSurface()==manager.GetDepthStencilSurface(2),"Lighting guessed a different view's depth instead of retaining the actual binding");device.Clear(Display::E_Clear_Stencil);}}
        device.SetColourDepthStencilSurface();light.DestroyVolatile();light.Destroy();manager.DestroyVolatile();manager.Destroy();Require(device.GetDisplay2().WaitIdle(),"Deferred GPU completion failed");
    }
    Require(Resources()==count,"Borrowed deferred target lifecycle leaked or acquired an extra GPU owner");
}
}
int main(int argc,char** argv){try{
    Require(argc==2,"Expected source-built shader resource root");const auto shaders=std::filesystem::absolute(argv[1]);auto& fs=Sys::C_FileSystem::Instance();Require(fs.SetWorkingDirectory("/"),"Fixture filesystem setup failed");Sys::SetDefaultFileSystem(&fs);Display2::SetNativeShaderResourceRoot(shaders.c_str());
    Display::C_Device device;Display::C_DeviceParams p;p.m_Width=p.m_Height=32;Require(device.Create(p)&&device.CreateVolatile(p),"Actual deferred device creation failed");
    DeviceOwner deviceOwner{device};
    Display::CreateVertexDeclarations();Display::C_View view;Display::C_ViewSetup setup;device.SetupView(&view,setup,32,32);
    {Probe probe(device);for(U32 cycle=0;cycle<2;++cycle)Run(device,probe,view);}
    std::cout<<"Original deferred normal/large-volume-light borrowed depth/stencil, real mask/depth GPU pixels, detach/rebind/bounds/samples and owner retry PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
