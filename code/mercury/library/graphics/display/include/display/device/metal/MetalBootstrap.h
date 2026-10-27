// [Blur2PC][macOS] Native device bring-up; not yet a Display2 backend.
#pragma once
#include <filesystem>
#include <string>

namespace Blur::Mac {
struct MetalProbeResult {
    std::string device;
    unsigned width = 0, height = 0;
    unsigned red = 0, green = 0, blue = 0;
};
// Compiles MSL, submits an indexed draw, waits for completion and validates
// GPU pixel readback. Writes a PNG only when a nonempty path is supplied.
MetalProbeResult runMetalProbe(const std::filesystem::path& png = {});
}
