// [Blur2PC][macOS] Real UI owner rollback and original bundle/font/schema readers.
#include "../../code/amax/game/stdafx.h"
#include <Ui/UiManager.h>
#include <Game/Game.h>
#include <ui/Game/UILua.h>
#include <ui/Game/UILocaliser.h>
#include <ui/Game/UIXt.h>
#include <ui/Graphics/Font/Font.h>
#include <ui/Graphics/Font/UIBitmapFont.h>
#include <ui/Graphics/UITextureManager.h>
#include <display2/context/MetalLegacyShader.h>
#include <system/serialise/C_FileSystem.h>
#include <pakfs/macos/NativePakArchive.h>
#include <overlayfs/OverlayFs.h>
#include <cstdlib>
#include "../../code/mercury/library/common/xt2/source/Common/XtFlask.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
namespace UI { extern C_UiInterface* g_pManager; }
static void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
static bool failAssert(const char*,const char*,const char*,int){throw std::runtime_error("original UI resource assertion");}
using Bytes=std::vector<U8>;
template<class T> static void append(Bytes& bytes,T value){const auto* p=reinterpret_cast<const U8*>(&value);bytes.insert(bytes.end(),p,p+sizeof(T));}
template<class T> static void put(Bytes& bytes,size_t offset,T value){require(offset+sizeof(T)<=bytes.size(),"Authored bytes overflow");std::memcpy(bytes.data()+offset,&value,sizeof(T));}
static void write(const std::filesystem::path& root,const char* name,const Bytes& bytes){const auto path=root/name;std::filesystem::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());require(bool(out),"Authored UI file write failed");}
static Bytes bundle(){
    const std::string name="FIX_FONT";const U32 info=28+name.size()+1;Bytes b(20+info+16,0);
    put(b,0,U32(0x425a5232));b[4]=3;b[5]=2;put(b,6,U16(info));put(b,8,U16(28));put(b,12,U32(16));
    put(b,28,U16(1));put(b,32,U32(16));put(b,40,U16(6));put(b,42,U16(1));put(b,44,U16(2));put(b,46,U16(2));
    std::memcpy(b.data()+48,name.c_str(),name.size()+1);for(U32 i=0;i<16;++i)b[20+info+i]=U8(17+i);return b;
}
static Bytes font(int version){
    Bytes b;append(b,S32(version ? -version : 1));if(version)append(b,U32(1));
    const size_t names=b.size();b.resize(b.size()+256,0);std::memcpy(b.data()+names,"FIX_FONT",9);
    for(S32 value:{0,2,2,2,2})append(b,value); // Original language/dimensions/count/height.
    if(version>1){append(b,S32(0));append(b,S32(0));}else{append(b,F32(0));append(b,F32(0));}
    append(b,F32(0));append(b,F32(1));
    for(U16 glyph:{U16(0x03a9),U16('A')}){
        const auto start=b.size();b.resize(start+(version>2?24:28),0);put(b,start,glyph);
        if(version>2){put(b,start+6,U16(2));put(b,start+8,U16(2));put(b,start+16,F32(2));put(b,start+20,F32(2));}
        else {put(b,start+12,F32(1));put(b,start+16,F32(1));put(b,start+22,S16(2));put(b,start+24,S16(2));}
    }
    append(b,S32(1));append(b,U16(0x03a9));append(b,U16('A'));
    if(version>2)append(b,F32(-1));else {append(b,S8(-1));append(b,U8(0));}
    return b;
}
static U16 name(Bytes& names,const char* text){const U16 offset=names.size();names.insert(names.end(),text,text+std::strlen(text)+1);return offset;}
template<class T> static FlaskBlob blob(Bytes& bytes,const std::vector<T>& values){while(bytes.size()%4)bytes.push_back(0);FlaskBlob result{XtU32(bytes.size()),XtU32(values.size())};if(!values.empty()){auto* p=reinterpret_cast<const U8*>(values.data());bytes.insert(bytes.end(),p,p+values.size()*sizeof(T));}return result;}
static Bytes schema(){
    Bytes names;std::vector<FlaskType> types{{name(names,"f32"),0,0,kXtTbAtom,kXtF32,4},{name(names,"Vec2"),2,0,kXtTbStruct,kXtNotAtom,8}};
    std::vector<FlaskField> fields{{name(names,"vx"),0,0,kXtFbAtom,0},{name(names,"vy"),0,4,kXtFbAtom,0}};
    Bytes bytes(sizeof(FlaskHeader));FlaskHeader header{};header.tag=kXtFlaskTag;header.version=1;
    header.types=blob(bytes,types);header.bases=blob(bytes,std::vector<FlaskBase>{});header.fields=blob(bytes,fields);header.strings=blob(bytes,names);
    header.refs=blob(bytes,std::vector<FlaskRef>{{0x1234,1,0}});header.records=blob(bytes,std::vector<FlaskRecord>{{0,1,8,0}});header.components=blob(bytes,std::vector<FlaskComponent>{{1,1,kXtCbObject}});
    Bytes values;append(values,F32(3));append(values,F32(4));header.data=blob(bytes,values);put(bytes,0,header);return bytes;
}
int main(int argc,char** argv){try{
    require(argc==3,"Expected ignored scratch parent and source-built shader resource root");g_pCoreAssertFunction=failAssert;
    const auto root=std::filesystem::absolute(argv[1])/"ui-required";std::filesystem::remove_all(root);std::filesystem::create_directories(root);
    auto& fs=Sys::C_FileSystem::Instance();require(fs.SetWorkingDirectory(root.c_str()),"Cannot set UI scratch directory");
    LogicalFs::C_OverlayFs overlay;LogicalFs::C_OverlayLayer::C_Params layer;layer.flags.layerType=LogicalFs::C_OverlayLayer::E_Type_Overlay;
    overlay.AddLayer("/",&fs,"/",layer,LogicalFs::E_MediaLayer_0,"Physical fixture and source-built shaders");
    overlay.AddLayer("/ui",&fs,(root/"Ui").c_str(),layer,LogicalFs::E_MediaLayer_0,"Original rooted UI path contract");
    require(overlay.SetWorkingDirectory(root.c_str()),"Cannot set UI virtual working directory");Sys::SetDefaultFileSystem(&overlay);
    Display2::SetNativeShaderResourceRoot(std::filesystem::absolute(argv[2]).c_str());
    Game::C_Game game;Display::C_Device device;Display::C_DeviceParams parameters;parameters.m_Width=32;parameters.m_Height=24;
    require(device.Create(parameters)&&device.CreateVolatile(parameters),"UI transaction GPU initialization failed");
#if TRACK_DISPLAY2_RESOURCES
    const auto baselineTextures=device.GetDisplay2().GetTextureCount(),baselineVertices=device.GetDisplay2().GetVertexBufferCount();
#endif
    UI::C_UILocaliser::Instance().DetermineLanguageAndRegion(LOC_MASK(UI::E_UILanguage_English),UI::E_UILanguage_English);
    require(UI::C_UILocaliser::Instance().Region()==UI::E_UIRegion_Europe&&UI::C_UILocaliser::Instance().Language()==UI::E_UILanguage_English,"Native locale did not preserve its original initialized configured default");
    for(unsigned lifetime=0;lifetime<2;++lifetime){
        UI::C_UiManager unstarted(UI::C_UiManager::E_LaunchMode_Debug);require(!unstarted.NativeIsReady()&&!UI::g_pUILua,"Native constructor acquired UI services");
        bool rejected=false;try{unstarted.LoadFrontendTextureBundle();}catch(const std::runtime_error&){rejected=true;}require(rejected,"Unstarted UI acquired a frontend bundle");
    }
    auto failedOwner=[&](const char* expected){
        {UI::C_UiManager owner(UI::C_UiManager::E_LaunchMode_Debug);
         require(!owner.NativeInitialise()&&!owner.NativeIsReady(),"Missing required resource published UI Ready");
         require(std::strstr(owner.NativeInitialisationError(),expected),"Required resource diagnostic lost its filename");
         bool rejected=false;try{owner.RenderAA();}catch(const std::runtime_error&){rejected=true;}require(rejected,"Unready UI reached render");}
        require(!UI::g_pUILua,"Failed UI owner retained its actual Lua state");
#if TRACK_DISPLAY2_RESOURCES
        require(device.GetDisplay2().GetTextureCount()==baselineTextures&&device.GetDisplay2().GetVertexBufferCount()==baselineVertices,"Failed UI owner retained real GPU resources");
#endif
    };
    failedOwner("ui_common");write(root,"Ui/Data/ui_common.bpr",bundle());failedOwner("ui_font_eur");
    write(root,"Ui/Data/ui_font_eur.bpr",bundle());failedOwner("ui_font_eur.dat");
    Bytes invalid;append(invalid,U32(65));write(root,"Ui/Data/ui_font_eur.dat",invalid);failedOwner("ui_font_eur.dat");
    // Direct lower readers use an actual state/host/bundle rather than fake renderer callbacks.
    UI::C_UiInterface host;UI::g_pManager=&host;
    lua_State* state=luaL_newstate();require(state,"Actual font fixture Lua allocation failed");UI::gUITextureManager.LuaState(state);
    require(UI::gUITextureManager.AddResourceBundle("Ui/Data/ui_font_eur")!=UI::C_TextureManager::m_InvalidResourceIndex,"Authored original bundle failed GPU load");UI::gUITextureManager.Update();
    for(int version=0;version<=3;++version){
        const auto bytes=font(version);write(root,"font.dat",bytes);UI::C_Font loaded;
        require(loaded.NativeInitialise("font.dat"),"Original bitmap font layout failed");const auto* glyph=loaded.GetFont(0)->LookupGlyph(u'\u03a9',true);
        require(glyph&&glyph->GetCharacter()==u'\u03a9'&&loaded.GetFont(0)->LookupKerning(u'\u03a9',u'A')==-1,"Original UTF16 glyph/kerning contents changed");
        require(!loaded.NativeInitialise("font.dat")&&loaded.GetFont(0)->LookupGlyph(u'\u03a9',true)==glyph,"Owned font was overwritten");
        loaded.Destroy();require(loaded.NativeInitialise("font.dat"),"Font could not reacquire after retirement");
    }
    auto good=font(3),shortFont=good;shortFont.pop_back();auto badName=good;std::memset(badName.data()+8,'X',128);auto absent=good;std::memset(absent.data()+8,0,128);std::memcpy(absent.data()+8,"ABSENT",7);auto count=good;put(count,8+256+12,S32(-1));
    for(const auto& broken:{shortFont,badName,absent,count}){write(root,"bad.dat",broken);UI::C_Font loaded;require(!loaded.NativeInitialise("bad.dat"),"Invalid bitmap font reported Ready");write(root,"retry.dat",good);require(loaded.NativeInitialise("retry.dat"),"Failed font retained partial ownership");}
    UI::gUITextureManager.DestroyAll();
    if(const char* rootPak=std::getenv("BLUR_MAC_UI_ROOT_PAK")) {
        // Private original bytes remain in the ignored scratch directory.
        Blur::Mac::NativePakArchive archive(rootPak);
        for(const char* file:{"ui_common.bpr","ui_font_eur.bpr","ui_font_eur.dat"}) {
            const auto bytes=archive.read(std::string("ui/data/")+file);
            write(root,file,Bytes(bytes.begin(),bytes.end()));
        }
        require(UI::gUITextureManager.AddResourceBundle("ui_common")!=UI::C_TextureManager::m_InvalidResourceIndex&&
                UI::gUITextureManager.AddResourceBundle("ui_font_eur")!=UI::C_TextureManager::m_InvalidResourceIndex,"Original installed UI bundles failed");
        {UI::C_Font original;require(original.NativeInitialise("ui_font_eur.dat"),"Original installed bitmap font failed validated reader");}
        UI::gUITextureManager.DestroyAll();std::cout<<"Private original root.pak UI common/font bundles and Latin bitmap font parsed successfully\n";
    }
    UI::gUITextureManager.LuaState(NULL);lua_close(state);UI::g_pManager=NULL;
    require(XtInitialise(),"Actual schema registration failed");write(root,"ui-schema.bin",schema());
    {UI::C_UIXt loader;require(!loader.NativeLoadDefinitions("missing-schema.bin"),"Missing schema reported success");write(root,"bad-schema.bin",Bytes{1,2,3});require(!loader.NativeLoadDefinitions("bad-schema.bin"),"Invalid schema reported success");
     require(loader.NativeLoadDefinitions("ui-schema.bin"),"Actual authored PC pack failed UI load");loader.Cleanup();loader.Cleanup();require(loader.NativeLoadDefinitions("ui-schema.bin"),"UI schema could not reacquire");loader.Cleanup();}
    {XtCallbackContext context(0);auto* db=XtDbCreate(XtGetApplicationSchema(),16,&context);require(db,"Shared schema database failed");UI::C_UIXt loader;
     require(loader.NativeLoadDefinitions("ui-schema.bin",db),"UI schema rejected shared database");XtHandle<XtVec2> loaded(XtGetRecord(db,0x1234));require(loaded&&loaded->vx==3&&loaded->vy==4,"Actual schema fields were not published");loader.Cleanup();require(!loaded,"UI pack close retained record handles");XtDbDestroy(db);}
    XtShutdown();require(device.GetDisplay2().WaitIdle(),"UI resource GPU completion failed");device.DestroyVolatile();device.Destroy();Sys::SetDefaultFileSystem(NULL);
    std::cout<<"Required UI bundle/font/schema status, original Lua/PC readers, partial rollback and retry PASS; full actual UI/race remains separate\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
