// [Blur2PC][macOS] Load native shader blobs and preserve engine register reflection.
#include "MetalNative.h"
#include <display2/context/MetalSamplerLod.h>
#include <display2/context/MetalLegacyShader.h>
#import <Foundation/Foundation.h>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace Display2 { namespace Metal {

static U32 word(const U8* data) {
    return U32(data[0]) | (U32(data[1]) << 8) | (U32(data[2]) << 16) | (U32(data[3]) << 24);
}
static std::shared_ptr<Shader> create(id<MTLDevice> device, U32 size, const U8* data, U32 stage, std::string* error) {
    @autoreleasepool {
        auto fail = [&](const char* reason) -> std::shared_ptr<Shader> { if (error) *error = reason; return {}; };
        if (error) error->clear();
        // [Blur2PC] Reuse the public D3D9 decoder for native game/effect payloads.
        // Keep source-built BMSL as-is and retain precise unsupported-shader errors.
        BlurMac::LegacyShader::Translation translated;
        if(device&&data&&size>=4&&size<=16*1024*1024&&((word(data)>>16)==0xfffe||(word(data)>>16)==0xffff)){
            if(!BlurMac::LegacyShader::Translate(data,size,stage?BlurMac::LegacyShader::Stage::Pixel:BlurMac::LegacyShader::Stage::Vertex,translated))return fail(translated.error.c_str());
            data=translated.blob.data();size=static_cast<U32>(translated.blob.size());
        }

        if (!device || !data || size < 40 || size > 16 * 1024 * 1024 || std::memcmp(data, "BMSL", 4) ||
            word(data + 4) != 1 || word(data + 8) != stage || word(data + 12) != size)
            return fail("Invalid native shader header or stage");
        const U32 metadata = word(data + 16), metadataSize = word(data + 20);
        const U32 source = word(data + 24), sourceSize = word(data + 28);
        const U32 entry = word(data + 32), entrySize = word(data + 36);
        if (metadata != 40 || !metadataSize || !sourceSize || !entrySize || entrySize > 256 ||
            U64(metadata) + metadataSize != source || U64(source) + sourceSize != entry || U64(entry) + entrySize != size)
            return fail("Native shader section bounds are invalid");
        NSError* nativeError = nil;
        NSDictionary* description = [NSJSONSerialization JSONObjectWithData:[NSData dataWithBytes:data + metadata length:metadataSize]
                                                                    options:0 error:&nativeError];
        if (![description isKindOfClass:[NSDictionary class]]) return fail("Native shader metadata is invalid");
        NSString* msl = [[NSString alloc] initWithBytes:data + source length:sourceSize encoding:NSUTF8StringEncoding];
        NSString* name = [[NSString alloc] initWithBytes:data + entry length:entrySize encoding:NSUTF8StringEncoding];
        if (!msl || !name) return fail("Native shader source/entry is not UTF-8");
        auto shader = std::make_shared<Shader>();
        NSDictionary* parameters = description[@"parameters"];
        NSArray* semantics = description[@"input_semantics"];
        if (![parameters isKindOfClass:[NSDictionary class]] || ![semantics isKindOfClass:[NSArray class]])
            return fail("Native shader reflection is missing");
        for (NSString* parameter in parameters) {
            NSArray* location = parameters[parameter];
            if (![parameter isKindOfClass:[NSString class]] || ![location isKindOfClass:[NSArray class]] || location.count != 2 ||
                ![location[0] isKindOfClass:[NSNumber class]] || ![location[1] isKindOfClass:[NSNumber class]] ||
                [location[0] unsignedLongLongValue] > 0xffff || ![location[1] unsignedLongLongValue] ||
                [location[1] unsignedLongLongValue] > 0x10000 - [location[0] unsignedLongLongValue])
                return fail("Native shader parameter range is invalid");
            shader->parameters.emplace(parameter.UTF8String, C_ShaderParam([location[0] unsignedIntValue], [location[1] unsignedIntValue]));
        }
        for (NSArray* semantic in semantics) {
            if (![semantic isKindOfClass:[NSArray class]] || semantic.count != 3 ||
                ![semantic[0] isKindOfClass:[NSNumber class]] || ![semantic[1] isKindOfClass:[NSNumber class]] ||
                ![semantic[2] isKindOfClass:[NSNumber class]] || [semantic[0] unsignedLongLongValue] > 13 ||
                [semantic[1] unsignedLongLongValue] > 255 || [semantic[2] unsignedLongLongValue] > 31)
                return fail("Native shader input semantic is invalid");
            C_ShaderSemantic input;
            input.m_Usage = [semantic[0] unsignedIntValue]; input.m_UsageIndex = [semantic[1] unsignedIntValue];
            shader->semantics.push_back(input);
            shader->locations.push_back([semantic[2] unsignedIntValue]);
        }
        NSDictionary* reflection=description[@"reflection"];
        if(![reflection isKindOfClass:[NSDictionary class]])return fail("Native shader binding reflection missing");
        for(NSString* key in @[@"images"])
            if([reflection[key] count])shader->unsupportedImages=true;
        for(NSString* key in @[@"textures",@"separate_images"]){
        NSArray* textures=reflection[key];
        if(textures&&![textures isKindOfClass:[NSArray class]])return fail("Invalid native texture bindings");
        for(NSDictionary* texture in textures){
            if(![texture isKindOfClass:[NSDictionary class]]||![texture[@"binding"] isKindOfClass:[NSNumber class]]||
               ![texture[@"type"] isKindOfClass:[NSString class]]||[texture[@"binding"] unsignedLongLongValue]>=16)
                return fail("Invalid native texture binding");
            NSString* kind=texture[@"type"];E_TextureType type=([kind isEqualToString:@"sampler2D"]||[kind isEqualToString:@"texture2D"])?E_TextureType_2D:
                ([kind isEqualToString:@"sampler3D"]||[kind isEqualToString:@"texture3D"])?E_TextureType_3D:
                ([kind isEqualToString:@"samplerCube"]||[kind isEqualToString:@"textureCube"])?E_TextureType_Cube:E_TextureType_Unknown;
            if(type==E_TextureType_Unknown)shader->unsupportedImages=true;
            shader->textures.push_back({[texture[@"binding"] unsignedIntValue],type});
            if([key isEqualToString:@"textures"])shader->samplers.push_back([texture[@"binding"] unsignedIntValue]);
        }
        }
        NSArray* samplers=reflection[@"separate_samplers"];
        if(samplers&&![samplers isKindOfClass:[NSArray class]])return fail("Invalid native sampler bindings");
        for(NSDictionary* sample in samplers){
            if(![sample isKindOfClass:[NSDictionary class]]||![sample[@"binding"] isKindOfClass:[NSNumber class]]||[sample[@"binding"] unsignedLongLongValue]>=16)return fail("Invalid native sampler binding");
            shader->samplers.push_back([sample[@"binding"] unsignedIntValue]);
        }
        NSArray* uniforms=reflection[@"ubos"];
        if(uniforms&&![uniforms isKindOfClass:[NSArray class]])return fail("Native uniform bindings are invalid");
        for(NSDictionary* uniform in uniforms) {
            if(![uniform isKindOfClass:[NSDictionary class]]||![uniform[@"binding"] isKindOfClass:[NSNumber class]]||
                ![uniform[@"block_size"] isKindOfClass:[NSNumber class]]||![uniform[@"name"] isKindOfClass:[NSString class]]||
                [uniform[@"binding"] unsignedLongLongValue]>30||[uniform[@"block_size"] unsignedLongLongValue]>1024*1024)
                return fail("Native uniform binding exceeds limits");
            U32 kind=[uniform[@"name"] isEqualToString:@"BlurPCAlphaTestState"]?1:
                     [uniform[@"name"] isEqualToString:@"BlurHDROutput"]?2:0;
            shader->uniforms.push_back({[uniform[@"binding"] unsignedIntValue],[uniform[@"block_size"] unsignedIntValue],kind});
        }
        // [Blur2PC] Both compiler paths consume the same current PC sampler
        // state, including explicit LOD instructions and stage-local bindings.
        U32 samplerMask=0,bufferMask=0;
        for(U32 binding:shader->samplers)samplerMask|=1u<<binding;
        for(const auto& uniform:shader->uniforms)if(uniform.slot<16)bufferMask|=1u<<uniform.slot;
        try {
            auto integrated=BlurMac::SamplerLod::Integrate(msl.UTF8String,name.UTF8String,stage==0,samplerMask,bufferMask);
            if(integrated.buffer!=0xffffffffu)shader->uniforms.push_back({integrated.buffer,16*2*sizeof(F32),3});
            shader->lodGradientSamplers=integrated.gradientSamplers;
            shader->lodUnsupportedSamplers=integrated.unsupportedSamplers;
            msl=[NSString stringWithUTF8String:integrated.source.c_str()];
        } catch(const std::exception& integrationError) { return fail(integrationError.what()); }
        MTLCompileOptions* options = [MTLCompileOptions new];
        options.languageVersion = MTLLanguageVersion2_3;
        shader->library = [device newLibraryWithSource:msl options:options error:&nativeError];
        if (!shader->library) return fail(nativeError.localizedDescription.UTF8String ?: "Metal compilation failed");
        shader->function = [shader->library newFunctionWithName:name];
        if (!shader->function || shader->function.functionType != (stage == 0 ? MTLFunctionTypeVertex : MTLFunctionTypeFragment))
            return fail("Metal entry point/stage mismatch");
        shader->blob.assign(data, data + size);
        return shader;
    }
}
} // namespace Metal
bool C_MetalDevice::CreateVertexShader(U32 size, const U8* data, U32 flags, C_VertexShader* shader, void* memory, std::string* error) {
    if (!m_State || !shader || shader->IsCreated() || memory) { if (error) *error = "Invalid vertex shader destination/device"; return false; }
    auto native = Metal::create(m_State->device, size, data, 0, error);
    if (!native) return false;
    shader->m_Shader = std::move(native); shader->m_Flags = flags; return true;
}
bool C_MetalDevice::CreatePixelShader(U32 size, const U8* data, U32 flags, C_PixelShader* shader, void* memory, std::string* error) {
    if (!m_State || !shader || shader->IsCreated() || memory) { if (error) *error = "Invalid pixel shader destination/device"; return false; }
    auto native = Metal::create(m_State->device, size, data, 1, error);
    if (!native) return false;
    shader->m_Shader = std::move(native); shader->m_Flags = flags; return true;
}
void C_MetalDevice::DestroyVertexShader(C_VertexShader* shader) {
    if (shader) { shader->m_Shader.reset(); shader->m_Flags = 0; shader->m_RegisterCount = 0; }
}
void C_MetalDevice::DestroyPixelShader(C_PixelShader* shader) {
    if (shader) { shader->m_Shader.reset(); shader->m_Flags = 0; shader->m_RegisterCount = 0; }
}
#define METAL_SHADER_METHODS(Type) \
    U32 Type::GetSize() const { return m_Shader ? U32(m_Shader->blob.size()) : 0; } \
    void* Type::GetAddress() const { return m_Shader ? m_Shader->blob.data() : nullptr; } \
    C_ShaderParam Type::GetParamFromName(const char* name) const { \
        if (m_Shader && name) { const auto found = m_Shader->parameters.find(name); \
            if (found != m_Shader->parameters.end()) return found->second; } \
        return {}; }
METAL_SHADER_METHODS(C_VertexShader)
METAL_SHADER_METHODS(C_PixelShader)
#undef METAL_SHADER_METHODS
// [Blur2PC] Keep shader reflection behind the public native shader interface.
U32 C_VertexShader::GetTextureType(U32 sampler) const {
    if (m_Shader) for (const auto& binding : m_Shader->textures)
        if (binding.slot == sampler) return U32(binding.type);
    return ~0U;
}
// [Blur2PC] Pixel FX bindings need the same declared sampler dimension as vertex bindings.
U32 C_PixelShader::GetTextureType(U32 sampler) const {
    if (m_Shader) for (const auto& binding : m_Shader->textures)
        if (binding.slot == sampler) return U32(binding.type);
    return ~0U;
}
bool C_VertexShader::GetSemantics(Btl::Vector<C_ShaderSemantic>& semantics) {
    if (!m_Shader) return false;
    for (const auto& input : m_Shader->semantics) semantics.PushBack(input);
    return true;
}
S32 C_VertexShader::GetInputLocation(U8 usage, U8 usageIndex) const {
    if (m_Shader) for (std::size_t i = 0; i < m_Shader->semantics.size(); ++i)
        if (m_Shader->semantics[i].m_Usage == usage && m_Shader->semantics[i].m_UsageIndex == usageIndex)
            return static_cast<S32>(m_Shader->locations[i]);
    return -1;
}
} // namespace Display2
