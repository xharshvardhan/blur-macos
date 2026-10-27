// [Blur2PC][macOS] Adapter based on PakRead.cpp, PakUtil.cpp,
// PakCompress{Raw,ZLib,PacketZLib}.cpp and VerifyFileHash in PakBuildHelpers.cpp.
#include <pakfs/macos/NativePakArchive.h>
#include <CommonCrypto/CommonDigest.h>
#include <zlib.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

// Compile the upstream disk-layout definitions directly. Never use host-sized
// long or pointers to deserialize PC game data on Apple's LP64 ABI.
using U8 = std::uint8_t;
using U32 = std::uint32_t;
using U64 = std::uint64_t;
#include <pakfs/impl/PakHeader.h>
static_assert(sizeof(Pak::C_PreHeader) == 12);
static_assert(sizeof(Pak::C_PakHeader) == 40);
static_assert(sizeof(Pak::C_PakHeaderV2) == 32);
static_assert(sizeof(Pak::C_PakFatEntryV2) == 40);

namespace Blur::Mac {
namespace {
using Bytes = std::vector<unsigned char>;
[[noreturn]] void fail(const std::string& message) { throw std::runtime_error(message); }
void range(std::uint64_t offset, std::uint64_t size, std::uint64_t total) {
    if (offset > total || size > total - offset) fail("Truncated pak or out-of-range offset");
}
std::uint32_t u32(const Bytes& bytes, std::size_t offset) {
    range(offset, 4, bytes.size());
    return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8) |
           (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
}
std::uint64_t u64(const Bytes& bytes, std::size_t offset) {
    return u32(bytes, offset) | (std::uint64_t(u32(bytes, offset + 4)) << 32);
}
Bytes load(const std::filesystem::path& path, std::uint64_t offset,
           std::uint64_t size, std::uint64_t fileBytes) {
    range(offset, size, fileBytes);
    std::ifstream stream(path, std::ios::binary);
    if (!stream) fail("Cannot open pak: " + path.string());
    stream.seekg(static_cast<std::streamoff>(offset));
    Bytes bytes(static_cast<std::size_t>(size));
    if (size && !stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size)))
        fail("Cannot read pak (file may have changed)");
    return bytes;
}
std::string normalise(std::string name, char separator) {
    for (char& c : name) {
        if (c == '/' || c == '\\') c = separator;
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    }
    if (name.empty() || name.front() != separator) name.insert(name.begin(), separator);
    return name;
}
Bytes inflateBlock(const unsigned char* data, std::size_t count,
                   std::size_t capacity, int windowBits) {
    if (count > std::numeric_limits<uInt>::max() || capacity > std::numeric_limits<uInt>::max())
        fail("Compressed block exceeds zlib limits");
    Bytes output(std::max<std::size_t>(capacity, 1));
    z_stream stream{};
    stream.next_in = const_cast<unsigned char*>(data);
    stream.avail_in = static_cast<uInt>(count);
    stream.next_out = output.data();
    stream.avail_out = static_cast<uInt>(output.size());
    if (inflateInit2(&stream, windowBits) != Z_OK) fail("zlib initialization failed");
    int result = inflate(&stream, Z_FINISH);
    auto actualBytes = stream.total_out;
    bool consumed = stream.avail_in == 0;
    inflateEnd(&stream);
    if (result != Z_STREAM_END || !consumed || actualBytes > capacity)
        fail("Invalid compressed block or decompressed length");
    output.resize(actualBytes);
    return output;
}
} // namespace

std::uint32_t NativePakArchive::pathCrc(const std::string& path, char separator) {
    // Pak::C_PakCrc32 uses reflected IEEE CRC with no final XOR.
    std::uint32_t value = 0xffffffff;
    for (unsigned char c : normalise(path, separator)) {
        value ^= c;
        for (int bit = 0; bit != 8; ++bit)
            value = (value >> 1) ^ ((value & 1) ? 0xedb88320u : 0);
    }
    return value;
}

NativePakArchive::NativePakArchive(const std::filesystem::path& path) : path_(path) {
    fileBytes_ = std::filesystem::file_size(path);
    const auto pre = load(path, 0, 16, fileBytes_);
    if (u32(pre, 0) != 0x50414b32) fail("Expected little-endian PC PAK2 archive");
    version_ = u32(pre, 4);
    sector_ = u32(pre, 8);
    if (version_ != 1 && version_ != 2) fail("Unsupported pak version");
    if (sector_ < 64 || sector_ > 32768 || (sector_ & (sector_ - 1))) fail("Invalid pak sector size");
    std::uint32_t headerBytes = version_ == 1 ? sizeof(Pak::C_PakHeader) : sizeof(Pak::C_PakHeaderV2);
    if (u32(pre, 12) != headerBytes) fail("Invalid pak header size");
    auto header = load(path, 16, headerBytes, fileBytes_);
    std::size_t fields = version_ == 1 ? 20 : 0;
    codec_ = header[fields];
    if (header[fields + 1] > 1) fail("Unsupported pak FAT entry type");
    secure_ = header[fields + 1] == 1;
    separator_ = static_cast<char>(header[fields + 2]);
    if (separator_ != '/' && separator_ != '\\') fail("Invalid pak path separator");
    std::uint64_t cursor = version_ == 1 ? 16 + headerBytes : sector_;
    const auto stringBytes = u32(load(path, cursor, 4, fileBytes_), 0);
    if (stringBytes > 32 * 1024 * 1024) fail("Pak string table exceeds 32 MiB bring-up limit");
    cursor += 4;
    auto strings = load(path, cursor, stringBytes, fileBytes_);
    // Exact rotating XOR used by upstream PakCryptDecrypt; this is metadata
    // decoding, not RSA signature verification or a game authentication path.
    static constexpr char cipher[] = "xVXo40j3@$%\\%`";
    static_assert(sizeof(cipher) - 1 == 14);
    for (std::size_t i = 0; i < strings.size(); ++i) strings[i] ^= cipher[(i + 1) % 14];
    cursor += stringBytes;
    cursor += sector_ - (cursor % sector_); // source consumes a full sector when already aligned
    auto fatHeader = load(path, cursor, 8, fileBytes_);
    cursor += 8;
    auto count = u32(fatHeader, 0), fatBytes = u32(fatHeader, 4);
    const std::uint32_t stride = (version_ == 1 ? 32 : 40) + (secure_ ? 24 : 0);
    if (count > 1000000 || std::uint64_t(count) * stride != fatBytes)
        fail("Invalid pak FAT length or entry count");
    const auto fat = load(path, cursor, fatBytes, fileBytes_);
    const auto dataMinimum = cursor + fatBytes;
    entries_.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        std::size_t offset = std::size_t(i) * stride;
        auto stringOffset = u32(fat, offset + 4);
        if (stringOffset >= strings.size()) fail("Pak filename offset outside string table");
        auto end = std::find(strings.begin() + stringOffset, strings.end(), 0);
        if (end == strings.end()) fail("Unterminated pak filename");
        PakEntry entry;
        entry.path.assign(strings.begin() + stringOffset, end);
        if (entry.path.size() > 4096) fail("Pak filename exceeds 4096-byte limit");
        entry.pathCrc = u32(fat, offset);
        entry.logicalBytes = u32(fat, offset + 12);
        auto startSector = u64(fat, offset + 24);
        entry.directory = startSector == std::numeric_limits<std::uint64_t>::max();
        auto parent = u32(fat, offset + 8);
        if (parent != 0xffffffff && (parent % stride || parent / stride >= count))
            fail("Invalid parent FAT offset");
        if (!entry.directory) {
            if (startSector > fileBytes_ / sector_) fail("Pak data sector outside archive");
            entry.dataOffset = startSector * sector_;
            if (entry.dataOffset < dataMinimum) fail("Pak payload overlaps metadata");
            if (version_ == 1) {
                entry.storedBytes = u32(load(path, entry.dataOffset, 4, fileBytes_), 0);
                entry.dataOffset += 4;
            } else entry.storedBytes = u32(fat, offset + 36);
            range(entry.dataOffset, entry.storedBytes, fileBytes_);
        }
        if (secure_) {
            entry.hasSha1 = true;
            auto hashOffset = offset + (version_ == 1 ? 32 : 40) + 4;
            std::copy_n(fat.begin() + hashOffset, 20, entry.sha1.begin());
        }
        entries_.push_back(std::move(entry));
    }
}

std::vector<unsigned char> NativePakArchive::read(const std::string& path, std::size_t maxBytes) const {
    const auto name = normalise(path, separator_);
    const auto crc = pathCrc(name, separator_);
    auto found = std::find_if(entries_.begin(), entries_.end(), [&](const PakEntry& entry) {
        return entry.pathCrc == crc && normalise(entry.path, separator_) == name;
    });
    if (found == entries_.end() || found->directory) fail("Pak entry not found: " + path);
    if (found->logicalBytes > maxBytes || found->storedBytes > maxBytes)
        fail("Pak entry exceeds configured read limit");
    auto input = load(path_, found->dataOffset, found->storedBytes, fileBytes_);
    if (found->hasSha1) {
        std::array<unsigned char, 20> actual{};
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        CC_SHA1(input.data(), static_cast<CC_LONG>(input.size()), actual.data());
#pragma clang diagnostic pop
        if (actual != found->sha1) fail("Pak stored-payload SHA-1 mismatch");
    }
    if (codec_ == 1) {
        if (input.size() != found->logicalBytes) fail("Raw pak entry length mismatch");
        return input;
    }
    if (codec_ == 2) {
        auto output = inflateBlock(input.data(), input.size(), found->logicalBytes, 15);
        if (output.size() != found->logicalBytes) fail("Zlib pak entry length mismatch");
        return output;
    }
    if (codec_ != 4) fail("Unsupported pak codec (XMem is not implemented on macOS)");
    if (u32(input, 0) != 65536) fail("Unsupported packet-zlib block size");
    std::size_t cursor = 4;
    Bytes output;
    output.reserve(found->logicalBytes);
    while (output.size() < found->logicalBytes) {
        auto size = u32(input, cursor);
        cursor += 4;
        bool raw = size == 0xffffffff;
        if (raw) size = 65536;
        range(cursor, size, input.size());
        Bytes packet = raw ? Bytes(input.begin() + cursor, input.begin() + cursor + size)
                           : inflateBlock(input.data() + cursor, size, 65536, -15);
        auto needed = std::min<std::size_t>(65536, found->logicalBytes - output.size());
        if (packet.size() < needed || packet.size() > 65536) fail("Packet-zlib output length mismatch");
        output.insert(output.end(), packet.begin(), packet.begin() + needed);
        cursor += size;
    }
    if (cursor != input.size()) fail("Unexpected trailing packet-zlib data");
    return output;
}

std::string inspectDataRoot(const std::filesystem::path& root) {
    if (!std::filesystem::is_directory(root)) fail("Game data root is not a directory");
    std::ostringstream report;
    // GameSystems.cpp: C_GameSystems::CreateFsLayers mounts these archives.
    for (const char* name : {"root.pak", "models.pak", "shaders-speed.pak", "gamedata.pak"}) {
        NativePakArchive archive(root / "cache" / name);
        report << name << ": version " << archive.version() << ", codec " << archive.codec()
               << ", " << archive.entries().size() << " entries";
        if (archive.secureEntries()) report << " (FAT authenticity not yet verified)";
        report << '\n';
    }
    report << "Archive indexes read. Engine, shaders, gameplay and LAN are not yet ported.\n";
    return report.str();
}
} // namespace Blur::Mac
