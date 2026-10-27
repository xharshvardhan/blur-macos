// [Blur2PC] Authored DXSO tokens translated by actual MojoShader and executed
// through the existing native Metal device. No retail shader/assets required.
#include <display2/context/MetalLegacyShader.h>
#include <display2/device/MetalDevice.h>
#include <display2/context/Context.h>
#include <display2/device/BaseDevice.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace Display2;
using BlurMac::LegacyShader::Stage;
using BlurMac::LegacyShader::Translation;
static void require(bool value, const char* error) { if (!value) throw std::runtime_error(error); }
static Translation translate(const std::vector<U32>& words, Stage stage) {
    Translation result;
    if (!BlurMac::LegacyShader::Translate(words.data(), words.size()*4, stage, result)) throw std::runtime_error(result.error);
    return result;
}
static std::vector<U32> vertex(U32 model) {
    if (model == 1) return {0xfffe0101, 0x00000002, 0xc00f0000, 0x90e40000, 0xa0e4000b, 0xffff};
    if (model == 2) return {0xfffe0200, 0x0200001f, 0x80000000, 0x900f0000,
        0x03000002, 0xc00f0000, 0x90e40000, 0xa0e4000b, 0xffff};
    return {0xfffe0300, 0x0200001f, 0x80000000, 0x900f0000,
        0x0200001f, 0x80000000, 0xe00f0000,
        0x03000002, 0xe00f0000, 0x90e40000, 0xa0e4000b, 0xffff};
}
static std::vector<U32> pixel(U32 model) {
    if (model == 1) return {0xffff0101, 0x00000001, 0x800f0000, 0xa0e40003, 0xffff};
    return {model == 2 ? 0xffff0200u : 0xffff0300u, 0x02000001, 0x800f0800, 0xa0e40025, 0xffff};
}
static std::vector<U32> withFloatTable(const std::vector<U32>& shader, U32 index, U32 count) {
    // D3DXSHADER_CONSTANTTABLE/CONSTANTINFO/TYPEINFO with offsets from the
    // CTAB payload, matching the original D3D9 binary-format authority.
    std::array<U32,24> table{};
    table[0]=28;table[1]=72;table[2]=shader[0];table[3]=1;table[4]=28;table[6]=86;
    table[7]=48;table[8]=(index<<16)|2;table[9]=count;table[10]=56;
    std::memcpy(reinterpret_cast<U8*>(table.data())+48,"Tint",5);
    table[14]=0x00030001;table[15]=0x00040001;table[16]=count;
    std::memcpy(reinterpret_cast<U8*>(table.data())+72,"Blur authored",14);
    std::memcpy(reinterpret_cast<U8*>(table.data())+86,(shader[0]>>16)==0xfffe?"vs_3_0":"ps_2_0",7);
    std::vector<U32> result{shader[0],0x0019fffe,0x42415443};
    result.insert(result.end(),table.begin(),table.end());result.insert(result.end(),shader.begin()+1,shader.end());return result;
}
int main() { try {
    C_MetalDevice device; require(device.Create(), "Native Metal device unavailable");
    auto& context = device.GetContext();
    C_Surface surface; C_VertexBuffer buffer; C_VertexDeclaration declaration;
    require(device.CreateSurface(32,32,E_Flags_Colour,E_SurfaceFormat_A8R8G8B8,E_Multisample_None,nullptr,&surface), "Legacy shader render target failed");
    require(device.CreateVertexBuffer(4*4*sizeof(F32),0,&buffer), "Legacy shader vertex allocation failed");
    const F32 positions[] = {-1,1,.5,1, 1,1,.5,1, -1,-1,.5,1, 1,-1,.5,1};
    auto bytes=buffer.Lock(); require(bytes, "Legacy shader vertex upload failed"); std::memcpy(bytes,positions,sizeof(positions));buffer.Unlock();
    C_VertexFormat format; format.AddElement(0,0,E_VertexElementType_F32_4,E_VertexElementUsage_Position,0);
    require(device.CreateVertexDeclaration(format,&declaration), "Legacy shader declaration failed");
    context.SetColourSurface(0,&surface); context.SetDepthStencilSurface(nullptr);
    context.SetVertexBuffer(0,&buffer,0,4*sizeof(F32),&declaration);
    context.SetCullMode(E_CullMode_None);context.SetDepthEnable(E_Bool_False);context.SetBlendEnable(E_Bool_False);
    auto read = [&](std::array<U8,4> expected) {
        auto mapped=surface.LockRect(16); require(mapped.pBits, "Legacy shader GPU readback failed");
        bool correct=true;
        for(U32 y=0;y<32;++y)for(U32 x=0;x<32;++x)for(U32 c=0;c<4;++c)
            if(std::abs(int(mapped.pBits[y*mapped.pitch+x*4+c])-int(expected[c]))>1)correct=false;
        surface.UnlockRect();require(correct,"MojoShader GPU result differs from independent original-register reference");
    };
    for (U32 model : {1u,2u,3u}) {
        auto v=translate(vertex(model),Stage::Vertex),p=translate(model==2?withFloatTable(pixel(model),37,1):pixel(model),Stage::Pixel);
        require(v.source.find("blur_pc_registers[11]")!=std::string::npos && p.source.find(model==1?"blur_pc_registers[3]":"blur_pc_registers[37]")!=std::string::npos,
                "Sparse D3D9 registers were compacted without preserving their PC upload indices");
        C_VertexShader vs;C_PixelShader ps;std::string error;
        require(device.CreateVertexShader(v.blob.size(),v.blob.data(),0,&vs,nullptr,&error),error.c_str());
        require(device.CreatePixelShader(p.blob.size(),p.blob.data(),0,&ps,nullptr,&error),error.c_str());
        require(vs.GetInputLocation(0,0)==0,"D3D9 vertex semantic metadata changed");
        if(model==2) require(ps.GetParamFromName("Tint").m_RegisterIndex==37&&ps.GetParamFromName("Tint").m_RegisterCount==1,"Original CTAB parameter name/register reflection changed");
        context.SetVertexShader(&vs);context.SetPixelShader(&ps);
        F32 offset[]={0,0,0,0};context.SetVertexShaderConstantF(11,offset,1);
        const F32 colour[]={.25f,.5f,.75f,1};context.SetPixelShaderConstantF(model==1?3:37,colour,1);
        auto draw=[&] {context.BeginScene();context.Clear(E_Clear_Colour,0xff123456,1,0);context.DrawPrimitive(E_PrimitiveType_TriangleStrip,0,4);context.EndScene();};
        draw();read({191,128,64,255});
        // The fixed-function alpha comparison is still the actual render state.
        context.SetAlphaTestEnable(E_Bool_True);context.SetAlphaTestFunc(E_CmpFunc_Never);
        draw();read({0x56,0x34,0x12,255});context.SetAlphaTestEnable(E_Bool_False);
        device.DestroyVertexShader(&vs);device.DestroyPixelShader(&ps);
    }
    // Legacy register oT0 and ps_2_0 t0 must link to the same source-shader
    // TEXCOORD0 location, and sampler s3 must keep its original slot.
    auto textureVertex=vertex(2);textureVertex.pop_back();
    textureVertex.insert(textureVertex.end(),{0x02000001,0xe00f0000,0xa0e40007,0xffff});
    const std::vector<U32> texturePixel={0xffff0200,
        0x0200001f,0x80000000,0xb00f0000, // dcl t0
        0x0200001f,0x90000000,0xa00f0803, // dcl_2d s3
        0x03000042,0x800f0000,0xb0e40000,0xa0e40803, // texld r0,t0,s3
        0x03000005,0x800f0800,0x80e40000,0xa0e40025,0xffff}; // mul oC0,r0,c37
    auto tv=translate(textureVertex,Stage::Vertex),tp=translate(texturePixel,Stage::Pixel);
    C_VertexShader textureVS;C_PixelShader texturePS;std::string textureError;
    require(device.CreateVertexShader(tv.blob.size(),tv.blob.data(),0,&textureVS,nullptr,&textureError),textureError.c_str());
    require(device.CreatePixelShader(tp.blob.size(),tp.blob.data(),0,&texturePS,nullptr,&textureError),textureError.c_str());
    C_Texture texture;require(device.CreateTexture2D(1,1,1,0,E_TextureFormat_A8R8G8B8,&texture),"Legacy sampler texture allocation failed");
    auto texels=texture.LockRect2D(0);require(texels.pBits,"Legacy sampler upload failed");
    const U32 texel=0xff804020;std::memcpy(texels.pBits,&texel,4);texture.UnlockRect2D(0);
    context.SetTexture(3,texture.GetGPUTexture());C_SamplerState sampler;sampler.SetTextureAddress(E_TextureAddress_Clamp);context.SetSamplerState(3,sampler);
    context.SetVertexShader(&textureVS);context.SetPixelShader(&texturePS);
    const F32 uv[]={.5,.5,0,0},multiply[]={.5,.5,.5,1};
    context.SetVertexShaderConstantF(7,uv,1);context.SetPixelShaderConstantF(37,multiply,1);
    context.BeginScene();context.Clear(E_Clear_Colour,0xff123456,1,0);context.DrawPrimitive(E_PrimitiveType_TriangleStrip,0,4);context.EndScene();read({16,32,64,255});
    device.DestroyTexture(&texture);device.DestroyVertexShader(&textureVS);device.DestroyPixelShader(&texturePS);

    // Mixed relative-addressed arrays must retain embedded DEF values even
    // when the CPU register bank contains a different value for that register.
    const std::vector<U32> relativeVertex={0xfffe0300,
        0x0200001f,0x80000000,0x900f0000,0x0200001f,0x80000000,0xe00f0000,
        0x05000051,0xa00f0026,0,0,0,0, // def c38,0,0,0,0
        0x0200002e,0xb0010000,0xa0000000, // mova a0.x,c0.x
        0x04000002,0xe00f0000,0x90e40000,0xa0e42025,0xb0000000,0xffff}; // add o0,v0,c37[a0.x]
    auto rv=translate(withFloatTable(relativeVertex,37,2),Stage::Vertex),rp=translate(pixel(2),Stage::Pixel);
    C_VertexShader relativeVS;C_PixelShader relativePS;std::string relativeError;
    require(device.CreateVertexShader(rv.blob.size(),rv.blob.data(),0,&relativeVS,nullptr,&relativeError),relativeError.c_str());
    require(device.CreatePixelShader(rp.blob.size(),rp.blob.data(),0,&relativePS,nullptr,&relativeError),relativeError.c_str());
    context.SetVertexShader(&relativeVS);context.SetPixelShader(&relativePS);
    const F32 arrayValues[]={0,0,0,0,5,0,0,0},arrayColour[]={.25,.5,.75,1};
    context.SetVertexShaderConstantF(37,arrayValues,2);context.SetPixelShaderConstantF(37,arrayColour,1);
    for(F32 relativeIndex:{0.f,1.f}) {
        const F32 index[]={relativeIndex,0,0,0};context.SetVertexShaderConstantF(0,index,1);
        context.BeginScene();context.Clear(E_Clear_Colour,0xff123456,1,0);context.DrawPrimitive(E_PrimitiveType_TriangleStrip,0,4);context.EndScene();read({191,128,64,255});
    }
    device.DestroyVertexShader(&relativeVS);device.DestroyPixelShader(&relativePS);

    Translation invalid;const U32 bad[]={0x43425844,0};
    require(!BlurMac::LegacyShader::Translate(bad,sizeof(bad),Stage::Vertex,invalid)&&invalid.blob.empty()&&!invalid.error.empty(),"Invalid foreign bytecode produced an executable shader");
    auto valid=pixel(2);
    require(!BlurMac::LegacyShader::Translate(valid.data(),valid.size()*4,Stage::Vertex,invalid)&&invalid.blob.empty(),"Mismatched D3D9 stage accepted");
    // if b0; mov oC0,c37; endif requires a runtime boolean bank. Until the
    // backend exposes that state, translation must fail without executable data.
    const U32 boolean[]={0xffff0300,0x01000028,0xe0e40800,0x02000001,0x800f0800,0xa0e40025,0x0000002b,0xffff};
    require(!BlurMac::LegacyShader::Translate(boolean,sizeof(boolean),Stage::Pixel,invalid)&&invalid.blob.empty()&&invalid.error.find("integer/boolean")!=std::string::npos,"Unsupported runtime boolean state produced executable data or failed to reach the backend-state check");
    device.DestroyVertexBuffer(&buffer);device.DestroyVertexDeclaration(&declaration);device.DestroySurface(&surface);
    std::cout<<"MojoShader D3D9 shader models 1/2/3 -> BMSL -> actual Metal GPU, sparse constants, input/interstage semantics, sampler3, alpha test and unsupported/malformed/stage rejection PASS\n";
    return 0;
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;} }
