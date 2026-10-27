// [Blur2PC][macOS] Real audio subsystem owners, borrowed registration and loader retirement; no banks/game/output.
#include "../../code/amax/game/GameLogic/stdafx.h"
#include <GameAudio/GameAudioLayer.h>
#include <GameAudio/MusicSubsystem.h>
#include <GameAudio/UISubsystem.h>
#include <GameAudio/SystemSubsystem.h>
#include <GameAudio/DialogueSubsystem.h>
#include <GameLogic/ActorStudioActors/DynamicObjectMessages.h>
#include <GameLogic/GameData/AmaxDialogue.h>
#include <Game/Messaging/GameflowMessages.h>
#include <ui/SoundEffects.h>
#include "../../code/amax/library/ui/UiMessages.h"
#include <misc_shared/Messaging/MessageSubscriberSystem.h>
#include <atomic>
#include <iostream>
#include <memory>
#include <stdexcept>

extern MT::Thread* g_pMusicSubSystemAsyncLoadThread;
extern MT::Thread* g_pUISubSystemAsyncLoadThread;
namespace {
void Require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
struct Counts{int destroyed=0;bool validSlot=true;};
class Owned final:public Audio::Subsystem {
public:
    Owned(Counts& counts,U32 id):m_Counts(counts),m_Id(id){}
    ~Owned(){++m_Counts.destroyed;m_Counts.validSlot&=Audio::InterfaceController::Get().GetSubsystem(m_Id)==this;}
private:Counts& m_Counts;U32 m_Id;
};
class Borrowed final:public Audio::Subsystem {
public:explicit Borrowed(int& count):m_Count(count){}~Borrowed(){++m_Count;}
private:int& m_Count;
};
// [Blur2PC] These are real entity instances in the original subsystem pool.
// No definition scripts/connectors are fabricated; TrackValue itself needs only
// a live entity owner and allocates the actual poll record/refcount under test.
class PollSubsystem final:public Audio::Subsystem {
public:
    explicit PollSubsystem(bool& balanced):m_Balanced(balanced){}
    ~PollSubsystem(){for(U32 i=0;i<m_aEntityInstance.GetHighestNodeIndex();++i)if(auto* entity=m_aEntityInstance.GetByIndex(i))m_Balanced&=entity->GetDataTrackerRefCount()==0;}
    AudioEntityHandle Acquire(){auto* entity=new Audio::EntityInstance;Require(entity->Init(0,0),"Actual empty entity allocation failed");auto handle=m_aEntityInstance.AddEntry(entity);AudioEntityHandle result(handle,U16(m_u32SubsystemId));entity->SetAudioEntityHandle(result);return result;}
    Audio::EntityInstance* Find(AudioEntityHandle handle){return GetEntityInstance(handle.ArrayHandle());}
    void DropLikeBaseDestructor(AudioEntityHandle handle){delete GetEntityInstance(handle.ArrayHandle());m_aEntityInstance.Free(handle.ArrayHandle());}
protected:
    bool ReleaseEntityInstance(Audio::EntityInstanceHandle handle)override{auto* entity=GetEntityInstance(handle);m_Balanced&=entity&&entity->GetDataTrackerRefCount()==0;delete entity;return m_aEntityInstance.Free(handle);}
private:bool& m_Balanced;
};
class PendingCallback final:public Audio::EntityCallbackBase {
public:PendingCallback(int& destroyed,int& fired):m_Destroyed(destroyed),m_Fired(fired){}~PendingCallback(){++m_Destroyed;}void Callback()override{++m_Fired;}
private:int& m_Destroyed;int& m_Fired;
};
void PollLifetimes(){
    auto& controller=Audio::InterfaceController::Get();
    std::vector<Audio::ReturnDataHandle> old;
    for(U32 cycle=0;cycle<3;++cycle){
        bool ownedBalanced=true,borrowedBalanced=true;
        PollSubsystem borrowed(borrowedBalanced);
        Require(controller.Init(2,nullptr,nullptr)&&controller.AddSubsystem(&borrowed,0),"Poll borrowed owner registration failed");
        auto* owned=new PollSubsystem(ownedBalanced);Require(controller.AddOwnedSubsystem(owned,1),"Poll explicit owner registration failed");
        auto ownedEntity=owned->Acquire(),borrowedEntity=borrowed.Acquire();
        std::vector<Audio::ReturnDataHandle> fresh;
        for(U32 i=0;i<40;++i)fresh.push_back(controller.TrackValue(ownedEntity,i));
        auto borrowedPoll=controller.TrackValue(borrowedEntity,0);
        Require(owned->Find(ownedEntity)->GetDataTrackerRefCount()==40&&borrowed.Find(borrowedEntity)->GetDataTrackerRefCount()==1,"Actual TrackValue did not acquire exact entity refs");
        F32 value=99.f;for(auto handle:old)Require(!controller.GetValue(value,handle),"Old >32 poll handle aliased a new controller lifetime");
        auto manual=controller.TrackValue(ownedEntity,41);Require(controller.UntrackValue(manual)&&!manual.IsValid()&&!controller.UntrackValue(manual),"Actual Untrack did not retire/invalidate exactly once");
        Require(controller.ReleaseOwnedSubsystem(1)&&ownedBalanced,"Owned entity died before actual tracker/ref retirement");
        Require(controller.m_DataTracker.GetSize()==1&&borrowed.Find(borrowedEntity)->GetDataTrackerRefCount()==1&&controller.GetValue(value,borrowedPoll),"Owned poll retirement removed unrelated borrowed observation");
        for(auto handle:fresh)Require(!controller.GetValue(value,handle),"Retired owned poll handle remained readable");
        // Genuine acquired poll whose entity disappears through the original raw base-destruction pattern.
        auto orphanEntity=borrowed.Acquire();auto orphan=controller.TrackValue(orphanEntity,0);borrowed.DropLikeBaseDestructor(orphanEntity);
        Require(controller.UntrackValue(orphan)&&!orphan.IsValid(),"Untrack leaked its owned record after target entity vanished");
        // Normal controller ReleaseEntity still performs its original observation retirement.
        auto normalEntity=borrowed.Acquire();auto normal=controller.TrackValue(normalEntity,0);Require(controller.ReleaseEntity(normalEntity)&&!controller.GetValue(value,normal),"Normal entity release no longer retired its poll");
        // Authored completion record at the real controller boundary. Actual action-out
        // producer registration is separate; no System callback producer is invented.
        int callbacksDestroyed=0,callbacksFired=0;
        auto* pending=new PendingCallback(callbacksDestroyed,callbacksFired);pending->m_hEntity=ownedEntity;
        auto callbackHandle=controller.m_CallbackDataList.AddEntry(pending);
        controller.Release();controller.Release();
        Require(controller.m_DataTracker.GetSize()==0&&controller.m_CallbackDataList.GetSize()==0&&callbacksDestroyed==1&&!callbacksFired,"Final controller ownership did not retire real records without dispatch");
        Require(borrowed.Find(borrowedEntity)->GetDataTrackerRefCount()==0&&borrowedBalanced,"Final release lost borrowed entity ref balancing");
        controller.ProcessCallback(callbackHandle.GetValue());Require(!callbacksFired,"Late callback dispatched after final retirement");
        Require(!controller.GetValue(value,borrowedPoll),"Final table retained a borrowed observation handle");
        old=fresh;
    }
}

class Loader final:public MT::Thread {
public:Loader(std::atomic<int>& done,std::atomic<int>& destroyed):m_Done(done),m_Destroyed(destroyed){}
    ~Loader(){++m_Destroyed;}
private:void ThreadMain()override{MT::Thread::Sleep(2);++m_Done;}
    std::atomic<int>& m_Done;std::atomic<int>& m_Destroyed;
};
}
int main(){try{
    auto& controller=Audio::InterfaceController::Get();controller.Release();
    for(U32 lifetime=0;lifetime<2;++lifetime){
        Counts counts;int borrowedDestroyed=0;
        {
            Borrowed borrowed(borrowedDestroyed);
            Require(controller.Init(4,nullptr,nullptr),"No-resource controller initialization failed");
            Require(controller.AddSubsystem(&borrowed,0),"Borrowed registration failed");
            Require(controller.AddOwnedSubsystem(new Owned(counts,1),1),"Explicit owned registration failed");
            {
                std::unique_ptr<Audio::Subsystem> rejected(new Owned(counts,0));
                Require(!controller.AddOwnedSubsystem(rejected.get(),0),"Duplicate registration replaced a borrowed slot");
            }
            // Rejected acquisitions remain caller-owned; their slot is intentionally occupied by another object.
            Require(counts.destroyed==1,"Rejected acquisition did not remain caller-owned");counts.validSlot=true;
            Require(!controller.AddOwnedSubsystem(nullptr,2),"Null ownership acquisition was accepted");
            Require(controller.ReleaseOwnedSubsystem(1)&&!controller.ReleaseOwnedSubsystem(1),"Owned retirement was not idempotent");
            Require(counts.destroyed==2&&counts.validSlot,"Owned destructor lost its own live lookup slot");
            Require(!controller.ReleaseOwnedSubsystem(0)&&!borrowedDestroyed,"Borrowed registration was deleted as owned");
            Require(controller.AddOwnedSubsystem(new Owned(counts,2),2),"Partial owner prefix registration failed");
            controller.Release();controller.Release();
            Require(counts.destroyed==3&&counts.validSlot,"Release leaked or double-deleted a partial acquired owner prefix");
            Require(!borrowedDestroyed&&!controller.GetSubsystem(0)&&!controller.GetAudioSystem()&&!controller.GetWaveManager(),
                    "Release retained a table/dependency pointer or destroyed a borrowed subsystem");
            controller.Update(); // A safely released zero-count table is inert.
        }
        Require(borrowedDestroyed==1,"Borrowed owner did not retain its one ordinary destructor");
    }

    PollLifetimes();

    Scheduler::Init();Scheduler::SetGroupPoolSize(64);
    for(U32 lifetime=0;lifetime<2;++lifetime){
        Require(controller.Init(Game::C_GameAudioLayer::TotalNumSubsystems,nullptr,nullptr),"No-resource controller retry failed");
        Messaging::C_MessageSubscriberSystem subscribers;
        auto music=std::make_unique<MusicSubsystem>();
        auto ui=std::make_unique<UISubsystem>();
        auto system=std::make_unique<SystemSubsystem>();
        auto dialogue=std::make_unique<DialogueSubsystem>();
        // Source-derived registration only; no eInit/entity load/game singleton is invented.
        system->SetSubscriberSystem(&subscribers);
        system->AddSubscriber(ActorMsg::E_DynamicObjMsg_VehicleAudioSloMoEntry);
        system->AddSubscriber(ActorMsg::E_DynamicObjMsg_VehicleAudioSloMoExit);
        system->AddSubscriber(Game::E_VehicleMessage_AudioWaterSplash);
        dialogue->SetSubscriberSystem(&subscribers);
        dialogue->AddSubscriber(E_AMAXDIALOGUE_ADDWAVEBANK);dialogue->AddSubscriber(E_AMAXDIALOGUE_PLAYAUDIO);
        ui->SetSubscriberSystem(&subscribers);
        const U32 uiMessages[]={GameMessaging::E_GameMsg_Update,UI::E_UiMsg_PlaySoundEffect,
            Game::E_GameLogicMessage_Failed_Rule,Game::E_GameLogicMessage_Passed_All_Rule,
            Game::E_GameflowMessage_IntroStarted,Game::E_GameflowMessage_IntroFinished,
            Game::E_GameflowMessage_RaceFinished,Game::E_GameflowMessage_VehicleFinished,
            ActorMsg::E_AudioMsg_PlayUIDialogue,ActorMsg::E_AudioMsg_StopUIDialogue};
        for(U32 message:uiMessages)ui->AddSubscriber(message);
        Require(controller.AddOwnedSubsystem(music.get(),Game::C_GameAudioLayer::eMusicSubsystem),"Music owner registration failed");music.release();
        Require(controller.AddOwnedSubsystem(ui.get(),Game::C_GameAudioLayer::eUISubsystem),"UI owner registration failed");ui.release();
        Require(controller.AddOwnedSubsystem(dialogue.get(),Game::C_GameAudioLayer::eDialogueSubsystem),"Dialogue owner registration failed");dialogue.release();
        Require(controller.AddOwnedSubsystem(system.get(),Game::C_GameAudioLayer::eSystemSubsystem),"System owner registration failed");system.release();
        Require(MusicSubsystem::NativeWaitForLoader()&&UISubsystem::NativeWaitForLoader(),"Never-started loaders were not inert");
        std::atomic<int> done{0},destroyed{0};
        g_pMusicSubSystemAsyncLoadThread=new Loader(done,destroyed);
        g_pUISubSystemAsyncLoadThread=new Loader(done,destroyed);
        g_pMusicSubSystemAsyncLoadThread->Start();g_pUISubSystemAsyncLoadThread->Start();
        Require(MusicSubsystem::NativeWaitForLoader()&&UISubsystem::NativeWaitForLoader(),"Started loaders did not join");
        Require(done==2&&destroyed==2&&!g_pMusicSubSystemAsyncLoadThread&&!g_pUISubSystemAsyncLoadThread,
                "Loader owner was retired before real MT work finished");
        Require(MusicSubsystem::NativeWaitForLoader()&&UISubsystem::NativeWaitForLoader(),"Repeated loader retirement was not inert");
        Require(controller.ReleaseOwnedSubsystem(Game::C_GameAudioLayer::eMusicSubsystem),"Music owner did not retire");
        Require(controller.ReleaseOwnedSubsystem(Game::C_GameAudioLayer::eUISubsystem),"UI owner did not retire its real scheduler group");
        Require(controller.ReleaseOwnedSubsystem(Game::C_GameAudioLayer::eDialogueSubsystem),"Dialogue owner did not retire");
        Require(controller.ReleaseOwnedSubsystem(Game::C_GameAudioLayer::eSystemSubsystem),"System owner did not retire last");
        controller.Release();controller.Release();
        // The real subscriber pool destructor below enforces that all 15 original owner subscriptions retired.
    }
    Scheduler::Shutdown();
    std::cout<<"Audio subsystem ownership: real poll/ref acquisition, owned/borrowed/orphan/final retirement, >32 stale handles across retries, late callback cancellation, loader joins and subscriber cleanup PASS (no bank playback/action producer claim)\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
