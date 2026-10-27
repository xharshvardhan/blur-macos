// [Blur2PC][macOS] Original Simple2D acquisition with explicit test-only shader fault boundaries.
#include "../../code/amax/game/stdafx.h"
#include <Ui/UiManager.h>
#include <ui/Graphics/Simple2D.h>
#include <ui/Graphics/Renderer.h>
#include <ui/UIDefines.h>
#include <system/memory/MemoryTracker.h>
#include <exception>
#include <iostream>
#include <stdexcept>
namespace UI {extern C_UiInterface* g_pManager;}
namespace Display2 {bool NativeTestCreatePixelShaderFromFunc(const Util::C_String&,U32,C_PixelShader*,U32,const U8*);}
// Compile the original utility under a test-only name. All successful calls still
// allocate original shaders on the real device; no production fault hooks exist.
#define CreatePixelShaderFromFunc NativeTestCreatePixelShaderFromFunc
#include "../../code/mercury/library/graphics/display/source/shader/PixelShaderUtils.cpp"
#undef CreatePixelShaderFromFunc
namespace Display2 {
bool NativeTestCreatePixelShaderFromFunc(const Util::C_String& name,U32 flags,C_PixelShader* shader,U32 size,const U8* bytes){return CreatePixelShaderFromFunc(name,flags,shader,size,bytes);}
}
struct InjectedShaderFailure:std::runtime_error {InjectedShaderFailure():std::runtime_error("Authored Simple2D GPU acquisition boundary") {}};
static U32 injectedMode=0,pixelCalls=0;
namespace Display {
bool CreatePixelShaderFromFunc(const Util::C_String& name,U32 flags,C_PixelShader* shader,U32 size,const S32* bytes){
    if(injectedMode&&++pixelCalls==3){if(injectedMode==2)throw InjectedShaderFailure();return false;}
    return NativeTestCreatePixelShaderFromFunc(name,flags,shader,size,bytes);
}
}
static void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main(){try{
    Display::C_Device device;Display::C_DeviceParams params;params.m_Width=params.m_Height=16;require(device.Create(params)&&device.CreateVolatile(params),"Simple2D fixture device failed");
    UI::C_UiInterface interface;UI::g_pManager=&interface;lua_State* state=luaL_newstate();require(state,"Actual Lua state allocation failed");lua_newtable(state);lua_setglobal(state,LUA_ENUM_TABLE_NAME);
#if TRACK_DISPLAY2_RESOURCES
    const auto vertexBaseline=device.GetDisplay2().GetVertexShaderCount(),pixelBaseline=device.GetDisplay2().GetPixelShaderCount();
#endif
    // Check the renderer itself before Simple2D's outer Exit can mask a leak.
    {
        UI::C_Renderer renderer;
        for(U32 mode:{1U,2U}){
            injectedMode=mode;pixelCalls=0;bool rejected=false,identity=false;
            try{rejected=!renderer.Create();}catch(const InjectedShaderFailure&){rejected=true;identity=true;}
            require(rejected&&(mode!=2||identity),"Original renderer did not preserve its false/throw failure boundary");
#if TRACK_DISPLAY2_RESOURCES
            require(device.GetDisplay2().GetVertexShaderCount()==vertexBaseline&&device.GetDisplay2().GetPixelShaderCount()==pixelBaseline,"Renderer itself retained partial shaders before owner teardown");
#endif
            injectedMode=0;require(renderer.Create(),"Original renderer could not retry after rollback");require(!renderer.Create(),"Original renderer overwrote a live acquisition");renderer.Destroy();renderer.Destroy();
        }
    }
    UI::C_Simple2D simple;simple.Exit();
    for(U32 mode:{1U,2U}){
        const auto context=MEMORY_TRACKER_CURRENT_CONTEXT();pixelCalls=0;injectedMode=mode;bool rejected=false,originalException=false;
        try{simple.Init(state,32,8,8,1);}catch(const InjectedShaderFailure&){rejected=true;originalException=true;}catch(const std::runtime_error&){rejected=true;}
        require(rejected&&(mode!=2||originalException),"Simple2D accepted a failed shader or replaced its actual exception");require(MEMORY_TRACKER_CURRENT_CONTEXT()==context,"Failed Simple2D initialization left an unbalanced memory tracker scope");simple.Exit();simple.Exit();
#if TRACK_DISPLAY2_RESOURCES
        require(device.GetDisplay2().GetVertexShaderCount()==vertexBaseline&&device.GetDisplay2().GetPixelShaderCount()==pixelBaseline,"Partial original Simple2D GPU shader owners survived failure");
#endif
        injectedMode=0;simple.Init(state,32,8,8,1);require(MEMORY_TRACKER_CURRENT_CONTEXT()==context,"Successful Simple2D initialization left an unbalanced tracker scope");
#if TRACK_DISPLAY2_RESOURCES
        require(device.GetDisplay2().GetVertexShaderCount()>vertexBaseline&&device.GetDisplay2().GetPixelShaderCount()>pixelBaseline,"Original Simple2D did not acquire real GPU shaders");
#endif
        bool duplicate=false;try{simple.Init(state,32,8,8,1);}catch(const std::logic_error&){duplicate=true;}require(duplicate,"Live Simple2D ownership was overwritten");
        simple.ReAllocSprites(64);simple.StartFrame();simple.Exit();simple.Exit();
#if TRACK_DISPLAY2_RESOURCES
        require(device.GetDisplay2().GetVertexShaderCount()==vertexBaseline&&device.GetDisplay2().GetPixelShaderCount()==pixelBaseline,"Successful Simple2D retirement retained GPU shaders");
#endif
    }
    UI::g_pManager=nullptr;lua_close(state);require(device.GetDisplay2().WaitIdle(),"Simple2D GPU completion failed");device.DestroyVolatile();device.Destroy();
    std::cout<<"Original Simple2D partial real GPU shader allocation + authored false/throw boundaries, exception identity/tracker balance, retry/live-owner rejection, aligned sprite growth and repeated full teardown PASS"<<std::endl;return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
