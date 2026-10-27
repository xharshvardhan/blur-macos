// [Blur2PC][macOS] Darwin implementation of the existing aligned allocator contract.
#pragma once
#include <cerrno>
#include <cstddef>
#include <cstdlib>

namespace CoreMemoryMac {
inline void* alignedAllocate(std::size_t size, std::size_t alignment) {
    // Engine callers can request 1/2/4-byte alignment. POSIX requires at least
    // sizeof(void*); strengthening a valid alignment preserves their contract.
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        errno = EINVAL;
        return nullptr;
    }
    if (alignment < sizeof(void*)) alignment = sizeof(void*);
    void* pointer = nullptr;
    const int result = posix_memalign(&pointer, alignment, size);
    if (result != 0) {
        errno = result;
        return nullptr;
    }
    return pointer;
}
}
