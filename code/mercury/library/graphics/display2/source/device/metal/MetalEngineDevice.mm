// [Blur2PC][macOS] Real adapter ownership and shared engine create/volatile/destroy stages.
#include <display2/device/MetalEngineDevice.h>
#include "MetalNative.h"
namespace Display2 {
Util::C_String g_VertexShaderFileExt="bmsl";
Util::C_String g_PixelShaderFileExt="bmsl";
C_Device::~C_Device(){try{Destroy();}catch(...){C_MetalDevice::Destroy();}}
void C_Device::GetParams(C_DeviceParams& params)const{params=m_Params;}
void C_Device::SetParams(const C_DeviceParams& params){if(m_Stage)throw std::logic_error("Device parameters require a destroyed engine device");m_Params=params;}
bool C_Device::Create(){
    if(m_Stage)return false;
    if(!CreateNative(m_Params.m_Adapter?(__bridge void*)m_Params.m_Adapter->device:nullptr))return false;
    m_Stage=1;return true;
}
bool C_Device::CreateVolatile(){
    if(m_Stage!=1)return false;
    if(m_Params.m_PresentationLayer&&!SetPresentationLayer(m_Params.m_PresentationLayer,m_Params.m_Width,m_Params.m_Height,m_Params.m_VSync))return false;
    m_Stage=2;return true;
}
void C_Device::DestroyVolatile(){
    if(m_Stage!=2)return;
    if(!WaitIdle())throw std::runtime_error("GPU failed during engine device shutdown");
    if(!SetPresentationLayer(nullptr,0,0))throw std::logic_error("Host device shutdown requires the main thread");
    m_Stage=1;
}
void C_Device::Destroy(){if(!m_Stage)return;DestroyVolatile();C_MetalDevice::Destroy();m_Stage=0;}
bool C_DeviceManager::Create(const C_DeviceManagerParams& params){@autoreleasepool{
    if(m_Adapter)return false;
    auto native=std::make_shared<Metal::Adapter>();native->device=MTLCreateSystemDefaultDevice();if(!native->device)return false;
    m_Params=params;m_Adapter=std::move(native);return true;
}}
void C_DeviceManager::Destroy(){m_Adapter.reset();}
void C_DeviceManager::GetParams(C_DeviceManagerParams& params)const{params=m_Params;}
void C_DeviceManager::SetParams(const C_DeviceManagerParams& params){m_Params=params;}
void C_DeviceManager::SetupDefaultDeviceParams(C_DeviceParams& params){if(!m_Adapter)throw std::logic_error("Native device manager is not created");params=C_DeviceParams{};params.m_Adapter=m_Adapter;}
std::string C_DeviceManager::GetDeviceName()const{@autoreleasepool{return m_Adapter?m_Adapter->device.name.UTF8String:"";}}
}
