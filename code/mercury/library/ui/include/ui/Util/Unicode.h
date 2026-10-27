#ifndef UI_UTIL_UNICODE_H
#define UI_UTIL_UNICODE_H
// [Blur2PC] Original PC UTF-16 strings use native code units and platform Unicode services.
#include <core/Core.h>
#include <algorithm>
#include <cstdarg>
#include <cstring>
#include <cwchar>
#include <string>
#if defined(PLATFORM_MACOS)
#include <core/utility/macos/Utf16.h>
#endif

#if defined(PLATFORM_MACOS)
#define UI_WIDE_LITERAL(text) u##text
#else
#define UI_WIDE_LITERAL(text) L##text
#endif

namespace UI { namespace Unicode {
inline std::size_t Length(const WCHAR* text) {
#if defined(PLATFORM_MACOS)
    return Ut::Utf16::Length(text);
#else
    return std::wcslen(text);
#endif
}
inline int Compare(const WCHAR* left, const WCHAR* right) {
#if defined(PLATFORM_MACOS)
    return Ut::Utf16::Compare(left,right);
#else
    return std::wcscmp(left,right);
#endif
}
inline int CompareInsensitive(const WCHAR* left,const WCHAR* right) {
#if defined(PLATFORM_MACOS)
    return Ut::Utf16::CompareInsensitive(left,right);
#else
    return _wcsicmp(left,right);
#endif
}
inline void Copy(WCHAR* destination,std::size_t capacity,const WCHAR* source) {
#if defined(PLATFORM_MACOS)
    Ut::Utf16::Copy(destination,capacity,source);
#else
    wcsncpy_s(destination,capacity,source,_TRUNCATE);
#endif
}
inline void Append(WCHAR* destination,std::size_t capacity,const WCHAR* source) {
#if defined(PLATFORM_MACOS)
    Ut::Utf16::Append(destination,capacity,source);
#else
    wcsncat_s(destination,capacity,source,_TRUNCATE);
#endif
}
#if defined(PLATFORM_MACOS)
using Ut::Utf16::FromUtf8;
using Ut::Utf16::ToUtf8;
using Ut::Utf16::FormatArgs;
using Ut::Utf16::ParseHex;
#endif
} }
#endif
