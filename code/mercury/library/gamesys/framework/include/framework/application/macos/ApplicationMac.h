// [Blur2PC][macOS] Native application host for the original shared framework. Actual game/world integration is separate.
#pragma once
#include <core/Core.h>
#include <framework/application/ApplicationBase.h>
#include <framework/application/macos/MacWindow.h>
#include <input/macos/NativeInput.h>
#include <functional>
#include <string>
namespace Framework {
class C_Application:public C_ApplicationBase {
public:
    struct InterpolationCallbacks {
        std::function<void()> tick,endRender;
        std::function<void(F32)> tickSeconds,beginRender;
    }; // Original WorldRender interpolation is the default; callers may replace the complete scope.
    C_Application();~C_Application()override;
    static C_Application* GetInstance(){return static_cast<C_Application*>(C_ApplicationBase::GetInstance());}
    HRESULT Initialise(const C_ApplicationParams& params)override;void Shutdown(bool force=false)override;HRESULT Run()override;
    bool CanRender()override;
    bool LoadingScreenEnabledState()const{return false;} // [Blur2PC] Original PC has no independent console loading renderer.
    // [Blur2PC] Original PC render preset; host window choices are separate.
    U32 GetGraphicalDetail()const{return m_GraphicalDetail;}
    void SetGraphicalDetail(U32 graphicalDetail);
    void ConfigureNativeWindow(const Mac::WindowConfig& config);
    void SetNativeInterpolationCallbacks(InterpolationCallbacks callbacks);
    const std::string& GetNativeError()const{return m_NativeError;}
    const Mac::Window& GetNativeWindow()const{return m_Window;}
    U32 GetDisplayWindowWidth()const{return m_DisplayDeviceParams.m_WindowBufferWidth;}
    U32 GetDisplayWindowHeight()const{return m_DisplayDeviceParams.m_WindowBufferHeight;}
    Display::E_Multisample GetDisplayWindowMultisample()const{return m_DisplayDeviceParams.m_WindowBufferMultisample;}
protected:
    HRESULT PlatformInitialise(const C_ApplicationParams& params)override;void PlatformShutdown()override;
    HRESULT CreateDevice()override;HRESULT CreateInputManager(const C_ApplicationParams& params)override;
    void UpdateGraphicsSettings()override;
private:
    Mac::Window m_Window;Mac::WindowConfig m_WindowConfig;Input::Mac::Events m_InputEvents;
    InterpolationCallbacks m_Interpolation;std::string m_NativeError;
    U32 m_GraphicalDetail=2; // [Blur2PC] ApplicationPC's no-settings default.
    U32 m_PendingWidth=0,m_PendingHeight=0;bool m_Running=false,m_ShutdownPending=false,m_HasFocus=false;
};
}
