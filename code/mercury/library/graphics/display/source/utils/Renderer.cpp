#if defined(__APPLE__) // [Blur2PC][macOS] Shared CPU Display code does not need a Win32 PCH.
#include "../device/metal/NativeDisplayIncludes.h"
#else
#include "stdafx.h"
#endif
#include "Display/Utils/Renderer.h"
#if defined(PLATFORM_MACOS) // [Blur2PC][macOS] Bind the actual upper CPU device state without the unported GPU device.
#include <display/device/DeviceBase.h>
#include <display/device/metal/NativeDeviceAccess.h>
#include <exception>
#include <stdexcept>
#else
#include "Display/Device/Device.h"
#endif

namespace Display
{
#if defined(PLATFORM_MACOS) // [Blur2PC] Keep the original failure while still rolling back partial native resources.
namespace {
template<class Rollback> [[noreturn]] void NativeRendererRollback(const Util::C_String& name,const char* stage,std::exception_ptr error,Rollback rollback)
{
	try{rollback();}
	catch(const std::exception& cleanup){LOGERROR(g_Log,"Native renderer '%s' %s rollback failed: %s",name.Chars(),stage,cleanup.what());}
	catch(...){LOGERROR(g_Log,"Native renderer '%s' %s rollback failed with a non-standard exception",name.Chars(),stage);}
	std::rethrow_exception(error);
}
}
#endif

I_Renderer::I_Renderer(const Util::C_String& name) :
	m_Name(name),
	m_Type(E_RendererType_Dynamic),
	m_pResourceBundle(0)
{
	memset(m_Created, 0, sizeof(m_Created));
	m_Initialised = false;

#if defined(PLATFORM_MACOS) // [Blur2PC][macOS]
	NativeDeviceBase().InsertRenderer(this);
#else
	Device()->InsertRenderer(this);
#endif
}

I_Renderer::~I_Renderer()
{
	assert(m_Created[E_RendererState_Involatile] == false);
	assert(m_Created[E_RendererState_Volatile] == false);
	assert(m_Initialised == false);
	
#if defined(PLATFORM_MACOS) // [Blur2PC][macOS]
	NativeDeviceBase().RemoveRenderer(this);
#else
	Device()->RemoveRenderer(this);
#endif
}

void I_Renderer::SetRendererType(const E_RendererType type)
{
	m_Type = type;
}

bool I_Renderer::Create()
{
	if(!m_Created[E_RendererState_Involatile])
	{
#if defined(PLATFORM_MACOS) // [Blur2PC] A thrown creation callback owns the same rollback as a false return.
		try{m_Created[E_RendererState_Involatile]=OnCreate();}
		catch(...){m_Created[E_RendererState_Involatile]=false;NativeRendererRollback(m_Name,"Create",std::current_exception(),[this]{OnDestroy();});}
#else
		m_Created[E_RendererState_Involatile] = OnCreate();
#endif

		// if creation failed call destroy incase of partial creation
		if(!m_Created[E_RendererState_Involatile])
			OnDestroy();

		return m_Created[E_RendererState_Involatile];
	}
	else
	{
		return true;
	}
}

bool I_Renderer::CreateVolatile()
{
	// [Blur2PC] Keep the legacy PC Bink-less compatibility result below. Native
	// movies support a genuine idle Create, so an absent native base is a failure.
	if(!m_Created[E_RendererState_Involatile])
#if defined(PLATFORM_MACOS) // [Blur2PC] An absent base renderer cannot truthfully have volatile resources ready.
		return false;
#else
		return true;
#endif
	if(!m_Created[E_RendererState_Volatile])
	{
#if defined(PLATFORM_MACOS) // [Blur2PC] Roll back the actual partially created volatile resources before propagating.
		try{m_Created[E_RendererState_Volatile]=OnCreateVolatile();}
		catch(...){m_Created[E_RendererState_Volatile]=false;NativeRendererRollback(m_Name,"CreateVolatile",std::current_exception(),[this]{OnDestroyVolatile();});}
#else
		m_Created[E_RendererState_Volatile] = OnCreateVolatile();
#endif

		if(!m_Created[E_RendererState_Volatile])
			OnDestroyVolatile();

		return m_Created[E_RendererState_Volatile];
	}
	else
	{
		return true;
	}
}

void I_Renderer::DestroyVolatile()
{
	if(m_Created[E_RendererState_Volatile])
	{
		OnDestroyVolatile();
		m_Created[E_RendererState_Volatile] = false;
	}
}

void I_Renderer::Destroy()
{
	if(m_Created[E_RendererState_Involatile])
	{
		OnDestroy();
		m_Created[E_RendererState_Involatile] = false;
	}
}

bool I_Renderer::Initialise()
{
	// return true if system is already initialised
	if(m_Initialised)
		return true;

#if defined(PLATFORM_MACOS) // [Blur2PC] Initialization failures follow their existing OnShutdown rollback contract too.
	try{m_Initialised=OnInitialise();}
	catch(...){m_Initialised=false;NativeRendererRollback(m_Name,"Initialise",std::current_exception(),[this]{OnShutdown();});}
#else
	m_Initialised = OnInitialise();
#endif
	
	// if system failed to initialise call shutdown incase of a partial initialisation
	if(!m_Initialised)
		OnShutdown();

	return m_Initialised;
}

bool I_Renderer::Reinitialise()
{
	// if system is already initialised call the shutdown method
	if(m_Initialised)
		Shutdown();

	return Initialise();
}

void I_Renderer::Shutdown()
{
#if defined(PLATFORM_MACOS) // [Blur2PC] Failed Initialise already rolled back; repeated shutdown must not repeat that callback.
	if(!m_Initialised)return;
#endif
	OnShutdown();
	m_Initialised = false;
}

void I_Renderer::Reset()
{
	OnReset();
}

bool I_Renderer::PreResize()
{
	return OnPreResize();
}

bool I_Renderer::PostResize()
{
	return OnPostResize();
}

bool I_Renderer::OnCreate()
{
	return true;
}

bool I_Renderer::OnCreateVolatile()
{
	return true;
}

void I_Renderer::OnDestroyVolatile()
{
}

void I_Renderer::OnDestroy()
{
}

bool I_Renderer::OnInitialise()
{
	return true;
}

void I_Renderer::OnShutdown()
{
}

void I_Renderer::OnReset()
{
}

bool I_Renderer::OnPreResize()
{
	return true;
}

bool I_Renderer::OnPostResize()
{
	return true;
}

void I_Renderer::DestroyRenderer()
{
	DestroyVolatile();
	Destroy();
}

}; // namespace Display
