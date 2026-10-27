// [Blur2PC] Preserve PC UTF-16 literals without Darwin's 32-bit wchar_t ABI.
#pragma once
#if defined(PLATFORM_MACOS)
#define AMAX_WIDE_LITERAL(value) u##value
#else
#define AMAX_WIDE_LITERAL(value) L##value
#endif
