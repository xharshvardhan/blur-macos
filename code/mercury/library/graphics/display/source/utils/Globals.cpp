#if defined(__APPLE__) // [Blur2PC][macOS] Shared CPU Display code does not need a Win32 PCH.
#include "../device/metal/NativeDisplayIncludes.h"
#else
#include "stdafx.h"
#endif
#include "Display/Utils/Globals.h"

namespace Display
{

Util::C_String FormatString(const Util::C_String& input)
{
#if defined(PLATFORM_PS3)
	return input.Replace("\\", "/");
#else
	return input;
#endif
}

Sys::I_FileSystem* gFileSystem = &Sys::C_FileSystem::Instance();

}; // namespace Display
