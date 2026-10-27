// [Blur2PC][macOS] Native FFmpeg decoding and real Metal/SDL playback.
#pragma once
#include <movie/MovieBase.h>
#include <memory>
#include <string>
namespace Movie {
class C_MacMovie : public C_MovieBase {
public:
    C_MacMovie();
    ~C_MacMovie() override;
    bool IsCreated()const;
    bool HasReachedEnd()const;
    void SetAudio(bool audio) override;
    void SetPaused(bool paused) override;
    void SetVolume(S32 volume);
    void SetBinkIOSize(U32 size);
    static bool CreateShaders(); // RGBA output is sampled by existing Display/UI shaders.
    static void DestroyShaders();
    void Rewind();
    bool Update();
    void Render();
    void DebugRender(F32& x,F32& y,F32 size);
    const std::string& GetNativeError()const;
    U64 GetDecodedVideoFrames()const;
    U64 GetDecodedAudioFrames()const;
    U64 GetSubmittedAudioFrames()const;
    F32 GetDecodedAudioPeak()const;
protected:
    bool CreateBink(const C_MovieParams& params); // Preserve the shared renderer lifecycle API.
    void DestroyBink();
    bool CreateTextures();
    void DestroyTextures();
private:
    struct State;
    std::unique_ptr<State> m_State;
    std::string m_Error;
    void UploadFrame();
};
}
