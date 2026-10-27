// [Blur2PC] Original Lua interpreter, allocator and UI script-pack/disk readers on authored scripts.
#include "stdafx.h"
#include <ui/Game/UILua.h>
#include <ui/UiInterface.h>
#include <system/serialise.h>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
#include <unistd.h>

static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
static bool failAssert(const char*,const char*,const char*,int){throw std::runtime_error("original Lua/UI assertion");}
[[noreturn]] static void unusedHostService(){throw std::logic_error("Script fixture unexpectedly requested a rendering/input service");}
class Host final:public UI::I_UiInterface {
public:
    void RegisterLibs(lua_State* state) override {
        const std::array<luaL_Reg,5> libraries{{{"base",luaopen_base},{"table",luaopen_table},{"string",luaopen_string},{"math",luaopen_math},{"debug",luaopen_debug}}};
        for(const auto& library:libraries){lua_pushcfunction(state,library.func);lua_pushstring(state,library.name);lua_call(state,1,0);}
    }
    const bool IsWideScreen() override{unusedHostService();}
    void PlaySound(S32,F32,F32) override{unusedHostService();}
    const bool GetMousePos(F32&,F32&) override{unusedHostService();}
    const bool GetLeftMouseDown() override{unusedHostService();}
    const bool UseMouseInput() override{unusedHostService();}
    void UseMouseInput(bool) override{unusedHostService();}
    const bool GetMouseScrollUp() override{unusedHostService();}
    const bool GetMouseScrollDown() override{unusedHostService();}
    Display::ResourceBundler::I_ResourceBundle* CreateResourceBundle() override{unusedHostService();}
    void RenameMovieFor50Hz(const char*,char*,S32) override{unusedHostService();}
    void RenameMovieForSmallNTSC(const char*,char*,S32) override{unusedHostService();}
    void PollPlatformCallbacks() override{unusedHostService();}
};
// [Blur2PC] DumpStack uses the host diagnostic sink in the actual game.
namespace Lua { void OutputString(const char* text){if(text)std::cout << text << std::endl;} }
static Host host;
namespace UI { I_UiInterface* UiInterface(){return &host;} }
static std::vector<U8> pack() {
    const std::array<std::string,3> names{{"UIEnums.lua","large.lua","bad.lua"}};
    std::string large="local s=0;for n=1,100 do s=s+n end;pack_sum=s;pack_root=math.sqrt(81);pack_text=string.upper('native');";
    for(U32 i=0;i<3000;++i)large+="-- authored native chunk boundary "+std::to_string(i)+"\n";
    const std::array<std::string,3> scripts{{"UIEnums={TOKEN=41};",large,"this is not valid Lua !!!"}};
    std::array<std::array<S32,4>,3> entries{};
    std::vector<U8> payload;
    for(U32 i=0;i<names.size();++i){entries[i][0]=payload.size();payload.insert(payload.end(),names[i].begin(),names[i].end());payload.push_back(0);}
    for(U32 i=0;i<scripts.size();++i){
        std::vector<U8> compressed(compressBound(scripts[i].size()));uLongf length=compressed.size();
        require(compress2(compressed.data(),&length,reinterpret_cast<const Bytef*>(scripts[i].data()),scripts[i].size(),Z_BEST_SPEED)==Z_OK,"Cannot compress authored UI script");
        entries[i][1]=payload.size();entries[i][2]=length;entries[i][3]=scripts[i].size();
        payload.insert(payload.end(),compressed.begin(),compressed.begin()+length);
    }
    std::vector<U8> result(8+sizeof(entries));
    const U32 magic=static_cast<U32>('BZUI'),count=names.size();
    std::memcpy(result.data(),&magic,4);std::memcpy(result.data()+4,&count,4);std::memcpy(result.data()+8,entries.data(),sizeof(entries));
    result.insert(result.end(),payload.begin(),payload.end());
    return result;
}
int main(int argc,char** argv){try{
    require(argc==2,"Lua fixture needs an ignored scratch parent");g_pCoreAssertFunction=failAssert;
    std::string pattern=(std::filesystem::path(argv[1])/"lua-ui-XXXXXX").string();require(mkdtemp(pattern.data()),"Cannot create Lua scratch directory");
    struct Cleanup{std::filesystem::path path;~Cleanup(){Sys::SetDefaultFileSystem(nullptr);std::error_code error;std::filesystem::remove_all(path,error);}} cleanup{pattern};
    auto& fileSystem=Sys::C_FileSystem::Instance();Sys::SetDefaultFileSystem(&fileSystem);
    require(fileSystem.SetWorkingDirectory(pattern.c_str()),"Cannot set Lua fixture working directory");
    std::ofstream(std::filesystem::path(pattern)/"disk.lua")<<"disk_sum=6*7;disk_text='from-real-file';\n";
    auto data=pack();UI::C_UILua ui;UI::g_pUILua=&ui;ui.Init(false,"",data.data());lua_State* state=ui.GetState();
    require(state&&lua_gettop(state)==0,"Original UI Lua startup/enum loading left invalid state");
    lua_pushstring(state,"authored diagnostic");Lua::DumpStack(state);lua_pop(state,1); // Exercise the real host diagnostic link closure.
    lua_getglobal(state,"UIEnums");lua_getfield(state,-1,"TOKEN");require(lua_tointeger(state,-1)==41,"Original compressed enum pack reader changed values");lua_settop(state,0);
    require(ui.LoadCallFile("large.lua")==0,"Original chunked compressed Lua script failed");
    lua_getglobal(state,"pack_sum");require(lua_tointeger(state,-1)==5050,"Original Lua interpreter table/loop execution changed");lua_pop(state,1);
    lua_getglobal(state,"pack_root");require(lua_tonumber(state,-1)==9,"Original Lua math library changed");lua_pop(state,1);
    lua_getglobal(state,"pack_text");require(!std::strcmp(lua_tostring(state,-1),"NATIVE"),"Original Lua string library changed");lua_pop(state,1);
    require(ui.LoadCallFile("missing.lua")!=0&&lua_gettop(state)==0,"Missing pack script reused an earlier script or corrupted Lua stack");
    require(ui.LoadCallFile("missing-disk.lua",true)!=0&&lua_gettop(state)==0,"Missing disk script corrupted Lua stack/status");
    require(ui.LoadCallFile("bad.lua")!=0&&lua_gettop(state)==0,"Original Lua syntax failure corrupted stack/status");
    require(ui.LoadCallFile("disk.lua",true)==0&&lua_gettop(state)==0,"Original disk script reader failed or leaked stack entries");
    lua_getglobal(state,"disk_sum");require(lua_tointeger(state,-1)==42,"Original disk reader did not execute real file contents");lua_pop(state,1);
    void* allocated=Lua::MemoryAllocator(nullptr,nullptr,0,32);require(allocated,"Original native Lua allocator failed");std::memset(allocated,0x5a,32);
    require(!Lua::MemoryAllocator(nullptr,allocated,32,std::numeric_limits<std::size_t>::max()),"Lua allocator truncated LP64 allocation request");
    require(static_cast<U8*>(allocated)[31]==0x5a,"Failed Lua allocation damaged prior block");Lua::MemoryAllocator(nullptr,allocated,32,0);
    ui.End();UI::g_pUILua=nullptr;
    std::cout<<"Original Lua5.1/UI compressed pack chunking/disk loading/errors/allocator PASS (authored scripts; not game menus)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
