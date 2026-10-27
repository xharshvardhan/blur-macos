#ifndef AMAX_GAMELOGIC_TEXT_H
#define AMAX_GAMELOGIC_TEXT_H
#include <AmaxText.h>
#include <Core/Core.h>
#include <string>
#include <cstring>
#if defined(PLATFORM_MACOS)
#include <core/utility/macos/Utf16.h>
#else
#include <wchar.h>
#endif
// [Blur2PC] Preserve the original UTF-16 CRT operations without Darwin wchar_t ABI casts.
namespace GameLogicText {
inline size_t Length(const WCHAR* text) {
#if defined(PLATFORM_MACOS)
 return Ut::Utf16::Length(text);
#else
 return wcslen(text);
#endif
}
inline int Compare(const WCHAR* left,const WCHAR* right) {
#if defined(PLATFORM_MACOS)
 return Ut::Utf16::Compare(left,right);
#else
 return wcscmp(left,right);
#endif
}
inline int CompareInsensitive(const WCHAR* left,const WCHAR* right) {
#if defined(PLATFORM_MACOS)
 return Ut::Utf16::CompareInsensitive(left,right);
#else
 return wcsicmp(left,right);
#endif
}
inline WCHAR* CopyN(WCHAR* destination,const WCHAR* source,size_t count) {
 // wcsncpy copies exactly count units, padding with zero after the first NUL.
 size_t i=0; for(;i<count && source[i];++i) destination[i]=source[i];
 for(;i<count;++i) destination[i]=0;
 return destination;
}
inline int ToUtf8Buffer(const WCHAR* text,size_t units,char* destination,size_t capacity) {
#if defined(PLATFORM_MACOS)
 // [Blur2PC] Match an explicit-length WideCharToMultiByte(CP_UTF8,0,...): no implicit NUL or partial writes.
 std::u16string bounded(text,units);
 // Windows flags=0 replaces isolated UTF-16 surrogates with U+FFFD.
 // https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte
 for(size_t i=0;i<bounded.size();++i) {
  const auto c=bounded[i];
  if(c>=0xd800 && c<=0xdbff) {
   if(i+1<bounded.size() && bounded[i+1]>=0xdc00 && bounded[i+1]<=0xdfff) ++i;
   else bounded[i]=0xfffd;
  } else if(c>=0xdc00 && c<=0xdfff) bounded[i]=0xfffd;
 }
 std::string utf8;
 for(size_t offset=0;offset<units;) {
  const size_t end=bounded.find(char16_t(0),offset);
  const size_t finish=end==std::u16string::npos?units:end;
  utf8+=Ut::Utf16::ToUtf8(bounded.c_str()+offset);
  if(finish<units) utf8.push_back(0);
  offset=finish+1;
 }
 if(capacity==0) return static_cast<int>(utf8.size()); // Win32 buffer-size query.
 if(utf8.size()>capacity) return 0;
 if(!utf8.empty()) memcpy(destination,utf8.data(),utf8.size());
 return static_cast<int>(utf8.size());
#else
 return WideCharToMultiByte(CP_UTF8,0,text,static_cast<int>(units),destination,static_cast<int>(capacity),NULL,NULL);
#endif
}

}
#endif
