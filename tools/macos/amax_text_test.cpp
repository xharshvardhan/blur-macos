// [Blur2PC] Actual Amax safe-string wrapper against the PC UTF-16 contract.
#include <core/Core.h>
#include <AmaxText.h>
#include <SafeString.inl>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
    static_assert(sizeof(WCHAR)==2,"Amax native text must remain PC UTF-16");
    std::array<WCHAR,96> output{};
    const auto expected=std::u16string(u"\u03bb\U0001f3ce / caf\u00e9 / 0012 / 1.25");
    const int length=Amax::C_SafeString::WideStringPrintF(output.data(),output.size(),
        AMAX_WIDE_LITERAL("%s / %S / %04d / %.2f"),u"\u03bb\U0001f3ce","caf\xc3\xa9",12,1.25);
    require(length==expected.size()&&std::u16string(output.data())==expected,
            "Amax formatter changed UTF-16 surrogate pairs, UTF-8 narrow strings or numeric formatting");
    std::array<WCHAR,8> guarded{u'X',u'X',u'X',u'X',u'X',u'X',u'X',u'Z'};
    require(Amax::C_SafeString::WideStringPrintF(guarded.data()+1,6,AMAX_WIDE_LITERAL("%s"),u"ABCDEFGH")==-1,
            "Amax UTF-16 truncation did not report failure");
    require(guarded.front()==u'X'&&guarded.back()==u'Z'&&guarded[6]==0,
            "Amax formatter wrote outside its code-unit capacity or omitted termination");
    WCHAR untouched=u'Q';require(Amax::C_SafeString::WideStringPrintF(&untouched,0,AMAX_WIDE_LITERAL("%s"),u"A")==-1&&untouched==u'Q',
            "Amax zero-capacity formatter touched destination storage");
    char narrow[32]{};
    require(Amax::C_SafeString::StringPrintF(narrow,sizeof(narrow),"%s-%04d","race",12)==9&&std::strcmp(narrow,"race-0012")==0,
            "Amax narrow formatting changed during native UTF-16 integration");
    std::cout<<"Actual Amax formatting: UTF-16 literal/surrogate preservation, Windows wide/narrow arguments, numeric formatting and bounded writes PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
