// [Blur2PC][macOS] Actual application ownership before user startup; no game assets, display or service are started.
#include "../../code/amax/game/stdafx.h"
#include <GameApp.h>
#include <Game/Game.h>
#include <iostream>
#include <stdexcept>
int main() {
    try {
        for(unsigned lifetime=0;lifetime<2;++lifetime) {
            C_GameApp application;
            if(application.GetParticleTextures()!=nullptr)
                throw std::runtime_error("Unstarted application unexpectedly owns a particle-texture container");
            application.NativeWaitForGlobalResourceCaching();
            application.NativeWaitForGlobalResourceCaching();
            application.ReleaseAllGlobalResources();
            application.ReleaseAllGlobalResources();
        }
        if(Game::C_Game::GetInstance()!=nullptr)
            throw std::runtime_error("Partial-start application retained its game owner");
        std::cout<<"Actual C_GameApp partial-start global resource/cache cleanup repeated across two lifetimes PASS (no populated asset-cache or gameplay claim)\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';return 1;
    }
}
