#include "LuaSharedPch.h"

#include <core/Core.h>
#if defined(PLATFORM_MACOS) // [Blur2PC] Lua allocation reports failure through its C allocator contract.
#include <new>
#endif


namespace Lua
{

void* LuaAlloc( const U32 size )
{
#if defined(PLATFORM_MACOS) // [Blur2PC] Avoid a C++ exception crossing Lua's C allocation boundary.
	return static_cast<void*>(new(std::nothrow) U8[size]);
#else
	return (void*)new U8[size];
#endif
}

void LuaFree( void* ptr )
{
	delete [](U8*)ptr;
}

} // namespace Lua
