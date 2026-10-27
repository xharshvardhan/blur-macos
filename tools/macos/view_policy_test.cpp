// [Blur2PC][macOS] Original PC preset state plus real offscreen resize/MSAA ownership.
// This boundary fixture does not execute an enabled full-world lighting/motion/DoF frame.
#include "../../code/amax/game/stdafx.h"
#include <Game/GameWorld/View/Main.h>
#include <Game/GameWorld/ViewManager.h>
#include <GameView/OffscreenView.h>
#include <GameView/ReflectionView.h>
#include <scenegraph/RenderGroupSettings.h>
#include <display/window/Window.h>
#include <fx/Fx.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace Game {extern Debug::Bool g_MainPrepassEnable;extern Debug::Float g_MainModelInstanceDrawDistanceScaleMax;}
namespace {
void Require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
S32 ResourceCount(){
#if TRACK_DISPLAY2_RESOURCES
    auto* d=Display2::Device();return d->GetVertexBufferCount()+d->GetIndexBufferCount()+d->GetVertexShaderCount()+d->GetPixelShaderCount()+d->GetSurfaceCount()+d->GetTextureCount()+d->GetRenderTexture2DCount()+d->GetRenderTextureCubeCount()+d->GetRenderBufferCount();
#else
    return 0;
#endif
}
void CheckPixels(const Render::C_TextureRender& target){
    const auto& texture=target.GetRenderTexture().GetTexture();auto pixels=texture.LockRect2D(0,16);
    Require(pixels.pBits,"Actual resized offscreen texture could not read GPU pixels");bool good=true;
    for(U32 y=0;y<texture.GetHeight();++y)for(U32 x=0;x<texture.GetWidth();++x){U32 pixel;std::memcpy(&pixel,pixels.pBits+y*pixels.pitch+x*4,4);good&=pixel==0xff314f75U;}
    texture.UnlockRect2D(0);Require(good,"Original offscreen Unbind did not resolve the actual multisampled colour pixels");
}
template<class View,class Format>void Offscreen(Display::C_Device& device,Format format){
    const S32 baseline=ResourceCount();
    {
        View view;format(view,16,8,Display::E_Multisample_None);view.Create();
        auto* target=const_cast<Render::C_TextureRender*>(view.GetTextureRender());Require(target&&target->IsRendererCreated()&&target->IsRendererCreated(Display::E_RendererState_Volatile),"Original offscreen child did not create required real GPU targets");
        Require(target->GetRenderTexture().GetTexture().GetWidth()==16&&target->GetRenderTexture().GetTexture().GetHeight()==8,"Initial offscreen dimensions changed");
        format(view,24,12,Display::E_Multisample_2);
        Require(target->GetRenderTexture().GetTexture().GetWidth()==16,"Pending original PC format change mutated live GPU resources before volatile recreation");
        target->DestroyVolatile();Require(target->CreateVolatile(),"Original offscreen target could not recreate after a pending format/MSAA change");
        Require(target->GetRenderTexture().GetTexture().GetWidth()==24&&target->GetRenderTexture().GetTexture().GetHeight()==12,"Original PC SetFormat failed to reach the existing owned render texture");
        Require(target->GetColourSurface().GetMultisample()==Display2::E_Multisample_2,"Offscreen recreation lost the requested real MSAA surface");
        Display::C_View nativeView;Display::C_ViewSetup setup;device.SetupView(&nativeView,setup,24,12);
        Require(device.BeginScene(),"Original offscreen validation scene could not begin");target->Bind();device.SetView(&nativeView);const auto* active=device.GetView();
        device.Clear(Display::E_Clear_Colour,Maths::RGBA(0xff314f75U));target->Unbind();
        Require(device.GetView()==active,"Original PC offscreen Unbind incorrectly reset the active view");device.EndScene();CheckPixels(*target);
        target->DestroyVolatile();format(view,0,12,Display::E_Multisample_None);bool rejected=false;
        try{rejected=!target->CreateVolatile();}catch(const std::exception&){rejected=true;}
        Require(rejected&&!target->IsRendererCreated(Display::E_RendererState_Volatile)&&!target->GetRenderTexture().IsCreated(),"Failed required offscreen allocation published volatile readiness or retained a partial target");
        format(view,8,4,Display::E_Multisample_None);Require(target->CreateVolatile(),"Original offscreen target could not retry after its real allocation failure");view.Destroy();view.Destroy();
    }
    Require(ResourceCount()==baseline,"Offscreen resize/failure/retry retained owned GPU resource wrappers");
}
}
int main(){try{
    Display::C_Device device;device.AddWindow(Maths::C_Rect(Maths::HVec2(0,0),Maths::HVec2(1,1)));Display::C_DeviceParams params;params.m_Width=32;params.m_Height=16;Require(device.Create(params)&&device.CreateVolatile(params),"Native view-policy device failed");
    FX::Initialise();
    {
        Game::C_ViewManager manager(nullptr,nullptr);Game::C_MainView main(&manager);
        Require(manager.GetNoDamageSubtechniqueHandle()==FX::RegisterSubtechnique("NoDamage")&&manager.GetNoDepthSubtechniqueHandle()==FX::RegisterSubtechnique("NoDepth"),"Original PC required subtechnique registrations are absent on native");
        main.SetGraphicalDetail(1,false);Require(!Game::g_MainPrepassEnable,"Original detail1 did not disable the PC prepass");
        for(U32 detail=2;detail<=4;++detail){main.SetGraphicalDetail(detail,false);Require(Game::g_MainPrepassEnable,"Original detail2-4 did not enable the PC prepass");}
        main.SetGraphicalDetail(2,true);Require(F32(Game::g_MainModelInstanceDrawDistanceScaleMax)==1.f,"Original front-end preset did not preserve its full model draw distance");
        main.SetGraphicalDetail(2,false);Require(F32(Game::g_MainModelInstanceDrawDistanceScaleMax)==.6f,"Original gameplay preset did not restore its authored model draw distance");
        SceneGraph::C_RenderGroupSettings settings;Require(!settings.GetStencilTexture(),"New render settings exposed an uninitialized stencil resource");auto* window=device.GetWindow(0);
        Display::C_Texture2D depth;Require(device.CreateTexture2D(1,1,1,Display::E_Usage_Managed,Display::E_Format_R32F,&depth),"Actual PC window depth metadata payload could not allocate");window->m_pDepthTexture=&depth;settings.SetWindow(0,true);
        Require(settings.GetColourTexture()==window->GetColourTexture()&&settings.GetDepthStencilTexture()==window->GetDepthStencilTexture(),"Original window colour/depth metadata failed to reach native render settings");
        Require(window->GetMaskTexture()&&window->GetMaskTexture()->IsCreated(),"Actual window stencil mask was not created");settings.SetStencilTexture(window->GetMaskTexture());Require(settings.GetStencilTexture()==window->GetMaskTexture(),"Original window stencil publication changed the actual texture owner");settings.SetStencilTexture(nullptr);settings.SetColourDepthStencilTexture(nullptr,nullptr);window->m_pDepthTexture=nullptr;device.Destroy(&depth);
        Offscreen<GameView::C_OffscreenView>(device,[](auto& v,U32 w,U32 h,Display::E_Multisample m){v.SetFormat(w,h,m);});
        Offscreen<GameView::C_ReflectionView>(device,[](auto& v,U32 w,U32 h,Display::E_Multisample m){v.SetFormat(w,h,m,1);});
    }
    FX::Shutdown();Require(device.GetDisplay2().WaitIdle(),"View-policy GPU completion failed");device.DestroyVolatile();device.Destroy();
    std::cout<<"Original PC native preset/subtechnique/window metadata and offscreen/reflection pending-format reset, real MSAA resolve pixels, active-view preservation, required-allocation rollback/retry and GPU owner retirement PASS; full enabled-prepass game frame remains a separate actual-game gate\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
