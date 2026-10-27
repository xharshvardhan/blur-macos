// [Blur2PC] Native adapter around upstream MojoShader's D3D9 Metal profile.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace BlurMac::LegacyShader {
enum class Stage { Vertex, Pixel };
struct Translation {
    std::vector<std::uint8_t> blob;
    std::string source;
    std::string error;
};
// Accepts raw little-endian DXSO tokens, not a DXBC container or FX wrapper.
// Failure clears all executable output. Runtime integer/boolean uniforms and
// TEXBEM require backend state which the current native context does not own.
bool Translate(const void* bytecode, std::size_t size, Stage stage, Translation& result);
}
