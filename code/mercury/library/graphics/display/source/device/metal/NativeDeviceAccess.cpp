// [Blur2PC][macOS] Explicit lifetime for the original singleton device's CPU services.
#include <display/device/metal/NativeDeviceAccess.h>
#include <atomic>
#include <stdexcept>
#include <exception>
namespace Display {
static std::atomic<C_DeviceBase*> active{nullptr};
C_DeviceBase& NativeDeviceBase(){auto* device=active.load();if(!device)throw std::logic_error("Native upper Display device is not bound");return *device;}
C_NativeDeviceBinding::C_NativeDeviceBinding(C_DeviceBase& device):m_Device(&device){C_DeviceBase* expected=nullptr;if(!active.compare_exchange_strong(expected,m_Device))throw std::logic_error("A native upper Display device is already bound");}
C_NativeDeviceBinding::~C_NativeDeviceBinding(){auto* expected=m_Device;if(!active.compare_exchange_strong(expected,nullptr))std::terminate();}
}
