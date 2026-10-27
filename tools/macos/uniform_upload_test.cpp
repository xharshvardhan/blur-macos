// [Blur2PC][macOS] Per-draw constant ownership, reflected padding and the4KB boundary.
#include <display2/device/MetalDevice.h>
#include <display2/context/Context.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace Display2;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static std::vector<U8> shader(bool vertex,U32 vectors){
    const U32 largeSlot=vertex?3:2,smallSlot=vertex?7:9;
    const std::string meta="{\"parameters\":{},\"input_semantics\":[],\"reflection\":{\"ubos\":[{\"binding\":"+std::to_string(largeSlot)+",\"block_size\":"+std::to_string(vectors*16+4)+",\"name\":\"Registers\"},{\"binding\":"+std::to_string(smallSlot)+",\"block_size\":16,\"name\":\"AlsoRegisters\"}]}}";
    const std::string source="#include <metal_stdlib>\nusing namespace metal;\nstruct V{float4 p[[position]];float r[[user(locn0)]];};\nstruct C{float4 values["+std::to_string(vectors)+"];float tail;};\nstruct S{float4 value;};\n"+(vertex?
        "vertex V blur_main(uint id[[vertex_id]],constant C& c[[buffer(3)]],constant S& s[[buffer(7)]]){float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};return {float4(p[id],0,1),c.values["+std::to_string(vectors-1)+"].x+c.tail+s.value.x};}\n":
        "fragment float4 blur_main(V i[[stage_in]],constant C& c[[buffer(2)]],constant S& s[[buffer(9)]]){return float4(i.r,c.values["+std::to_string(vectors-1)+"].y+c.tail+s.value.y,.25,.75);}\n");
    const std::string entry="blur_main";std::vector<U8> result(40);std::memcpy(result.data(),"BMSL",4);
    auto word=[&](U32 p,U32 n){for(U32 i=0;i<4;++i)result[p+i]=U8(n>>(8*i));};
    word(4,1);word(8,vertex?0:1);word(16,40);word(20,U32(meta.size()));word(24,U32(40+meta.size()));word(28,U32(source.size()));word(32,U32(40+meta.size()+source.size()));word(36,U32(entry.size()));
    result.insert(result.end(),meta.begin(),meta.end());result.insert(result.end(),source.begin(),source.end());result.insert(result.end(),entry.begin(),entry.end());word(12,U32(result.size()));return result;
}
static std::vector<U32> pixels(C_Surface& surface){
    const auto map=surface.LockRect(16);require(map.pBits,"Uniform fixture GPU readback failed");std::vector<U32> result(surface.GetWidth()*surface.GetHeight());
    for(U32 y=0;y<surface.GetHeight();++y)std::memcpy(result.data()+y*surface.GetWidth(),map.pBits+y*map.pitch,surface.GetWidth()*4);
    surface.UnlockRect();return result;
}
static void runCase(C_MetalDevice& device,U32 vectors,bool benchmark){
    auto& c=device.GetContext();C_Surface target;C_VertexShader vs;C_PixelShader ps;C_VertexDeclaration declaration;
    const auto vertex=shader(true,vectors),pixel=shader(false,vectors);std::string error;
    require(device.CreateVertexShader(U32(vertex.size()),vertex.data(),0,&vs,nullptr,&error),error.c_str());
    require(device.CreatePixelShader(U32(pixel.size()),pixel.data(),0,&ps,nullptr,&error),error.c_str());
    C_VertexFormat format;format.AddElement(0,0,E_VertexElementType_F32_3,E_VertexElementUsage_Position,0);
    require(device.CreateVertexDeclaration(format,&declaration),"Uniform fixture declaration failed");
    require(device.CreateSurface(64,8,E_Flags_Colour,E_SurfaceFormat_A8R8G8B8,E_Multisample_None,nullptr,&target),"Uniform fixture target failed");
    c.SetColourSurface(0,&target);c.SetDepthStencilSurface(nullptr);c.SetVertexDeclaration(&declaration);c.SetVertexShader(&vs);c.SetPixelShader(&ps);
    c.SetDepthEnable(E_Bool_False);c.SetStencilEnable(E_Bool_False);c.SetBlendEnable(E_Bool_False);c.SetCullMode(E_CullMode_None);c.SetColourWriteEnable(15);
    std::vector<F32> v((vectors+1)*4,0),p(v.size(),0);
    auto upload=[&](U32 stripe){
        std::fill(v.begin(),v.end(),0);std::fill(p.begin(),p.end(),0);
        v[0]=.125f+F32(stripe)/64;v[(vectors-1)*4]+=.125f;v[vectors*4]=.0625f;
        p[1]=.0625f+F32(7-stripe)/64;p[(vectors-1)*4+1]+=.1875f;p[vectors*4]=.0625f;
        c.SetVertexShaderConstantF(0,v.data(),vectors+1);c.SetPixelShaderConstantF(0,p.data(),vectors+1);
        // SetConstant copies caller memory; encoded transient bindings must also
        // outlive stack/scratch reuse and the subsequent draws below.
        std::fill(v.begin(),v.end(),-100);std::fill(p.begin(),p.end(),-100);
    };
    // Two ordinary UBO bindings per stage intentionally read the same original
    // register bank, including different first/last/tail members.
    for(U32 repetition=0;repetition<3;++repetition){
        c.SetScissorEnable(E_Bool_False);c.BeginScene();c.Clear(E_Clear_Colour,0);
        for(U32 stripe=0;stripe<8;++stripe){
            if(stripe==4){c.EndScene();c.BeginScene();} // actual submit boundary
            if(stripe==2||stripe==6)c.ResolveSurface(&target); // encoder boundary
            upload(stripe);c.SetScissorEnable(E_Bool_True);c.SetScissorRect({S32(stripe*8),0,S32(stripe*8+8),8});c.DrawPrimitive(E_PrimitiveType_TriangleList,0,3);
        }
        c.EndScene();const auto actual=pixels(target);
        for(U32 y=0;y<8;++y)for(U32 x=0;x<64;++x){
            const U32 stripe=x/8,colour=actual[y*64+x];
            const F32 red=(vectors==1?.5625f:.3125f)+F32(stripe)/64*(vectors==1?2:1);
            const F32 green=(vectors==1?.5625f:.3125f)+F32(7-stripe)/64*(vectors==1?2:1);
            if(!(std::abs(S32((colour>>16)&255)-S32(std::lround(red*255)))<=1&&std::abs(S32((colour>>8)&255)-S32(std::lround(green*255)))<=1&&(colour&255)==64&&std::abs(S32(colour>>24)-191)<=1)){
                std::cerr<<"vectors="<<vectors<<" repetition="<<repetition<<" x="<<x<<" y="<<y<<" actual=0x"<<std::hex<<colour<<std::dec<<" expected red="<<red<<" green="<<green<<'\n';
                throw std::runtime_error("Distinct constant values/padding/bindings were lost across GPU draws");
            }
        }
    }
    if(benchmark){
        constexpr U32 draws=2000;std::vector<double> times;
        for(U32 trial=0;trial<5;++trial){
            c.SetScissorEnable(E_Bool_False);c.BeginScene();upload(0);c.DrawPrimitive(E_PrimitiveType_TriangleList,0,3); // warmed pipeline
            const auto start=std::chrono::steady_clock::now();
            for(U32 draw=0;draw<draws;++draw){upload(draw%8);c.DrawPrimitive(E_PrimitiveType_TriangleList,0,3);}
            const auto end=std::chrono::steady_clock::now();times.push_back(std::chrono::duration<double,std::milli>(end-start).count());
            c.EndScene();require(device.WaitIdle(),"Uniform benchmark command failed");
        }
        std::sort(times.begin(),times.end());std::cout<<"constant_encode_median_ms="<<times[times.size()/2]<<" draws="<<draws<<" trials="<<times.size()<<'\n';
    }
    c.SetScissorEnable(E_Bool_False);c.SetColourSurface(0,nullptr);c.SetVertexShader(nullptr);c.SetPixelShader(nullptr);c.SetVertexDeclaration(nullptr);
    device.Destroy(&target);device.Destroy(&vs);device.Destroy(&ps);device.Destroy(&declaration);
    std::cout<<"uniform logical_bytes="<<vectors*16+4<<" reflected_bytes="<<(vectors+1)*16<<" passed\n";
}
int main(int argc,char** argv){try{
    const bool benchmark=argc==2&&std::string(argv[1])=="--benchmark";require(argc==1||benchmark,"Usage: uniform_upload_test [--benchmark]");
    C_MetalDevice device;require(device.Create(),"Uniform fixture Metal device failed");
    for(U32 vectors:{1u,25u,254u,255u,256u})runCase(device,vectors,benchmark&&vectors==25);
    require(device.WaitIdle(),"Uniform fixture command failed");std::cout<<"Per-draw VS/PS constant copies, multiple slots, padded4KB boundary, encoder/submit transitions and large fallback passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
