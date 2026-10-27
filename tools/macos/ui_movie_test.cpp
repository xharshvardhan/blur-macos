// [Blur2PC] Real UI movie/Lua readiness, deferred completion and decoded GPU pixels.
#include "../../code/amax/game/stdafx.h"
#include <Ui/UiManager.h>
#include <ui/Graphics/Movie.h>
#include <ui/Game/Gadgets/ButtonMessages.h>
#include <system/serialise/C_FileSystem.h>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
namespace UI {extern C_UiInterface* g_pManager;int luaopen_system(lua_State*);}
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::vector<U8> read(const std::filesystem::path& path){std::ifstream in(path,std::ios::binary);require(bool(in),"Missing authored movie, shader or original Lua source");return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};}
void lua(lua_State* state,const std::string& code){if(luaL_loadbuffer(state,code.data(),code.size(),"original-movie-helper")||lua_pcall(state,0,0,0)){const std::string error=lua_tostring(state,-1);lua_pop(state,1);throw std::runtime_error(error);}}
class Finished final:public Messaging::C_MessageHandler {
public:
    U32 count=0;
    Finished(){SetSubscriberSystem(&UI::gButtons.GetSubscriberSystem());AddSubscriber(UI::E_UiMsg_MovieFinished);}
    bool MessageHandler(const Messaging::C_MessageBase* base)override{auto* message=static_cast<const UI::C_UiMessageData*>(base);require(message->m_NumArgs==1&&message->m_Args[0]==0,"Movie completion changed its original slot payload");++count;return true;}
};
U32 expected(U32 x,U32 y,U32 frame){return 0xff000000|(((2*x+16*frame)%256)<<16)|(((4*y+8*frame)%256)<<8)|((2*(x+y)+12*frame)%256);}
}
int main(int argc,char** argv){try{
    require(argc==3,"Expected authored movie directory and shader root");const std::filesystem::path root=argv[1],shaders=argv[2];
    // Optional audio-owner stress remains explicit: CoreAudio can dispose its
    // system C++ objects asynchronously after SDL closes the playback device.
    const bool audio=std::getenv("BLUR_MAC_UI_MOVIE_AUDIO")!=nullptr;
    const char* clip=audio?"authored.nut":"video-only.nut";
    auto& fs=Sys::C_FileSystem::Instance();require(fs.SetWorkingDirectory(root.c_str()),"UI movie filesystem setup failed");Sys::SetDefaultFileSystem(&fs);
    Display::C_Device device;Display::C_DeviceParams params;params.m_Width=34;params.m_Height=18;require(device.Create(params)&&device.CreateVolatile(params),"UI movie display setup failed");
    struct UiOwners {
        lua_State* state=nullptr;
        ~UiOwners(){for(auto& movie:UI::gMovie)movie.Destroy();UI::gButtons.NativeCloseDeferredMessages();UI::g_pUILua=nullptr;UI::g_pManager=nullptr;if(state)lua_close(state);}
    } uiOwners;
    UI::C_UiInterface interface;UI::g_pManager=&interface;UI::C_UILua owner;UI::g_pUILua=&owner;owner.mDoingFullInit=true;
    lua_State* state=luaL_newstate();require(state,"UI movie Lua state failed");uiOwners.state=state;luaopen_base(state);lua_settop(state,0);UI::luaopen_system(state);lua_pop(state,1);
    UI::gButtons.NativeBeginDeferredMessages();Finished finished;
    auto& gpu=device.GetDisplay2();
    struct GpuOwners {
        Display2::C_Device& device;Display2::C_VertexShader vs;Display2::C_PixelShader ps;Display2::C_VertexDeclaration declaration;Display2::C_VertexBuffer vertices;
        explicit GpuOwners(Display2::C_Device& owner):device(owner){}
        ~GpuOwners(){device.Destroy(&vertices);device.Destroy(&declaration);device.Destroy(&ps);device.Destroy(&vs);}
    } gpuOwners(gpu);
    auto& vs=gpuOwners.vs;auto& ps=gpuOwners.ps;auto& declaration=gpuOwners.declaration;auto& vertices=gpuOwners.vertices;std::string error;
    const auto v=read(shaders/"display/vsDefault_XY.bmsl"),p=read(shaders/"display/psDefaultTexture.bmsl");
    require(gpu.CreateVertexShader(v.size(),v.data(),0,&vs,nullptr,&error)&&gpu.CreatePixelShader(p.size(),p.data(),0,&ps,nullptr,&error),error.c_str());
    Display2::C_VertexFormat format;format.AddElement(0,0,Display2::E_VertexElementType_F32_2,Display2::E_VertexElementUsage_Position,0);format.AddElement(0,8,Display2::E_VertexElementType_F32_2,Display2::E_VertexElementUsage_TexCoord,0);
    require(gpu.CreateVertexDeclaration(format,&declaration)&&gpu.CreateVertexBuffer(64,0,&vertices),"UI movie sampling geometry failed");
    const F32 quad[]={-1,1,0,0,1,1,1,0,-1,-1,0,1,1,-1,1,1};void* memory=vertices.Lock();require(memory,"UI movie vertex map failed");std::memcpy(memory,quad,64);vertices.Unlock();
    auto sample=[&](U32 frame){
        const auto* texture=UI::gMovie[0].GetTexture();require(texture&&texture->IsCreated(),"UI movie exposed no real decoded texture");
        require(device.BeginScene(),"UI movie sampling scene failed");auto& context=gpu.GetContext();context.SetVertexShader(&vs);context.SetPixelShader(&ps);context.SetVertexBuffer(0,&vertices,0,16,&declaration);context.SetTexture(0,texture);context.SetDepthEnable(Display2::E_Bool_False);context.SetStencilEnable(Display2::E_Bool_False);context.SetCullMode(Display2::E_CullMode_None);context.SetColourWriteEnable(15);context.SetBlendEnable(Display2::E_Bool_False);
        Display2::C_SamplerState sampler;sampler.SetTextureAddress(Display2::E_TextureAddress_Clamp);context.SetSamplerState(0,sampler);context.DrawPrimitive(Display2::E_PrimitiveType_TriangleStrip,0,4);device.EndScene();
        auto pixels=device.GetNativeColourSurface()->LockRect(16);require(pixels.pBits,"UI movie GPU readback failed");bool valid=true;for(U32 y=0;y<18;++y)for(U32 x=0;x<34;++x){U32 colour;std::memcpy(&colour,pixels.pBits+y*pixels.pitch+x*4,4);valid&=colour==expected(x,y,frame);}device.GetNativeColourSurface()->UnlockRect();require(valid,"Actual UI movie texture differed from authored pixels");
    };
    // Observe descriptors emitted by the actual source helper; this collector is
    // not a replacement gadget renderer. GPU checks above use the actual player.
    lua(state,R"(SCREEN_WIDTH=640;SCREEN_LEFT=0;UIEnums={Layer={LAYER_1=1},ButtonTypes={GRAPHIC=1},Justify={MiddleCentre=1},TextureType={MOVIE0=1},BlendMode={Solid=1},EffectIndex={Movie=42},Platform={PC=1},CurrentPlatform=1};UITexture={Textures={null=0}};Amax={GetMovieVolume=function() return 0 end};UIButtons={AddButton=function(t) if t.effect_index==42 then movie_descriptors=movie_descriptors+1 end end};movie_descriptors=0)");
    const auto helperPath=std::filesystem::path(__FILE__).parent_path()/"../../code/amax/library/Ui/game/lua_scripts/screens/GUIBank.lua";const auto textBytes=read(helperPath);const std::string text(textBytes.begin(),textBytes.end());const auto begin=text.find("function SetupMovie("),end=text.find("\nfunction LocalGUI(",begin);require(begin!=std::string::npos&&end!=std::string::npos,"Original SetupMovie helper bounds unavailable");lua(state,text.substr(begin,end-begin));
    for(U32 retry=0;retry<3;++retry){
        const U32 before=finished.count;
        lua(state,std::string("assert(SetupMovie(0,'")+(retry==1?"invalid.nut":"missing.nut")+"',false)==false);assert(movie_descriptors==0)");
        require(!UI::gMovie[0].IsCreated()&&UI::gMovie[0].IsPaused()&&!UI::gMovie[0].MoviePlayer()&&!UI::gMovie[0].GetTexture(),"Failed UI movie published active/empty resources");
        UI::gMovie[0].Pause(false);UI::gMovie[0].Update();UI::gButtons.ProcessDeferredMessages();UI::gMovie[0].Update();UI::gButtons.ProcessDeferredMessages();require(finished.count==before+1,"Failed UI movie did not complete exactly once");
        lua(state,"assert(UISystem.InitMovie(-1,'missing.nut',false)==false);assert(UISystem.InitMovie(0.5,'missing.nut',false)==false);assert(UISystem.InitMovie(1/0,'missing.nut',false)==false)");
        lua(state,std::string("SetupMovie(0,'")+clip+"',false);assert(movie_descriptors==1);UISystem.EnableMovieLooping(0,false);UISystem.PauseMovie(0,true)");
        require(UI::gMovie[0].IsCreated()&&UI::gMovie[0].IsPaused(),"Real UI movie retry was not ready/paused");sample(0);
        const auto* livePlayer=UI::gMovie[0].MoviePlayer();const auto* liveTexture=UI::gMovie[0].GetTexture();lua(state,"assert(UISystem.InitMovie(0,'invalid.nut',false)==false)");require(UI::gMovie[0].MoviePlayer()==livePlayer&&UI::gMovie[0].GetTexture()==liveTexture,"Duplicate Lua movie request replaced a live owner");
        const auto pausedTime=livePlayer->GetCurrentTime();std::this_thread::sleep_for(std::chrono::milliseconds(150));UI::gMovie[0].Update();require(livePlayer->GetCurrentTime()==pausedTime,"UI pause advanced native media clock");
        lua(state,"UISystem.PauseMovie(0,false)");UI::gMovie[0].Update();require(livePlayer->GetCurrentTime()<pausedTime+.1f,"UI resume included paused wall time");const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(!UI::gMovie[0].IsPaused()&&std::chrono::steady_clock::now()<deadline){UI::gMovie[0].Update();UI::gButtons.ProcessDeferredMessages();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
        require(UI::gMovie[0].IsPaused()&&finished.count==before+2,"Successful UI movie did not finish exactly once");sample(audio?7:1);UI::gMovie[0].Update();UI::gButtons.ProcessDeferredMessages();require(finished.count==before+2,"Finished UI movie repeated completion");
        lua(state,"UISystem.DestroyMovie(0);UISystem.DestroyMovie(0);movie_descriptors=0");require(!UI::gMovie[0].GetTexture()&&!UI::gMovie[0].MoviePlayer(),"UI movie teardown retained its owner");
    }
    owner.mDoingFullInit=false;lua(state,"SetupMovie(0,'missing.nut',false);assert(movie_descriptors==1)");require(!UI::gMovie[0].MoviePlayer(),"Lua preview created playback");
    UI::gButtons.NativeCloseDeferredMessages();UI::g_pUILua=nullptr;UI::g_pManager=nullptr;lua_close(state);uiOwners.state=nullptr;gpu.Destroy(&vertices);gpu.Destroy(&declaration);gpu.Destroy(&ps);gpu.Destroy(&vs);require(gpu.WaitIdle(),"UI movie GPU completion failed");
#if TRACK_DISPLAY2_RESOURCES
    require(gpu.GetRenderTexture2DCount()==0,"UI movie retained GPU output owners");
#endif
    Sys::SetDefaultFileSystem(nullptr);device.DestroyVolatile();device.Destroy();std::cout<<"Actual UI movie + native Lua InitMovie/source SetupMovie: missing/corrupt-open rejection, one completion, descriptor gate, real decoded GPU pixels, pause/end/retry/retirement and preview PASS ("<<(audio?"audio stress":"video-only")<<"; descriptor collector is not full gadget rendering)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
