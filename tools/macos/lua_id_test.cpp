// [Blur2PC] Original Lua/UI spinner PC32 numeric IDs and LP64 pointer independence.
#include "stdafx.h"
#include <ui/Game/UILua.h>
#include <ui/UiManager.h>
#include <system/serialise.h>
#include <array>
#include <cmath>
#include <lua_shared/LuaHelper.h>
#include <ui/Game/Gadgets/ButtonController.h>
#include <ui/Game/Gadgets/UIVariableSpinner.h>
#include <ui/Game/Gadgets/UISlider.h>
#include <ui/Game/UIDataFinder.h>
#include <ui/Game/UIScreenControl.h>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
static bool failAssert(const char*,const char*,const char*,int){throw std::runtime_error("original Lua/UI assertion");}
[[noreturn]] static void unusedHostService(){throw std::logic_error("Script fixture unexpectedly requested a rendering/input service");}
class Host final:public UI::C_UiInterface {
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
static Host host;

namespace UI { int luaopen_buttons(lua_State*); extern C_UiInterface* g_pManager; }
class Finder final : public UI::C_UIBaseGameDataFinder {
public:
    Finder(): C_UIBaseGameDataFinder("FIX_") {}
    const WCHAR* GetVariableText(const char*) override { return u"authored item"; }
};
static void script(lua_State* state,const char* code){
    if(luaL_dostring(state,code)){const std::string error=lua_tostring(state,-1);lua_pop(state,1);throw std::runtime_error(error);}
}
static int convert(lua_State* state){lua_pushnumber(state,Lua::ToU32Bits(state,1));return 1;}
static std::vector<U8> pack() {
    const std::array<std::string,1> names{{"UIEnums.lua"}};
    const std::array<std::string,1> scripts{{"UIEnums={};"}};
    std::array<std::array<S32,4>,1> entries{};
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
int main(){try{
    g_pCoreAssertFunction=failAssert;
    static_assert(sizeof(lua_Integer)==8,"Host Lua integer must remain LP64");
    UI::g_pManager=&host;auto data=pack();UI::C_UILua ui;UI::g_pUILua=&ui;ui.Init(false,"",data.data());lua_State* state=ui.GetState();require(state,"Cannot create actual Lua interpreter");
    struct LuaCleanup { UI::C_UILua& ui; ~LuaCleanup(){ui.End();UI::g_pUILua=nullptr;UI::g_pManager=nullptr;} } luaCleanup{ui};
    const U32 ids[]={0,1,0x7fffffffU,0x80000000U,4112414174U,0xffffffffU};
    for(U32 id:ids){
        Lua::PushInteger32(state,id);
        const S64 expected=id<=0x7fffffffU?S64(id):S64(id)-4294967296LL;
        require(lua_tonumber(state,-1)==expected,"PC32 ID producer signedness mismatch");
        require(Lua::ToU32Bits(state,-1)==id,"PC32 ID bits failed negative roundtrip");lua_pop(state,1);
        lua_pushnumber(state,id);require(Lua::ToU32Bits(state,-1)==id,"Positive legacy alias failed roundtrip");lua_pop(state,1);
    }
    U32 out=123;require(!Lua::NumberToU32Bits(std::numeric_limits<F64>::quiet_NaN(),out)&&out==123,"NaN changed out parameter");
    require(!Lua::NumberToU32Bits(std::numeric_limits<F64>::infinity(),out)&&out==123,"Infinity changed out parameter");
    require(Lua::NumberToU32Bits(-4294967297.75,out)&&out==0xffffffffU,"Defined truncation/modulo changed");
    lua_pushcfunction(state,convert);lua_setglobal(state,"convert32");
    script(state,"assert(convert32('-182553122')==4112414174);assert(convert32('no number')==0);assert(not pcall(convert32,0/0));assert(not pcall(convert32,math.huge))");
    lua_newtable(state);Lua::AddDataMember(state,"Route",U32(4112414174U));Lua::AddDataMember(state,"float",F64(0.375));
    void* address=state;require(reinterpret_cast<std::uintptr_t>(address)>0xffffffffULL,"Fixture address did not exercise LP64");
    Lua::AddDataMember(state,"pointer",address);lua_getfield(state,-1,"pointer");require(lua_touserdata(state,-1)==address,"Native pointer truncated at ID boundary");lua_pop(state,1);
    U32 bits=0;S32 signedBits=0;Lua::GetDataMember(state,"Route",bits);Lua::GetDataMember(state,"Route",signedBits);
    require(bits==4112414174U&&signedBits==-182553122,"Actual table reader lost ID bits");
    bits=77;Lua::GetDataMember(state,"missing",bits,true);require(bits==77,"Optional table member changed absent semantics");
    Lua::AddDataMember(state,"string","123");Lua::GetDataMember(state,"string",bits,true);require(bits==77,"Safe table member coerced string");
    Lua::GetDataMember(state,"string",bits,false);require(bits==123,"Unsafe numeric string coercion changed");lua_setglobal(state,"level");
    lua_newtable(state);lua_newtable(state);Lua::AddDataMember(state,"id",U32(4112414174U));lua_rawseti(state,-2,1);lua_setglobal(state,"routes");
    script(state,"function id_result() return -182553122 end;function float_result() return 0.375 end");
    require(Lua::Call(state,"id_result",">u",&bits)&&bits==4112414174U,"Lua::Call U32 result lost negative ID bits");
    F64 floating=0;require(Lua::Call(state,"float_result",">d",&floating)&&floating==0.375,"Floating call result changed");
    script(state,"function nan_result() return 0/0 end;function inf_result() return math.huge end;function mixed_result() return 123,0/0 end");
    const int stackHeight=lua_gettop(state);bits=77;signedBits=88;
    require(!Lua::Call(state,"nan_result",">u",&bits)&&bits==77&&lua_gettop(state)==stackHeight,"NaN return changed output or corrupted stack");
    require(!Lua::Call(state,"inf_result",">i",&signedBits)&&signedBits==88&&lua_gettop(state)==stackHeight,"Infinity return changed signed output or corrupted stack");
    U32 otherBits=99;require(!Lua::Call(state,"mixed_result",">uu",&bits,&otherBits)&&bits==77&&otherBits==99,"Invalid integer result partially changed outputs");
    require(Lua::Call(state,"id_result",">u",&bits)&&bits==4112414174U&&lua_gettop(state)==stackHeight,"Finite retry after invalid return failed");
    Finder finder;UI::C_UIDataFinder::Instance().AddGameDataFinder(&finder);
    struct GadgetCleanup { ~GadgetCleanup(){
        auto& screens=UI::C_ScreenControlManager::Instance();
        for(S32 i=0;i<screens.Count();++i)screens.Get(i).Exit();
        screens.Exit();UI::gButtons.ExitCreator();UI::C_UIDataFinder::Instance().Cleanup();
    } } gadgetCleanup;
    UI::gButtons.InitCreator(UI::E_UIButtonType_EndMarker,false);UI::gButtons.Register<UI::UIVariableSpinner>();UI::gButtons.Register<UI::UISlider>();
    auto& screens=UI::C_ScreenControlManager::Instance();require(screens.Add()==0,"Unexpected first context");screens.Context(0);
    auto* gadget=UI::gButtons.AddButton(static_cast<UI::E_UIButtonType>(UI::UIVariableSpinner::TypeID()),0);require(gadget,"Original spinner factory failed");
    lua_pushnumber(state,gadget->m_ID.Number());lua_setglobal(state,"spinner");UI::luaopen_buttons(state);lua_pop(state,1);
    script(state,"UIButtons.AddItem(spinner,-1,'FIX_SENTINEL',false);UIButtons.AddItem(spinner,level.Route,'FIX_ROUTE',false);UIButtons.SetSelection(spinner,level.Route);assert(UIButtons.GetSelection(spinner)==level.Route);local found=false;for _,r in ipairs(routes) do if r.id==UIButtons.GetSelection(spinner) then found=true end end;assert(found,'actual menu route equality failed');UIButtons.SetSelection(spinner,4112414174);assert(UIButtons.GetSelection(spinner)==-182553122);UIButtons.SetSelection(spinner,-1);assert(UIButtons.GetSelection(spinner)==-1)");
    auto* selection=static_cast<UI::BaseUIVariableSelect*>(gadget);
    require(!selection->SetCurrentItemByValue(std::numeric_limits<F64>::infinity())&&selection->GetCurrentItemValue(0)==-1,"Nonfinite state-free selection changed real gadget");
    require(!selection->SetCurrentItemByValue(std::numeric_limits<F64>::quiet_NaN())&&selection->GetCurrentItemValue(0)==-1,"NaN selection changed real gadget");
    script(state,"assert(level.float==0.375);assert(UIButtons.GetSelectionIndex(spinner)==0)");
    auto* slider=UI::gButtons.AddButton(static_cast<UI::E_UIButtonType>(UI::UISlider::TypeID()),0);
    lua_pushnumber(state,slider->m_ID.Number());lua_setglobal(state,"slider");
    script(state,"UIButtons.SetSliderValue(slider,0.375);assert(UIButtons.GetSliderValue(slider)==0.375)");
    std::cout<<"Original Lua/UI spinner PC32 IDs, signed sentinels, finite conversion, LP64 pointers and floating values PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
