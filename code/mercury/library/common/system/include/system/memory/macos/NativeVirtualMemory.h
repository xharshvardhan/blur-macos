// [Blur2PC][macOS] Diagnostics for the real Memory virtual/physical mapping API.
#pragma once
#include <core/Core.h>
#include <system/memory/VirtualMemory.h>

namespace Memory {
struct NativeVirtualStats { U64 regions, reservedBytes, committedBytes; };
NativeVirtualStats nativeVirtualStats();
}
