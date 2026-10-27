// [Blur2PC][macOS] Original camera archive, fixed-tick render interpolation and actual GPU capture. No game assets.
#include <core/Maths.h>
#include <system/debuglog.h>
#include <display/device/Device.h>
#include <display/view/ViewSetup.h>
#include <camera2/Camera.h>
#include <camera2/tv/AnimData.h>
#include <camera2/stock/DebugFeed.h>
#include <camera2/capture/LockScreen.h>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
static void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
static bool near(F32 a,F32 b){return std::abs(a-b)<.0001f;}
int main(){try{
    Display::C_Device device;Display::C_DeviceParams params;params.m_Width=32;params.m_Height=16;params.NormalizeWindowDimensions();require(params.m_WindowBufferWidth==32&&params.m_WindowBufferHeight==16&&params.m_ScreenWidth==32&&params.m_ScreenHeight==16,"Native host dimension defaults failed");params.m_WindowBufferWidth=24;params.m_ScreenHeight=12;params.NormalizeWindowDimensions();require(params.m_WindowBufferWidth==24&&params.m_ScreenHeight==12,"Native defaults overwrote explicit render/screen dimensions");
    require(device.Create(params)&&device.CreateVolatile(params),"Actual camera capture GPU setup failed");
    Cam2::C_AnimData animation;animation.m_frame.Resize(2);for(U32 i=0;i<2;++i){auto& frame=animation.m_frame[i];frame.m_time=F32(i)*2;frame.m_position=Maths::HVec3(F32(i)*8,2,4);frame.m_orient=Maths::C_Quaternion(0,0,0,1);frame.SetAttr(Cam2::E_Attr_Fov,1+F32(i));}
    Maths::HVec3 position;Maths::C_Quaternion orientation;animation.EvaluatePosition(.5f,position,orientation);require(near(position.X(),2)&&near(position.Y(),2)&&near(animation.EvaluateAttribute(.5f,Cam2::E_Attr_Fov),1.25f)&&near(animation.CalcLength(),2),"Original camera animation interpolation failed");
    Cam2::C_Camera camera;camera.SetPosition(Maths::HVec3(0,2,4));camera.SetOrient(Maths::C_Quaternion(0,0,0,1));camera.SetFov(1);camera.SetShutterAngle(0);Cam2::I_Feed::Blur2PCAdvanceTick();camera.StartUpdate(1.f/60);camera.SetPosition(Maths::HVec3(8,2,4));camera.SetFov(2);camera.EndUpdate();
    require(near(camera.InterPosition(.25f).X(),2)&&near(camera.InterFov(.25f),1.25f),"Original camera previous/current tick pose changed");
    Display::C_ViewSetup3D view;for(U32 sample=0;sample<=8;++sample){F32 fraction=sample/8.f;Cam2::I_Feed::SetRenderOffset(fraction-1);camera.ToViewSetup(view,true);require(near(view.m_CameraToWorld.AW().X(),fraction*8)&&near(view.m_FieldOfView,1+fraction),"Render-only camera interpolation did not preserve the fixed simulation tick");}
    require(near(camera.Position().X(),8)&&near(camera.Fov(),2)&&near(camera.Period(),1.f/60),"Rendering mutated the camera simulation state");
    Cam2::I_Feed::SetRenderOffset(0);Cam2::C_DebugFeed feed(&camera);feed.Update(1.f/240);require(near(feed.Position().X(),8),"Original debug feed failed to expose its application camera");
    Cam2::C_LockScreenInfo screen;require(device.BeginScene(),"Camera capture scene failed");device.Clear(Display::E_Clear_Colour,Maths::RGBA(0x12,0x34,0x56,0xff));require(Cam2::LockScreen(screen)!=0&&!screen.bits,"Capture read unsubmitted GPU data");device.EndScene();
    require(Cam2::LockScreen(screen)==0&&screen.bits&&screen.width==32&&screen.height==16&&screen.format==0&&screen.pitch>=128,"Native camera framebuffer capture failed");
    Cam2::C_LockScreenInfo nested;require(Cam2::LockScreen(nested)!=0&&!nested.bits,"Nested native capture replaced an outstanding snapshot");device.DestroyVolatile();device.Destroy();
    for(S32 y=0;y<screen.height;++y)for(S32 x=0;x<screen.width;++x){U32 pixel;std::memcpy(&pixel,static_cast<const U8*>(screen.bits)+y*screen.pitch+x*4,4);require(pixel==0xff123456,"Actual capture bytes changed after GPU owner retirement");}Cam2::UnlockScreen();Cam2::UnlockScreen();require(Cam2::LockScreen(nested)!=0&&!nested.bits,"Capture succeeded without a created framebuffer");
    std::cout<<"Original camera archive/animation/fixed-tick render interpolation/debug feed/native GPU capture/lifetime PASS (not game startup)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
