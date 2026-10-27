// [Blur2PC] Original UI physical-control binding methods, isolated from Xt schema registration.
#include "Stdafx.h"
#include "UiXtConfig.h"
#include <input/GameInputMapping.h>
#if defined(PLATFORM_PC) || defined(PLATFORM_MACOS)
#include <input/Pad360.h>
#elif defined(PLATFORM_PS3)
#include <input/PadPS3.h>
#endif
namespace UI {
void C_ControlConfig::RemapGameControls( Input::C_GameInput* pGameInput, Input::C_Device* pPad )
{
	if( !pGameInput || !pPad )
		return;

	// Remap
	for( U32 i = Framework::E_Button_Count; i < Game::E_Button_Count; i++ )
	{
		bool changeControl = true;

		// Don't change the photo mode controls:
		switch (i)
		{
			case Game::E_Button_PhotoRoll:
			case Game::E_Button_PhotoZoom:
			case Game::E_Button_PhotoRollReset:
			case Game::E_Button_PhotoZoomReset:
				changeControl = false;
			break;
		}

		if (changeControl)
			pGameInput->SetButtonInput( i, CreateButtonInput( pPad, i ) );
	}
}

Input::C_ButtonInput* C_ControlConfig::CreateButtonInput( Input::C_Device* pPad, const U32 buttonId ) const	// [Blur2PC] const
{
#if defined(PLATFORM_PS3)
	if( m_GameFunctionA == buttonId )
		return new Input::C_ButtonInputFromDevice(pPad, Input::C_PadPS3::E_Button_Cross );
	if( m_GameFunctionB == buttonId )
		return new Input::C_ButtonInputFromDevice(pPad, Input::C_PadPS3::E_Button_Circle );
	if( m_GameFunctionX == buttonId )
		return new Input::C_ButtonInputFromDevice(pPad, Input::C_PadPS3::E_Button_Square );
	if( m_GameFunctionY == buttonId )
		return new Input::C_ButtonInputFromDevice(pPad, Input::C_PadPS3::E_Button_Triangle );
	if( m_GameFunctionLT == buttonId )
		return new Input::C_ButtonInputFromDevice(pPad, Input::C_PadPS3::E_Button_L2 );
	if( m_GameFunctionRT == buttonId )
		return new Input::C_ButtonInputFromDevice(pPad, Input::C_PadPS3::E_Button_R2 );
	if( m_GameFunctionLB == buttonId )
		return new Input::C_ButtonInputFromDevice(pPad, Input::C_PadPS3::E_Button_L1 );
	if( m_GameFunctionRB == buttonId )
		return new Input::C_ButtonInputFromDevice(pPad, Input::C_PadPS3::E_Button_R1 );
	if( m_GameFunctionLThumb == buttonId )
		return new Input::C_ButtonInputFromDevice(pPad, Input::C_PadPS3::E_Button_L3 );
	if( m_GameFunctionRThumb == buttonId )
		return new Input::C_ButtonInputFromDevice(pPad, Input::C_PadPS3::E_Button_R3 );
#elif defined(PLATFORM_PC) || defined(PLATFORM_MACOS) // [Blur2PC] Native exposes the same pad buttons.
	if( m_GameFunctionA == buttonId )
		return new Input::C_ButtonInputFromDevice(pPad, Input::C_Pad360::E_Button_A );
	if( m_GameFunctionB == buttonId )
		return new Input::C_ButtonInputFromDevice(pPad, Input::C_Pad360::E_Button_B );
	if( m_GameFunctionX == buttonId )
		return new Input::C_ButtonInputFromDevice(pPad, Input::C_Pad360::E_Button_X );
	if( m_GameFunctionY == buttonId )
		return new Input::C_ButtonInputFromDevice(pPad, Input::C_Pad360::E_Button_Y );
	if( m_GameFunctionLT == buttonId )
		return new Input::C_ButtonInputFromDevice(pPad, Input::C_Pad360::E_Button_LeftTrigger );
	if( m_GameFunctionRT == buttonId )
		return new Input::C_ButtonInputFromDevice(pPad, Input::C_Pad360::E_Button_RightTrigger );
	if( m_GameFunctionLB == buttonId )
		return new Input::C_ButtonInputFromDevice(pPad, Input::C_Pad360::E_Button_LeftShoulder );
	if( m_GameFunctionRB == buttonId )
		return new Input::C_ButtonInputFromDevice(pPad, Input::C_Pad360::E_Button_RightShoulder );
	if( m_GameFunctionLThumb == buttonId )
		return new Input::C_ButtonInputFromDevice(pPad, Input::C_Pad360::E_Button_LeftThumb );
	if( m_GameFunctionRThumb == buttonId )
		return new Input::C_ButtonInputFromDevice(pPad, Input::C_Pad360::E_Button_RightThumb );
#endif
	// Clear the mapping for this function
	return NULL;
}

}
