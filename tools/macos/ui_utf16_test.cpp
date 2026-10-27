// [Blur2PC] Native UTF-16/data-width/formatting checks using public platform services.
#include <core/utility/macos/Utf16.h>
#include <array>
#include <iostream>
#include <locale.h>
#include <string>
#include <stdexcept>

static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
static int format(WCHAR* destination,std::size_t capacity,const WCHAR* pattern,...) {
    va_list arguments;va_start(arguments,pattern);
    const int result=Ut::Utf16::FormatArgs(destination,capacity,pattern,arguments);
    va_end(arguments);return result;
}
int main(){try{
    using namespace Ut::Utf16;
    const std::string processLocale=setlocale(LC_CTYPE,nullptr);
    const locale_t threadLocale=uselocale(nullptr);
    static_assert(sizeof(WCHAR)==2,"PC string field width changed");
    const char* utf8=u8"café — गाड़ी 🚗";
    std::array<WCHAR,64> decoded{};FromUtf8(decoded.data(),decoded.size(),utf8);
    require(ToUtf8(decoded.data())==utf8,"Native UTF8/UTF16 round trip changed non-ASCII/supplementary text");
    require(CompareInsensitive(u"Café",u"CAFÉ")==0&&CompareInsensitive(u"ガレージ",u"ガレージ")==0&&CompareInsensitive(u"alpha",u"beta")<0,"Native Unicode case-insensitive comparison changed names/order");
    require(Length(u"A🚗Z")==4,"Native string length counts bytes or UTF32 instead of UTF16 units");
    std::array<WCHAR,5> bounded{{u'!',u'?',u'?',u'?',u'!'}};
    Copy(bounded.data()+1,3,u"abcdef");require(bounded[0]==u'!'&&bounded[1]==u'a'&&bounded[2]==u'b'&&bounded[3]==0&&bounded[4]==u'!',"Bounded native copy overwrote guards or omitted NUL");
    WCHAR append[6]=u"ab";Append(append,6,u"cdef");require(Compare(append,u"abcde")==0,"Bounded native append changed data/truncation");
    Copy(nullptr,0,nullptr);Append(nullptr,0,nullptr);FromUtf8(nullptr,0,nullptr);
    require(format(nullptr,0,nullptr)==-1,"Zero-capacity formatting did not report truncation");
    WCHAR output[128];
    const int count=format(output,128,u"%s | %S | %05d | %.2f | %%",u"車🚗",u8"café",23,1.25);
    require(Compare(output,u"車🚗 | café | 00023 | 1.25 | %")==0&&count==static_cast<int>(Length(output)),"Native Windows-wide formatting changed string/numeric/width/percent semantics");
    require(format(output,128,u"%ls/%hs/%c/%C",u"日本",u8"été",int(u'車'),int('A'))>0&&Compare(output,u"日本/été/車/A")==0,"Native explicit wide/narrow string or character conversion changed");
    require(format(bounded.data()+1,3,u"%s",u"abcdef")==-1&&bounded[0]==u'!'&&bounded[1]==u'a'&&bounded[2]==u'b'&&bounded[3]==0&&bounded[4]==u'!',"Native format truncation changed guards/NUL/result");
    const int wideCount=format(output,128,u"%lld/%llu",-1234567890123LL,18446744073709551615ULL);
    require(wideCount>0&&Compare(output,u"-1234567890123/18446744073709551615")==0,"Native64-bit integer formatting changed");
    bool invalidWide=false;try{ToUtf8(u"\xd800");}catch(const std::exception&){invalidWide=true;}require(invalidWide,"Invalid UTF16 was silently encoded as UTF8");
    require(format(output,128,u"%S:%llu",u8"café",18446744073709551615ULL)>0&&Compare(output,u"café:18446744073709551615")==0,"Mixed UTF8/unsigned64 formatting changed values");
    output[0]=u'X';require(format(output,128,u"%S","\xff")==-1&&output[0]==0,"Invalid format string data left stale native output");
    U32 colour=0;require(ParseHex(u"ff80ab10",colour)&&colour==0xff80ab10u&&!ParseHex(u"invalid",colour),"Native UTF16 hex parsing changed color bits");
    bool rejected=false;try{FromUtf8(output,128,"\xff");}catch(const std::exception&){rejected=true;}require(rejected,"Invalid UTF8 was silently accepted");
    require(processLocale==setlocale(LC_CTYPE,nullptr)&&threadLocale==uselocale(nullptr),"Native formatting altered the caller locale");
    std::cout<<"Native UTF16 width/roundtrip/formatting/strings/characters/bounds/hex PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
