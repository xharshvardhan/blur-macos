// [Blur2PC][macOS] Link the original built-in shader symbols to native resources.
#include <display2/device/MetalDevice.h>
#include <display/shader/ShaderUtils.h>
#include <iostream>
#include <stdexcept>
EXTERN_SHADER(vsBasicRenderColoured3D)
EXTERN_SHADER(psBasicRenderColoured)
int main() {
    try {
        Display2::C_MetalDevice device;
        Display2::C_VertexShader vertex;
        Display2::C_PixelShader pixel;
        if (!device.Create()) throw std::runtime_error("Metal device unavailable");
        auto loadVertex = [&](U32 size, const S32* data) {
            return device.CreateVertexShader(size, reinterpret_cast<const U8*>(data), 0, &vertex);
        };
        auto loadPixel = [&](U32 size, const S32* data) {
            return device.CreatePixelShader(size, reinterpret_cast<const U8*>(data), 0, &pixel);
        };
        if (!loadVertex(REF_SHADER(vsBasicRenderColoured3D)) || !loadPixel(REF_SHADER(psBasicRenderColoured)) ||
            vertex.GetParamFromName("g_ViewProjMatrix").m_RegisterIndex != 4)
            throw std::runtime_error("Original built-in symbols failed native shader resource loading");
        std::cout << "Original EXTERN_SHADER/REF_SHADER -> native Metal shader resource PASS\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
