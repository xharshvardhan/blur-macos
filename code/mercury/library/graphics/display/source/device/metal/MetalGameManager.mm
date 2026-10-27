// [Blur2PC][macOS] Native read-only adapter/display enumeration. No display-mode changes or fabricated MSAA support.
#include "NativeDisplayIncludes.h"
#include <display/device/metal/MetalGameManager.h>
#import <CoreGraphics/CoreGraphics.h>
#include <algorithm>
#include <cmath>
namespace Display::Metal {
namespace {
bool modeInfo(CGDisplayModeRef native,C_VideoMode& value){
    if(!native)return false;const size_t width=CGDisplayModeGetPixelWidth(native),height=CGDisplayModeGetPixelHeight(native);
    if(!width||!height||width>~U32(0)||height>~U32(0))return false;
    value={};value.m_Width=U32(width);value.m_Height=U32(height);value.m_Format=E_Format_A8R8G8B8;
    const double rate=CGDisplayModeGetRefreshRate(native);value.m_RefreshRate=std::isfinite(rate)&&rate>0&&rate<65536?U32(std::lround(rate)):0;
    value.m_WideScreen=false;value.m_HiDef=false; // Retain PC legacy flags; native views use the actual dynamic aspect.
    return true;
}
}
C_PlatformManager::~C_PlatformManager(){Shutdown();}
bool C_PlatformManager::Initialise(){
    if(IsInitialised())return true;Shutdown();Display2::C_DeviceManagerParams params;
    if(!m_Adapter.Create(params)){m_Error="No native Metal adapter is available";return false;}
    try{
        const auto add=[&](CGDisplayModeRef mode){C_VideoMode value;if(modeInfo(mode,value)&&value.m_Width>=640&&value.m_Height>=480&&value.m_Width<=16384&&value.m_Height<=16384&&FindVideoMode(value.m_Width,value.m_Height,value.m_Format)<0)m_VideoModes.push_back(value);};
        auto current=CGDisplayCopyDisplayMode(CGMainDisplayID());if(current){try{add(current);}catch(...){CGDisplayModeRelease(current);throw;}CGDisplayModeRelease(current);}
        auto modes=CGDisplayCopyAllDisplayModes(CGMainDisplayID(),nullptr);
        if(modes){try{for(CFIndex i=0;i<CFArrayGetCount(modes);++i)add((CGDisplayModeRef)CFArrayGetValueAtIndex(modes,i));}catch(...){CFRelease(modes);throw;}CFRelease(modes);}
        std::sort(m_VideoModes.begin(),m_VideoModes.end());
        m_Multisamples.push_back({true,false,E_Format_A8R8G8B8,E_Format_D24S8,E_Multisample_None});
        // [Blur2PC] Probe the selected adapter through the actual backend allocation path.
        Display2::C_DeviceParams probeParams;m_Adapter.SetupDefaultDeviceParams(probeParams);
        Display2::C_Device probe;probe.SetParams(probeParams);
        if(!probe.Create())throw std::runtime_error("Native capability probe device creation failed");
        auto& gpu=static_cast<Display2::C_MetalDevice&>(probe);
        for(auto samples:{E_Multisample_2,E_Multisample_4}){
            Display2::C_Surface colour,depth;
            if(gpu.CreateSurface(16,16,Display2::E_Flags_Colour,Display2::E_SurfaceFormat_A8R8G8B8,Display2::E_Multisample(U32(samples)),nullptr,&colour)&&
               gpu.CreateSurface(16,16,Display2::E_Flags_DepthStencil,Display2::E_SurfaceFormat_D24S8,Display2::E_Multisample(U32(samples)),nullptr,&depth))
                m_Multisamples.push_back({true,false,E_Format_A8R8G8B8,E_Format_D24S8,samples});
        }
        CreateErrors();m_Error.clear();return true;
    }catch(const std::exception& error){const std::string message=error.what();Shutdown();m_Error=message;return false;}
}
void C_PlatformManager::Shutdown(){m_Multisamples.clear();m_VideoModes.clear();m_Adapter.Destroy();DestroyErrors();}
const C_Multisample& C_PlatformManager::GetMultisample(S32 index)const{if(index<0||index>=GetMultisampleCount())throw std::out_of_range("Native multisample index is invalid");return m_Multisamples[index];}
const C_VideoMode& C_PlatformManager::GetVideoMode(S32 index)const{if(index<0||index>=GetVideoModeCount())throw std::out_of_range("Native video-mode index is invalid");return m_VideoModes[index];}
S32 C_PlatformManager::FindMultisample(E_Multisample value)const{for(S32 i=0;i<GetMultisampleCount();++i)if(m_Multisamples[i].m_Multisample==value)return i;return -1;}
S32 C_PlatformManager::FindVideoMode(U32 width,U32 height,E_Format format)const{for(S32 i=0;i<GetVideoModeCount();++i){const auto& value=m_VideoModes[i];if(value.m_Width==width&&value.m_Height==height&&value.m_Format==format)return i;}return -1;}
bool C_PlatformManager::GetDesktopVideoMode(C_VideoMode* result)const{if(!result)return false;auto native=CGDisplayCopyDisplayMode(CGMainDisplayID());const bool valid=modeInfo(native,*result);if(native)CGDisplayModeRelease(native);return valid;}
}
