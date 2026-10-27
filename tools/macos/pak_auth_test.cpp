// [Blur2PC][macOS] Exercise actual original SHA1, RSA-PSS, FAT and payload checks.
// Authored archives/signatures are independently produced by test_pak_auth.py.
#include <core/Core.h>
#include <core/Maths.h>
#include <core/containers.h>
#include <core/threads.h>
#include <system/serialise/C_FileSystem.h>
#include <system/io/ASync.h>
#include <system/serialise/FileSystemGlobals.h>
#include <system/scheduler/jobqueue.h>
#include <pakfs/PakFs.h>
#include <pakfs/PakGlobals.h>
#include <pakfs/impl/PakSha1.h>
#include <pakfs/impl/PakBuildHelpers.h>
#include <pakfs/impl/PakFile.h>
#include <pakfs/impl/PakFatEntry.h>
#include <pakfs/impl/PakIoMediator.h>
#include <pakfs/impl/PakUtil.h>
#include <pakfs/macos/NativePakArchive.h>
#include <Security/RSASecurity.h>
#include "../../code/amax/game/Game/PublicKey/amax_public_key.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
using Bytes=std::vector<U8>;
static void require(bool value,const char* why) {if(!value)throw std::runtime_error(why);}
static Bytes fileBytes(const std::filesystem::path& path) {
    std::ifstream stream(path,std::ios::binary);require(bool(stream),"authored fixture file missing");
    return {std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
}
static std::array<U8,20> digest(const char* hex) {
    std::array<U8,20> result{};for(int i=0;i<20;++i){unsigned value;require(std::sscanf(hex+i*2,"%2x",&value)==1,"invalid expected digest");result[i]=U8(value);}return result;
}
static void knownHashes() {
    for(auto vector: {std::pair<const char*,const char*>{"","da39a3ee5e6b4b0d3255bfef95601890afd80709"},
                     {"abc","a9993e364706816aba3e25717850c26c9cd0d89d"}}) {
        Pak::Sha1::C_HashContext context;require(context.Valid(),"native SHA1 initialization failed");
        for(const char* p=vector.first;*p;++p)require(context.UpdateHash(reinterpret_cast<const U8*>(p),1),"SHA1 byte update failed");
        std::array<U8,20> actual;actual.fill(0xa5);require(context.GetHash(actual.data())&&actual==digest(vector.second),"SHA1 empty/abc NIST vector changed");
        require(!context.Valid(),"finalized SHA1 state retained as valid");
    }
    Pak::Sha1::C_HashContext million;std::array<U8,1000> block;block.fill('a');
    for(int i=0;i<1000;++i)require(million.UpdateHash(block.data(),block.size()),"SHA1 block update failed");
    std::array<U8,20> actual;require(million.GetHash(actual.data())&&actual==digest("34aa973cd4c4daa4f61eeb2bdbad27316534016f"),"SHA1 million-a NIST vector changed");
    for(bool swap:{false,true}) {
        Pak::Sha1::C_HashContext context;require(context.UpdateHash(0x12345678U,swap),"SHA1 endian field update failed");
        require(context.GetHash(actual.data())&&actual==digest(swap?"9bce73d0c8b9eca4f24154f3bd3b8aa473b1c3a9":"c0c619003e6cc5c51c1802bf771c064e366950d8"),"SHA1 serialized U32 byte order changed");
    }
    { Pak::Sha1::C_HashContext writer(false,actual.data(),actual.size());require(writer.UpdateHash(reinterpret_cast<const U8*>("abc"),3),"writer SHA1 update failed"); }
    require(actual==digest("a9993e364706816aba3e25717850c26c9cd0d89d"),"writer destructor did not finalize actual SHA1");
    Pak::Sha1::C_HashContext expected(true,actual.data(),actual.size());require(expected.VerifyHash(actual.data(),actual.size()),"digest verifier rejected equal bytes");
    actual[19]^=1;require(!expected.VerifyHash(actual.data(),actual.size()),"digest verifier accepted altered tail byte");
}
struct CompressionScheduler {
    CompressionScheduler() {static const Scheduler::Policy workers[8]={5,5,5,5,5,5,5,5};Pak::SetPacketCompressionPolicy(Scheduler::CreatePolicy(workers));Scheduler::Init();Scheduler::SetGroupPoolSize(64);Scheduler::jqEnableWorkers(Scheduler::JQ_CORE_1|Scheduler::JQ_CORE_3);Scheduler::Start();}
    ~CompressionScheduler(){Scheduler::Shutdown();Pak::SetPacketCompressionPolicy(-1);}
};
static void checkArchive(const std::filesystem::path& archive,const std::filesystem::path& signature,const Bytes& publicKey,const char* entry,const Bytes* expected=nullptr) {
    Security::RSASecurity security(1024);security.InitKey(publicKey.data(),publicKey.size());security.InitSignature(signature.c_str());
    LogicalFs::C_PakFs pak;require(pak.OpenPak(&Sys::C_FileSystem::Instance(),archive.c_str(),&security),"RSA-authenticated actual engine mount failed");
    Blur::Mac::NativePakArchive independent(archive);const auto independentBytes=independent.read(entry);
    if(expected)require(independentBytes==*expected,"independent PAK reader differs from authored payload");
    std::unique_ptr<Sys::I_File> input(pak.Open(entry));require(input&&input->Length()==independentBytes.size(),"actual authenticated entry could not open");
    Bytes actual(independentBytes.size());require(input->Read(actual.data(),U32(actual.size()))&&actual==independentBytes,"actual payload verification/decompression changed bytes");
    input.reset();
    // A changed expected stored-payload digest must fail the original engine
    // verification routine, including v1 prefix and v2 known-length contracts.
    auto* state=pak.GetPak();auto* metadata=pak.GetPakFatEntryAccountingForSerialAccess(entry);
    require(metadata!=nullptr,"authenticated entry metadata missing");
    auto* secure=static_cast<const Pak::C_PakFatEntrySecure*>(metadata);
    std::array<U8,20> altered;std::copy_n(secure->fileHash,altered.size(),altered.begin());altered.back()^=1;
    Pak::Sha1::C_HashContext wrongExpected(true,altered.data(),altered.size());
    S64 storedLength=-1;
    if(state->pStoredSizes) {
        const auto stride=Pak::GetFatEntrySize(static_cast<Pak::E_FatEntryType>(state->header.fatEntryType));
        const auto index=(reinterpret_cast<const U8*>(metadata)-state->pFatEntries)/stride;storedLength=state->pStoredSizes[index];
    }
    require(!Pak::VerifyFileHash(&wrongExpected,state->pMediator,metadata->dataStartSector*state->pMediator->GetSectorSize(),storedLength),
            "original stored-payload verifier accepted a changed expected digest");
    pak.ClosePak();
}
int main(int argc,char** argv) {
    try {
        knownHashes();require(argc>=2,"authored fixture directory missing");const std::filesystem::path fixture=argv[1];
        require(IoInitialise(),"native I/O initialization failed");CompressionScheduler scheduler;Sys::SetDefaultFileSystem(&Sys::C_FileSystem::Instance());
        const auto key=fileBytes(fixture/"public.der");const auto payload=fileBytes(fixture/"payload.bin");
        for(int version:{1,2})for(int codec:{1,2,4}) {
            const auto stem="v"+std::to_string(version)+"-c"+std::to_string(codec);
            checkArchive(fixture/(stem+".pak"),fixture/(stem+".sig"),key,"/fixture/entry.bin",&payload);
            Security::RSASecurity security(1024);security.InitKey(key.data(),key.size());security.InitSignature((fixture/(stem+".sig")).c_str());
            auto hash=fileBytes(fixture/(stem+".sha1"));require(security.Verify(hash.data(),hash.size(),"authored FAT"),"OpenSSL RSA-PSS signature rejected");
            hash[19]^=1;require(!security.Verify(hash.data(),hash.size(),"deliberately changed authored FAT"),"native Debug RSA accepted changed FAT digest");
        }
        if(argc==3) {
            const std::filesystem::path cache=std::filesystem::path(argv[2])/"cache";const Bytes keyBytes(std::begin(g_amax_publicKey),std::end(g_amax_publicKey));
            checkArchive(cache/"gamedata.pak",cache/"gamedata.sig",keyBytes,"/gamedata/xt/ui.bin");
            // Boot archive entry names vary with game versions; verify the full
            // original RSA FAT contract by mounting it, without guessing a path.
            Security::RSASecurity security(1024);security.InitKey(keyBytes.data(),keyBytes.size());security.InitSignature((cache/"gamedata_boot.sig").c_str());
            LogicalFs::C_PakFs boot;require(boot.OpenPak(&Sys::C_FileSystem::Instance(),(cache/"gamedata_boot.pak").c_str(),&security),"retail boot RSA FAT rejected");boot.ClosePak();
            std::cout<<"Private retail gamedata/boot RSA FATs and ui.bin payload verification PASS\n";
        }
        Sys::SetDefaultFileSystem(nullptr);IoShutdown();
        std::cout<<"Actual SHA1 vectors and secure PAK v1/v2 raw/zlib/packet-zlib with RSA rejection PASS\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
