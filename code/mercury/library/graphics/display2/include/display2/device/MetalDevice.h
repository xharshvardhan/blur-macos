// [Blur2PC][macOS] Native device implementation, extended as context APIs are ported.
#pragma once
#include <display2/context/MetalBuffers.h>
#include <display2/context/MetalShaders.h>
#include <display2/context/MetalVertices.h>
#include <display2/context/MetalContext.h>
#include <string>

namespace Display2 {
class C_MetalDevice {
public:
    C_MetalDevice();
    ~C_MetalDevice();
    C_MetalDevice(const C_MetalDevice&) = delete;
    C_MetalDevice& operator=(const C_MetalDevice&) = delete;
    bool Create();
    void Destroy();
    U32 IsCreated() const;
    std::string GetDeviceName() const;
    C_Context& GetContext();
    const C_Context& GetContext() const;
    C_ContextNoCheckNoTrack& GetContextNoCheckNoTrack();
    const C_ContextNoCheckNoTrack& GetContextNoCheckNoTrack()const;
    C_ContextNoCheck& GetContextNoCheck();
    const C_ContextNoCheck& GetContextNoCheck()const;
    // Host supplies a CAMetalLayer. Configuration belongs to the main thread;
    // PresentSurface queues a real drawable blit/present after EndScene.
    bool SetPresentationLayer(void* layer,U32 width,U32 height,bool vsync=true);
    bool SetPresentationInterval(E_PresentInterval interval);
    bool PresentSurface(const C_Surface* surface);
    U64 GetSubmittedPresentCount()const;
    bool CreateTexture2D(U32 width,U32 height,U32 levels,U32 flags,E_TextureFormat format,C_Texture* texture,void* memory=nullptr,U32 memorySize=0);
    bool CreateTextureCube(U32 width,U32 levels,U32 flags,E_TextureFormat format,C_Texture* texture,void* memory=nullptr,U32 memorySize=0);
    bool CreateTexture3D(U32 width,U32 height,U32 depth,U32 levels,U32 flags,E_TextureFormat format,C_Texture* texture,void* memory=nullptr,U32 memorySize=0);
    bool GetTextureSurface(const C_Texture* texture,U32 face,U32 level,C_Surface* surface);
    void DestroyTexture(C_Texture* texture);
    void Destroy(C_Texture* texture){DestroyTexture(texture);}
    bool CreateRenderTexture2D(U32 width,U32 height,U32 levels,U32 flags,E_TextureFormat textureFormat,E_SurfaceFormat surfaceFormat,E_Multisample samples,const C_SurfaceParams* params,C_RenderTexture2D* texture,void* memory=nullptr,U32 memorySize=0);
    void DestroyRenderTexture2D(C_RenderTexture2D* texture);void Destroy(C_RenderTexture2D* texture){DestroyRenderTexture2D(texture);}
    bool CreateRenderTextureCube(U32 width,U32 levels,U32 flags,E_TextureFormat textureFormat,E_SurfaceFormat surfaceFormat,E_Multisample samples,const C_SurfaceParams* params,C_RenderTextureCube* texture,void* memory=nullptr,U32 memorySize=0);
    void DestroyRenderTextureCube(C_RenderTextureCube* texture);void Destroy(C_RenderTextureCube* texture){DestroyRenderTextureCube(texture);}
    bool CreateSurface(U32 width,U32 height,U32 flags,E_SurfaceFormat format,E_Multisample multisample,
                       const C_SurfaceParams* params,C_Surface* surface,void* memory=nullptr,U32 memorySize=0);
    void DestroySurface(C_Surface* surface);
    void Destroy(C_Surface* surface) { DestroySurface(surface); }
    bool CreateVertexDeclaration(const C_VertexFormat& format, C_VertexDeclaration* declaration);
    bool CreateVertexDeclaration(const C_VertexElement* elements, C_VertexDeclaration* declaration);
    C_VertexDeclaration* CreateVertexDeclaration(const C_VertexFormat& format);
    C_VertexDeclaration* CreateVertexDeclaration(const C_VertexElement* elements);
    void DestroyVertexDeclaration(C_VertexDeclaration* declaration);
    void DestroyVertexDeclarations();
    void Destroy(C_VertexDeclaration* declaration) { DestroyVertexDeclaration(declaration); }
    bool CreateVertexBuffer(U32 size, U32 flags, C_VertexBuffer* buffer, void* memory = nullptr);
    void DestroyVertexBuffer(C_VertexBuffer* buffer);
    void Destroy(C_VertexBuffer* buffer) { DestroyVertexBuffer(buffer); }
    bool CreateIndexBuffer(U32 size, U32 flags, E_IndexFormat format, C_IndexBuffer* buffer, void* memory = nullptr);
    void DestroyIndexBuffer(C_IndexBuffer* buffer);
    void Destroy(C_IndexBuffer* buffer) { DestroyIndexBuffer(buffer); }
    bool CreateVertexShader(U32 size, const U8* data, U32 flags, C_VertexShader* shader,
                            void* memory = nullptr, std::string* error = nullptr);
    bool CreatePixelShader(U32 size, const U8* data, U32 flags, C_PixelShader* shader,
                           void* memory = nullptr, std::string* error = nullptr);
    void DestroyVertexShader(C_VertexShader* shader);
    void DestroyPixelShader(C_PixelShader* shader);
    void Destroy(C_VertexShader* shader) { DestroyVertexShader(shader); }
    void Destroy(C_PixelShader* shader) { DestroyPixelShader(shader); }
    // Queues an actual GPU blit. A subsequent CPU lock waits for completion.
    bool CopyBuffer(Metal::Buffer* destination, U32 destinationOffset,
                    Metal::Buffer* source, U32 sourceOffset, U32 bytes);
    bool CopyTexture(C_Texture* destination,const C_Texture* source,bool commonMipTail=false);
    bool WaitIdle();
protected:
    bool CreateNative(void* device);
private:
    friend class C_Context;
    struct State;
    bool CreateTextureNative(U32 width,U32 height,U32 depth,U32 levels,U32 flags,E_TextureFormat format,E_TextureType type,C_Texture* texture,void* memory,U32 memorySize);
    std::unique_ptr<State> m_State;
};
}
