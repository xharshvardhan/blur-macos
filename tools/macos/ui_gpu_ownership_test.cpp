// [Blur2PC][macOS] Actual UI layer callback retires its global GPU owners.
#include "../../code/amax/game/stdafx.h"
#include <Game/Game.h>
#include <framework/view/View.h>
#include <Ui/UiLayer.h>
#include <Ui/LightPaintSystem.h>
#include <system/scheduler/jobqueue.h>
#include <iostream>
#include <stdexcept>
static void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main(){try{
    Scheduler::Init();Scheduler::SetGroupPoolSize(64);
    {
        Game::C_Game game;
        Display::C_Device device;Display::C_DeviceParams params;params.m_Width=32;params.m_Height=24;
        require(device.Create(params)&&device.CreateVolatile(params),"UI ownership fixture device initialization failed");
#if TRACK_DISPLAY2_RESOURCES
        const auto baseline=device.GetDisplay2().GetVertexBufferCount();
#endif
        for(unsigned lifetime=0;lifetime<2;++lifetime){
            UI::C_UiLayer layer;
            require(!LightPaint_IsCreated(),"UI light-paint owners survived the previous lifetime");
            LightPaint_Init(4,8);require(LightPaint_IsCreated(),"Actual UI light-paint GPU acquisition failed");
            LightPaint_Init(4,8);
#if TRACK_DISPLAY2_RESOURCES
            require(device.GetDisplay2().GetVertexBufferCount()==baseline+2,"Actual UI sparkle buffers were absent or duplicated");
#endif
            require(SUCCEEDED(layer.Destroy()),"Actual UI layer Destroy callback failed");
            require(!LightPaint_IsCreated(),"UI layer callback retained global light-paint owners");
            require(SUCCEEDED(layer.Destroy()),"Repeated UI layer retirement failed");
#if TRACK_DISPLAY2_RESOURCES
            require(device.GetDisplay2().GetVertexBufferCount()==baseline,"UI layer retained real GPU vertex buffers");
#endif
        }
        require(device.GetDisplay2().WaitIdle(),"UI ownership GPU completion failed");device.DestroyVolatile();device.Destroy();
    }
    Scheduler::Shutdown();
    std::cout<<"Actual UI layer Destroy callback, original light-paint GPU buffers, repeated retirement and two lifetimes PASS; full UI startup/race remains a separate actual-game check\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
