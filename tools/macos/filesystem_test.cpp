// [Blur2PC][macOS] Existing C_FileSystem/default-filesystem and packed serializers.
#include <core/Core.h>
#include <core/Maths.h>
#include <core/containers.h>
#include <system/serialise.h>
#include <system/serialise/BinarySerialiseAutoEndian.h>
#include <system/serialise/macos/NativeFile.h>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
#include <unistd.h>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
struct Scratch {
    std::filesystem::path original, directory;
    explicit Scratch(const char* parent) : original(std::filesystem::current_path()) {
        std::string pattern = (std::filesystem::path(parent) / "fs-fixture-XXXXXX").string();
        require(mkdtemp(pattern.data()), "cannot create filesystem fixture directory");
        directory = pattern;
    }
    ~Scratch() {
        std::error_code error;
        std::filesystem::current_path(original, error);
        std::filesystem::remove_all(directory, error);
    }
};
template<class Endian> static void writeRecord(Sys::I_File& file) {
    Sys::BinarySerialiser<Endian> stream(file);
    stream << U8(0x7f) << U16(0x1234) << S32(-17) << F32(1.5f)
           << U64(0x0123456789abcdefULL) << F64(-2.25) << char('Z');
    file.Flush();
}
template<class Endian> static void readRecord(Sys::I_File& file) {
    Sys::BinaryDeserialiser<Endian> stream(file);
    U8 byte; U16 word; S32 signedWord; F32 single; U64 wide; F64 real; char character;
    stream >> byte >> word >> signedWord >> single >> wide >> real >> character;
    require(byte == 0x7f && word == 0x1234 && signedWord == -17 && single == 1.5f &&
            wide == 0x0123456789abcdefULL && real == -2.25 && character == 'Z',
            "packed scalar record round-trip changed values");
}
static void files(const std::filesystem::path& root) {
    namespace fs = std::filesystem;
    using namespace Sys;
    fs::create_directories(root / "MiXeD" / "SubDir");
    std::ofstream(root / "MiXeD" / "SubDir" / "Retail.Asset", std::ios::binary) << "asset";
    auto& native = C_FileSystem::Instance();
    // Same default filesystem installation used by C_CoreSystems at startup.
    SetDefaultFileSystem(&native);
    I_FileSystem& system = *GetDefaultFileSystem();
    require(system.SetWorkingDirectory(root.string().c_str()), "engine working-directory setup failed");
    require(std::strcmp(system.GetWorkingDirectory().Chars(), root.c_str()) == 0,
            "native working directory lost real OS case");
    require(GetLocalDirToken() == '/' && GetNotLocalDirToken() == '\\', "Darwin engine path separators incorrect");
    require(!std::strcmp(C_FilePath("MiXeD\\SubDir\\Retail.Asset").Chars(), "mixed/subdir/retail.asset"),
            "engine canonical key format changed");
    require(system.FileExists("MIXED/subdir/retail.asset"), "canonical path did not resolve existing disk spelling");
    require(fs::path(system.GetSystemFilePath("mixed/subdir/retail.asset").Chars()).filename() == "Retail.Asset",
            "physical path failed to retain original asset case");
    const std::vector<U8> golden = {0x7f,0x34,0x12,0xef,0xff,0xff,0xff,0x00,0x00,0xc0,0x3f,
        0xef,0xcd,0xab,0x89,0x67,0x45,0x23,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0xc0,0x5a};
    // [Blur2PC] Auto-endian profile fields are packed at odd offsets too. Verify
    // exact independent PC bytes and both endian modes through buffered/unbuffered paths.
    for(bool swap:{false,true})for(U32 buffer:{0U,1U,7U,4096U}) {
        const C_OpenParams params(buffer,false,false);
        {
            std::unique_ptr<I_File> file(system.Open("auto.bin",E_FileAccess_Write,params));
            require(bool(file),"Cannot create auto-endian scalar fixture");file->SetEndianSwapFlag(swap);
            BinarySerialiserAutoEndian stream(*file);
            stream<<U8(0x7f)<<U16(0x1234)<<S32(-17)<<F32(1.5f)<<U64(0x0123456789abcdefULL)<<F64(-2.25)<<char('Z');
        }
        std::unique_ptr<I_File> file(system.Open("auto.bin",E_FileAccess_Read,params));
        require(file&&file->Length()==golden.size(),"Auto-endian writer added padding or lost data");
        auto expected=golden;
        if(swap){size_t at=0;for(size_t bytes:{1U,2U,4U,4U,8U,8U,1U}){std::reverse(expected.begin()+at,expected.begin()+at+bytes);at+=bytes;}}
        std::vector<U8> actual(expected.size());require(file->Read(actual.data(),actual.size())&&actual==expected,"Auto-endian scalar bytes differ from PC format");
        file->SetPosition(0);file->SetEndianSwapFlag(swap);BinaryDeserialiserAutoEndian stream(*file);
        U8 byte;U16 word;S32 signedWord;F32 single;U64 wide;F64 real;char character;
        stream>>byte>>word>>signedWord>>single>>wide>>real>>character;
        require(byte==0x7f&&word==0x1234&&signedWord==-17&&single==1.5f&&wide==0x0123456789abcdefULL&&real==-2.25&&character=='Z',"Packed auto-endian scalar values changed");
    }
    for (U32 buffer : {0U,1U,7U,4096U}) {
        const C_OpenParams params(buffer, false, false);
        {
            std::unique_ptr<I_File> file(system.Open("mixed\\subdir\\fixture.bin", E_FileAccess_Write, params));
            require(file && file->IsWriting(), "C_FileSystem did not open native write stream");
            writeRecord<Ut::NativeToLittleEndian>(*file);
        }
        {
            std::unique_ptr<I_File> file(system.Open("mixed/subdir/fixture.bin", E_FileAccess_Read, params));
            require(file && !file->IsWriting() && file->Length() == golden.size(), "native filesystem read length/mode failed");
            std::vector<U8> bytes(golden.size());
            require(file->Read(bytes.data(), bytes.size()) && bytes == golden, "serialized bytes disagree with independent PC fixture");
            file->SetPosition(0);
            readRecord<Ut::LittleEndianToNative>(*file);
        }
    }
    {
        std::unique_ptr<I_File> file(system.Open("big.bin", E_FileAccess_Write, C_OpenParams(3,false,false)));
        writeRecord<Ut::NativeToBigEndian>(*file);
        file.reset(system.Open("big.bin", E_FileAccess_Read, C_OpenParams(5,false,false)));
        readRecord<Ut::BigEndianToNative>(*file);
        auto shortFile = NativeFile::FromMemory({1,2,3}, "truncated");
        BinaryDeserialiser<Ut::LittleEndianToNative> stream(*shortFile);
        U64 value = 42;
        bool failed = false;
        try { stream >> value; } catch (const std::runtime_error&) { failed = true; }
        require(failed && value == 42, "truncated scalar silently changed destination");
    }
    require(!system.Open("missing.bin", E_FileAccess_Read, C_OpenParams(64,false,false)), "missing engine file falsely opened");
    C_FileInfo info;
    require(system.FileInfo("mixed/subdir/retail.asset", &info) && info.m_FileType == E_FileType_File &&
            info.m_FileSize == 5 && !std::strcmp(info.m_originalFilename.Chars(), "Retail.Asset"),
            "native file metadata lost original name/type/size");
    constexpr Time::C_Time stamp = 1709210096123456ULL;
    require(system.FileTime("mixed/subdir/fixture.bin", stamp) &&
            system.FileInfo("mixed/subdir/fixture.bin", &info) && info.m_FileTime == stamp,
            "native file timestamps lost epoch/microseconds");
    require(system.FileAccess("mixed/subdir/fixture.bin", E_FileAccess_Read) &&
            system.FileInfo("mixed/subdir/fixture.bin", &info) && !(info.m_FileAccess & E_FileAccess_Write),
            "native read-only permission update failed");
    require(system.FileAccess("mixed/subdir/fixture.bin", E_FileAccess_Read | E_FileAccess_Write), "cannot restore fixture permissions");
    C_DirectoryInfo entries;
    require(system.DirectoryInfo("mixed/subdir", &entries) && entries.NumFiles() == 2, "directory enumeration failed");
    C_DirectoryInfo copy(entries);
    require(copy.NumFiles() == entries.NumFiles() && copy.FileInfo(0).m_FileName == entries.FileInfo(0).m_FileName,
            "original directory-info copy lost strings");
    require(!system.MoveFile("mixed/subdir/fixture.bin", "mixed/subdir/retail.asset") &&
            system.FileInfo("mixed/subdir/retail.asset", &info) && info.m_FileSize == 5,
            "native rename overwrote an existing destination");
    require(system.MoveFile("mixed/subdir/fixture.bin", "mixed/subdir/renamed.bin") &&
            !system.FileExists("mixed/subdir/fixture.bin"), "native rename failed");
    require(!system.DeleteFile("mixed/subdir"), "nonempty directory was recursively removed");
    require(system.CreateDirectory("one\\two\\three") && system.FileInfo("one/two/three", &info) &&
            info.m_FileType == E_FileType_Directory, "original recursive directory creation failed on Darwin");
    fs::create_directories(root / "Actual" / "Child");
    fs::create_directory_symlink(root / "Actual" / "Child", root / "Shortcut");
    require(fs::path(system.GetSystemFilePath("shortcut/..").Chars()) == root / "Actual",
            "native parent traversal ignored a directory symlink");
    require(system.DeleteFile("mixed/subdir/renamed.bin") && !system.FileExists("mixed/subdir/renamed.bin"),
            "native file delete failed");
    SetDefaultFileSystem(nullptr);
}
struct First { virtual ~First() = default; U64 first = 11; };
struct Second { virtual ~Second() = default; U64 second = 19; };
struct Derived : First, Second { U64 last = 31; };
static void singletonPointers() {
    Derived object;
    Second* base = &object;
    require(reinterpret_cast<INTPTR>(base) != reinterpret_cast<INTPTR>(&object), "multiple-inheritance offset fixture ineffective");
    auto* result = Ut::BaseToDerived<Second,Derived>::GetPointer(base);
    require(result == &object && result->last == 31, "native base-to-derived pointer adjustment failed");
}
int main(int argc, char** argv) {
    try {
        require(argc == 2, "test requires an ignored build directory");
        Scratch scratch(argv[1]);
        singletonPointers(); files(scratch.directory);
        std::cout << "Game C_FileSystem/default mount: paths, case, packed records, metadata, rename and directories passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
