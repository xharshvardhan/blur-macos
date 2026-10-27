// [Blur2PC][macOS] Actual UI handlers retire borrowed world pointers and owned scene light.
#include "../../code/amax/game/stdafx.h"
// Fixture-only access to existing borrower fields; no production test API/layout.
#define private public
#include <ui/Game/Gadgets/Hud/UIPerks2.h>
#include <ui/Game/Gadgets/UICarNode.h>
#include <ui/Game/Gadgets/Hud/UIHudDummy.h>
#undef private
#include <Game/messaging/GameflowMessages.h>
#include <ui/Game/Gadgets/ButtonController.h>
#include <ui/Game/Gadgets/ButtonMessages.h>
#define SCENEGRAPH_ALLOW_INCLUDE
#define RENDERINGDATA_ALLOW_INCLUDE
#include <scenegraph/SceneGraphImpl.h>
#include <scenegraph/light/Manager.h>
#include <misc_shared/archive/archive.h>
#include <renderingdata/target.h>
#include <renderingdata/streaming.h>
#include <fx/Fx.h>
#include <array>
#include <iostream>
#include <stdexcept>

static void require(bool value,const char* reason){if(!value){std::cerr<<reason<<std::endl;throw std::runtime_error(reason);}}
int main(){try{
    struct SchedulerLifetime{SchedulerLifetime(){Scheduler::Init();Scheduler::SetGroupPoolSize(32);Scheduler::jqEnableWorkers(Scheduler::JQ_CORE_1);Scheduler::Start();}~SchedulerLifetime(){Scheduler::Shutdown();}} scheduler;
    Display::C_Device device;Display::C_DeviceParams params;params.m_Width=params.m_Height=16;
    require(device.Create(params)&&device.CreateVolatile(params),"UI borrower scene device initialization failed");FX::Initialise();
    {
        UI::UIPerks2 perks;UI::UICarNode carNode;UI::UIHudDummy hud;
        for(UI::C_GadgetBase* gadget:{static_cast<UI::C_GadgetBase*>(&perks),static_cast<UI::C_GadgetBase*>(&carNode),static_cast<UI::C_GadgetBase*>(&hud)}){
            gadget->SetSubscriberSystem(&UI::gButtons.GetSubscriberSystem());
            gadget->AddSubscriber(UI::E_UiMsg_Render); // Original AddButton's baseline subscription; HudDummy removes it.
            gadget->Init();gadget->Activate(false,false); // Inactive gadgets still own/borrow world objects.
        }
        perks.m_AttachedToCar=true;
        UI::C_UiMessageBase<Game::E_GameflowMessage_ClearStoredActorPtrs> clear;
        alignas(16) std::array<U8,64> opaqueBorrowedMarkers{};
        // These markers are never dereferenced. Actual actor creation/physics is
        // outside this ownership fixture; the real light manager below is live.
        auto* vehicle=reinterpret_cast<Game::Actors::C_VehicleActor*>(opaqueBorrowedMarkers.data());
        auto* logic=reinterpret_cast<Game::Actors::C_VehicleLogicActor*>(opaqueBorrowedMarkers.data()+16);
        auto* pickup=reinterpret_cast<VehiclePickupInfo*>(opaqueBorrowedMarkers.data()+32);
        auto* physics=reinterpret_cast<RacingPhysics::Car*>(opaqueBorrowedMarkers.data()+48);
        auto seedBorrowers=[&]{
            perks.m_pVehicleActor=vehicle;perks.m_pLogicActor=logic;perks.m_pPickupInfo=pickup;perks.m_sLocalPlayerIndex=0;
            perks.m_Blur2PCBuilt=false;UI::UIPerks2::s_pBlur2PCCarGadget=&perks;
            carNode.m_pVehicleActor=vehicle;carNode.m_pLogicActor=logic;carNode.m_sLocalPlayerIndex=0;hud.m_pVehicle=physics;
        };
        auto verifyDetached=[&]{
            require(!perks.m_PointLightHandle.Object(),"Perk gadget retained its actual scene light");
            require(!perks.m_pVehicleActor&&!perks.m_pLogicActor&&!perks.m_pPickupInfo&&perks.m_sLocalPlayerIndex==-1,"Perk world borrowers/local-player binding were not reset");
            require(!UI::UIPerks2::s_pBlur2PCCarGadget,"Perk interpolation retained a detached car gadget");
            require(!carNode.m_pVehicleActor&&!carNode.m_pLogicActor&&carNode.m_sLocalPlayerIndex==-1&&!hud.m_pVehicle,"Other cached UI world borrowers were not reset");
        };
        for(int world=0;world<2;++world){
            SceneGraph::C_PointLightHandle retired;
            {
                SceneGraph::C_SceneGraph scene;scene.GetLightManager()->ReservePointLights(1);
                perks.m_PointLightHandle=SceneGraph::AddPointLight(&scene);require(perks.m_PointLightHandle.Object(),"Actual perk point-light acquisition failed");retired=perks.m_PointLightHandle;seedBorrowers();
                UI::gButtons.NativeBeginDeferredMessages();UI::gButtons.BroadcastMessage(&clear);
                require(perks.m_PointLightHandle.Object()&&carNode.m_pLogicActor&&hud.m_pVehicle,"Deferred clear unexpectedly retired borrowers synchronously");
                UI::gButtons.BroadcastMessageNow(0xffffffff,&clear);verifyDetached();
                require(!retired.Object(),"Original scene point-light owner did not invalidate its retired handle");
                UI::gButtons.BroadcastMessageNow(0xffffffff,&clear);perks.Exit();verifyDetached(); // Idempotent before owner destruction.
                require(UI::gButtons.NativeCloseDeferredMessages()==1,"Queued shutdown clear was not cancelled independently of synchronous retirement");
                // The real manager limit proves the pooled object was released,
                // without reaching into or force-clearing allocator bookkeeping.
                auto replacement=SceneGraph::AddPointLight(&scene);require(replacement.Object(),"Perk retirement did not return actual point-light capacity");
                require(!retired.Object(),"Retired perk handle revived after actual pool reuse");SceneGraph::RemovePointLight(&scene,replacement);
            } // Actual scene/light-manager destructor asserts every light has been paired.
            perks.Exit();UI::gButtons.BroadcastMessageNow(0xffffffff,&clear);verifyDetached(); // No global GameWorld exists or is queried.
        }
        UI::gButtons.NativeCloseDeferredMessages();
    } // Actual MessageHandler base destructors remove every gadget subscription.
    RenderingData::Target::StreamManager::Instance().Destroy();
    FX::Shutdown();device.DestroyVolatile();device.Destroy();
    std::cout<<"Original inactive UI gadgets + synchronous/deferred message contract + actual scene light release/reuse/repeat + world-borrower reset PASS (authored lifecycle; no actors/race)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
