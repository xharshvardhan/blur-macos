// [Blur2PC][macOS] Bridge decoded PAK contents to the actual engine stream API.
#pragma once
#include <pakfs/macos/NativePakArchive.h>
#include <system/serialise/macos/NativeFile.h>

namespace Blur::Mac {
inline std::unique_ptr<Sys::I_File> openPakFile(const NativePakArchive& archive,
                                              const std::string& entry) {
    // Archive.read retains codec, integrity and allocation-limit checks.
    // FAT RSA authenticity is still unverified; not a trusted game mount.
    return Sys::NativeFile::FromMemory(archive.read(entry), "pak:" + entry);
}
}
