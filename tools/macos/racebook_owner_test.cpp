// [Blur2PC] Actual RaceBook owned pool/data lifecycle, not gameplay or fabricated progression.
#include "../../code/amax/game/GameLogic/stdafx.h"
#include <GameApp.h>
#include <Game/Messaging/GameflowMessages.h>
#include <RaceBook/RaceBookManager.h>
#include <RaceBook/Data/RaceBookData.h>
#include <GameLogic/Profile/GameProfileManager.h>
#include <system/memory/MemoryTracker.h>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct AuthoredMetadata {
    AmaxVehicle model;RaceBookVehicle vehicle;RaceBookCharacter character;
    AuthoredMetadata():model(),vehicle(),character(){
        Require(C_RaceBookData::Instance().mCharacters.Number()==0&&C_RaceBookData::Instance().mVehicles.Number()==0&&C_GameVehicleData::Instance().mVehicles.Number()==0,"Fixture must not replace preexisting game metadata");
        model.ID=0x7ed16301;model.tag=XtSz8("OwnerModel");
        vehicle.ID=0x7ed16302;vehicle.tag=XtSz8("OwnerVehicle");vehicle.mNumberOfBaseSlots=2;
        character.ID=0x7ed16303;character.tag=XtSz8("OwnerCharacter");character.type=E_CHARACTER_TYPE_BUDDY;
        // Original optional vehicle/base and starting-garage fallbacks consume these real metadata lists.
        C_GameVehicleData::Instance().mVehicles.Add(&model);
        C_RaceBookData::Instance().mVehicles.Add(&vehicle);
        C_RaceBookData::Instance().mCharacters.Add(&character);
    }
    ~AuthoredMetadata(){C_RaceBookData::Instance().mCharacters.Destroy(&character);C_RaceBookData::Instance().mVehicles.Destroy(&vehicle);C_GameVehicleData::Instance().mVehicles.Destroy(&model);}
};
void VerifyLive(RaceBook::C_RaceBookManager& owner){
    Require(owner.NumProfiles()==1&&owner.GetNumVehicles()==1,"Actual Init did not publish one owned profile and vehicle");
    Require(owner.IsSubscribedTo(Game::E_GameflowMessage_SaveGameLoaded),"Actual Init did not acquire its real self subscription");
    Require(owner.GetProfileByIndex(0)->GetID()==0x7ed16303&&owner.GetVehicle(0)->mBaseModelID==0x7ed16301,"Original metadata fallback populated unexpected owners");
}
void Lifecycle(){
    AuthoredMetadata metadata;
    {
        RaceBook::C_RaceBookManager owner;
        for(unsigned cycle=0;cycle<3;++cycle){
            owner.Init();VerifyLive(owner);
            // Populate an actual owned dialogue payload so profile destruction retires nested allocation.
            auto* profile=owner.GetProfileByIndex(0);
            profile->mPlayerCollidesWithDriverItems.PushBack(new Game::C_InGameDialogueConversation);
            owner.Destroy();owner.Destroy();
            Require(!owner.IsSubscribedTo(Game::E_GameflowMessage_SaveGameLoaded),"Destroy retained self-pool callback");
            Require(owner.NumProfiles()==1&&owner.GetNumVehicles()==1&&profile->mPlayerCollidesWithDriverItems.Size()==1,"Destroy prematurely freed data required through Audio teardown");
            owner.NativeRetireOwnedState();owner.NativeRetireOwnedState();
            Require(owner.NumProfiles()==0&&owner.GetNumVehicles()==0,"Final retirement retained owned profile or vehicle");
        }
        // Reinitialization of an acquired state must retire old objects before reconstructing.
        owner.Init();VerifyLive(owner);owner.Init();VerifyLive(owner);
        // Destructor fallback must detach before its own pool member is destroyed.
    }
    auto& owner=RaceBook::C_RaceBookManager::Instance();
    auto& profiles=Game::C_GameProfileManager::Instance();
    Require(profiles.GetNumProfiles()>0,"Actual game profile array absent");
    auto* borrower=profiles.GetProfile(0);auto* previous=borrower->mRaceBookProfile;
    for(unsigned cycle=0;cycle<2;++cycle){
        {C_GameApp application;application.CreateGlobalXtDatabase();owner.Init();VerifyLive(owner);borrower->mRaceBookProfile=owner.GetProfileByIndex(0);owner.Destroy();Require(borrower->mRaceBookProfile!=NULL,"Layer-style Destroy cleared a live downstream borrower too early");}
        Require(owner.NumProfiles()==0&&owner.GetNumVehicles()==0&&!owner.IsSubscribedTo(Game::E_GameflowMessage_SaveGameLoaded)&&borrower->mRaceBookProfile==NULL,"Actual App final-owner hook left RaceBook owners or matching borrower alive");
    }
    borrower->mRaceBookProfile=previous;
}
}
int main(){try{
    Require(XtInitialise(),"Real fixture schema lease failed");
    const auto tracker=MEMORY_TRACKER_CURRENT_CONTEXT();Lifecycle();
    Require(MEMORY_TRACKER_CURRENT_CONTEXT()==tracker,"RaceBook lifecycle stranded tracker context");
    XtShutdown();
    std::cout<<"Actual RaceBook Init/reinit/self-pool retirement, retained Audio data, nested profiles, App final-owner hook and destructor fallback PASS; process must exit0 (not gameplay)\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"RaceBook owner fixture failed: "<<error.what()<<'\n';return 1;}}
