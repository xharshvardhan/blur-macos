// [Blur2PC][macOS] Upper Display's active CPU device state, independent of a Win32 device declaration.
#pragma once
namespace Display {
class C_DeviceBase;
C_DeviceBase& NativeDeviceBase();
class C_NativeDeviceBinding {
public:
    explicit C_NativeDeviceBinding(C_DeviceBase& device);
    ~C_NativeDeviceBinding();
    C_NativeDeviceBinding(const C_NativeDeviceBinding&)=delete;
    C_NativeDeviceBinding& operator=(const C_NativeDeviceBinding&)=delete;
private:C_DeviceBase* m_Device;
};
}
