// [Blur2PC][macOS] Compile translated authored shaders on the real Metal device.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <display2/device/MetalDevice.h>
#include <atomic>
#include <iostream>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

int main(int argc, char** argv) {
    @autoreleasepool {
        if (argc != 2) { std::cerr << "Usage: blur_mac_check_shaders /path/to/report.json\n"; return 2; }
        NSError* error = nil;
        NSString* path = [NSString stringWithUTF8String:argv[1]];
        NSData* data = [NSData dataWithContentsOfFile:path];
        NSDictionary* report = data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:&error] : nil;
        NSArray* rows = report[@"shaders"];
        Display2::C_MetalDevice device;
        if (!device.Create() || ![rows isKindOfClass:[NSArray class]] || rows.count == 0) {
            std::cerr << "Metal device or shader report unavailable\n"; return 2;
        }
        std::atomic<unsigned> next{0}, succeeded{0};
        std::mutex mutex;
        NSMutableArray* failures = [NSMutableArray new];
        auto check = [&] {
            while (true) {
                const unsigned index = next++;
                if (index >= rows.count) return;
                @autoreleasepool {
                    NSDictionary* row = rows[index];
                    NSString* problem = nil;
                    if (![row[@"passed"] boolValue]) problem = @"HLSL translation failed";
                    if (!problem) {
                        std::ifstream input([row[@"blob"] fileSystemRepresentation], std::ios::binary);
                        std::vector<U8> bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
                        Display2::C_VertexShader vertex;
                        Display2::C_PixelShader pixel;
                        std::string compilerError;
                        const bool vertexStage = [row[@"stage"] isEqualToString:@"vert"];
                        const bool ok = vertexStage ? device.CreateVertexShader(U32(bytes.size()), bytes.data(), 0, &vertex, nullptr, &compilerError) :
                                                     device.CreatePixelShader(U32(bytes.size()), bytes.data(), 0, &pixel, nullptr, &compilerError);
                        if (!ok) problem = [NSString stringWithUTF8String:compilerError.c_str()];
                        if (ok && [row[@"symbol"] isEqualToString:@"vsOceanOld"] &&
                            (vertex.GetParamFromName("g_Waves").m_RegisterIndex != 215 ||
                             vertex.GetParamFromName("g_Waves").m_RegisterCount != 14 ||
                             vertex.GetParamFromName("g_Position").m_RegisterIndex != 229 ||
                             vertex.GetParamFromName("g_Position").m_RegisterCount != 4))
                            problem = @"Ocean struct constants lost their authored register layout";
                        if (ok && [row[@"symbol"] isEqualToString:@"vsCrowdFlag"] &&
                            (vertex.GetParamFromName("g_FlagSH").m_RegisterIndex != 207 || vertex.GetParamFromName("g_FlagSH").m_RegisterCount != 3))
                            problem = @"Crowd spherical-harmonic constants lost their authored register layout";
                        if (ok && [row[@"symbol"] isEqualToString:@"psPerlin"] &&
                            (pixel.GetParamFromName("g_Settings").m_RegisterIndex != 0 || pixel.GetParamFromName("g_Settings").m_RegisterCount != 6))
                            problem = @"Perlin struct constants lost their authored register layout";
                        if (ok && [row[@"symbol"] isEqualToString:@"psDoFClassifyBlurAll"] &&
                            (pixel.GetParamFromName("g_vTapDistances").m_RegisterCount != 6 ||
                             pixel.GetParamFromName("g_vTapWeights").m_RegisterIndex != 6 ||
                             pixel.GetParamFromName("g_vTaps").m_RegisterIndex != 12 ||
                             pixel.GetParamFromName("g_vTaps").m_RegisterCount != 12))
                            problem = @"Arithmetic array extents lost their authored register layout";
                        if (ok && [row[@"symbol"] isEqualToString:@"vsBasicRenderColoured3D"]) {
                            const auto world = vertex.GetParamFromName("g_WorldMatrix");
                            const auto view = vertex.GetParamFromName("g_ViewProjMatrix");
                            if (world.m_RegisterIndex != 0 || world.m_RegisterCount != 4 ||
                                view.m_RegisterIndex != 4 || view.m_RegisterCount != 4 ||
                                vertex.GetInputLocation(0, 0) != 0 || vertex.GetInputLocation(10, 0) != 1)
                                problem = @"Engine reflection disagrees with original register/vertex declarations";
                            Btl::Vector<Display2::C_ShaderSemantic> semantics;
                            if (!vertex.GetSemantics(semantics) || semantics.Size() != 2 ||
                                semantics[0].m_Usage != 0 || semantics[1].m_Usage != 10 ||
                                vertex.GetParamFromName("missing-fixture-parameter").m_RegisterCount != 0)
                                problem = @"Engine semantic or missing-parameter lookup changed";
                            if (device.CreateVertexShader(U32(bytes.size()), bytes.data(), 0, &vertex))
                                problem = @"Existing shader resource was overwritten";
                            auto alias = vertex; device.Destroy(&vertex);
                            if (!alias.IsCreated() || alias.GetParamFromName("g_WorldMatrix").m_RegisterCount != 4)
                                problem = @"Shader alias lost native function/reflection ownership";
                            if (device.CreatePixelShader(U32(bytes.size()), bytes.data(), 0, &pixel) ||
                                device.CreateVertexShader(39, bytes.data(), 0, &vertex))
                                problem = @"Native shader accepted wrong stage or truncated data";
                            auto malformed = bytes;
                            for (int byte = 20; byte < 24; ++byte) malformed[byte] = 0xff;
                            if (device.CreateVertexShader(U32(malformed.size()), malformed.data(), 0, &vertex))
                                problem = @"Native shader accepted overflowing section bounds";
                        }
                    }
                    if (!problem) ++succeeded;
                    else {
                        std::lock_guard<std::mutex> guard(mutex);
                        [failures addObject:@{@"target":row[@"target"], @"symbol":row[@"symbol"], @"error":problem}];
                    }
                }
            }
        };
        std::vector<std::thread> workers;
        for (int i = 0; i < 4; ++i) workers.emplace_back(check);
        for (auto& worker : workers) worker.join();
        NSDictionary* result = @{@"total":@(rows.count), @"compiled":@(succeeded.load()),
                                 @"device":[NSString stringWithUTF8String:device.GetDeviceName().c_str()], @"failures":failures};
        NSString* output = [[path stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"metal-check.json"];
        NSData* json = [NSJSONSerialization dataWithJSONObject:result options:NSJSONWritingPrettyPrinted error:&error];
        if (!json || ![json writeToFile:output atomically:YES]) { std::cerr << "Could not write Metal compiler report\n"; return 2; }
        std::cout << "Authored shaders accepted by engine Metal resources: " << succeeded << '/' << rows.count << " on " << device.GetDeviceName() << '\n';
        for (unsigned i = 0; i < std::min<unsigned>(8, failures.count); ++i) {
            NSDictionary* failure = failures[i];
            std::cerr << [failure[@"symbol"] UTF8String] << ": " << [failure[@"error"] UTF8String] << '\n';
        }
        return failures.count ? 1 : 0;
    }
}
