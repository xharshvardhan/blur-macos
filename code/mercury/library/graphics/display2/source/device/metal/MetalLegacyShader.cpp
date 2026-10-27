// [Blur2PC] MojoShader owns instruction decoding and translation. This adapter
// connects its packed uniforms and semantic names to the existing BMSL ABI.
#include <display2/context/MetalLegacyShader.h>
#include <mojoshader.h>
#include <algorithm>
#include <cstring>
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace BlurMac::LegacyShader {
namespace {
using U32 = std::uint32_t;
static void require(bool value, const char* error) { if (!value) throw std::runtime_error(error); }
static std::string quote(const char* value) {
    std::string result = "\"";
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(value); *p; ++p) {
        if (*p == '"' || *p == '\\') { result += '\\'; result += char(*p); }
        else if (*p < 32) { const char digits[] = "0123456789abcdef"; result += "\\u00"; result += digits[*p >> 4]; result += digits[*p & 15]; }
        else result += char(*p);
    }
    return result + '"';
}
static void replace(std::string& source, const std::string& from, const std::string& to) {
    std::size_t at = 0;
    while ((at = source.find(from, at)) != std::string::npos) { source.replace(at, from.size(), to); at += to.size(); }
}
static void word(std::vector<std::uint8_t>& out, U32 value) {
    for (U32 i = 0; i < 4; ++i) out.push_back(std::uint8_t(value >> (i * 8)));
}
// Same calibrated PC target-precision conversion as alpha_test.py; buffer 13
// is populated by the actual native render context immediately before drawing.
constexpr const char* alphaHelper = R"(
struct BlurLegacyAlpha { uint enabled; uint function; float reference; uint format; };
float BlurLegacyAlphaConvert(float value, uint format) {
    if (format == 1) return as_type<float>(as_type<uint>(value) & 0xffffe000u);
    if (format < 8) return value;
    uint shift= format+4, scale=(1u<<format)-1u;
    uint fixedPoint=uint(min(floor(saturate(value)*float(1u<<shift)),float((1u<<shift)-1u)));
    uint bias=(1u<<(shift-1u))-1u;
    if (shift<=16u) return float((fixedPoint*scale+bias)>>shift);
    uint low=(fixedPoint&0xffffu)*scale;
    uint high=(fixedPoint>>16)*scale+(low>>16)+(bias>>16)+(((low&0xffffu)+(bias&0xffffu))>>16);
    return float(high>>(shift-16u));
}
void BlurLegacyAlphaTest(float alpha, constant BlurLegacyAlpha& state) {
    if (!state.enabled) return;
    alpha=BlurLegacyAlphaConvert(alpha,state.format);
    float reference=BlurLegacyAlphaConvert(state.reference,state.format);
    bool passed=false;
    switch (state.function) {
        case 1:break; case 2:passed=alpha<reference;break;
        case 3:passed=alpha==reference;break; case 4:passed=alpha<=reference;break;
        case 5:passed=alpha>reference;break; case 6:passed=alpha!=reference;break;
        case 7:passed=alpha>=reference;break; case 8:passed=true;break;
    }
    if (!passed) discard_fragment();
}
)";
}
bool Translate(const void* bytecode, std::size_t size, Stage stage, Translation& result) {
    result = {};
    try {
        require(bytecode && size >= 8 && size <= 16 * 1024 * 1024 && size % 4 == 0,
                "D3D9 shader bytecode has invalid size or alignment");
        // MojoShader expects a word-aligned input buffer on all platforms.
        std::vector<U32> aligned(size / 4); std::memcpy(aligned.data(), bytecode, size);
        std::unique_ptr<const MOJOSHADER_parseData, decltype(&MOJOSHADER_freeParseData)> parsed(
            MOJOSHADER_parse(MOJOSHADER_PROFILE_METAL, "blur_main",
                reinterpret_cast<const unsigned char*>(aligned.data()), unsigned(size),
                nullptr, 0, nullptr, 0, nullptr, nullptr, nullptr), MOJOSHADER_freeParseData);
        require(bool(parsed), "MojoShader allocation failed");
        if (parsed->error_count) {
            std::ostringstream errors; errors << "MojoShader: ";
            for (int i = 0; i < parsed->error_count; ++i) {
                if (i) errors << "; ";
                errors << (parsed->errors[i].error ? parsed->errors[i].error : "unknown parse error")
                       << " (byte " << parsed->errors[i].error_position << ')';
            }
            throw std::runtime_error(errors.str());
        }
        require(parsed->output && parsed->output_len > 0 && parsed->major_ver >= 1 && parsed->major_ver <= 3,
                "MojoShader produced no supported D3D9 shader");
        require(parsed->shader_type == (stage == Stage::Vertex ? MOJOSHADER_TYPE_VERTEX : MOJOSHADER_TYPE_PIXEL),
                "D3D9 shader stage does not match destination");
        std::string source(parsed->output, std::size_t(parsed->output_len));
        U32 registers = 0, packed = 0;
        std::ostringstream initialise;
        for (int i = 0; i < parsed->uniform_count; ++i) {
            const auto& uniform = parsed->uniforms[i];
            if (uniform.constant) continue;
            require(uniform.type == MOJOSHADER_UNIFORM_FLOAT,
                    "Runtime D3D9 integer/boolean registers require native backend register banks");
            const U32 count = uniform.array_count > 0 ? U32(uniform.array_count) : 1;
            require(uniform.index >= 0 && U32(uniform.index) < 65536 && count <= 65536 - U32(uniform.index),
                    "D3D9 floating-point register range exceeds the native bank");
            registers = std::max(registers, U32(uniform.index) + count);
            for (U32 member = 0; member < count; ++member) {
                const U32 slot = packed++, index = U32(uniform.index) + member;
                initialise << "    uniforms.uniforms_float4[" << slot << "] = blur_pc_registers[" << index << "];\n";
                // Relative-addressed arrays can mix runtime registers and DEF
                // values. The embedded bytecode value wins over a CPU upload.
                for (int j = 0; j < parsed->constant_count; ++j) {
                    const auto& constant = parsed->constants[j];
                    if (constant.type != MOJOSHADER_UNIFORM_FLOAT || constant.index != int(index)) continue;
                    initialise << "    uniforms.uniforms_float4[" << slot << "] = float4(";
                    for (U32 component = 0; component < 4; ++component) {
                        U32 bits; std::memcpy(&bits, &constant.value.f[component], sizeof(bits));
                        if (component) initialise << ',';
                        initialise << "as_type<float>(" << bits << "u)";
                    }
                    initialise << ");\n";
                }
            }
        }
        if (packed) {
            const std::string signature = "constant blur_main_Uniforms &uniforms [[buffer(0)]]";
            require(source.find(signature) != std::string::npos, "MojoShader uniform ABI changed");
            replace(source, signature, "constant float4* blur_pc_registers [[buffer(0)]]");
            std::smatch count;
            require(std::regex_search(source, count, std::regex(R"(float4 uniforms_float4\[(\d+)\];)")) &&
                    std::stoul(count[1]) == packed, "MojoShader packed uniform mapping does not match metadata");
            initialise.str("    blur_main_Uniforms uniforms;\n" + initialise.str());
        }
        std::ostringstream samplers;
        for (int i = 0; i < parsed->sampler_count; ++i) {
            const auto& sampler = parsed->samplers[i];
            require(!sampler.texbem, "D3D9 TEXBEM needs texture-stage bump state not exposed by the native context");
            require(sampler.index >= 0 && sampler.index < (stage == Stage::Vertex ? 4 : 16),
                    "D3D9 sampler slot exceeds native stage limits");
            const char* type = sampler.type == MOJOSHADER_SAMPLER_2D ? "sampler2D" :
                sampler.type == MOJOSHADER_SAMPLER_CUBE ? "samplerCube" :
                sampler.type == MOJOSHADER_SAMPLER_VOLUME ? "sampler3D" : nullptr;
            require(type, "D3D9 sampler dimension is unknown");
            if (i) samplers << ',';
            samplers << "{\"binding\":" << sampler.index << ",\"type\":" << quote(type) << '}';
        }
        // SPIRV-Cross source shaders use these same semantic locations.
        for (U32 i = 0; i < 16; ++i) replace(source, "user(texcoord" + std::to_string(i) + ")", "user(locn" + std::to_string(i) + ")");
        for (U32 i = 0; i < 2; ++i) replace(source, "user(color" + std::to_string(i) + ")", "user(locn" + std::to_string(16 + i) + ")");
        std::ostringstream semantics;
        if (stage == Stage::Vertex) for (int i = 0; i < parsed->attribute_count; ++i) {
            const auto& attribute = parsed->attributes[i]; std::cmatch location;
            require(attribute.name && attribute.usage >= 0 && attribute.usage <= 13 && attribute.index >= 0 && attribute.index <= 255 &&
                    std::regex_search(attribute.name, location, std::regex(R"((?:^|_)v(\d+)$)")) && std::stoul(location[1]) < 32,
                    "MojoShader vertex input metadata cannot map to the native declaration");
            if (i) semantics << ',';
            semantics << '[' << attribute.usage << ',' << attribute.index << ',' << std::stoul(location[1]) << ']';
        }
        std::ostringstream parameters;
        bool parameterComma = false;
        for (int i = 0; i < parsed->symbol_count; ++i) {
            const auto& symbol = parsed->symbols[i];
            if (symbol.register_set != MOJOSHADER_SYMREGSET_FLOAT4 && symbol.register_set != MOJOSHADER_SYMREGSET_SAMPLER) continue;
            require(symbol.name && symbol.register_count && symbol.register_index < 65536 && symbol.register_count <= 65536 - symbol.register_index,
                    "D3D9 CTAB parameter range is invalid");
            if (parameterComma) parameters << ','; parameterComma = true;
            parameters << quote(symbol.name) << ":[" << symbol.register_index << ',' << symbol.register_count << ']';
        }
        const auto entry = source.find(" blur_main (");
        require(entry != std::string::npos, "MojoShader entry point ABI changed");
        auto body = source.find(") {", entry);
        require(body != std::string::npos, "MojoShader entry point body missing");
        bool alpha = false;
        if (stage == Stage::Pixel) {
            std::smatch colour;
            if (std::regex_search(source, colour, std::regex(R"(float4\s+(\w+)\s+\[\[color\(0\)\]\];)"))) {
                alpha = true;
                const std::string colourName = colour[1].str();
                const auto open = source.find('(', entry);
                bool hasArguments = source.find_first_not_of(" \t\r\n", open + 1) < body;
                source.insert(body, std::string(hasArguments ? ",\n" : "") + "constant BlurLegacyAlpha& blur_alpha [[buffer(13)]]\n");
                body = source.find(") {", entry);
                replace(source, "return output;", "BlurLegacyAlphaTest(output." + colourName + ".a, blur_alpha);\n    return output;");
            }
        }
        source.insert(body + 3, "\n" + initialise.str());
        if (alpha) {
            const auto usingMetal = source.find("using namespace metal;");
            require(usingMetal != std::string::npos, "MojoShader Metal preamble missing");
            source.insert(usingMetal + std::strlen("using namespace metal;"), alphaHelper);
        }
        source = "#include <metal_stdlib>\n" + source;
        // The upstream array alias is a macro expression; the terminating
        // semicolon is not part of that expression when used inside arithmetic.
        source = std::regex_replace(source, std::regex(R"((#define \w+ uniforms\.uniforms_float4\[\d+\]);)"), "$1");
        std::ostringstream uniforms;
        if (packed) uniforms << "{\"name\":\"BlurPCLegacyFloatRegisters\",\"binding\":0,\"block_size\":" << registers * 16 << '}';
        if (alpha) uniforms << (packed ? "," : "") << "{\"name\":\"BlurPCAlphaTestState\",\"binding\":13,\"block_size\":16}";
        const std::string metadata = "{\"parameters\":{" + parameters.str() + "},\"input_semantics\":[" + semantics.str() +
            "],\"reflection\":{\"textures\":[" + samplers.str() + "],\"ubos\":[" + uniforms.str() + "]}}";
        const std::string function = "blur_main";
        require(metadata.size() + source.size() + function.size() + 40 <= 16 * 1024 * 1024,
                "Translated shader exceeds the native blob size limit");
        const U32 sourceAt = 40 + U32(metadata.size()), entryAt = sourceAt + U32(source.size());
        result.blob.insert(result.blob.end(), {'B','M','S','L'});
        for (U32 value : {1u, stage == Stage::Vertex ? 0u : 1u, entryAt + U32(function.size()), 40u,
                U32(metadata.size()), sourceAt, U32(source.size()), entryAt, U32(function.size())}) word(result.blob, value);
        result.blob.insert(result.blob.end(), metadata.begin(), metadata.end());
        result.blob.insert(result.blob.end(), source.begin(), source.end());
        result.blob.insert(result.blob.end(), function.begin(), function.end());
        result.source = std::move(source);
        return true;
    } catch (const std::exception& error) { result = {}; result.error = error.what(); return false; }
}
}
