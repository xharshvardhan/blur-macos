// [Blur2PC][macOS] Upper resource names refer to actual Display2 Metal objects.
#pragma once
#include <core/Maths.h>
#include <Display2.h>
#include <display/device/DeviceTypesEnums.h>
#include <display/buffer/VertexFormat.h>
#include <display/texture/TextureParams.h>
#include <stdexcept>
namespace Display {
using C_VertexDeclaration=Display2::C_VertexDeclaration;
using C_VertexBuffer=Display2::C_VertexBuffer;using C_IndexBuffer=Display2::C_IndexBuffer;
using C_VertexShader=Display2::C_VertexShader;using C_PixelShader=Display2::C_PixelShader;
using C_VertexShaderParameter=Display2::C_VertexShaderParam;using C_PixelShaderParameter=Display2::C_PixelShaderParam;
using C_PixelShaderParameterArray=Display2::C_PixelShaderParam;
using C_Surface=Display2::C_Surface;using C_SurfaceParams=Display2::C_SurfaceParams;
using C_Texture=Display2::C_Texture;using C_Texture2D=Display2::C_Texture;
using C_Texture3D=Display2::C_Texture;using C_TextureCube=Display2::C_Texture;
using C_RenderTexture2D=Display2::C_RenderTexture2D;using C_RenderTextureCube=Display2::C_RenderTextureCube;
struct C_PixelShaderConstantLock {U32 startRegister,endRegister,salt;bool enabled;};
struct C_PixelShaderConstantLockKey {U32 index,salt;};
enum E_PixelShaderConstantLock {E_PixelShaderConstantLock_Count=4};
}
// Original PC metadata shape. Runtime legacy-bytecode translation is not provided by these declarations.
struct C_ShaderProfile {int m_Size;int* m_pData;};
struct C_ShaderData {C_ShaderProfile m_Profiles[Display::E_ShaderProfile_Count];const C_ShaderProfile& GetProfile(Display::E_ShaderProfile profile)const{if(U32(profile)>=Display::E_ShaderProfile_Count)throw std::out_of_range("Native upper shader profile is invalid");return m_Profiles[profile];}};
