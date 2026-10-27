#if defined(__APPLE__) // [Blur2PC][macOS] Original manager/window metadata without Windows PCH.
#include "../device/metal/NativeDisplayIncludes.h"
#else
#include "stdafx.h"
#endif
#include "Display/Manager/Manager.h"
#include "Display/Buffer/VertexDeclarations.h"
#include "Display/Shader/VertexShaders.h"
#include "Display/Shader/PixelShaders.h"

#ifdef CFG_ALLOW_OPTIMIZE_OFF
//#pragma optimize( "", off ) // CFG_ALLOW_OPTIMIZE_OFF
#endif

namespace Display
{

C_Manager::C_Manager()
{
	BIZ_ASSERT(!g_pManager);
	g_pManager = this;
}

C_Manager::~C_Manager()
{
#if defined(PLATFORM_MACOS) // [Blur2PC][macOS] Release registered owned devices while both original singletons are valid.
    if(m_DeviceList.Size())Destroy();
#endif
	BIZ_ASSERT(m_DeviceList.Size() == 0);
	
	BIZ_ASSERT(g_pManager == this);
	g_pManager = NULL;
}

C_Device* C_Manager::CreateDevice(C_DeviceParams& deviceParams)
{
#if defined(PLATFORM_MACOS) // [Blur2PC][macOS] The original engine has one upper-device singleton.
    if(DeviceExists()){SetNativeError("An upper display device already exists");return nullptr;}
#endif
	C_Device* pDevice = new C_Device();

	if(!CreateDeviceIndirect(pDevice, deviceParams))
		SAFE_DELETE_PTR(pDevice);

	return pDevice;
}

bool C_Manager::CreateDevice(C_Device* pDevice, C_DeviceParams& deviceParams)
{
	return CreateDeviceIndirect(pDevice, deviceParams);
}

void C_Manager::DestroyDevice(C_Device* pDevice)
{
	DestroyDeviceIndirect(pDevice);

	SAFE_DELETE_PTR(pDevice);
}

bool C_Manager::CreateDeviceIndirect(C_Device* pDevice, C_DeviceParams& deviceParams)
{
#if defined(PLATFORM_MACOS) // [Blur2PC][macOS] Report native setup failures and roll back partial shader/declaration ownership.
    if(!pDevice||!IsInitialised()||pDevice!=g_pDevice||pDevice->IsCreated()||m_DeviceList.Size()){
        SetNativeError("Native device creation requires an initialized manager and an uncreated singleton device");return false;
    }
    try {
#endif
	if(!pDevice)
		return false;

	RESOURCE_MEMORY_ID("Device");

	
	if(!pDevice->Create(deviceParams)) {
#if defined(PLATFORM_MACOS) // [Blur2PC][macOS]
        SetNativeError(pDevice->GetNativeError());
#endif
        return false;
    }

	CreateVertexDeclarations();
	CreateVertexShaders();
	CreatePixelShaders();

	m_DeviceList.Insert(pDevice);
#if defined(PLATFORM_MACOS) // [Blur2PC][macOS]
    SetNativeError("");
#endif
	return true;
#if defined(PLATFORM_MACOS) // [Blur2PC][macOS] Missing native shaders must not strand the singleton or a half-created GPU device.
    } catch(const std::exception& error) {
        std::string message=error.what();
        try{DestroyVertexShaders();DestroyPixelShaders();pDevice->DestroyVolatile();pDevice->Destroy();}
        catch(const std::exception& cleanup){message+="; cleanup: ";message+=cleanup.what();}
        SetNativeError(message);return false;
    }
#endif
}

void C_Manager::DestroyDeviceIndirect(C_Device* pDevice)
{
#if defined(PLATFORM_MACOS) // [Blur2PC][macOS] Empty/failed factories own no global shader resources.
    if(!m_DeviceList.Size())return;
#endif
	if(!pDevice)
		return;
	
	DestroyVertexShaders();
	DestroyPixelShaders();

	pDevice->DestroyVolatile();
	pDevice->Destroy();

	m_DeviceList.Erase(pDevice);
}

void C_Manager::Destroy()
{
#if defined(PLATFORM_MACOS) // [Blur2PC][macOS] An empty manager may have no upper-device singleton.
    if(!m_DeviceList.Size())return;
#endif
	DestroyVertexShaders();
	DestroyPixelShaders();

	Ut::UtSet<void*>::Iterator it = m_DeviceList.Begin();

	while(it != m_DeviceList.End())
	{
		void *_pDevice = *it;
		C_Device* pDevice = (C_Device*)_pDevice;
		it++;

		pDevice->DestroyVolatile();
		pDevice->Destroy();

		SAFE_DELETE_PTR(pDevice);
	}

	m_DeviceList.Clear();
}

C_Manager* g_pManager = NULL;

void Manager(C_Manager* pManager)
{
	g_pManager = pManager;
}

}; // namespace Display
