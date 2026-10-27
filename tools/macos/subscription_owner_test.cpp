// [Blur2PC][macOS] Real per-owner binding retirement, distinct live pools and reusable static game handlers.
#include "../../code/amax/game/GameLogic/stdafx.h"
#include <misc_shared/Messaging/MessageHandler.h>
#include <misc_shared/Messaging/MessageMultiHandler.h>
#include <misc_shared/Messaging/MessageSubscriberSystem.h>
#include <Game/Game.h>
#include <GameLogic/Layers/RaceBookLayer.h>
#include <GameLogic/RaceModes/RaceModes.h>
#include <GameLogic/StickerManager.h>
#include <Game/Messaging/GameflowMessages.h>
#include <Game/Messaging/PickupActorMessages.h>
#include <iostream>
#include <stdexcept>
namespace {
constexpr Messaging::TMsgType first=0x7ef00001,second=0x7ef00002;
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Single final:Messaging::C_MessageHandler {
    unsigned calls=0;
    bool MessageHandler(const Messaging::C_MessageBase*)override{++calls;return true;}
    void Enter(Messaging::C_MessageSubscriberSystem& pool,bool fail){SetSubscriberSystem(&pool);AddSubscriber(first);if(fail)throw std::runtime_error("Authored partial subscriber Init");AddSubscriber(second);}
};
struct Multi final:Messaging::C_MessageMultiHandler {
    unsigned calls=0;
    bool MessageHandler(const Messaging::C_MessageBase*)override{++calls;return true;}
};
void Bindings(){
    Single single;Multi multi;
    for(unsigned cycle=0;cycle<3;++cycle){
        Messaging::C_MessageSubscriberSystem game,borrowed;
        Require(!single.NativeDetachSubscriberSystem(&game)&&!multi.NativeDetachSubscriberSystem(&game),"Unentered retirement manufactured an acquired binding");
        bool failed=false;try{single.Enter(game,true);}catch(const std::runtime_error&){failed=true;}
        Require(failed&&single.IsSubscribedTo(first),"Real partial binding was not acquired");
        Require(!single.NativeDetachSubscriberSystem(&borrowed)&&single.IsSubscribedTo(first),"Retirement touched an unrelated pool");
        game.BroadcastMessage(first);const unsigned before=single.calls;
        Require(single.NativeDetachSubscriberSystem(&game)&&!single.NativeDetachSubscriberSystem(&game),"Partial binding did not retire once");
        game.BroadcastMessage(first);Require(single.calls==before,"Retired single owner received a callback");
        single.Enter(game,false);game.BroadcastMessage(second);Require(single.calls==before+1,"Single owner could not reacquire after partial failure");single.NativeDetachSubscriberSystem(&game);
        multi.SetSubscriberSystem(&game);multi.AddSubscriber(first);
        multi.SetSubscriberSystem(&borrowed);multi.AddSubscriber(second);
        const unsigned count=multi.calls;
        Require(multi.NativeDetachSubscriberSystem(&game),"Multi owner did not retire the expected game forwarder");
        game.BroadcastMessage(first);borrowed.BroadcastMessage(second);Require(multi.calls==count+1,"Multi retirement removed a borrowed-pool binding or left a game callback");
        Require(multi.AddSubscriber(first),"Retiring another pool disturbed the current borrowed forwarder");
        borrowed.BroadcastMessage(first);Require(multi.calls==count+2,"Current borrowed forwarder is not retained");
        Require(multi.NativeDetachSubscriberSystem(&borrowed)&&!multi.NativeDetachSubscriberSystem(&borrowed)&&!multi.AddSubscriber(first),"Current forwarder was not retired, or cleanup constructed a new one");
        // Exercise the converse: removing current game binding must retain the earlier borrowed one.
        multi.SetSubscriberSystem(&borrowed);multi.AddSubscriber(second);multi.SetSubscriberSystem(&game);multi.AddSubscriber(first);
        Require(multi.NativeDetachSubscriberSystem(&game)&&!multi.AddSubscriber(first),"Retiring current pool selected an unrelated forwarder");
        const unsigned retained=multi.calls;borrowed.BroadcastMessage(second);Require(multi.calls==retained+1,"Earlier borrowed forwarder disappeared");
        multi.SetSubscriberSystem(&borrowed);multi.NativeDetachSubscriberSystem(&borrowed);
    } // Both real pools assert empty before these handlers die; no pool clearing.
}
void ActualStaticOwners(){
    for(unsigned cycle=0;cycle<2;++cycle){
        Game::C_Game game;auto& pool=game.GetSubscriberSystem();
        auto* street=Game::gRaceModeStore(Game::E_RaceMode_StreetRace);
        auto* multiplayer=Game::gRaceModeStore(Game::E_RaceMode_Multiplayer_Racing);
        street->Initialise(&game.GetLevelState());multiplayer->Initialise(&game.GetLevelState());
        Require(street->IsSubscribedTo(Game::E_GameflowMessage_IntroStarted)&&multiplayer->IsSubscribedTo(Game::E_GameflowMessage_EliminationTimeoutStart),"Actual race modes did not acquire their real boot subscriptions");
        {
            // A real never-Create RaceBook layer must not retire handlers it did not acquire.
            Game::C_RaceBookLayer unentered(&pool);unentered.NativeRetireOwnedSubscriptions();
            Require(street->IsSubscribedTo(Game::E_GameflowMessage_IntroStarted),"Unentered RaceBook retired another owner's handler");
        }
        auto& stickers=Game::C_StickerManager::Instance();stickers.Init(0);
        stickers.Shutdown();
        Require(stickers.NativeDetachSubscriberSystem(&pool)&&!stickers.NativeDetachSubscriberSystem(&pool),"Sticker per-race shutdown lost its boot binding or final retirement repeated");
        Require(street->NativeDetachSubscriberSystem(&pool)&&multiplayer->NativeDetachSubscriberSystem(&pool),"Actual static race modes did not retire their expected binding");
        street->SetRLC(nullptr);street->SetRaceLogicLayer(nullptr);street->SetLevelState(nullptr);
        multiplayer->SetRLC(nullptr);multiplayer->SetRaceLogicLayer(nullptr);multiplayer->SetLevelState(nullptr);
    } // Each actual C_Game pool must be empty before the static mode/Sticker destructors.
}
}
int main(){try{
    Bindings();ActualStaticOwners();
    std::cout<<"Actual single/multi expected-pool retirement, preserved borrowed bindings, partial Init failure/retry,3 repeated pool lifetimes, real unentered RaceBook,2 actual C_Game/StreetRace/MP/Sticker lifetimes, per-race Sticker retention and true static exit PASS (not gameplay)\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
