// [Blur2PC] Real Multiplayer getter registration and PC32 garage/profile ID identity.
// Authored in-memory values only: no game/config launch, profile loading or save API.
#include "../../code/amax/game/GameLogic/stdafx.h"
#include <GameLogic/GameData/GameVehicleData.h>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
// Fixture-only access to actual profile values; no production accessor/layout changes.
#define private public
#include <GameLogic/Profile/GameProfileManager.h>
#undef private
#include <lua_shared/LuaHelper.h>

namespace UI { LUALIB_API int luaopen_multiplayer(lua_State* state); }
namespace {
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void Script(lua_State* state,const char* code){
    if(luaL_dostring(state,code)){
        const char* text=lua_tostring(state,-1);
        const std::string message=text?text:"Unknown Lua error";
        lua_pop(state,1);throw std::runtime_error(message);
    }
}
struct Fixture {
    lua_State* state=nullptr;
    std::filesystem::path directory;
    SysProfile::C_Manager& system=SysProfile::C_Manager::Instance();
    Game::C_GameProfile* profile=nullptr;
    S32 previousPrimary=system.m_PrimaryPadNo;
    U32 previousGlobal=0;
    std::array<U32,AmaxCarClass_Count> previousClass{};
    bool ownsProfileValues=false;
    Fixture(){
        // No SysProfile::Initialise: even the OS-account identity store remains unopened.
        char location[]="/tmp/blur-multiplayer-garage-id-XXXXXX";
        Require(mkdtemp(location)!=nullptr,"Cannot create isolated fixture directory");
        directory=location;
        const auto store=directory/"local-profile-id";
        Require(setenv("BLUR_MAC_STATE_DIR",directory.c_str(),1)==0&&
                setenv("BLUR_MAC_PROFILE_STORE",store.c_str(),1)==0,
                "Cannot isolate profile persistence paths");
        profile=Game::C_GameProfileManager::Instance().GetProfile(0);
        Require(profile!=nullptr,"Actual static game-profile slot is absent");
        previousGlobal=profile->mMultiplayerProfile.mLastCarUsed;
        for(int i=0;i<AmaxCarClass_Count;++i)
            previousClass[i]=profile->mMultiplayerProfile.mLastCarUsedClass[i];
        ownsProfileValues=true;
        system.m_PrimaryPadNo=0;
        state=lua_open();Require(state!=nullptr,"Cannot create actual Lua VM");
        luaopen_base(state);lua_settop(state,0);
        UI::luaopen_multiplayer(state);lua_settop(state,0);
    }
    ~Fixture(){
        if(state)lua_close(state);
        if(ownsProfileValues){
            profile->mMultiplayerProfile.mLastCarUsed=previousGlobal;
            for(int i=0;i<AmaxCarClass_Count;++i)
                profile->mMultiplayerProfile.mLastCarUsedClass[i]=previousClass[i];
        }
        system.m_PrimaryPadNo=previousPrimary;
        if(!directory.empty()){std::error_code ignored;std::filesystem::remove_all(directory,ignored);}
    }
};
void GarageTable(lua_State* state,U32 bits){
    // GetGarage uses rawseti(...,pVehicleAM->ID), whose index is a signed int.
    // Deliberately build the oracle independently of the profile getter helper.
    const S64 signedValue=bits<=0x7fffffffU?S64(bits):S64(bits)-4294967296LL;
    lua_newtable(state);lua_newtable(state);
    Lua::AddDataMember(state,"id",bits);
    lua_rawseti(state,-2,static_cast<S32>(signedValue));lua_setglobal(state,"cars");
    lua_pushnumber(state,static_cast<lua_Number>(signedValue));lua_setglobal(state,"expected");
}
void Check(lua_State* state,const char* invocation){
    const std::string code=std::string("local id=")+invocation+R"lua(
        assert(type(id)=='number' and id==expected, 'getter changed PC32 ID identity')
        assert(cars[id] and cars[id].id==id, 'selected getter ID misses native garage key')
        -- Original Garage Init compares each class car key with viewing_id_old.
        local found=false
        for key,car in pairs(cars) do
            if key==id then found=true;assert(car.id==id) end
        end
        assert(found, 'original class-list selection comparison cannot find saved car')
    )lua";
    Script(state,code.c_str());
    Require(lua_gettop(state)==0,"Garage callback/test leaked a Lua stack value");
}
}
int main(){try{
    static_assert(sizeof(lua_Integer)==sizeof(void*)&&sizeof(lua_Integer)==8,
                  "Fix must retain LP64 Lua integers/pointers, not globally narrow Lua");
    static_assert(sizeof(S32)==4&&sizeof(U32)==4,"Original garage identity must remain PC32");
    Fixture fixture;
    auto* state=fixture.state;
    const std::array<U32,6> ids{{0U,1U,0x7fffffffU,0x80000000U,4112414174U,0xffffffffU}};
    for(U32 id:ids){
        fixture.profile->mMultiplayerProfile.mLastCarUsed=id;
        // Distinct class values prove the indexed path does not accidentally return global.
        for(int n=0;n<AmaxCarClass_Count;++n)
            fixture.profile->mMultiplayerProfile.mLastCarUsedClass[n]=ids[(n+1)%ids.size()];
        GarageTable(state,id);Check(state,"Multiplayer.GetGarageCarByID()");
        const std::string any="Multiplayer.GetGarageCarByID("+std::to_string(AmaxCarClass_ANY)+")";
        Check(state,any.c_str());
        // Existing nonnumber-argument behavior is the global-car path.
        Check(state,"Multiplayer.GetGarageCarByID(false)");
        for(int n=0;n<AmaxCarClass_Count;++n){
            const U32 classId=fixture.profile->mMultiplayerProfile.mLastCarUsedClass[n];
            GarageTable(state,classId);
            const std::string call="Multiplayer.GetGarageCarByID("+std::to_string(n)+")";
            Check(state,call.c_str());
        }
        Require(fixture.profile->mMultiplayerProfile.GetLastCarUsed()==id,
                "Read-only Lua getter mutated global profile selection");
        for(int n=0;n<AmaxCarClass_Count;++n)
            Require(fixture.profile->mMultiplayerProfile.GetLastCarUsed(static_cast<AmaxCarClass>(n))==ids[(n+1)%ids.size()],
                    "Read-only Lua getter mutated class profile selection");
    }
    fixture.system.m_PrimaryPadNo=-1;
    Script(state,"assert(select('#',Multiplayer.GetGarageCarByID())==0);assert(select('#',Multiplayer.GetGarageCarByID(0))==0);assert(Multiplayer.GetGarageCarByID()==nil)");
    fixture.system.m_PrimaryPadNo=0;
    // PC32 ID repair must not narrow independent native pointer values.
    Require(reinterpret_cast<std::uintptr_t>(state)>0xffffffffULL,"Fixture did not exercise a high LP64 address");
    lua_pushlightuserdata(state,state);
    Require(lua_touserdata(state,-1)==state,"Native pointer ABI was narrowed");lua_pop(state,1);
    Require(std::filesystem::is_empty(fixture.directory),"Garage getter unexpectedly persisted profile data");
    std::cout<<"Actual Multiplayer garage getter: signed PC32 low/high IDs, global/class/ANY keys, original selection comparison, missing profile and LP64 pointers PASS; no profile persistence\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"Multiplayer garage ID fixture failed: "<<error.what()<<'\n';return 1;}}
