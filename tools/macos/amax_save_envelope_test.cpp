// [Blur2PC] Exercise the actual Amax compressed-save interface and native file
// worker with an authored serializer payload. This is a format-boundary test,
// not a serialized career profile or proof of gameplay save compatibility.
#include "../../code/amax/game/GameLogic/stdafx.h"
#include <ui/Game/SaveSystem/SaveSystem.h>
#include <system/serialise/BinarySerialiseAutoEndian.h>
#include <system/serialise/C_MemoryStream.h>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
#include <unistd.h>

static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
namespace EnvelopeFixture {
std::array<U8,65536> payload{};
U32 saves=0,loads=0;bool failSave=false;
int Version(){return 1378;}
}
static bool IsLoadableSaveGameVersion(int version){return version==1378;}
namespace Game {
// Test-local serialization callbacks isolate the original envelope from the
// initialized game's Lua, career, social and RaceBook state. No game serializer
// is replaced in production or claimed to have been validated by this fixture.
static bool SaveGameImpl(Sys::BinarySerialiserAutoEndian& serial){
    ++EnvelopeFixture::saves;
    if(EnvelopeFixture::failSave)return false;
    serial<<U8(1)<<U32(1378);
    return serial.Write(EnvelopeFixture::payload.data(),EnvelopeFixture::payload.size());
}
static bool LoadGameImpl(Sys::BinaryDeserialiserAutoEndian& serial){
    U8 writing=1;U32 marker=0;serial>>writing>>marker;
    std::array<U8,EnvelopeFixture::payload.size()> actual{};++EnvelopeFixture::loads;
    return serial.Read(actual.data(),actual.size())&&writing==0&&marker==1378&&actual==EnvelopeFixture::payload;
}
}
// A distinct fixture type avoids ODR collisions with the production class while
// compiling every tested constructor/Setup/Save/Load body directly from source.
// Its log-only version query describes the authored payload, avoiding unrelated
// whole-game singleton lifetimes in this independent file-format fixture.
#define C_SaveInfoInterfaceProfile C_SaveEnvelopeFixtureInterface
#define GetLoadingSaveGameVersion EnvelopeFixture::Version
#include "../../code/amax/game/GameLogic/Lua/SaveInfoInterfaces.inl"
#undef GetLoadingSaveGameVersion
#undef C_SaveInfoInterfaceProfile

template<class Poll>static void wait(Poll poll){
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(!poll()){require(std::chrono::steady_clock::now()<deadline,"Native save worker timed out");std::this_thread::yield();}
}
static U32 little32(const U8* p){return U32(p[0])|(U32(p[1])<<8)|(U32(p[2])<<16)|(U32(p[3])<<24);}
int main(int argc,char** argv){try{
    require(argc==2,"Provide an ignored scratch parent");
    std::string scratch=(std::filesystem::path(argv[1])/"amax-envelope-XXXXXX").string();
    require(mkdtemp(scratch.data()),"Cannot create isolated envelope directory");
    struct Cleanup{std::filesystem::path path;~Cleanup(){Sys::SetDefaultFileSystem(nullptr);std::error_code e;std::filesystem::remove_all(path,e);}} cleanup{scratch};
    require(!setenv("BLUR2PC_SAVE_ROOT",scratch.c_str(),1),"Cannot isolate native saves");
    auto& fs=Sys::C_FileSystem::Instance();Sys::SetDefaultFileSystem(&fs);
    require(fs.SetWorkingDirectory(scratch.c_str()),"Cannot configure scratch filesystem");
    for(U32 i=0;i<EnvelopeFixture::payload.size();++i)EnvelopeFixture::payload[i]=U8(i%17);
    UI::C_SaveEnvelopeFixtureInterface record;
    require(record.m_SaveRoot=="SAVE"&&record.m_EnumerationFileExtension=="save","Native profile directory/type diverged from PC");
    record.Setup(false);
    require(record.m_FileInfo.Size()==1&&record.m_FileInfo[0].m_FileName=="BLURSAVE","Native filename diverged from PC");
    auto& system=UI::C_SaveSystem::Instance();system.SaveInfoInterface(&record);
    require(system.StartAsyncSave(0),"Original Amax save worker failed to start");wait([&]{return system.ContinueAsyncSave();});
    require(system.GetSaveErrorCode()==UI::E_UiGameProfile_Error_Okay&&EnvelopeFixture::saves==1,"Original envelope did not save successfully");
    const auto file=std::filesystem::path(scratch)/"SAVE/BLURSAVE";
    std::ifstream input(file,std::ios::binary);std::vector<U8> bytes((std::istreambuf_iterator<char>(input)),{});
    require(bytes.size()>12&&little32(bytes.data())==0xABCDE337,"Compressed header magic/endian changed");
    require(little32(bytes.data()+4)==EnvelopeFixture::payload.size()+5,"Uncompressed serializer length changed");
    require(little32(bytes.data()+8)==bytes.size()-12,"Compressed length/header boundary changed");
    record.Setup(true);
    require(system.StartAsyncLoad(0),"Original Amax load worker failed to start");wait([&]{return system.ContinueAsyncLoad();});
    require(system.GetLoadErrorCode()==UI::E_UiGameProfile_Error_Okay&&EnvelopeFixture::loads==1,"Actual compressed Amax envelope did not round-trip");
    // The existing uncompressed path remains accepted, using the same authored
    // payload and serializer framing rather than a second envelope implementation.
    Sys::C_MemoryStream raw(4096);Sys::BinarySerialiserAutoEndian writer(raw);
    writer<<U8(0)<<U32(1378);writer.Write(EnvelopeFixture::payload.data(),EnvelopeFixture::payload.size());
    require(record.Load(0,raw.GetBufferPtr(),raw.GetBufferSize(),0)&&EnvelopeFixture::loads==2,"Existing uncompressed load path changed");
    // [Blur2PC] Invalid envelope framing/decode must never call the mutable game deserializer.
    const U32 acceptedLoads=EnvelopeFixture::loads;
    auto reject=[&](std::vector<U8> invalid){
        require(!record.Load(0,invalid.data(),static_cast<U32>(invalid.size()),0),"Malformed profile envelope was accepted");
        require(EnvelopeFixture::loads==acceptedLoads,"Malformed envelope reached mutable serializer callback");
    };
    for(U32 length=0;length<12;++length)reject(std::vector<U8>(bytes.begin(),bytes.begin()+length));
    auto set32=[](std::vector<U8>& data,U32 offset,U32 value){for(U32 i=0;i<4;++i)data[offset+i]=U8(value>>(8*i));};
    auto invalid=bytes;set32(invalid,4,0);reject(invalid);
    invalid=bytes;set32(invalid,4,0x80000000u);reject(invalid); // actual signed MemoryStream extent, not invented capacity
    invalid=bytes;set32(invalid,8,0);reject(invalid);
    invalid=bytes;set32(invalid,8,little32(bytes.data()+8)+1);reject(invalid);
    invalid=bytes;invalid.pop_back();reject(invalid);
    invalid=bytes;invalid.pop_back();set32(invalid,8,static_cast<U32>(invalid.size()-12));reject(invalid);
    invalid=bytes;invalid.push_back(0);reject(invalid);
    invalid=bytes;invalid[12]=0xff;reject(invalid); // genuine zlib decode error, no fatal fallback
    invalid=bytes;set32(invalid,4,little32(bytes.data()+4)+1);reject(invalid);
    invalid=bytes;set32(invalid,4,little32(bytes.data()+4)-1);reject(invalid);
    std::vector<U8> badRaw(raw.GetBufferPtr(),raw.GetBufferPtr()+raw.GetBufferSize());
    badRaw[0]=1;reject(badRaw);badRaw[0]=0;set32(badRaw,1,999);reject(badRaw);
    // [Blur2PC] Abandoned preparation is owned until reset/destruction; failed Setup never writes zero files.
    require(record.m_pStream==NULL,"Consumed record retained a prepared stream");
    record.Setup(false);require(record.m_pStream,"Preparation did not acquire a stream");
    record.Setup(true);require(!record.m_pStream,"Loading reset did not retire abandoned prepared stream");
    EnvelopeFixture::failSave=true;record.Setup(false);
    require(!record.m_pStream&&record.m_FileInfo.Empty(),"Failed preparation published a pending record");
    require(!system.StartAsyncSave(0)&&system.GetSaveErrorCode()==UI::E_UiGameProfile_Error_CannotWriteFile,"Failed preparation reported a successful zero-file save");
    EnvelopeFixture::failSave=false;record.Setup(false);
    require(record.m_pStream&&system.StartAsyncSave(0),"Failed preparation prevented a real retry");
    wait([&]{return system.ContinueAsyncSave();});require(system.GetSaveErrorCode()==UI::E_UiGameProfile_Error_Okay,"Prepared retry did not write original envelope");
    record.Setup(true);require(system.StartAsyncLoad(0),"Valid original full-flush retry failed to start");wait([&]{return system.ContinueAsyncLoad();});
    require(system.GetLoadErrorCode()==UI::E_UiGameProfile_Error_Okay&&EnvelopeFixture::loads==acceptedLoads+1,"Valid original full-flush retry did not deserialize");
    system.NativeQuiesceAndDetach(&record);require(!system.NativeHasInterface(&record),"Owned target remained borrowed after close");
    {UI::C_SaveEnvelopeFixtureInterface abandoned;abandoned.Setup(false);require(abandoned.m_pStream,"Pending destructor fixture never acquired a stream");}
    std::cout<<"Actual Amax compressed envelope/native async round-trip, malformed no-deserialize gates, failed preparation/retry and pending stream ownership PASS (authored payload; career serialization unverified)\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
