// [Blur2PC] Preserve network profile names as PC UTF-16 on native Darwin.
#ifndef NETWORK_TEXT_H
#define NETWORK_TEXT_H
#include <Core/Core.h>
#include <cstdarg>
#include <bdPlatform/bdPlatformTiming/bdPlatformTiming.h>
#if defined(PLATFORM_MACOS)
#include <core/utility/macos/Utf16.h>
#define NETWORK_WIDE_LITERAL(value) u##value
#else
#include <wchar.h>
#define NETWORK_WIDE_LITERAL(value) L##value
#endif
namespace NetPlay { namespace Text {
inline U32 TickMilliseconds() {
#if defined(PLATFORM_MACOS)
 return bdPlatformTiming::getLoResTimeStamp(); // [Blur2PC] Monotonic milliseconds, same U32 wrap as GetTickCount.
#else
 return GetTickCount();
#endif
}
inline size_t Length(const WCHAR* value) {
#if defined(PLATFORM_MACOS)
 return Ut::Utf16::Length(value);
#else
 return wcslen(value);
#endif
}
inline int Format(WCHAR* destination,size_t capacity,const WCHAR* format,...) {
 va_list args;va_start(args,format);
#if defined(PLATFORM_MACOS)
 const int count=Ut::Utf16::FormatArgs(destination,capacity,format,args);
#else
 const int count=_vsnwprintf_s(destination,capacity,_TRUNCATE,format,args);
#endif
 va_end(args);return count;
}
inline void Copy(WCHAR* destination,size_t capacity,const WCHAR* source) {
#if defined(PLATFORM_MACOS)
 Ut::Utf16::Copy(destination,capacity,source);
#else
 wcsncpy_s(destination,capacity,source,_TRUNCATE);
#endif
}
}}
#endif
