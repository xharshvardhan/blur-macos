#if defined(__APPLE__) // [Blur2PC][macOS] Original drawing/registry code without Windows PCH.
#include "../device/metal/NativeDisplayIncludes.h"
#include <system/debug/debug.h>
#else
#include "stdafx.h"
#endif
#include "Display/Shader/PixelShaders.h"
#include "Display/Device/Device.h"
#include "Display/Shaders/Constants.h"

EXTERN_SHADER(psDefaultColour);
EXTERN_SHADER(psDefaultVertexColour);
EXTERN_SHADER(psDefaultTexture);
EXTERN_SHADER(psDefaultTextureRGB);
EXTERN_SHADER(psDefaultTextureR);
EXTERN_SHADER(psDefaultTextureG);
EXTERN_SHADER(psDefaultTextureB);
EXTERN_SHADER(psDefaultTextureA);
EXTERN_SHADER(psDefaultTextureD);

#if defined(PLATFORM_PS3)
EXTERN_SHADER(psDownsampleDepth)
EXTERN_SHADER(psDownsampleTexture2x2)
#endif

#if defined(PLATFORM_PS3)
EXTERN_SHADER(psDownsampleTexture2xAA)
EXTERN_SHADER(psDownsampleTexture4xAA)
EXTERN_SHADER(psDownsampleFast)
EXTERN_SHADER(psDownsampleColAndDep)
#endif

namespace Display
{

C_PixelShader			PixelShader::m_PixelShaderColour;
C_PixelShaderParameter	PixelShader::m_PixelShaderColour_Colour;

C_PixelShader			PixelShader::m_PixelShaderVertexColour;
C_PixelShader			PixelShader::m_PixelShaderTexture;
C_PixelShader			PixelShader::m_PixelShaderTextureRGB;
C_PixelShader			PixelShader::m_PixelShaderTextureR;
C_PixelShader			PixelShader::m_PixelShaderTextureG;
C_PixelShader			PixelShader::m_PixelShaderTextureB;
C_PixelShader			PixelShader::m_PixelShaderTextureA;
C_PixelShader			PixelShader::m_PixelShaderTextureD;

#if defined(PLATFORM_PS3)
C_PixelShader			PixelShader::m_PixelShaderDepth;
C_PixelShaderParameter	PixelShader::m_PixelShaderDepth_PixelSize;

C_PixelShader			PixelShader::m_PixelShaderTexture2x2;
C_PixelShaderParameter	PixelShader::m_PixelShaderTexture2x2_PixelSize;
#endif

#if defined(PLATFORM_PS3)
C_PixelShader			PixelShader::m_PixelShaderTexture2xAA;
C_PixelShaderParameter	PixelShader::m_PixelShaderTexture2xAA_PixelSize;

C_PixelShader			PixelShader::m_PixelShaderTexture4xAA;
C_PixelShaderParameter	PixelShader::m_PixelShaderTexture4xAA_PixelSize;

C_PixelShader			PixelShader::m_PixelShaderTextureFast;
C_PixelShaderParameter  PixelShader::m_PixelShaderTextureFast_PixelSize;

C_PixelShader			PixelShader::m_PixelShaderColAndDep;
C_PixelShaderParameter	PixelShader::m_PixelShaderColAndDep_PixelSize;
#endif

void CreatePixelShaders()
{
#if defined(PLATFORM_MACOS) // [Blur2PC][macOS] Use the same authored program symbols through native file containers.
    RESOURCE_MEMORY_ID("DisplayShaders");
    if(PixelShader::m_PixelShaderColour.IsCreated())throw std::logic_error("Native default pixel shaders already exist");
    if(PixelShader::m_PixelShaderVertexColour.IsCreated())throw std::logic_error("Native default pixel shaders already exist");
    if(PixelShader::m_PixelShaderTexture.IsCreated())throw std::logic_error("Native default pixel shaders already exist");
    if(PixelShader::m_PixelShaderTextureRGB.IsCreated())throw std::logic_error("Native default pixel shaders already exist");
    if(PixelShader::m_PixelShaderTextureR.IsCreated())throw std::logic_error("Native default pixel shaders already exist");
    if(PixelShader::m_PixelShaderTextureG.IsCreated())throw std::logic_error("Native default pixel shaders already exist");
    if(PixelShader::m_PixelShaderTextureB.IsCreated())throw std::logic_error("Native default pixel shaders already exist");
    if(PixelShader::m_PixelShaderTextureA.IsCreated())throw std::logic_error("Native default pixel shaders already exist");
    if(PixelShader::m_PixelShaderTextureD.IsCreated())throw std::logic_error("Native default pixel shaders already exist");
    bool success=true;
    success=CreatePixelShaderFromFile("Shaders/psDefaultColour",0,&PixelShader::m_PixelShaderColour)&&success;
    success=CreatePixelShaderFromFile("Shaders/psDefaultVertexColour",0,&PixelShader::m_PixelShaderVertexColour)&&success;
    success=CreatePixelShaderFromFile("Shaders/psDefaultTexture",0,&PixelShader::m_PixelShaderTexture)&&success;
    success=CreatePixelShaderFromFile("Shaders/psDefaultTextureRGB",0,&PixelShader::m_PixelShaderTextureRGB)&&success;
    success=CreatePixelShaderFromFile("Shaders/psDefaultTextureR",0,&PixelShader::m_PixelShaderTextureR)&&success;
    success=CreatePixelShaderFromFile("Shaders/psDefaultTextureG",0,&PixelShader::m_PixelShaderTextureG)&&success;
    success=CreatePixelShaderFromFile("Shaders/psDefaultTextureB",0,&PixelShader::m_PixelShaderTextureB)&&success;
    success=CreatePixelShaderFromFile("Shaders/psDefaultTextureA",0,&PixelShader::m_PixelShaderTextureA)&&success;
    success=CreatePixelShaderFromFile("Shaders/psDefaultTextureD",0,&PixelShader::m_PixelShaderTextureD)&&success;
    if(!success){DestroyPixelShaders();throw std::runtime_error("Native default pixel shader loading failed");}
    PixelShader::m_PixelShaderColour_Colour=PixelShader::m_PixelShaderColour.GetParameter(CP_DISPLAY_COLOUR,"g_Colour");
#else

	RESOURCE_MEMORY_ID("DisplayShaders");

	CreatePixelShaderFromFunc("Shaders/psDefaultColour",			0, &PixelShader::m_PixelShaderColour,		REF_SHADER(psDefaultColour));
	PixelShader::m_PixelShaderColour_Colour							= PixelShader::m_PixelShaderColour.GetParameter(CP_DISPLAY_COLOUR, "g_Colour");

	CreatePixelShaderFromFunc("Shaders/psDefaultTexture",			0, &PixelShader::m_PixelShaderVertexColour,	REF_SHADER(psDefaultVertexColour));
	CreatePixelShaderFromFunc("Shaders/psDefaultTexture",			0, &PixelShader::m_PixelShaderTexture,		REF_SHADER(psDefaultTexture));
	CreatePixelShaderFromFunc("Shaders/psDefaultTextureRGB",		0, &PixelShader::m_PixelShaderTextureRGB,	REF_SHADER(psDefaultTextureRGB));
	CreatePixelShaderFromFunc("Shaders/psDefaultTextureR", 			0, &PixelShader::m_PixelShaderTextureR, 	REF_SHADER(psDefaultTextureR));
	CreatePixelShaderFromFunc("Shaders/psDefaultTextureG", 			0, &PixelShader::m_PixelShaderTextureG, 	REF_SHADER(psDefaultTextureG));
	CreatePixelShaderFromFunc("Shaders/psDefaultTextureB", 			0, &PixelShader::m_PixelShaderTextureB, 	REF_SHADER(psDefaultTextureB));
	CreatePixelShaderFromFunc("Shaders/psDefaultTextureA", 			0, &PixelShader::m_PixelShaderTextureA, 	REF_SHADER(psDefaultTextureA));
	CreatePixelShaderFromFunc("Shaders/psDefaultTextureD", 			0, &PixelShader::m_PixelShaderTextureD, 	REF_SHADER(psDefaultTextureD));

#if defined(PLATFORM_PS3)
	CreatePixelShaderFromFunc("Shaders/psDownsampleDepth",			0, &PixelShader::m_PixelShaderDepth,		REF_SHADER(psDownsampleDepth));
	PixelShader::m_PixelShaderDepth_PixelSize						= PixelShader::m_PixelShaderDepth.GetParameter(CP_DISPLAY_PIXEL_SIZE, "g_PixelSize");

	CreatePixelShaderFromFunc("Shaders/psDownsampleTexture2x2",		0, &PixelShader::m_PixelShaderTexture2x2,	REF_SHADER(psDownsampleTexture2x2));
	PixelShader::m_PixelShaderTexture2x2_PixelSize					= PixelShader::m_PixelShaderTexture2x2.GetParameter(CP_DISPLAY_PIXEL_SIZE, "g_PixelSize");
#endif

#if defined(PLATFORM_PS3)
	CreatePixelShaderFromFunc("Shaders/psDownsampleTexture2xAA",	0, &PixelShader::m_PixelShaderTexture2xAA,	REF_SHADER(psDownsampleTexture2xAA));
	PixelShader::m_PixelShaderTexture2xAA_PixelSize					= PixelShader::m_PixelShaderTexture2xAA.GetParameter(CP_DISPLAY_PIXEL_SIZE, "g_PixelSize");

	CreatePixelShaderFromFunc("Shaders/psDownsampleTexture4xAA",	0, &PixelShader::m_PixelShaderTexture4xAA,	REF_SHADER(psDownsampleTexture4xAA));
	PixelShader::m_PixelShaderTexture4xAA_PixelSize					= PixelShader::m_PixelShaderTexture4xAA.GetParameter(CP_DISPLAY_PIXEL_SIZE, "g_PixelSize");

	CreatePixelShaderFromFunc("Shaders/psDownsampleFast",			0, &PixelShader::m_PixelShaderTextureFast,	REF_SHADER(psDownsampleFast));
	PixelShader::m_PixelShaderTextureFast_PixelSize					= PixelShader::m_PixelShaderTextureFast.GetParameter(CP_DISPLAY_PIXEL_SIZE, "g_PixelSize");

	CreatePixelShaderFromFunc("Shaders/psDownsampleColAndDep",		0, &PixelShader::m_PixelShaderColAndDep,	REF_SHADER(psDownsampleColAndDep));
	PixelShader::m_PixelShaderColAndDep_PixelSize					= PixelShader::m_PixelShaderColAndDep.GetParameter(CP_DISPLAY_PIXEL_SIZE, "g_PixelSize");
#endif


#endif
}

void DestroyPixelShaders()
{
	Display::Device()->Destroy(&PixelShader::m_PixelShaderColour);
	
	Display::Device()->Destroy(&PixelShader::m_PixelShaderVertexColour);
	Display::Device()->Destroy(&PixelShader::m_PixelShaderTexture);
	Display::Device()->Destroy(&PixelShader::m_PixelShaderTextureRGB);
	Display::Device()->Destroy(&PixelShader::m_PixelShaderTextureR);
	Display::Device()->Destroy(&PixelShader::m_PixelShaderTextureG);
	Display::Device()->Destroy(&PixelShader::m_PixelShaderTextureB);
	Display::Device()->Destroy(&PixelShader::m_PixelShaderTextureA);
	Display::Device()->Destroy(&PixelShader::m_PixelShaderTextureD);
	
#if defined(PLATFORM_PS3)
	Display::Device()->Destroy(&PixelShader::m_PixelShaderDepth);
	Display::Device()->Destroy(&PixelShader::m_PixelShaderTexture2x2);
#endif

#if defined(PLATFORM_PS3)
	Display::Device()->Destroy(&PixelShader::m_PixelShaderTexture2xAA);
	Display::Device()->Destroy(&PixelShader::m_PixelShaderTexture4xAA);
	Display::Device()->Destroy(&PixelShader::m_PixelShaderTextureFast);
	Display::Device()->Destroy(&PixelShader::m_PixelShaderColAndDep);
#endif
}

}; // namespace Display
