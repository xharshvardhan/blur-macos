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
#include <iterator>
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
// [Blur2PC] Execute the real parent Options script in the original Lua VM.
// Authored host/UI services expose row creation and dispatch; they do not repeat
// the production capability predicate. This is a script contract test, not UI/GPU proof.
static void runLua(lua_State* state,const std::string& source,const char* name){
    if(luaL_loadbuffer(state,source.data(),source.size(),name)||lua_pcall(state,0,0,0)){
        const char* message=lua_tostring(state,-1);std::string error=message?message:"Unknown Options Lua fixture failure";lua_pop(state,1);throw std::runtime_error(error);
    }
}
static void verifyOptionsContract(lua_State* state){
    const auto path=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()/"code/amax/library/Ui/game/lua_scripts/screens/Shared/Options.lua";
    std::ifstream stream(path);require(bool(stream),"Cannot read actual Options.lua for contract fixture");
    const std::string script((std::istreambuf_iterator<char>(stream)),std::istreambuf_iterator<char>());
    const std::string host=R"lua(
fixture_rows={};fixture_selection=nil;fixture_next=nil;fixture_previous=4;fixture_scale=1
UIEnums={Platform={PC=1,Xenon=2,PS3=3},CurrentPlatform=1,GameMode={SinglePlayer=1},Message={MenuNext=10,SaveLocationFound=11},XboxDeviceSelector={ForceUI="authored-force-ui"}}
UIGlobals={IsIngame=true}
SCUI={name_to_id={menu=100,ScaleDummy=101,_CarouselDummy=102}}
UIText=setmetatable({},{__index=function(_,key)return key end})
Amax={Options=function()return {} end,GetOptionsDefaults=function()return {} end,
 UpdateCustomKeyboardControls=function()return {IsCustom=0} end,
 GetGameMode=function()return 1 end,CanUseShare=function()return false end,
 ChangeUiCamera=function()end,GraphicResolutionCount=function()return 3 end,
 GraphicAACount=function()return 2 end,UpdateGraphicsOptions=function()end,
 GetScreenResolution=function()return 1280,720 end}
UIButtons={ChangeScale=function(_,scale)fixture_scale=scale end,NodeListScan=function()end,
 SetSelection=function(_,value)assert(fixture_rows[value],"selected a missing Options row");fixture_selection=value end,
 GetSelection=function()return fixture_selection end}
function Mp_SetupMenuItemHelpText(_,value)assert(not fixture_rows[value]);fixture_rows[value]=true;return 1000+value end
function IsTable(value)return type(value)=="table" end
function IsNumber(value)return type(value)=="number" end
function IsSplitScreen()return false end
function IsControllerLocked()return false end
function SCUI_MessageUpdate()return false end
function GetScreenOtions()return {selection=fixture_previous} end
function AddSCUI_Elements()end
function CarouselApp_SetScreenTimers()end
function StoreInfoLine()end
function SetupMenuOptions()end
function SetupScreenTitle()end
function PlaySfxNext()end
function GoScreen(target)fixture_next=target end
Profile={GetPrimaryPad=function()return 0 end}
GameProfile={HasLinkCode=function()return true end}
UIHardware={FindXbox360SaveLocation=function(pad,bytes,flags)assert(pad==0 and bytes==0 and flags==UIEnums.XboxDeviceSelector.ForceUI);fixture_next="storage" end}
function CanSave()return true end
)lua";
    auto scenario=[&](const std::string& setup,const std::string& assertions){
        runLua(state,host,"options-authored-host");runLua(state,script,path.string().c_str());
        runLua(state,setup+"\nInit();PostInit()\n"+assertions,"options-contract-case");
        require(lua_gettop(state)==0,"Options script contract leaked Lua stack entries");
    };
    scenario("",R"lua(
assert(fixture_rows[1] and fixture_rows[2] and fixture_rows[3] and fixture_rows[4])
assert(fixture_selection==4)
MessageUpdate(UIEnums.Message.MenuNext,0,true)
assert(fixture_next==[[Shared\Options_Graphics.lua]] and UIGlobals.SaveGraphicOptions==true)
)lua");
    // A partial backend must neither advertise Graphics nor restore a remembered
    // missing row; selection 1 (Keyboard) and 3 (Audio) retain their retail routes.
    for(const char* member:{"GraphicResolutionCount","GraphicAACount","UpdateGraphicsOptions","GetScreenResolution"}){
        scenario(std::string("Amax.")+member+"=nil",R"lua(
assert(fixture_rows[1] and fixture_rows[2] and fixture_rows[3] and not fixture_rows[4])
assert(fixture_selection==1)
MessageUpdate(UIEnums.Message.MenuNext,0,true)
assert(fixture_next==[[Shared\Options_Control.lua]])
fixture_selection=3;MessageUpdate(UIEnums.Message.MenuNext,0,true)
assert(fixture_next==[[Shared\Options_Audio.lua]] and UIGlobals.SaveOptions==true)
fixture_next=nil;fixture_selection=4;MessageUpdate(UIEnums.Message.MenuNext,0,true)
assert(fixture_next==nil and UIGlobals.SaveGraphicOptions==nil)
)lua");
    }
    scenario("Amax.GraphicAACount={}",R"lua(
assert(not fixture_rows[4] and fixture_selection==1)
)lua");
    scenario("",R"lua(
Amax.UpdateGraphicsOptions=nil
MessageUpdate(UIEnums.Message.MenuNext,0,true)
assert(fixture_next==nil and UIGlobals.SaveGraphicOptions==nil)
)lua");
    scenario("UIGlobals.IsIngame=false",R"lua(
assert(fixture_rows[0] and fixture_rows[4] and fixture_rows[5] and fixture_rows[9])
assert(math.abs(fixture_scale-0.94)<0.00001)
)lua");
    scenario("UIGlobals.IsIngame=false;Amax.GraphicAACount=nil",R"lua(
assert(fixture_rows[0] and not fixture_rows[4] and fixture_rows[5] and fixture_rows[9])
assert(fixture_selection==0 and fixture_scale==1)
)lua");
    scenario("UIEnums.CurrentPlatform=UIEnums.Platform.PS3;fixture_previous=2",R"lua(
assert(fixture_rows[1] and fixture_rows[2] and fixture_rows[3] and not fixture_rows[4])
MessageUpdate(UIEnums.Message.MenuNext,0,true)
assert(fixture_next==[[Shared\Options_ImageAdjust.lua]])
)lua");
    scenario("UIEnums.CurrentPlatform=UIEnums.Platform.Xenon",R"lua(
assert(fixture_rows[1] and fixture_rows[2] and fixture_rows[3] and fixture_rows[4])
MessageUpdate(UIEnums.Message.MenuNext,0,true)
assert(fixture_next=="storage")
)lua");
    std::cout<<"Actual Options.lua complete/partial graphics capability, remembered-row fallback, keyboard/audio dispatch and console image/storage preservation PASS (authored UI host)\n";
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
    verifyOptionsContract(state);
    ui.End();UI::g_pUILua=nullptr;
    std::cout<<"Original Lua5.1/UI compressed pack chunking/disk loading/errors/allocator PASS (authored scripts; not game menus)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
