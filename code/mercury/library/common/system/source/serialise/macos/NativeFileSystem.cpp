// [Blur2PC][macOS] Included by C_FileSystem.cpp, implementing the actual game API.
#include "SystemPch.h"
#include <system/serialise/C_FileSystem.h>
#include <system/serialise/macos/NativeFile.h>
#include <system/serialise/macos/NativePath.h>
#include <system/io/ASync.h>
#include <system/io/macos/NativeIo.h>
#include <limits>
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <set>
#include <stdexcept>
#include <system_error>
#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace Sys {
namespace {
namespace fs = std::filesystem;
std::string fold(std::string value) {
    // Match C_FilePath's ASCII case folding, independently of user locale.
    for (char& c : value) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return value;
}
fs::path resolve(const C_FilePath& input) {
    const fs::path requested(input.Chars());
    fs::path current = requested.is_absolute() ? requested.root_path() : fs::current_path();
    // Engine keys are lowercase. Resolve against original disk spelling so a
    // case-sensitive APFS volume can still load Windows-authored game paths.
    for (const auto& part : requested.relative_path()) {
        const auto component = part.string();
        if (component.empty() || component == ".") continue;
        if (component == "..") {
            // Follow an existing symlink before walking to its parent, as the
            // operating system would when opening the original path.
            std::error_code error;
            const auto physical = fs::canonical(current, error);
            current = (error ? current : physical).parent_path();
            continue;
        }
        std::error_code error;
        fs::path match;
        if (fs::is_directory(current, error)) {
            for (const auto& entry : fs::directory_iterator(current)) {
                if (fold(entry.path().filename().string()) == fold(component)) {
                    if (!match.empty()) throw fs::filesystem_error("ambiguous engine path case", requested,
                        std::make_error_code(std::errc::file_exists));
                    match = entry.path();
                }
            }
        }
        current = match.empty() ? current / part : match;
    }
    return current.lexically_normal();
}
C_FilePath physicalPath(const fs::path& path) {
    const auto name = path.string();
    if (name.size() >= C_FilePath::kMaxPath) throw std::length_error("native path exceeds engine C_FilePath limit");
    C_FilePath result;
    result.SetFormatted(name.c_str()); // Preserve real OS case, as the API's escape hatch intends.
    return result;
}
bool infoFor(const fs::path& path, const C_FilePath& name, C_FileInfo* output) {
    if (!output) return false;
    output->Clear();
    output->m_FileName = name;
    struct stat info;
    if (stat(path.c_str(), &info)) return false;
    output->m_originalFilename = physicalPath(path.filename());
    output->m_FileType = S_ISDIR(info.st_mode) ? E_FileType_Directory :
                        (S_ISREG(info.st_mode) ? E_FileType_File : E_FileType_NotPresent);
    output->m_FileSize = static_cast<C_StreamSize>(info.st_size);
    output->m_FileAccess = ((info.st_mode & S_IRUSR) ? E_FileAccess_Read : 0) |
                          ((info.st_mode & S_IWUSR) ? E_FileAccess_Write : 0);
    output->m_FileTime = U64(info.st_mtimespec.tv_sec) * 1000000 + info.st_mtimespec.tv_nsec / 1000;
    return output->m_FileType != E_FileType_NotPresent;
}
}

std::filesystem::path NativeResolvePath(const C_FilePath& path) { return resolve(path); }

I_File* C_FileSystem::Open(const C_FilePath& name, E_FileAccess access, const C_OpenParams& params) {
    // [Blur2PC] Preserve the failed boundary instead of reporting every exception
    // as a missing file. Diagnostics must not query/change I/O routing on failure.
    const char* route = "selecting";
    const char* operation = "select-route";
    try {
        // Match the original default async read path once CoreSystems starts
        // I/O. Inspection tools and worker callbacks use synchronous native I/O
        // (waiting on the same worker queue would deadlock a callback).
        if (access == E_FileAccess_Read && IsNativeIoInitialised() && !IsNativeIoWorkerThread()) {
            route = "async";
            operation = "validate-async-buffer";
            const auto bytes = params.m_BufferSizeHint == C_OpenParams::kBufferSizeHintDefault
                             ? 128 * 1024 : params.m_BufferSizeHint;
            if (bytes > static_cast<U32>(std::numeric_limits<int>::max()))
                throw std::length_error("async buffer size exceeds engine integer range");
            operation = "allocate-async-stream";
            auto stream = std::make_unique<AsyncFile>();
            operation = "async-open";
            if (!stream->Open(name.Chars(), static_cast<int>(bytes), params.m_IsCritical, params.m_NumBuffers)) return nullptr;
            return stream.release();
        }
        route = "sync";
        const auto bytes = params.m_BufferSizeHint == C_OpenParams::kBufferSizeHintDefault
                         ? 64 * 1024 : params.m_BufferSizeHint;
        operation = "resolve-native-path";
        const auto path = resolve(name);
        operation = "native-open";
        return NativeFile::Open(path, access, bytes).release();
    } catch (const std::exception& error) {
        // [Blur2PC] Only system_error carries an OS/category code. Global errno
        // may be stale after allocation/validation errors; never attribute it.
        if (const auto* systemError = dynamic_cast<const std::system_error*>(&error))
            std::fprintf(stderr, "Native filesystem Open exception: operation=%s route=%s path=%s access=%u error=%s system_error_code=%d category=%s\n",
                operation, route, name.Chars(), unsigned(access), error.what(),
                systemError->code().value(), systemError->code().category().name());
        else
            std::fprintf(stderr, "Native filesystem Open exception: operation=%s route=%s path=%s access=%u error=%s\n",
                operation, route, name.Chars(), unsigned(access), error.what());
        FileOpenError(name.Chars(), access != E_FileAccess_Read, params.m_IsCritical);
        return nullptr;
    }
}
C_FileSystem::~C_FileSystem() = default;
bool C_FileSystem::FileExists(const C_FilePath& name) {
    try { return fs::exists(resolve(name)); } catch (const std::exception&) { return false; }
}
bool C_FileSystem::FileInfo(const C_FilePath& name, C_FileInfo* output) {
    try { return infoFor(resolve(name), name, output); }
    catch (const std::exception&) { if (output) output->Clear(); return false; }
}
bool C_FileSystem::FileAccess(const C_FilePath& name, U32 access) {
    try {
        const auto path = resolve(name);
        struct stat info;
        if (stat(path.c_str(), &info)) return false;
        // Keep execute/search and group/other bits meaningful on Darwin; map the
        // source's read/write controls onto the owning user's permission bits.
        mode_t mode = info.st_mode & ~(S_IRUSR | S_IWUSR);
        if (access & E_FileAccess_Read) mode |= S_IRUSR;
        if (access & E_FileAccess_Write) mode |= S_IWUSR;
        return chmod(path.c_str(), mode) == 0;
    } catch (const std::exception&) { return false; }
}
bool C_FileSystem::FileTime(const C_FilePath& name, const Time::C_Time& time) {
    try {
        timespec times[2]{};
        times[0].tv_sec = times[1].tv_sec = time / 1000000;
        times[0].tv_nsec = times[1].tv_nsec = (time % 1000000) * 1000;
        return utimensat(AT_FDCWD, resolve(name).c_str(), times, 0) == 0;
    } catch (const std::exception&) { return false; }
}
bool C_FileSystem::DeleteFile(const C_FilePath& name) {
    try { return fs::remove(resolve(name)); } catch (const std::exception&) { return false; }
}
bool C_FileSystem::MoveFile(const C_FilePath& from, const C_FilePath& to) {
    try {
        // Match the original Windows rename behavior: existing destinations
        // must fail without silently overwriting a save or another asset.
        return renamex_np(resolve(from).c_str(), resolve(to).c_str(), RENAME_EXCL) == 0;
    } catch (const std::exception&) { return false; }
}
bool C_FileSystem::DirectoryInfo(const C_FilePath& name, C_DirectoryInfo* output) {
    if (!output) return false;
    output->FileInfoArray().Clear();
    try {
        const auto path = resolve(name);
        if (!fs::is_directory(path)) return false;
        std::set<std::string> names;
        for (const auto& entry : fs::directory_iterator(path)) {
            const auto basename = entry.path().filename().string();
            if (!names.insert(fold(basename)).second)
                throw std::runtime_error("directory has ambiguous engine path case");
            C_FileInfo info;
            if (infoFor(entry.path(), name + "/" + basename.c_str(), &info)) output->AddFileInfo(info);
        }
        return true;
    } catch (const std::exception&) { output->FileInfoArray().Clear(); return false; }
}
bool C_FileSystem::CreateSingleDirectory(const C_FilePath& name) {
    try { return fs::create_directory(resolve(name)); } catch (const std::exception&) { return false; }
}
bool C_FileSystem::SetWorkingDirectory(const C_FilePath& name) {
    try { fs::current_path(resolve(name)); return true; } catch (const std::exception&) { return false; }
}
C_FilePath C_FileSystem::GetWorkingDirectory() const { return _GetWorkingDirectory(); }
C_FilePath C_FileSystem::_GetWorkingDirectory() const { return physicalPath(fs::current_path()); }
C_FilePath C_FileSystem::GetSystemFilePath(const C_FilePath& name) const { return physicalPath(resolve(name)); }

// The original directory-info container methods live with the old backend.
// Retain their actual Btl container behavior for callers of the native class.
const U32 C_DirectoryInfo::NumFiles() { return U32(m_fileInfo.Size()); }
C_FileInfo& C_DirectoryInfo::FileInfo(U32 index) { return m_fileInfo[index]; }
C_DirectoryInfo::~C_DirectoryInfo() = default;
C_DirectoryInfo::C_DirectoryInfo() = default;
C_DirectoryInfo::C_DirectoryInfo(const C_DirectoryInfo& rhs) { *this = rhs; }
const C_DirectoryInfo& C_DirectoryInfo::operator=(const C_DirectoryInfo& rhs) {
    m_fileInfo = rhs.m_fileInfo; return *this;
}
void C_DirectoryInfo::SetNumFiles(int count) { m_fileInfo.Reserve(count); }
void C_DirectoryInfo::AddFileInfo(const C_FileInfo& info) { m_fileInfo.PushBack(info); }
FileInfoArr& C_DirectoryInfo::FileInfoArray() { return m_fileInfo; }
const FileInfoArr& C_DirectoryInfo::FileInfoArray() const { return m_fileInfo; }
}
