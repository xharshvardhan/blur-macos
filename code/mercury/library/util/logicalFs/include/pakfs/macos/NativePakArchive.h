// [Blur2PC][macOS] Read-only adapter for the existing Pak serializers.
#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Blur::Mac {
struct PakEntry {
    std::string path;
    std::uint32_t pathCrc = 0;
    std::uint32_t logicalBytes = 0;
    std::uint32_t storedBytes = 0;
    std::uint64_t dataOffset = 0;
    bool directory = false;
    bool hasSha1 = false;
    std::array<unsigned char, 20> sha1{};
};

class NativePakArchive {
public:
    explicit NativePakArchive(const std::filesystem::path& path);
    std::uint32_t version() const { return version_; }
    std::uint32_t sectorSize() const { return sector_; }
    unsigned codec() const { return codec_; }
    bool secureEntries() const { return secure_; }
    // Entry hashes are checked on read. RSA authenticity of the FAT is not
    // established by this bring-up adapter; it is not a trusted game mount.
    const std::vector<PakEntry>& entries() const { return entries_; }
    std::vector<unsigned char> read(const std::string& path,
                                   std::size_t maxBytes = 128 * 1024 * 1024) const;
    static std::uint32_t pathCrc(const std::string& path, char separator);
private:
    std::filesystem::path path_;
    std::uint64_t fileBytes_ = 0;
    std::uint32_t version_ = 0, sector_ = 0;
    unsigned codec_ = 0;
    char separator_ = '\\';
    bool secure_ = false;
    std::vector<PakEntry> entries_;
};
std::string inspectDataRoot(const std::filesystem::path& root);
} // namespace Blur::Mac
