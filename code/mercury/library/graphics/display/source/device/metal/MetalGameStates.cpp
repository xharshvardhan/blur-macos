// [Blur2PC][macOS] Original upper PC blend table from D3D11GameDevice.cpp, with native state/CPU-cache setters.
#include "NativeDisplayIncludes.h"
#include <display/device/metal/MetalGameDevice.h>
#include <stdexcept>
#include <string>
namespace Display::Metal {
void C_GameDevice::SetAlphaTestEnable(E_Bool value){m_Display2.GetContext().SetAlphaTestEnable(ToBool2(value));m_AlphaTestEnable=value;m_BlendMode=E_BlendMode_Unknown;}
void C_GameDevice::SetAlphaTestFunc(E_CmpFunc value){m_Display2.GetContext().SetAlphaTestFunc(ToCmpFunc2(value));m_AlphaTestFunc=value;m_BlendMode=E_BlendMode_Unknown;}
void C_GameDevice::SetAlphaTestRef(U32 value){m_Display2.GetContext().SetAlphaTestRef(value);m_AlphaTestRef=value;m_BlendMode=E_BlendMode_Unknown;}
void C_GameDevice::SetBlendEnable(E_Bool value){m_Display2.GetContext().SetBlendEnable(ToBool2(value));m_BlendEnable=value;m_BlendMode=E_BlendMode_Unknown;}
void C_GameDevice::SetBlendSrc(E_Blend value){m_Display2.GetContext().SetBlendSrc(ToBlend2(value));m_BlendSrc=value;m_BlendMode=E_BlendMode_Unknown;}
void C_GameDevice::SetBlendDst(E_Blend value){m_Display2.GetContext().SetBlendDst(ToBlend2(value));m_BlendDst=value;m_BlendMode=E_BlendMode_Unknown;}
void C_GameDevice::SetBlendOp(E_BlendOp value){m_Display2.GetContext().SetBlendOp(ToBlendOp2(value));m_BlendOp=value;m_BlendMode=E_BlendMode_Unknown;}
void C_GameDevice::SetBlendSrcAlpha(E_Blend value){m_Display2.GetContext().SetBlendSrcAlpha(ToBlend2(value));m_BlendSrcAlpha=value;m_BlendMode=E_BlendMode_Unknown;}
void C_GameDevice::SetBlendDstAlpha(E_Blend value){m_Display2.GetContext().SetBlendDstAlpha(ToBlend2(value));m_BlendDstAlpha=value;m_BlendMode=E_BlendMode_Unknown;}
void C_GameDevice::SetBlendOpAlpha(E_BlendOp value){m_Display2.GetContext().SetBlendOpAlpha(ToBlendOp2(value));m_BlendOpAlpha=value;m_BlendMode=E_BlendMode_Unknown;}
void C_GameDevice::SetColourWriteEnable(U32 value){if(value==E_ColourWriteEnable_Default)value=m_ColourWriteEnableDefault;m_Display2.GetContext().SetColourWriteEnable(value);m_ColourWriteEnable=value;m_BlendMode=E_BlendMode_Unknown;}
void C_GameDevice::SetBlendMode(const E_BlendMode blendMode)
{
    // Unknown is the original Simple2d cache-invalidation sentinel; Default uses
    // the original PC switch's disabled-alpha/disabled-blend RGB-write branch.
    if((U32(blendMode)>=E_BlendMode_Count&&blendMode!=E_BlendMode_Default&&blendMode!=E_BlendMode_Unknown)||blendMode==E_BlendMode_SolidAlphaMask)
        throw std::logic_error("Unsupported native device blend mode "+std::to_string(U32(blendMode)));
    if(blendMode==E_BlendMode_Unknown){m_BlendMode=blendMode;return;}
    m_BlendMode=E_BlendMode_Unknown; // Other immediate contexts can change the same GPU state; reapply the original table.
	if(m_BlendMode != blendMode)
    {
        switch(blendMode)
        {
			case E_BlendMode_Solid:
			{
				SetAlphaTestEnable(E_Bool_False);

				SetBlendEnable(E_Bool_False);

				if(m_LuminanceWrite == E_LuminanceWrite_Enabled)
					SetColourWriteEnable(E_ColourWriteEnable_All);

			}
			break;

			case E_BlendMode_SolidAlphaTest:
			{
				SetAlphaTestEnable(E_Bool_True);
				SetAlphaTestFunc(E_CmpFunc_Greater);
				SetAlphaTestRef(0x80);

				SetBlendEnable(E_Bool_False);

				if(m_LuminanceWrite == E_LuminanceWrite_Enabled)
					SetColourWriteEnable(E_ColourWriteEnable_Red|E_ColourWriteEnable_Green|E_ColourWriteEnable_Blue);

			}
			break;

			case E_BlendMode_Alpha:
			{
				SetAlphaTestEnable(E_Bool_True);
				SetAlphaTestFunc(E_CmpFunc_Greater);
				SetAlphaTestRef(0x00);

				SetBlendEnable(E_Bool_True);
				SetBlendSrc(E_Blend_SrcAlpha);
				SetBlendDst(E_Blend_InvSrcAlpha);
				SetBlendOp(E_BlendOp_Add);
				SetBlendSrcAlpha(E_Blend_SrcAlpha);
				SetBlendDstAlpha(E_Blend_InvSrcAlpha);
				SetBlendOpAlpha(E_BlendOp_Add);

				if(m_LuminanceWrite == E_LuminanceWrite_Enabled)
					SetColourWriteEnable(E_ColourWriteEnable_Red|E_ColourWriteEnable_Green|E_ColourWriteEnable_Blue);

			}
			break;

			case E_BlendMode_AlphaPreMultiply:
			{
				SetAlphaTestEnable(E_Bool_False);
				SetAlphaTestFunc(E_CmpFunc_Greater);
				SetAlphaTestRef(0x00);

				SetBlendEnable(E_Bool_True);
				SetBlendSrc(E_Blend_One);
				SetBlendDst(E_Blend_InvSrcAlpha);
				SetBlendOp(E_BlendOp_Add);
				SetBlendSrcAlpha(E_Blend_One);
				SetBlendDstAlpha(E_Blend_InvSrcAlpha);
				SetBlendOpAlpha(E_BlendOp_Add);

				if(m_LuminanceWrite == E_LuminanceWrite_Enabled)
					SetColourWriteEnable(E_ColourWriteEnable_Red|E_ColourWriteEnable_Green|E_ColourWriteEnable_Blue);

			}
			break;

			case E_BlendMode_Additive:
			{
				SetAlphaTestEnable(E_Bool_True);

				SetBlendEnable(E_Bool_True);
				SetBlendSrc(E_Blend_One);
				SetBlendDst(E_Blend_One);
				SetBlendOp(E_BlendOp_Add);
				SetBlendSrcAlpha(E_Blend_One);
				SetBlendDstAlpha(E_Blend_One);
				SetBlendOpAlpha(E_BlendOp_Add);

				if(m_LuminanceWrite == E_LuminanceWrite_Enabled)
					SetColourWriteEnable(E_ColourWriteEnable_Red|E_ColourWriteEnable_Green|E_ColourWriteEnable_Blue);

			}
			break;

			case E_BlendMode_AlphaAdditive:
			{
        		SetAlphaTestEnable(E_Bool_True);
				SetAlphaTestFunc(E_CmpFunc_Greater);
				SetAlphaTestRef(0x00);

				SetBlendEnable(E_Bool_True);
				SetBlendSrc(E_Blend_SrcAlpha);
				SetBlendDst(E_Blend_One);
				SetBlendOp(E_BlendOp_Add);
				SetBlendSrcAlpha(E_Blend_SrcAlpha);
				SetBlendDstAlpha(E_Blend_One);
				SetBlendOpAlpha(E_BlendOp_Add);

				if(m_LuminanceWrite == E_LuminanceWrite_Enabled)
					SetColourWriteEnable(E_ColourWriteEnable_Red|E_ColourWriteEnable_Green|E_ColourWriteEnable_Blue);

			}
			break;

			case E_BlendMode_AlphaSubtractive:
			{
        		SetAlphaTestEnable(E_Bool_True);
				SetAlphaTestFunc(E_CmpFunc_Greater);
				SetAlphaTestRef(0x00);

				SetBlendEnable(E_Bool_True);
				SetBlendSrc(E_Blend_SrcAlpha);
				SetBlendDst(E_Blend_One);
				SetBlendOp(E_BlendOp_RevSubtract);
				SetBlendSrcAlpha(E_Blend_SrcAlpha);
				SetBlendDstAlpha(E_Blend_One);
				SetBlendOpAlpha(E_BlendOp_RevSubtract);

				if(m_LuminanceWrite == E_LuminanceWrite_Enabled)
					SetColourWriteEnable(E_ColourWriteEnable_Red|E_ColourWriteEnable_Green|E_ColourWriteEnable_Blue);

			}
			break;

			case E_BlendMode_Subtractive:
			{
				SetAlphaTestEnable(E_Bool_False);

				SetBlendEnable(E_Bool_True);
				SetBlendSrc(E_Blend_One);
				SetBlendDst(E_Blend_One);
				SetBlendOp(E_BlendOp_RevSubtract);
				SetBlendSrcAlpha(E_Blend_One);
				SetBlendDstAlpha(E_Blend_One);
				SetBlendOpAlpha(E_BlendOp_RevSubtract);

				if(m_LuminanceWrite == E_LuminanceWrite_Enabled)
					SetColourWriteEnable(E_ColourWriteEnable_Red|E_ColourWriteEnable_Green|E_ColourWriteEnable_Blue);

			}
			break;

			case E_BlendMode_Multiply:
			{
				SetAlphaTestEnable(E_Bool_False);

				SetBlendEnable(E_Bool_True);
				SetBlendSrc(E_Blend_Zero);
				SetBlendDst(E_Blend_SrcColour);
				SetBlendOp(E_BlendOp_Add);
				SetBlendSrcAlpha(E_Blend_Zero);
				SetBlendDstAlpha(E_Blend_SrcColour);
				SetBlendOpAlpha(E_BlendOp_Add);

				if(m_LuminanceWrite == E_LuminanceWrite_Enabled)
					SetColourWriteEnable(E_ColourWriteEnable_Red|E_ColourWriteEnable_Green|E_ColourWriteEnable_Blue);

			}
			break;

			case E_BlendMode_AdditiveSmooth:
			{
				SetAlphaTestEnable(E_Bool_True);

				SetBlendEnable(E_Bool_True);
				SetBlendSrc(E_Blend_One);
				SetBlendDst(E_Blend_InvSrcColour);
				SetBlendOp(E_BlendOp_Add);
				SetBlendSrcAlpha(E_Blend_One);
				SetBlendDstAlpha(E_Blend_InvSrcColour);
				SetBlendOpAlpha(E_BlendOp_Add);

				if(m_LuminanceWrite == E_LuminanceWrite_Enabled)
					SetColourWriteEnable(E_ColourWriteEnable_Red|E_ColourWriteEnable_Green|E_ColourWriteEnable_Blue);

			}
			break;

			case E_BlendMode_Unknown:
				break;

			default:
			{
				SetAlphaTestEnable(E_Bool_False);

				SetBlendEnable(E_Bool_False);

				if(m_LuminanceWrite == E_LuminanceWrite_Enabled)
					SetColourWriteEnable(E_ColourWriteEnable_Red|E_ColourWriteEnable_Green|E_ColourWriteEnable_Blue);

			}
			break;
        }

        m_BlendMode = blendMode;
    }
}
}

// [Blur2PC][macOS] Original upper PC default/cull-flip selection, forwarding to the checked native context.
namespace Display::Metal {
void C_GameDevice::SetDepthEnable(E_Bool value){if(value==E_Bool_Default)value=m_DepthEnableDefault;m_Display2.GetContext().SetDepthEnable(ToBool2(value));m_DepthEnable=value;}
void C_GameDevice::SetDepthWriteEnable(E_Bool value){if(value==E_Bool_Default)value=m_DepthWriteEnableDefault;m_Display2.GetContext().SetDepthWriteEnable(ToBool2(value));m_DepthWriteEnable=value;}
void C_GameDevice::SetDepthFunc(E_CmpFunc value){if(value==E_CmpFunc_Default)value=m_DepthFuncDefault;m_Display2.GetContext().SetDepthFunc(ToCmpFunc2(value));m_DepthFunc=value;}
void C_GameDevice::SetCullMode(E_CullMode value){
    if(value==E_CullMode_Front)value=E_CullMode_Reversed;else if(value==E_CullMode_Back)value=E_CullMode_Default;
    if(value==E_CullMode_Default)value=m_CullModeDefault;else if(value==E_CullMode_Reversed)value=FlippedCullMode(m_CullModeDefault);
    if(m_CullModeFlip)value=FlippedCullMode(value);m_Display2.GetContext().SetCullMode(ToCullMode2(value));m_CullMode=value;
}
void C_GameDevice::SetViewport(const C_Viewport& value){
    if(m_DepthViewportFlip)throw std::logic_error("Native flipped depth viewport requires reverse-range integration");
    m_Display2.GetContext().SetViewport(Display2::C_Viewport(value.GetX(),value.GetY(),value.GetW(),value.GetH(),value.GetMinZ(),value.GetMaxZ()));m_Viewport=value;
}
}
