// [Blur2PC][macOS] Decode authored media, upload/render actual movie frames and queue silent-gain PCM to CoreAudio.
#include <core/core.h>
#include <core/maths.h>
#include <display.h>
#include <movie.h>
#include <system/serialise/C_FileSystem.h>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
static std::vector<U8> read(const std::filesystem::path& path){std::ifstream input(path,std::ios::binary);require(bool(input),"Missing authored movie/shader bytes");return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};}
static U32 expected(U32 x,U32 y,U32 frame){return 0xff000000|(((2*x+16*frame)%256)<<16)|(((4*y+8*frame)%256)<<8)|((2*(x+y)+12*frame)%256);}
int main(int argc,char** argv){try{
    require(argc==3,"Expected authored movie and shader directories");const std::filesystem::path root=argv[1],shaders=argv[2];
    require(Sys::C_FileSystem::Instance().SetWorkingDirectory(root.string().c_str()),"Movie engine filesystem setup failed");
    Sys::SetDefaultFileSystem(&Sys::C_FileSystem::Instance());
    Display::C_Device device;Display::C_DeviceParams dp;dp.m_Width=34;dp.m_Height=18;require(device.Create(dp)&&device.CreateVolatile(dp),"Movie native display initialization failed");
    Display2::C_VertexShader vs;Display2::C_PixelShader ps;Display2::C_VertexDeclaration declaration;Display2::C_VertexBuffer vertices;std::string error;
    auto v=read(shaders/"display/vsDefault_XY.bmsl"),p=read(shaders/"display/psDefaultTexture.bmsl");auto& gpu=device.GetDisplay2();
    require(gpu.CreateVertexShader(v.size(),v.data(),0,&vs,nullptr,&error),error.c_str());require(gpu.CreatePixelShader(p.size(),p.data(),0,&ps,nullptr,&error),error.c_str());
    Display2::C_VertexFormat format;format.AddElement(0,0,Display2::E_VertexElementType_F32_2,Display2::E_VertexElementUsage_Position,0);format.AddElement(0,8,Display2::E_VertexElementType_F32_2,Display2::E_VertexElementUsage_TexCoord,0);require(gpu.CreateVertexDeclaration(format,&declaration),"Movie sampling declaration failed");
    const F32 quad[]={-1,1,0,0,1,1,1,0,-1,-1,0,1,1,-1,1,1};require(gpu.CreateVertexBuffer(sizeof(quad),0,&vertices),"Movie sampling vertices failed");void* bytes=vertices.Lock();require(bytes,"Movie vertex upload failed");std::memcpy(bytes,quad,sizeof(quad));vertices.Unlock();
    auto frame=[&](Movie::C_Movie& movie,U32 index){const auto* texture=movie.GetTexture();require(texture&&texture->IsCreated()&&texture->GetWidth()==34&&texture->GetHeight()==18,"Movie has no real decoded output texture");const auto lock=texture->Lock(0);require(lock.pBits,"Movie GPU texture readback failed");bool valid=true;for(U32 y=0;y<18;++y)for(U32 x=0;x<34;++x){U32 actual=0;std::memcpy(&actual,lock.pBits+y*lock.pitch+x*4,4);if(actual!=expected(x,y,index)){if(valid)std::cerr<<"movie frame="<<index<<" x="<<x<<" y="<<y<<" actual="<<std::hex<<actual<<" expected="<<expected(x,y,index)<<std::dec<<'\n';valid=false;}}texture->Unlock(0);require(valid,"Decoded movie GPU pixels differ from authored source bytes");};
    auto render=[&](Movie::C_Movie& movie){require(device.BeginScene(),"Movie UI render scene failed");movie.Render();auto& context=gpu.GetContext();context.SetVertexShader(&vs);context.SetPixelShader(&ps);context.SetVertexBuffer(0,&vertices,0,16,&declaration);context.SetTexture(0,movie.GetTexture());context.SetDepthEnable(Display2::E_Bool_False);context.SetCullMode(Display2::E_CullMode_None);context.SetColourWriteEnable(15);context.SetBlendEnable(Display2::E_Bool_False);Display2::C_SamplerState sampler;sampler.SetTextureAddress(Display2::E_TextureAddress_Clamp);context.SetSamplerState(0,sampler);context.DrawPrimitive(Display2::E_PrimitiveType_TriangleStrip,0,4);device.EndScene();require(movie.GetNativeError().empty(),movie.GetNativeError().c_str());};
    {
        Movie::C_Movie movie;Movie::C_MovieParams params;movie.SetVolume(0);movie.SetLooping(false);
        require(movie.Create()&&movie.CreateVolatile(),"Registered idle movie blocked global renderer creation");require(!movie.IsCreated()&&movie.HasReachedEnd()&&!movie.GetTexture()->IsCreated()&&movie.GetDecodedVideoFrames()==0&&movie.GetDecodedAudioFrames()==0,"Idle renderer invented playback resources");require(movie.Update(),"Idle movie update did not report absent playback");movie.Render();movie.DestroyVolatile();require(movie.CreateVolatile()&&!movie.IsCreated(),"Idle movie reset attempted playback");
        require(!movie.Open("",params)&&!movie.IsCreated()&&!movie.GetNativeError().empty(),"An explicit empty movie open reported success");
        require(!movie.Open("missing.nut",params)&&!movie.IsCreated()&&!movie.GetNativeError().empty(),"Missing movie reported opened success");
        params.m_CreateFlags|=Movie::E_MovieCreateFlags_SetBufferIOSize;params.m_BinkIOSize=4096;
        require(movie.Open("authored.nut",params),movie.GetNativeError().c_str());require(movie.IsCreated()&&!movie.HasReachedEnd()&&movie.GetTotalTime()>.75f&&movie.GetTotalTime()<.85f,"Movie stream timing metadata differs");
        require(movie.GetDecodedVideoFrames()>0&&movie.GetDecodedAudioFrames()>0&&movie.GetSubmittedAudioFrames()>0&&movie.GetDecodedAudioPeak()>.01f,"Movie did not decode and queue real nonzero PCM audio");frame(movie,0);
        movie.SetPaused(true);const F32 paused=movie.GetCurrentTime();std::this_thread::sleep_for(std::chrono::milliseconds(80));render(movie);require(movie.GetCurrentTime()==paused,"Paused movie advanced its elapsed media clock");frame(movie,0);
        movie.SetVolume(16384);require(movie.GetVolume()==16384,"Movie volume change was discarded");movie.SetVolume(0);movie.SetAudio(false);movie.SetAudio(true);movie.SetPaused(false);
        std::this_thread::sleep_for(std::chrono::milliseconds(150));render(movie);const U32 index=std::min<U32>(7,U32(movie.GetCurrentTime()*10));require(index>=1&&index<=3,"Movie advanced by rendered-frame count instead of elapsed media time");frame(movie,index);
        auto* target=device.GetNativeColourSurface();auto pixels=target->LockRect(16);require(pixels.pBits,"Rendered movie framebuffer readback failed");bool valid=true;for(U32 y=0;y<18;++y)for(U32 x=0;x<34;++x){U32 actual=0;std::memcpy(&actual,pixels.pBits+y*pixels.pitch+x*4,4);valid&=actual==expected(x,y,index);}target->UnlockRect();require(valid,"UI shader did not sample actual decoded movie GPU pixels");
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(!movie.HasReachedEnd()&&std::chrono::steady_clock::now()<deadline){std::this_thread::sleep_for(std::chrono::milliseconds(10));render(movie);}require(movie.HasReachedEnd()&&movie.GetCurrentTime()>.75f,"Nonlooping movie failed to reach its timestamped end");frame(movie,7);
        require(movie.GetDecodedAudioFrames()>=38400&&movie.GetDecodedAudioFrames()<=38402,"Movie resampler dropped its final PCM samples");
        movie.Rewind();render(movie);require(!movie.HasReachedEnd()&&movie.GetCurrentTime()<.1f,"Movie rewind retained old timestamp/end state");frame(movie,0);
        movie.SetLooping(true);const auto loopDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(!movie.HasReachedEnd()&&std::chrono::steady_clock::now()<loopDeadline){std::this_thread::sleep_for(std::chrono::milliseconds(10));render(movie);}require(movie.HasReachedEnd(),"Looping movie did not expose its end boundary to UI");render(movie);require(!movie.HasReachedEnd()&&movie.GetCurrentTime()<.1f,"Looping movie failed to seek and restart");frame(movie,0);
        movie.SetPaused(true);require(movie.Open("video-only.nut",Movie::C_MovieParams()),movie.GetNativeError().c_str());require(movie.GetDecodedAudioFrames()==0&&movie.GetSubmittedAudioFrames()==0,"Video-only reopen retained old audio decoder/queue owners");frame(movie,0);
        movie.DestroyVolatile();require(!movie.IsCreated(),"Volatile movie teardown retained decoder/texture owners");require(movie.CreateVolatile(),movie.GetNativeError().c_str());frame(movie,0);
        Movie::C_MovieParams preload;preload.m_CreateFlags=Movie::E_MovieCreateFlags_LoadIntoMemory;require(movie.Open("authored.nut",preload),movie.GetNativeError().c_str());frame(movie,0);
        Movie::C_MovieParams namedMemory;namedMemory.m_CreateFlags=Movie::E_MovieCreateFlags_PlayFromMemory;require(movie.Open("authored.nut",namedMemory),movie.GetNativeError().c_str());require(movie.GetDecodedVideoFrames()>0&&movie.GetDecodedAudioFrames()>0,"Filename-backed memory movie did not decode real media");frame(movie,0);
        const auto memory=read(root/"authored.nut");Movie::C_MovieParams fromMemory;fromMemory.m_CreateFlags=Movie::E_MovieCreateFlags_PlayFromMemory;fromMemory.m_pMovieData=const_cast<U8*>(memory.data());fromMemory.m_MovieDataSize=memory.size();require(movie.Open("unused-memory-name",fromMemory),movie.GetNativeError().c_str());frame(movie,0);
        fromMemory.m_MovieDataSize=0;require(!movie.Open("unbounded-memory",fromMemory)&&!movie.IsCreated(),"Unbounded memory movie was accepted");
        namedMemory.m_MovieDataSize=32;require(!movie.Open("authored.nut",namedMemory)&&!movie.IsCreated(),"Memory byte size without a pointer was accepted");
        Movie::C_MovieParams invalid;invalid.m_CreateFlags=0x8000;require(!movie.Open("authored.nut",invalid),"Unknown movie creation flags reported success");require(!movie.Open("invalid.nut",Movie::C_MovieParams()),"Invalid media payload reported opened success");
        Movie::C_MovieParams noTracks;for(auto& track:noTracks.m_TrackIDsToPlay)track=~U32(0);require(movie.Open("authored.nut",noTracks),movie.GetNativeError().c_str());require(movie.GetDecodedAudioFrames()==0&&movie.GetSubmittedAudioFrames()==0,"Movie ignored explicit audio track selection");movie.Close();movie.Close();require(!movie.IsCreated()&&movie.HasReachedEnd(),"Movie close retained playback state");require(movie.Create()&&movie.CreateVolatile()&&!movie.IsCreated(),"Closed registered movie restarted its previous clip");movie.Close();
    }
    // Optional private verification uses actual installed movies without copying
    // their copyrighted data into source control or the authored fixture suite.
    if(const char* retail=std::getenv("BLUR_MAC_MOVIE_RETAIL_DIR")){
        for(const char* name:{"atvi.bik","BizarreSplash.bik"}){
            Movie::C_Movie movie;movie.SetVolume(0);movie.SetLooping(false);Movie::C_MovieParams params;params.m_CreateFlags=Movie::E_MovieCreateFlags_PlayFromMemory;
            const auto path=std::filesystem::path(retail)/"UI/movies"/name;require(movie.Open(path.string().c_str(),params),movie.GetNativeError().c_str());
            const auto* texture=movie.GetTexture();require(texture->GetWidth()==1280&&texture->GetHeight()==720&&movie.GetTotalTime()>5,"Original Bink metadata was not preserved");
            auto pixels=[&](){const auto lock=texture->Lock(0);require(lock.pBits,"Original Bink GPU readback failed");std::vector<U8> result(1280*720*4);for(U32 y=0;y<720;++y)std::memcpy(result.data()+y*1280*4,lock.pBits+y*lock.pitch,1280*4);texture->Unlock(0);return result;};
            movie.SetPaused(true);movie.Render();const auto first=pixels();movie.SetPaused(false);
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(S64(movie.GetTotalTime()*1000)+4000);while(!movie.HasReachedEnd()&&std::chrono::steady_clock::now()<deadline){std::this_thread::sleep_for(std::chrono::milliseconds(20));movie.Render();require(movie.GetNativeError().empty(),movie.GetNativeError().c_str());}
            require(movie.HasReachedEnd(),"Original Bink playback did not reach its timestamped end");require(movie.GetDecodedVideoFrames()==U64(std::strcmp(name,"atvi.bik")==0?225:180),"Original Bink video frames were lost");
            require(std::strcmp(name,"atvi.bik")!=0||(movie.GetDecodedAudioFrames()>300000&&movie.GetSubmittedAudioFrames()>300000),"Original Bink audio was not decoded/queued");
            movie.SetPaused(true);movie.Rewind();movie.Render();require(!movie.HasReachedEnd()&&movie.GetCurrentTime()==0&&pixels()==first,"Original Bink rewind failed to restore the actual first GPU frame");std::cout<<"Original "<<name<<": complete decoded playback, GPU upload, audio selection and rewind passed\n";
        }
    }
    gpu.Destroy(&vs);gpu.Destroy(&ps);gpu.Destroy(&declaration);gpu.Destroy(&vertices);require(gpu.WaitIdle(),"Movie GPU command failed");
#if TRACK_DISPLAY2_RESOURCES
    require(gpu.GetRenderTexture2DCount()==0,"Native movie leaked tracked GPU textures");
#endif
    device.DestroyVolatile();device.Destroy();std::cout<<"Native FFmpeg movie: authored exact GPU frames/UI sampling, real resampled PCM/CoreAudio queue at zero gain, elapsed time, pause/end/loop/rewind, file/preload/bounded memory, track selection and teardown passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
