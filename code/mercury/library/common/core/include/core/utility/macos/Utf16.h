#ifndef CORE_UTILITY_MACOS_UTF16_H
#define CORE_UTILITY_MACOS_UTF16_H
// [Blur2PC] PC-compatible UTF-16 operations backed by native Unicode/formatting services.
#include <core/Core.h>
#include <algorithm>
#include <cstdarg>
#include <cstring>
#include <string>

namespace Ut { namespace Utf16 {
inline std::size_t Length(const WCHAR* text) {
    return std::char_traits<WCHAR>::length(text);
}
inline int Compare(const WCHAR* left, const WCHAR* right) {
    const std::size_t leftSize=Length(left),rightSize=Length(right);
    const int value=std::char_traits<WCHAR>::compare(left,right,std::min(leftSize,rightSize));
    return value?value:(leftSize>rightSize?1:leftSize<rightSize?-1:0);
}
inline void Copy(WCHAR* destination,std::size_t capacity,const WCHAR* source) {
    if(!capacity)return;
    const std::size_t count=std::min(Length(source),capacity-1);
    std::memmove(destination,source,count*sizeof(WCHAR));destination[count]=0;
}
inline void Append(WCHAR* destination,std::size_t capacity,const WCHAR* source) {
    if(!capacity)return;
    const std::size_t used=Length(destination);
    if(used<capacity)Copy(destination+used,capacity-used,source);
}
// Invalid UTF-8/UTF-16 throws; bounded copies always terminate when capacity is nonzero.
// Case-insensitive Unicode comparison uses the public native string service.
int CompareInsensitive(const WCHAR* left,const WCHAR* right);
void FromUtf8(WCHAR* destination,std::size_t capacity,const char* source);
std::string ToUtf8(const WCHAR* source);
// Windows-wide string/character formats are normalized for GNU libunistring.
// Returns code units excluding NUL, or -1 on truncation; output remains NUL terminated.
int FormatArgs(WCHAR* destination,std::size_t capacity,const WCHAR* format,va_list arguments);
bool ParseHex(const WCHAR* source,U32& value);
} }

#endif
