// [Blur2PC][macOS] Native diagnostics. No game bytecode is executed.
#include <pakfs/macos/NativePakArchive.h>
#include <pakfs/macos/NativePakFile.h>
#include <display/device/metal/MetalBootstrap.h>
#include <CommonCrypto/CommonDigest.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

static std::string quote(const std::string& text) {
    std::ostringstream result;
    result << '"';
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') result << '\\' << c;
        else if (c < 32 || c >= 127) result << "\\u00" << std::hex << std::setfill('0') << std::setw(2) << unsigned(c);
        else result << c;
    }
    result << '"';
    return result.str();
}

int main(int argc, char** argv) {
    try {
        if (argc >= 2 && std::string(argv[1]) == "--metal-test" && argc <= 3) {
            auto result = Blur::Mac::runMetalProbe(argc == 3 ? argv[2] : "");
            std::cout << "{\"metal\":\"passed\",\"device\":" << quote(result.device)
                << ",\"width\":" << result.width << ",\"height\":" << result.height
                << ",\"center_rgb\":[" << result.red << ',' << result.green << ',' << result.blue << "]}\n";
            return 0;
        }
        if (argc == 3 && std::string(argv[1]) == "--data-root") {
            std::cout << Blur::Mac::inspectDataRoot(argv[2]);
            return 0;
        }
        if ((argc == 3 || (argc == 5 && std::string(argv[3]) == "--read")) && std::string(argv[1]) == "--pak") {
            Blur::Mac::NativePakArchive pak(argv[2]);
            std::ostringstream report;
            report << "{\"version\":" << pak.version() << ",\"codec\":" << pak.codec()
                << ",\"entries\":" << pak.entries().size()
                << ",\"secure_entries\":" << (pak.secureEntries() ? "true" : "false")
                << ",\"fat_authenticity_verified\":false";
            if (argc == 5) {
                // Read through the real engine interface, rather than stopping
                // at the standalone decoder's returned vector.
                auto file = Blur::Mac::openPakFile(pak, argv[4]);
                std::vector<unsigned char> content(static_cast<size_t>(file->Length()));
                if (!file->Read(content.data(), static_cast<U32>(content.size())) ||
                    file->GetPosition() != content.size())
                    throw std::runtime_error("engine archive stream read failed");
                unsigned char digest[CC_SHA256_DIGEST_LENGTH];
                CC_SHA256(content.data(), static_cast<CC_LONG>(content.size()), digest);
                std::ostringstream hash;
                for (unsigned char c : digest) hash << std::hex << std::setfill('0') << std::setw(2) << unsigned(c);
                report << ",\"read_bytes\":" << content.size() << ",\"sha256\":" << quote(hash.str())
                    << ",\"engine_stream\":true";
            }
            std::cout << report.str() << "}\n";
            return 0;
        }
        std::cerr << "Native Blur bring-up, not the playable game.\n"
            << "Usage: blur_mac_probe --metal-test [verification.png]\n"
            << "       blur_mac_probe --pak archive.pak [--read /path/in/archive]\n"
            << "       blur_mac_probe --data-root /path/to/Blur\n";
        return argc == 2 && std::string(argv[1]) == "--help" ? 0 : 2;
    } catch (const std::exception& error) {
        std::cerr << "Native bootstrap error: " << error.what() << '\n';
        return 1;
    }
}
