// [Blur2PC] Actual Amax lazy save-interface owner and real Lua registration lifecycle.
// Loading preparation only: no authored career bytes, progress mutation, saved-file write or assets required.
#include "../../code/amax/game/GameLogic/stdafx.h"
#include <ui/Game/SaveSystem/SaveSystem.h>
#include <ui/Game/UILua.h>
#include <iostream>
#include <stdexcept>

namespace Game { LUALIB_API int luaopen_game_profile(lua_State* state); }
namespace UI { void NativeRetireProfileSaveInterfaces(); }
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static void prepare(lua_State* state,const char* name,bool photo=false){
    lua_getglobal(state,"GameProfile");lua_getfield(state,-1,name);lua_pushboolean(state,true);
    if(photo)lua_pushinteger(state,-1);
    const int status=lua_pcall(state,photo?2:1,0,0);
    require(status==0,"Actual profile interface Lua callback failed");lua_pop(state,1);
}
class Borrowed final:public UI::C_SaveInfoInterface {
public:
    bool Save(S32,U8*,U32,S32)override{return false;}
    bool Load(S32,U8*,U32,S32)override{return false;}
};
int main(){lua_State* state=NULL;try{
    state=lua_open();require(state,"Cannot create actual Lua state");
    auto& saves=UI::C_SaveSystem::Instance();
    for(U32 generation=0;generation<3;++generation) {
        Game::luaopen_game_profile(state);lua_settop(state,0);
        prepare(state,"SetSaveInfoInterfaceProfile");
        auto* profile=saves.SaveInfoInterface();
        require(profile&&profile->m_SaveRoot=="SAVE"&&profile->m_FileInfo.Size()==1&&profile->m_FileInfo[0].m_FileName=="BLURSAVE","Actual lazy profile preparation did not publish original metadata");
        prepare(state,"SetSaveInfoInterfacePhotos",true);
        auto* photos=saves.SaveInfoInterface();require(photos&&photos!=profile,"Actual photo interface did not acquire a distinct owner");
        UI::NativeRetireProfileSaveInterfaces();
        require(saves.NativeHasInterface(NULL),"Actual lazy owner did not detach its photo borrower");
        prepare(state,"SetSaveInfoInterfaceProfile");prepare(state,"SetSaveInfoInterfacePhotos",true);
        require(saves.NativeHasInterface(NULL)&&!saves.StartAsyncLoad(0)&&!saves.StartAsyncSave(0)&&!saves.StartAsyncDelete(0),"Late screen callback reopened a retired owner/request generation");
        UI::NativeRetireProfileSaveInterfaces();
    }
    // [Blur2PC] Retiring acquired lazy interfaces must preserve an unrelated borrowed owner exactly.
    Game::luaopen_game_profile(state);lua_settop(state,0);
    prepare(state,"SetSaveInfoInterfaceProfile");prepare(state,"SetSaveInfoInterfacePhotos",true);
    Borrowed borrowed;saves.SaveInfoInterface(&borrowed);
    UI::NativeRetireProfileSaveInterfaces();require(saves.NativeHasInterface(&borrowed),"Lazy owner deleted/detached an unrelated borrowed interface");
    saves.NativeQuiesceAndDetach(&borrowed);require(saves.NativeHasInterface(NULL),"Fixture borrowed owner did not retire its exact borrow");
    lua_close(state);state=NULL;
    std::cout<<"Actual Amax lazy profile/photo owner: three Lua generations, exact retirement, late-request rejection and borrowed isolation PASS (no career serialization)\n";
    return 0;
}catch(const std::exception& error){
    try{UI::NativeRetireProfileSaveInterfaces();}catch(...){}
    if(state)lua_close(state);std::cerr<<error.what()<<'\n';return 1;
}}
