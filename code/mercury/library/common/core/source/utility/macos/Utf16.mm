// [Blur2PC] Reuse GNU libunistring formatting and CoreFoundation Unicode conversion for PC UTF-16 UI text.
#include <core/utility/macos/Utf16.h>
#include <CoreFoundation/CoreFoundation.h>
#include <unistdio.h>
#include <locale.h>
#include <climits>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace Ut { namespace Utf16 {
namespace {
struct String {
    CFStringRef value;
    explicit String(CFStringRef string):value(string) {
        if(!value)throw std::runtime_error("Native engine Unicode conversion failed");
    }
    ~String(){CFRelease(value);}
    String(const String&)=delete;
    String& operator=(const String&)=delete;
};
std::u16string formatForNative(const WCHAR* format) {
    std::u16string result;
    for(std::size_t index=0;format[index];++index) {
        const WCHAR character=format[index];result+=character;
        if(character!=u'%')continue;
        if(format[index+1]==u'%'){result+=format[++index];continue;}
        // Flags/width/precision pass through to GNU libunistring unchanged.
        while(format[index+1]&&std::u16string(u"-+ #0.123456789*$").find(format[index+1])!=std::u16string::npos)
            result+=format[++index];
        std::u16string length;
        while(format[index+1]&&std::u16string(u"hljztL").find(format[index+1])!=std::u16string::npos)
            length+=format[++index];
        const WCHAR conversion=format[index+1];
        if(!conversion)throw std::invalid_argument("Incomplete native engine format");
        ++index;
        if(conversion==u's'||conversion==u'S'||conversion==u'c'||conversion==u'C') {
            // Windows wide printf: %s/%c are UTF-16; %S/%C are narrow.
            // Explicit h/l length modifiers select narrow/wide independently.
            bool wide=conversion==u's'||conversion==u'c';
            if(length==u"h")wide=false;
            else if(length==u"l")wide=true;
            else if(!length.empty())throw std::invalid_argument("Unsupported UI string format length");
            const bool string=conversion==u's'||conversion==u'S';
            // GNU Unicode %lU/%U explicitly select UTF-16/UTF-8, independent of locale.
            result+=string?(wide?u"lU":u"U"):(wide?u"lc":u"c");
        } else {
            result+=length;result+=conversion;
        }
    }
    return result;
}
}
int CompareInsensitive(const WCHAR* left,const WCHAR* right) {
    String lhs(CFStringCreateWithCharacters(kCFAllocatorDefault,reinterpret_cast<const UniChar*>(left),Length(left)));
    String rhs(CFStringCreateWithCharacters(kCFAllocatorDefault,reinterpret_cast<const UniChar*>(right),Length(right)));
    return static_cast<int>(CFStringCompare(lhs.value,rhs.value,kCFCompareCaseInsensitive));
}
void FromUtf8(WCHAR* destination,std::size_t capacity,const char* source) {
    if(!capacity)return;
    String string(CFStringCreateWithCString(kCFAllocatorDefault,source,kCFStringEncodingUTF8));
    const auto count=std::min<std::size_t>(CFStringGetLength(string.value),capacity-1);
    CFStringGetCharacters(string.value,CFRangeMake(0,count),reinterpret_cast<UniChar*>(destination));
    destination[count]=0;
}
std::string ToUtf8(const WCHAR* source) {
    const auto length=Length(source);
    String string(CFStringCreateWithCharacters(kCFAllocatorDefault,reinterpret_cast<const UniChar*>(source),length));
    const CFIndex capacity=CFStringGetMaximumSizeForEncoding(length,kCFStringEncodingUTF8)+1;
    std::string result(static_cast<std::size_t>(capacity),0);
    if(!CFStringGetCString(string.value,result.data(),capacity,kCFStringEncodingUTF8))
        throw std::runtime_error("Invalid UTF-16 in native engine string");
    result.resize(std::strlen(result.c_str()));return result;
}
int FormatArgs(WCHAR* destination,std::size_t capacity,const WCHAR* format,va_list arguments) {
    if(!capacity)return -1;
    const auto normalized=formatForNative(format);
    // Native printf's %lc uses LC_CTYPE. Scope UTF-8 to this thread and restore it.
    static locale_t utf8Locale=newlocale(LC_CTYPE_MASK,"en_US.UTF-8",nullptr);
    if(!utf8Locale)throw std::runtime_error("Native UTF-8 character locale is unavailable");
    struct LocaleScope {
        locale_t previous;
        explicit LocaleScope(locale_t locale):previous(uselocale(locale)) {
            if(!previous)throw std::runtime_error("Cannot select thread-local UTF-8 character locale");
        }
        ~LocaleScope(){uselocale(previous);}
    } localeScope(utf8Locale);
    destination[0]=0;
    const int length=u16_u16_vsnprintf(reinterpret_cast<uint16_t*>(destination),capacity,
        reinterpret_cast<const uint16_t*>(normalized.c_str()),arguments);
    destination[capacity-1]=0;
    return length<0||static_cast<std::size_t>(length)>=capacity?-1:length;
}
bool ParseHex(const WCHAR* source,U32& value) {
    const auto utf8=ToUtf8(source);char* end=nullptr;
    const unsigned long parsed=std::strtoul(utf8.c_str(),&end,16);
    if(end==utf8.c_str()||parsed>std::numeric_limits<U32>::max())return false;
    value=static_cast<U32>(parsed);return true;
}
} }
