#if defined(__APPLE__) // [Blur2PC][macOS] Explicit shared dependencies.
#include "../device/metal/NativeDisplayIncludes.h"
#else
#include "stdafx.h"
#endif
#include "Display/Texture/Texture.h"

namespace Display
{

C_RefTexture::C_RefTexture()
{
	m_RefCount = 0;
	m_FileName = "";
}

S32 C_RefTexture::Acquire()
{
	return ++m_RefCount;
}

S32 C_RefTexture::Release()
{
	if(m_RefCount > 0)
		--m_RefCount;
	return m_RefCount;
}

void C_RefTexture::SetFileName(const Util::C_String& fileName)
{
	m_FileName = fileName;
}

}; // namespace Display
