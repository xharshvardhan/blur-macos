// [Blur2PC][macOS] Native Metal bring-up. The synthetic triangle verifies the
// backend; it is deliberately not presented as Blur gameplay or game shaders.
#include <display/device/metal/MetalBootstrap.h>
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace Blur::Mac {
namespace {
[[noreturn]] void fail(NSString* message) {
    throw std::runtime_error(message.UTF8String ?: "Unknown Metal error");
}
void writePng(const std::filesystem::path& path, unsigned width, unsigned height,
              const std::vector<unsigned char>& bgra) {
    auto parent = path.parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    std::vector<unsigned char> rgba = bgra;
    for (std::size_t i = 0; i < rgba.size(); i += 4) std::swap(rgba[i], rgba[i + 2]);
    CFDataRef data = CFDataCreate(kCFAllocatorDefault, rgba.data(), rgba.size());
    CGDataProviderRef provider = CGDataProviderCreateWithCFData(data);
    CGColorSpaceRef colour = CGColorSpaceCreateDeviceRGB();
    CGImageRef image = CGImageCreate(width, height, 8, 32, width * 4, colour,
        kCGImageAlphaLast, provider, nullptr, false, kCGRenderingIntentDefault);
    NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path.c_str()]];
    CGImageDestinationRef dest = CGImageDestinationCreateWithURL((__bridge CFURLRef)url, CFSTR("public.png"), 1, nullptr);
    bool ok = false;
    if (dest && image) {
        CGImageDestinationAddImage(dest, image, nullptr);
        ok = CGImageDestinationFinalize(dest);
    }
    if (dest) CFRelease(dest);
    if (image) CGImageRelease(image);
    CGColorSpaceRelease(colour);
    CGDataProviderRelease(provider);
    CFRelease(data);
    if (!ok) fail(@"Could not write Metal verification image");
}
}

MetalProbeResult runMetalProbe(const std::filesystem::path& png) {
    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) fail(@"No Metal device available");
        id<MTLCommandQueue> queue = [device newCommandQueue];
        if (!queue) fail(@"Could not create Metal command queue");
        // Runtime compilation avoids requiring a separately installed offline
        // Metal toolchain for the bootstrap. Authored game effects still need porting.
        NSString* source = @R"msl(
            #include <metal_stdlib>
            using namespace metal;
            struct Vertex { float2 position; float2 padding; float4 colour; };
            struct Interpolant { float4 position [[position]]; float4 colour; };
            vertex Interpolant vs_main(const device Vertex* vertices [[buffer(0)]], uint id [[vertex_id]]) {
                Interpolant output;
                output.position = float4(vertices[id].position, 0.0, 1.0);
                output.colour = vertices[id].colour;
                return output;
            }
            fragment float4 ps_main(Interpolant input [[stage_in]]) { return input.colour; }
        )msl";
        NSError* error = nil;
        id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];
        if (!library) fail(error.localizedDescription ?: @"MSL compilation failed");
        MTLRenderPipelineDescriptor* pipelineDesc = [MTLRenderPipelineDescriptor new];
        pipelineDesc.vertexFunction = [library newFunctionWithName:@"vs_main"];
        pipelineDesc.fragmentFunction = [library newFunctionWithName:@"ps_main"];
        pipelineDesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        id<MTLRenderPipelineState> pipeline = [device newRenderPipelineStateWithDescriptor:pipelineDesc error:&error];
        if (!pipeline) fail(error.localizedDescription ?: @"Metal pipeline creation failed");
        constexpr unsigned width = 256, height = 256;
        MTLTextureDescriptor* textureDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
            MTLPixelFormatBGRA8Unorm width:width height:height mipmapped:NO];
        textureDesc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        textureDesc.storageMode = MTLStorageModeShared;
        id<MTLTexture> texture = [device newTextureWithDescriptor:textureDesc];
        if (!texture) fail(@"Could not allocate Metal render target");
        struct Vertex { float x, y, padding[2], r, g, b, a; };
        const Vertex vertices[] = {
            {-0.75f, -0.75f, {}, 0.9f, 0.18f, 0.7f, 1.0f},
            { 0.00f,  0.75f, {}, 0.9f, 0.18f, 0.7f, 1.0f},
            { 0.75f, -0.75f, {}, 0.9f, 0.18f, 0.7f, 1.0f}
        };
        const std::uint16_t indices[] = {0, 1, 2};
        id<MTLBuffer> vertexBuffer = [device newBufferWithBytes:vertices length:sizeof(vertices) options:MTLResourceStorageModeShared];
        id<MTLBuffer> indexBuffer = [device newBufferWithBytes:indices length:sizeof(indices) options:MTLResourceStorageModeShared];
        if (!vertexBuffer || !indexBuffer) fail(@"Could not allocate Metal geometry buffers");
        MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = texture;
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass.colorAttachments[0].clearColor = MTLClearColorMake(0.01, 0.02, 0.03, 1.0);
        id<MTLCommandBuffer> command = [queue commandBuffer];
        if (!command) fail(@"Could not create Metal command buffer");
        id<MTLRenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
        if (!encoder) fail(@"Could not create Metal render encoder");
        [encoder setRenderPipelineState:pipeline];
        [encoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];
        [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:3
            indexType:MTLIndexTypeUInt16 indexBuffer:indexBuffer indexBufferOffset:0];
        [encoder endEncoding];
        [command commit];
        [command waitUntilCompleted];
        if (command.status != MTLCommandBufferStatusCompleted)
            fail(command.error.localizedDescription ?: @"Metal submission failed");
        std::vector<unsigned char> pixels(width * height * 4);
        [texture getBytes:pixels.data() bytesPerRow:width * 4
            fromRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0];
        std::size_t centre = (height / 2 * width + width / 2) * 4;
        const auto red = pixels[centre + 2], green = pixels[centre + 1], blue = pixels[centre];
        if (std::abs(int(red) - 230) > 2 || std::abs(int(green) - 46) > 2 || std::abs(int(blue) - 179) > 2)
            fail(@"Metal indexed draw failed center pixel readback");
        if (pixels[2] > 5 || pixels[1] > 8 || pixels[0] > 11 || pixels[3] != 255)
            fail(@"Metal render-pass clear failed corner pixel readback");
        if (!png.empty()) writePng(png, width, height, pixels);
        return {device.name.UTF8String, width, height, red, green, blue};
    }
}
} // namespace Blur::Mac
