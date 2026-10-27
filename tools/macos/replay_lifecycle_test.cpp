// [Blur2PC][macOS] Actual replay/game ownership without starting a game, renderer,
// Artery service or asset load. Subscriber pool destructors enforce detachment.
#include "../../code/amax/game/GameLogic/stdafx.h"
#include <GameApp.h>
#include <Game/Game.h>
#include <GameLogic/Replays/ReplayDirector.h>
#include <GameLogic/Replays/ReplaySaveManager.h>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>

static void require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

int main()
{
    try
    {
        require(Game::C_Game::GetInstance() == nullptr, "Game unexpectedly exists before fixture");
        // Local instances lack the namespace-static singleton's implicit zero fill.
        alignas(Game::C_ReplayDirector) unsigned char storage[sizeof(Game::C_ReplayDirector)];
        std::memset(storage, 0x11, sizeof(storage));
        auto* unused = new (storage) Game::C_ReplayDirector();
        unused->Destroy();
        unused->Destroy();
        unused->~C_ReplayDirector();

        auto director = std::make_unique<Game::C_ReplayDirector>();
        auto game = std::make_unique<Game::C_Game>();
        for (int lifetime = 0; lifetime < 2; ++lifetime)
        {
            director->Init();
            auto* save = director->GetSaveManager();
            require(save != nullptr, "Original replay resource initialization failed");
            director->Init();
            require(save == director->GetSaveManager(), "Repeated Init replaced live replay resources");
            save->WaitForWorkers();
            save->WaitForWorkers();
            {
                Messaging::C_MessageSubscriberSystem actorMessages;
                director->GameInit(&actorMessages);
                // Destroy must also detach the real actor forwarder if GameEnd
                // has not run; its subscriber pool goes out of scope next.
                director->Destroy();
                require(director->GetSaveManager() == nullptr, "Replay save manager remained owned after Destroy");
            }
            director->Destroy();
        }
        // In the real application this is C_GameApp's ownership boundary. The
        // detached director's later destructor must not consult this dead game.
        game.reset();
        require(Game::C_Game::GetInstance() == nullptr, "C_Game did not clear its singleton");
        director.reset();
        {
            C_GameApp owner;
            Game::C_ReplayDirector::Instance().Init();
            require(Game::C_ReplayDirector::Instance().GetSaveManager() != nullptr,
                    "Application-owned replay initialization failed");
        }
        require(Game::C_Game::GetInstance() == nullptr &&
                Game::C_ReplayDirector::Instance().GetSaveManager() == nullptr,
                "Actual application destructor did not release replay before C_Game");
        std::cout << "Actual replay lifecycle: poisoned never-Init, repeat Init/Destroy, game+actor subscriptions, "
                     "resource release and C_GameApp owner teardown PASS\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
