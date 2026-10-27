#if defined(__APPLE__) // [Blur2PC][macOS] Original drawing/registry code without Windows PCH.
#include "../device/metal/NativeDisplayIncludes.h"
#include <system/debug/debug.h>
#else
#include "stdafx.h"
#endif
#include "Display/Shader/VertexShaders.h"
#include "Display/Device/Device.h"

#ifndef OBJ_SHADERS
#if defined(DEVICE_PS3GCM)
#include "Display/Shaders/Render/PS3/vsDefault_XY.shader"
#endif
#endif

EXTERN_SHADER(vsDefault_XY);
EXTERN_SHADER(vsDefault_XYVertexColour);

namespace Display
{

void CreateVertexShaders()
{
	RESOURCE_MEMORY_ID("DisplayShaders");

#if defined(DEVICE_MACMETAL) // [Blur2PC][macOS] Translated authored programs, with transactional failure.
    if(VertexShader_XY::m_VertexShader.IsCreated()||VertexShader_XY::m_VertexShaderVertexColour.IsCreated())throw std::logic_error("Native default vertex shaders already exist");
    if(!CreateVertexShaderFromFile("Shaders/vsDefault_XY",0,&VertexShader_XY::m_VertexShader)||!CreateVertexShaderFromFile("Shaders/vsDefault_XYVertexColour",0,&VertexShader_XY::m_VertexShaderVertexColour)){
        DestroyVertexShaders();throw std::runtime_error("Native default vertex shader loading failed");
    }
#elif defined(DEVICE_PCD3D11) || defined(DEVICE_PS3GCM) // [Blur1PCDX11]
	CreateVertexShaderFromFunc("Shaders/vsDefault_XY", 0, &VertexShader_XY::m_VertexShader, REF_SHADER(vsDefault_XY));
	CreateVertexShaderFromFunc("Shaders/vsDefault_XYVertexColour", 0, &VertexShader_XY::m_VertexShaderVertexColour, REF_SHADER(vsDefault_XYVertexColour));
#endif
}

void DestroyVertexShaders()
{
#if defined(DEVICE_MACMETAL) || defined(DEVICE_PCD3D11) || defined(DEVICE_PS3GCM) // [Blur2PC][macOS] Actual tracked native destruction.
	Display::Device()->Destroy(&VertexShader_XY::m_VertexShader);
	Display::Device()->Destroy(&VertexShader_XY::m_VertexShaderVertexColour);
#endif
}

C_VertexShader VertexShader_XY::m_VertexShader;
C_VertexShader VertexShader_XY::m_VertexShaderVertexColour;

}; // namespace Display
