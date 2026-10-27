// [Blur2PC][macOS] Real Metal adapter and read-only primary display mode/capability enumeration.
#pragma once
#include <display/manager/ManagerBase.h>
#include <display/window/WindowBase.h>
#include <display2/device/MetalEngineDevice.h>
#include <string>
#include <vector>
namespace Display {
struct C_Multisample {bool m_Windowed=false,m_FullScreen=false;E_Format m_ColourFormat=E_Format_Unknown,m_DepthStencilFormat=E_Format_Unknown;E_Multisample m_Multisample=E_Multisample_None;};
namespace Metal {
class C_PlatformManager:public C_ManagerBase {
public:
    ~C_PlatformManager();
    bool IsInitialised()const{return m_Adapter.IsCreated();}
    bool Initialise();void Shutdown();
    S32 GetMultisampleCount()const{return static_cast<S32>(m_Multisamples.size());}
    const C_Multisample& GetMultisample(S32 index)const;
    S32 GetVideoModeCount()const{return static_cast<S32>(m_VideoModes.size());}
    const C_VideoMode& GetVideoMode(S32 index)const;
    S32 FindMultisample(E_Multisample sample)const;
    S32 FindVideoMode(U32 width,U32 height,E_Format format)const;
    bool GetVideoMode(C_VideoMode* mode)const{return GetDesktopVideoMode(mode);}
    bool GetDesktopVideoMode(C_VideoMode* mode)const;
    const std::string& GetNativeError()const{return m_Error;}
    std::string GetNativeDeviceName()const{return m_Adapter.GetDeviceName();}
protected:
    void SetNativeError(const std::string& error){m_Error=error;}
private:
    Display2::C_DeviceManager m_Adapter;std::vector<C_Multisample> m_Multisamples;std::vector<C_VideoMode> m_VideoModes;std::string m_Error;
};
}}
