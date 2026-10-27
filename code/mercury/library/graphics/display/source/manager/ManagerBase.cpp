#if defined(__APPLE__) // [Blur2PC][macOS] Original manager/window metadata without Windows PCH.
#include "../device/metal/NativeDisplayIncludes.h"
#else
#include "stdafx.h"
#endif
#include <core\containers\sortedbinarytree.h>
#include "Display/Manager/ManagerBase.h"

#ifdef CFG_ALLOW_OPTIMIZE_OFF
//#pragma optimize( "", off ) // CFG_ALLOW_OPTIMIZE_OFF
#endif

namespace Display
{

Util::C_Array<C_Error, E_Error_Count> C_ManagerBase::m_ErrorList;

C_ManagerBase::C_ManagerBase()
{
}

C_ManagerBase::~C_ManagerBase()
{
}

void C_ManagerBase::SetError(const E_Error error, const Util::C_String& text)
{
	m_ErrorList[error].m_Text = text;
}

void C_ManagerBase::PrintError(const E_Error error)
{
#ifdef PLATFORM_PC
	MessageBox(NULL, m_ErrorList[error].m_Text.Chars(), "ERROR", MB_OK);
#else
	LOGFATAL(g_Log, "%s\n", m_ErrorList[error].m_Text.Chars());
#endif
}

void C_ManagerBase::CreateErrors()
{
#if defined(PLATFORM_MACOS) // [Blur2PC][macOS] Retain category IDs while describing the native backend accurately.
    m_ErrorList[E_Error_NotSupported_D3D9].m_Text="Native Metal platform setup failed.";
    m_ErrorList[E_Error_NotSupported_HAL].m_Text="A native Metal hardware device is unavailable.";
    m_ErrorList[E_Error_NotSupported_PS20].m_Text="Native pixel shader loading failed.";
    m_ErrorList[E_Error_NotSupported_VS20].m_Text="Native vertex shader loading failed.";
    m_ErrorList[E_Error_NotSupported_CAPS].m_Text="A required native graphics capability is unavailable.";
#else
	m_ErrorList[E_Error_NotSupported_D3D9].m_Text	= "Your graphics card does not support Direct3D9. Press ok to exit the game.";
	m_ErrorList[E_Error_NotSupported_HAL].m_Text	= "Your graphics card does not support Hardware Acceleration. Press ok to exit the game.";
	m_ErrorList[E_Error_NotSupported_PS20].m_Text	= "Your graphics card does not support Pixel Shader Model 2.0. Press ok to exit the game.";
	m_ErrorList[E_Error_NotSupported_VS20].m_Text	= "Your graphics card does not support Vertex Shader Model 2.0. Press ok to exit the game.";
	m_ErrorList[E_Error_NotSupported_CAPS].m_Text	= "Your graphics card does not support the minimum specs. Press ok to exit the game.";
#endif
}

void C_ManagerBase::DestroyErrors()
{
	for(S32 i=0; i<m_ErrorList.Size(); i++)
	{
		m_ErrorList[i].m_Text = "";
	}
}

}; // namespace Display
