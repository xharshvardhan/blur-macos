#if defined(__APPLE__) // [Blur2PC][macOS] Compile the original state/draw forwarding without Win32 PCH.
#include "../device/metal/NativeDisplayIncludes.h"
#include <string>
#include <display/device/Device.h>
#else
#include "stdafx.h"
#endif
#include "Display/Context/Context_Base.h"
#include "Display/DisplayToDisplay2.h"

#if PLATFORM_SPU
//#define CONTEXT_BASE_STUB 1
#endif

namespace Display
{

C_ContextBase::C_ContextBase()
{
	m_CullMode				= E_CullMode_Unknown;
	m_Multisample			= E_Multisample_None;
	m_pVertexDeclaration	= NULL;
	m_pIndexBuffer			= NULL;
#if defined(PLATFORM_MACOS) // [Blur2PC][macOS] Deterministic metadata before CloneState/SetWindow.
    m_pColourTexture = m_pDepthStencilTexture = NULL;
#endif
#if PLATFORM_PC || defined(PLATFORM_MACOS) // [Blur2PC][macOS] PC stencil-mask metadata.
	m_pStencilTexture		= NULL;	// [Blur2PC]
#endif

#ifndef PLATFORM_SPU
	m_pView = new Display::C_View;
#else
	m_pView = NULL;
#endif
}

C_ContextBase::~C_ContextBase()
{
#ifndef PLATFORM_SPU
	delete m_pView;
#endif
}

void C_ContextBase::SetVertexDeclaration(const Display2::C_VertexDeclaration* pVertexDeclaration)
{
	#if CONTEXT_BASE_STUB
	return;
	#endif

	m_Context.SetVertexDeclaration(pVertexDeclaration);
	
	m_pVertexDeclaration = pVertexDeclaration;
}

void C_ContextBase::SetVertexBuffer(const U32 stream, const Display2::C_VertexBuffer* pVertexBuffer, const U32 offset, const U32 stride)
{
	#if CONTEXT_BASE_STUB
	return;
	#endif

#if defined(PLATFORM_MACOS) // [Blur2PC] Match Context_D3D11: a VB bind preserves the device's current declaration.
	m_Context.SetVertexBuffer(stream, pVertexBuffer, offset, stride);
#else
	m_Context.SetVertexBuffer(stream, pVertexBuffer, offset, stride, m_pVertexDeclaration);
#endif
}

void C_ContextBase::SetIndexBuffer(const Display2::C_IndexBuffer* pIndexBuffer)
{
	#if CONTEXT_BASE_STUB
	return;
	#endif

	m_Context.SetIndexBuffer(pIndexBuffer);

	m_pIndexBuffer = pIndexBuffer;
}

void C_ContextBase::DrawPrimitive(const Display2::E_PrimitiveType primitiveType, const U32 startVertex, const U32 vertexCount)
{
	#if CONTEXT_BASE_STUB
	return;
	#endif

	m_Context.DrawPrimitive(primitiveType, startVertex, vertexCount);
}

void C_ContextBase::DrawIndexedPrimitive(const Display2::E_PrimitiveType primitiveType, const U32 baseVertex, const U32 minIndex, const U32 vertexCount, const U32 startIndex, const U32 indexCount)
{
	#if CONTEXT_BASE_STUB
	return;
	#endif

#if defined(PLATFORM_MACOS) // [Blur2PC] Original PC context draws with the current device index binding, not a borrowed cached wrapper.
	m_Context.DrawIndexedPrimitive(primitiveType, baseVertex, minIndex, vertexCount, startIndex, indexCount);
#else
	m_Context.DrawIndexedPrimitive(primitiveType, baseVertex, minIndex, vertexCount, startIndex, indexCount, m_pIndexBuffer);
#endif
}

void C_ContextBase::SetVertexShaderConstantF(const U32 startRegister, const F32* pData, const U32 registerCount)
{
	#if CONTEXT_BASE_STUB
	return;
	#endif

	m_Context.SetVertexShaderConstantF(startRegister, pData, registerCount);
}

void C_ContextBase::SetVertexShader(const Display2::C_VertexShader* pShader)
{
	#if CONTEXT_BASE_STUB
	return;
	#endif

	m_Context.SetVertexShader(pShader);
}

void C_ContextBase::SetPixelShaderConstantF(const U32 startRegister, const F32* pData, const U32 registerCount)
{
	#if CONTEXT_BASE_STUB
	return;
	#endif

#if !PLATFORM_PPU_SPU
	m_Context.SetPixelShaderConstantF(startRegister, pData, registerCount);
#endif
}

void C_ContextBase::SetPixelShader(const Display2::C_PixelShader* pShader)
{
	#if CONTEXT_BASE_STUB
	return;
	#endif

	m_Context.SetPixelShader(pShader);
}

void C_ContextBase::SetPixelShaderParameterF(C_PixelShaderParameter parameter, const F32* pData)
{
	#if CONTEXT_BASE_STUB
	return;
	#endif

	m_Context.SetPixelShaderParamF(parameter, pData);
}

void C_ContextBase::SetPixelShaderParameterArrayF(const C_PixelShaderParameterArray& parameterArray, const F32* pData)
{
	#if CONTEXT_BASE_STUB
	return;
	#endif

	m_Context.SetPixelShaderParamArrayF(parameterArray, pData);
}

void C_ContextBase::SetPixelShaderParameters(const C_PixelShader* pPixelShader)
{
	#if CONTEXT_BASE_STUB
	return;
	#endif

	#if PLATFORM_PPU_SPU
	m_Context.SetPixelShaderParams(pPixelShader);
	#endif
}

void C_ContextBase::SetBlendEnable(const E_Bool blendEnable)
{
	#if CONTEXT_BASE_STUB
	return;
	#endif

	m_Context.SetBlendEnable((Display2::E_Bool)blendEnable);
}

void C_ContextBase::SetColourWriteEnable(const U32 flags)
{
	#if CONTEXT_BASE_STUB
	return;
	#endif

#if defined(PLATFORM_MACOS) // [Blur2PC][macOS] Original PC context resolves upper policy sentinels through its device.
    Display::Device()->SetColourWriteEnable(flags);
#else
	m_Context.SetColourWriteEnable(flags);
#endif
}

void C_ContextBase::SetCullMode(const E_CullMode cullMode)
{
	#if CONTEXT_BASE_STUB
	return;
	#endif

	Display2::C_CullState cullState;

	switch(cullMode)
	{
	case E_CullMode_Default:
		cullState.SetCullMode((Display2::E_CullMode)m_CullMode);
		break;
	case E_CullMode_Reversed:
		switch(m_CullMode)
		{
		case E_CullMode_Cw:
			cullState.SetCullMode(Display2::E_CullMode_Ccw);
			break;
		case E_CullMode_Ccw:
			cullState.SetCullMode(Display2::E_CullMode_Cw);
			break;
		default:
			cullState.SetCullMode(Display2::E_CullMode_None);
			break;
		}
		break;
	default:
		cullState.SetCullMode(ToCullMode2(cullMode));
		break;
	}

	m_Context.SetCullState(cullState);
}

void C_ContextBase::SetDepthEnable(const E_Bool depthEnable)
{
	#if CONTEXT_BASE_STUB
	return;
	#endif

	m_Context.SetDepthEnable((Display2::E_Bool)depthEnable);
}

void C_ContextBase::SetDepthFunc(const E_CmpFunc depthFunc)
{
	#if CONTEXT_BASE_STUB
	return;
	#endif

	m_Context.SetDepthFunc((Display2::E_CmpFunc)depthFunc);
}

void C_ContextBase::SetDepthWriteEnable(const E_Bool depthWriteEnable)
{
	#if CONTEXT_BASE_STUB
	return;
	#endif
	
	m_Context.SetDepthWriteEnable((Display2::E_Bool)depthWriteEnable);
}

void C_ContextBase::SetFillMode(E_FillMode fillMode)
{
	#if CONTEXT_BASE_STUB
	return;
	#endif

	m_Context.SetFillMode((Display2::E_FillMode)fillMode);
}

void C_ContextBase::SetStencilState(const C_StencilState& stencilState)
{
	#if CONTEXT_BASE_STUB
	return;
	#endif

	m_Context.SetStencilEnable((Display2::E_Bool)stencilState.m_StencilEnable);

	Display2::C_StencilState stencilState2;
	stencilState2.SetStencilFunc((Display2::E_CmpFunc)stencilState.m_StencilFunc);
	stencilState2.SetStencilFail((Display2::E_StencilOp)stencilState.m_StencilFail);
	stencilState2.SetStencilDepthFail((Display2::E_StencilOp)stencilState.m_StencilDepthFail);
	stencilState2.SetStencilDepthPass((Display2::E_StencilOp)stencilState.m_StencilDepthPass);
	stencilState2.SetStencilRef(stencilState.m_StencilRef);
	stencilState2.SetStencilMask(stencilState.m_StencilMask);
	stencilState2.SetStencilWriteMask(stencilState.m_StencilWriteMask);
	m_Context.SetStencilState(stencilState2);
}

void C_ContextBase::SetBlendMode(E_BlendMode blendMode)
{
#if defined(PLATFORM_MACOS) // [Blur2PC][macOS] Retain the original shared table's Default/Unknown state semantics.
    if((U32(blendMode)>=E_BlendMode_Count&&blendMode!=E_BlendMode_Default&&blendMode!=E_BlendMode_Unknown)||blendMode==E_BlendMode_SolidAlphaMask)
        throw std::logic_error("Unsupported native context blend mode "+std::to_string(U32(blendMode)));
#endif
	#if CONTEXT_BASE_STUB
	return;
	#endif

	Display2::C_BlendState blendState;
	Display2::C_AlphaTestState alphaTestState;

	blendState.SetDefaultStates();
	alphaTestState.SetDefaultStates();

	switch(blendMode)
	{
		case E_BlendMode_Solid:
		{
			m_Context.SetAlphaTestEnable(Display2::E_Bool_False);
			m_Context.SetBlendEnable(Display2::E_Bool_False);
		}
		break;

		case E_BlendMode_SolidAlphaTest:
		{
			m_Context.SetAlphaTestEnable(Display2::E_Bool_True);
			m_Context.SetBlendEnable(Display2::E_Bool_False);

			alphaTestState.SetAlphaTestCmpFunc(Display2::E_CmpFunc_Greater);
			alphaTestState.SetAlphaTestRef(0x80);
		}
		break;

		case E_BlendMode_Alpha:
		{
			m_Context.SetAlphaTestEnable(Display2::E_Bool_True);
			m_Context.SetBlendEnable(Display2::E_Bool_True);

			alphaTestState.SetAlphaTestCmpFunc(Display2::E_CmpFunc_Greater);
			alphaTestState.SetAlphaTestRef(0x00);
			
			blendState.SetBlendColourSrc(Display2::E_Blend_SrcAlpha);
			blendState.SetBlendColourDst(Display2::E_Blend_InvSrcAlpha);
			blendState.SetBlendColourOp(Display2::E_BlendOp_Add);
		}
		break;

		case E_BlendMode_AlphaPreMultiply:
		{
			m_Context.SetAlphaTestEnable(Display2::E_Bool_False);
			m_Context.SetBlendEnable(Display2::E_Bool_True);

			alphaTestState.SetAlphaTestCmpFunc(Display2::E_CmpFunc_Greater);
			alphaTestState.SetAlphaTestRef(0x00);
			
			blendState.SetBlendColourSrc(Display2::E_Blend_One);
			blendState.SetBlendColourDst(Display2::E_Blend_InvSrcAlpha);
			blendState.SetBlendColourOp(Display2::E_BlendOp_Add);
		}
		break;

		case E_BlendMode_Additive:
		{
			m_Context.SetAlphaTestEnable(Display2::E_Bool_True);
			m_Context.SetBlendEnable(Display2::E_Bool_True);

			alphaTestState.SetAlphaTestCmpFunc(Display2::E_CmpFunc_Greater);
			alphaTestState.SetAlphaTestRef(0x00);

			blendState.SetBlendColourSrc(Display2::E_Blend_One);
			blendState.SetBlendColourDst(Display2::E_Blend_One);
			blendState.SetBlendColourOp(Display2::E_BlendOp_Add);
		}
		break;

		case E_BlendMode_AlphaAdditive:
		{
    		m_Context.SetAlphaTestEnable(Display2::E_Bool_True);
			m_Context.SetBlendEnable(Display2::E_Bool_True);

			alphaTestState.SetAlphaTestCmpFunc(Display2::E_CmpFunc_Greater);
			alphaTestState.SetAlphaTestRef(0x00);
			
			blendState.SetBlendColourSrc(Display2::E_Blend_SrcAlpha);
			blendState.SetBlendColourDst(Display2::E_Blend_One);
			blendState.SetBlendColourOp(Display2::E_BlendOp_Add);
		}
		break;

		case E_BlendMode_AlphaSubtractive:
		{
    		m_Context.SetAlphaTestEnable(Display2::E_Bool_True);
			m_Context.SetBlendEnable(Display2::E_Bool_True);

			alphaTestState.SetAlphaTestCmpFunc(Display2::E_CmpFunc_Greater);
			alphaTestState.SetAlphaTestRef(0x00);

			blendState.SetBlendColourSrc(Display2::E_Blend_SrcAlpha);
			blendState.SetBlendColourDst(Display2::E_Blend_One);
			blendState.SetBlendColourOp(Display2::E_BlendOp_RevSubtract);
		}
		break;

		case E_BlendMode_Subtractive:
		{
			m_Context.SetAlphaTestEnable(Display2::E_Bool_True);
			m_Context.SetBlendEnable(Display2::E_Bool_True);

			alphaTestState.SetAlphaTestCmpFunc(Display2::E_CmpFunc_Greater);
			alphaTestState.SetAlphaTestRef(0x00);

			blendState.SetBlendColourSrc(Display2::E_Blend_One);
			blendState.SetBlendColourDst(Display2::E_Blend_One);
			blendState.SetBlendColourOp(Display2::E_BlendOp_RevSubtract);
		}
		break;

		case E_BlendMode_Multiply:
		{
			m_Context.SetAlphaTestEnable(Display2::E_Bool_False);
			m_Context.SetBlendEnable(Display2::E_Bool_True);
			
			blendState.SetBlendColourSrc(Display2::E_Blend_Zero);
			blendState.SetBlendColourDst(Display2::E_Blend_SrcColour);
			blendState.SetBlendColourOp(Display2::E_BlendOp_Add);

			blendState.SetBlendAlphaSrc(Display2::E_Blend_Zero);
			blendState.SetBlendAlphaDst(Display2::E_Blend_SrcColour);
			blendState.SetBlendAlphaOp(Display2::E_BlendOp_Add);

		}
		break;

		case E_BlendMode_AdditiveSmooth:
		{
			m_Context.SetAlphaTestEnable(Display2::E_Bool_True);
			m_Context.SetBlendEnable(Display2::E_Bool_True);

			alphaTestState.SetAlphaTestCmpFunc(Display2::E_CmpFunc_Greater);
			alphaTestState.SetAlphaTestRef(0x00);
			
			blendState.SetBlendColourSrc(Display2::E_Blend_One);
			blendState.SetBlendColourDst(Display2::E_Blend_InvSrcColour);
			blendState.SetBlendColourOp(Display2::E_BlendOp_Add);
		}
		break;
	}

	m_Context.SetBlendState(blendState);
	m_Context.SetAlphaTestState(alphaTestState);
}

void C_ContextBase::SetBlendState(const E_Blend blendColourSrc, const E_Blend blendColourDst, const E_BlendOp blendColourOp)
{
	#if CONTEXT_BASE_STUB
	return;
	#endif

	Display2::C_BlendState blendState;
	blendState.SetBlendColourSrc((Display2::E_Blend)blendColourSrc);
	blendState.SetBlendColourDst((Display2::E_Blend)blendColourDst);
	blendState.SetBlendColourOp((Display2::E_BlendOp)blendColourOp);
	blendState.SetBlendAlphaSrc(Display2::E_Blend_One);
	blendState.SetBlendAlphaDst(Display2::E_Blend_Zero);
	blendState.SetBlendAlphaOp(Display2::E_BlendOp_Add);
	m_Context.SetBlendState(blendState);
}

void C_ContextBase::SetBlendState(const E_Blend blendColourSrc, const E_Blend blendColourDst, const E_BlendOp blendColourOp,
							  const E_Blend blendAlphaSrc, const E_Blend blendAlphaDst, const E_BlendOp blendAlphaOp)
{
	#if CONTEXT_BASE_STUB
	return;
	#endif

	Display2::C_BlendState blendState;
	blendState.SetBlendColourSrc((Display2::E_Blend)blendColourSrc);
	blendState.SetBlendColourDst((Display2::E_Blend)blendColourDst);
	blendState.SetBlendColourOp((Display2::E_BlendOp)blendColourOp);
	blendState.SetBlendAlphaSrc((Display2::E_Blend)blendAlphaSrc);
	blendState.SetBlendAlphaDst((Display2::E_Blend)blendAlphaDst);
	blendState.SetBlendAlphaOp((Display2::E_BlendOp)blendAlphaOp);
	m_Context.SetBlendState(blendState);
}

void C_ContextBase::SetAlphaTestState(const E_Bool alphaTestEnable, const U32 alphaRef)
{
	#if CONTEXT_BASE_STUB
	return;
	#endif

	m_Context.SetAlphaTestEnable((Display2::E_Bool)alphaTestEnable);
		
	Display2::C_AlphaTestState alphaTestState;
	alphaTestState.SetAlphaTestCmpFunc(Display2::E_CmpFunc_Greater);
	alphaTestState.SetAlphaTestRef(alphaRef);
	m_Context.SetAlphaTestState(alphaTestState);
}

void C_ContextBase::SetAlphaToMaskEnable(const E_Bool alphaToMaskEnable)
{
#if defined(PLATFORM_MACOS) // [Blur2PC][macOS] MSAA/coverage is not implemented; never report silent success.
    if(alphaToMaskEnable != E_Bool_False) throw std::logic_error("Native alpha-to-coverage requires multisample integration");
#endif
	#if CONTEXT_BASE_STUB
	return;
	#endif
}

#if !PLATFORM_PPU_SPU
void C_ContextBase::SetSamplerState(const U32 sampler, const Display::C_SamplerState *pS)
{
	#if CONTEXT_BASE_STUB
	return;
	#endif

	#if defined(PLATFORM_MACOS) // [Blur2PC][macOS] Retain per-slot anisotropy from the upper state.
    if(!pS) throw std::invalid_argument("Missing native pixel sampler state");
    m_Context.SetSamplerState(sampler, *pS, U32(pS->m_MaxAnisotropy));
#else
    m_Context.SetSamplerState(sampler, *pS);
#endif

}

void C_ContextBase::SetVertexSamplerState(const U32 sampler, const Display::C_SamplerState *pS)
{
	#if CONTEXT_BASE_STUB
	return;
	#endif
	
	#if defined(PLATFORM_MACOS) // [Blur2PC][macOS]
    if(!pS) throw std::invalid_argument("Missing native vertex sampler state");
    m_Context.SetVertexSamplerState(sampler, *pS, U32(pS->m_MaxAnisotropy));
#else
    m_Context.SetVertexSamplerState(sampler, *pS);
#endif
}
#endif

void C_ContextBase::SetPointSpriteEnable(const E_Bool pointSpriteEnable)
{
	#if CONTEXT_BASE_STUB
	return;
	#endif

	#if defined(PLATFORM_MACOS) // [Blur2PC][macOS] Shader point expansion remains pending.
    if(pointSpriteEnable != E_Bool_False) throw std::logic_error("Native point sprites require shader integration");
#else
    m_Context.SetPointSpriteEnable((Display2::E_Bool)pointSpriteEnable);
#endif
}

} // namespace Display
