#if defined(__APPLE__) && defined(__MACH__)
// [Blur2PC][macOS] Allocation has no dependency on the legacy math PCH.
#include <core/Core.h>
#include <core/allocators/macos/NativeAllocation.h>
#else
#include "CorePch.h"
#endif
#include <core/allocators/CoreMemoryFunctions.h>

void* BizAlignedMalloc(size_t size, size_t alignment)
{
#if defined(PLATFORM_MACOS)
	return CoreMemoryMac::alignedAllocate(size, alignment); // [Blur2PC][macOS]
#elif !defined( COMPILER_VC )
	return memalign(alignment, size);
#else
	return _aligned_malloc(size, alignment);
#endif
}

void BizAlignedFree(void* p)
{
#if !defined( COMPILER_VC )
	free(p);
#else
	_aligned_free(p);
#endif
}

void* BizHeapAlignedMalloc(size_t size, size_t alignment)
{
#if defined(PLATFORM_MACOS)
	return CoreMemoryMac::alignedAllocate(size, alignment); // [Blur2PC][macOS]
#elif !defined( COMPILER_VC )
	return memalign(alignment, size);
#else
	return _aligned_malloc(size, alignment);
#endif
}

void BizHeapAlignedFree(void* p)
{
#if !defined( COMPILER_VC )
	free(p);
#else
	_aligned_free(p);
#endif
}
