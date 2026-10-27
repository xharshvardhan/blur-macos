// [Blur2PC][macOS] Original camera values, debug snapshots and nested owned effects.
#include "../../code/mercury/library/graphics/camera2/stdafx.h"
#include <memory>
#include <iostream>
#include <stdexcept>
// Fixture-only inspection of actual value fields; no production test accessors/layout.
#define private public
#define protected public
#include <camera2/Camera.h>
#include <camera2/EffectInfo.h>
#include <camera2/stock/ShakeCamera.h>
#include <camera2/debug/DebugTools.h>
#include <camera2/network/NetworkLink.h>
#undef protected
#undef private
#include <System/Artery/Artery.h>

static void Require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
static void Populate(Cam2::C_Camera& camera,int n){
    camera.SetPosition(Maths::HVec3(F32(n),F32(n+1),F32(n+2)));
    camera.SetOrient(Maths::C_Quaternion());camera.SetShaken(Maths::C_Quaternion());
    camera.SetFov(.7f);camera.SetNearClip(.3f);camera.SetFarClip(321.f);
    camera.SetShutterAngle(.25f);camera.SetIdepth(.08f);camera.SetAperture(-1.f);
    camera.SetScreenOffset(.1f,-.2f);camera.SetObject(Cam2::C_CamObj(Maths::HVec3(4.f,5.f,6.f)));
    camera.StartUpdate(.0125f);camera.m_FovShake=.125f;camera.m_Blur2PCTick=91;
    camera.m_Blur2PCPrevPosition=Maths::HVec3(7.f,8.f,9.f);camera.m_Blur2PCPrevFov=.625f;
    camera.SetName("camera ownership fixture");
    Cam2::C_EffectInfo effect;effect.SetInfo(1.f,2.f,3.f,4.f,F32(n));camera.SetEffectInfo(effect);
}
static void VerifyValue(const Cam2::C_Camera& copy,const Cam2::C_Camera& original){
    Require(copy.Position().X()==original.Position().X()&&copy.m_PrevPosition.X()==original.m_PrevPosition.X(),"Camera pose/history changed in a value copy");
    Require(copy.Fov()==original.Fov()&&copy.NearClip()==original.NearClip()&&copy.FarClip()==original.FarClip(),"Camera projection/added shake was not copied");
    Require(copy.Period()==original.Period()&&copy.ShutterAngle()==original.ShutterAngle()&&copy.Aperture()==original.Aperture()&&copy.Idepth()==original.Idepth(),"Camera exposure/update value was not copied");
    Require(copy.m_Blur2PCTick==original.m_Blur2PCTick&&copy.m_Blur2PCPrevPosition.X()==original.m_Blur2PCPrevPosition.X()&&copy.m_Blur2PCPrevFov==original.m_Blur2PCPrevFov,"Camera interpolation state was not copied");
    Require(copy.Object().position.Z()==original.Object().position.Z()&&copy.m_ScreenOffsetX==original.m_ScreenOffsetX&&copy.m_ScreenOffsetY==original.m_ScreenOffsetY&&copy.Type()==original.Type(),"Base feed state was not copied");
    Require(copy.m_EffectInfo&&copy.m_EffectInfo!=original.m_EffectInfo&&copy.m_EffectInfo->FarScale()==original.m_EffectInfo->FarScale(),"Camera copied an effect owner by alias");
    Require(copy.m_pShakeWorld==original.m_pShakeWorld,"Borrowed shake-world identity changed");
}
int main(){try{
    for(int iteration=0;iteration<8;++iteration){
        Cam2::C_Camera snapshot;
        {
            auto source=std::make_unique<Cam2::C_Camera>();Populate(*source,iteration+1);
            source->SetShakeXYZ(2.f,3.f,4.f,5.f);source->m_pShakeCamera->m_shakePos=12345;
            Populate(*source->m_pShakeCamera,51);source->m_pShakeCamera->SetShake(.5f,2.f);
            Populate(*source->m_pShakeCamera->m_pShakeCamera,73);
            // A borrowed scene marker is never dereferenced or deleted by a snapshot.
            alignas(16) U8 borrowedWorld[16]{};
            source->m_pShakeWorld=reinterpret_cast<Cam2::C_ShakeWorld*>(borrowedWorld);
            Cam2::C_Camera constructed(*source);VerifyValue(constructed,*source);
            Populate(snapshot,99);snapshot.SetShake(.1f);snapshot=*source;VerifyValue(snapshot,*source);
            Require(snapshot.m_pShakeCamera!=source->m_pShakeCamera&&snapshot.m_pShakeCamera->m_shakePos==12345,"Shake phase/independent owner was not copied");
            VerifyValue(*snapshot.m_pShakeCamera,*source->m_pShakeCamera);
            Require(snapshot.m_pShakeCamera->m_pShakeCamera!=source->m_pShakeCamera->m_pShakeCamera,"Nested shake owner was aliased");
            VerifyValue(*snapshot.m_pShakeCamera->m_pShakeCamera,*source->m_pShakeCamera->m_pShakeCamera);
            auto* effect=snapshot.m_EffectInfo;auto* shake=snapshot.m_pShakeCamera;snapshot=snapshot;
            Require(snapshot.m_EffectInfo==effect&&snapshot.m_pShakeCamera==shake,"Self-assignment replaced valid camera owners");
            source->m_EffectInfo->SetInfo(9.f,9.f,9.f,9.f,999.f);
            Require(snapshot.m_EffectInfo->FarScale()==F32(iteration+1),"Changing source changed snapshot effect");
            source.reset(); // Showroom/source disappears before all debug snapshots.
            Require(snapshot.m_pShakeCamera->m_EffectInfo->FarScale()==51.f&&snapshot.m_pShakeCamera->m_pShakeCamera->m_EffectInfo->FarScale()==73.f,"Source retirement invalidated nested snapshot state");
        }
        Cam2::C_Camera empty;snapshot=empty;Require(!snapshot.m_EffectInfo&&!snapshot.m_pShakeCamera,"Assigning an empty value retained old snapshot owners");
    }
#ifndef CFG_GOLD
    for(int iteration=0;iteration<3;++iteration){
        Require(ArteryStartup(false,"CameraOwnershipFixture"),"Actual debug camera transport initialization failed");
        {
            Cam2::C_DebugTools tools;Require(tools.Init(),"Original debug tools initialization failed");
            {
                Cam2::C_Camera showroom;Populate(showroom,37);showroom.SetShake(.25f);
                tools.SetLocalCamera(showroom);VerifyValue(tools.LocalCamera(),showroom);
                const auto& transport=Cam2::C_NetworkLink::Instance().m_localCamera;
                VerifyValue(transport,tools.LocalCamera());
                Require(transport.m_pShakeCamera!=tools.LocalCamera().m_pShakeCamera,"Network snapshot retained debug snapshot shake owner");
            }
            Require(tools.LocalCamera().m_EffectInfo->FarScale()==37.f&&Cam2::C_NetworkLink::Instance().m_localCamera.m_EffectInfo->FarScale()==37.f,"Showroom retirement invalidated debug/network snapshots");
            tools.Exit();tools.Exit(); // Actual owner repeat cleanup; member cameras retire in destructor.
        }
        ArteryCleanup();
    }
#endif
    std::cout<<"Original camera nested owner/value and debug/network snapshot lifecycle PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
