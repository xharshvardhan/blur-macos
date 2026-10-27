// [Blur2PC] PC per-sampler LOD state for both native shader compiler paths.
#pragma once
#include <cstdint>
#include <string>
namespace BlurMac::SamplerLod {
struct Integration {
    std::string source;
    std::uint32_t buffer = 0xffffffffu;
    std::uint32_t gradientSamplers = 0;
    std::uint32_t unsupportedSamplers = 0;
};
// Binding masks describe the parsed shader ABI, never inferred texture names.
Integration Integrate(const std::string& source, const std::string& entry,
                      bool vertex, std::uint32_t samplers, std::uint32_t buffers);
}
