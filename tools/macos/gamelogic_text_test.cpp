// [Blur2PC] GameLogic string boundary behavior, independent of game data and network services.
#include <GameLogic/GameLogicText.h>
#include <array>
#include <iostream>
#include <stdexcept>
static void require(bool v,const char* message){if(!v)throw std::runtime_error(message);}
int main(){try{
 static_assert(sizeof(WCHAR)==2,"GameLogic must preserve the PC UTF-16 ABI");
 require(GameLogicText::Length(u"A\U0001f3ce")==3,"UTF-16 length changed surrogate units");
 require(GameLogicText::Compare(u"a",u"b")<0,"Code-unit ordering changed");
 require(GameLogicText::CompareInsensitive(u"Caf\u00e9",u"CAF\u00c9")==0,"Non-ASCII profile comparison changed");
 std::array<WCHAR,7> dest{u'X',u'X',u'X',u'X',u'X',u'X',u'Z'};
 require(GameLogicText::CopyN(dest.data()+1,u"AB",5)==dest.data()+1,"CopyN return changed");
 require(dest==std::array<WCHAR,7>{u'X',u'A',u'B',0,0,0,u'Z'},"CopyN lost CRT zero-padding or overran storage");
 GameLogicText::CopyN(dest.data()+1,u"ABCDE",3);
 require(dest[1]==u'A'&&dest[3]==u'C'&&dest[4]==0,"CopyN must preserve explicit nonterminated full-count copies");
 std::array<char,11> bytes;bytes.fill('X');
 require(GameLogicText::ToUtf8Buffer(u"A\U0001f3ce",3,bytes.data()+1,5)==5,"Actual UTF-8 conversion length changed");
 require(bytes[0]=='X'&&bytes[6]=='X'&&std::string(bytes.data()+1,5)=="A\xf0\x9f\x8f\x8e","Conversion changed supplementary Unicode or appended an implicit terminator");
 const auto before=bytes;
 require(GameLogicText::ToUtf8Buffer(u"A\U0001f3ce",3,nullptr,0)==5&&bytes==before,"UTF-8 capacity query changed");
 require(GameLogicText::ToUtf8Buffer(u"A\U0001f3ce",3,bytes.data()+1,4)==0&&bytes==before,"Insufficient UTF-8 capacity must not partially write");
 const WCHAR embedded[]{u'A',0,u'B'};
 require(GameLogicText::ToUtf8Buffer(embedded,3,bytes.data()+1,3)==3&&bytes[1]=='A'&&bytes[2]==0&&bytes[3]=='B',"Explicit conversion length lost embedded NUL");
 const WCHAR malformed[]{0xd800,u'A',0xdc00};
 require(GameLogicText::ToUtf8Buffer(malformed,3,bytes.data()+1,7)==7&&std::string(bytes.data()+1,7)=="\xef\xbf\xbd" "A" "\xef\xbf\xbd","Windows UTF-8 flags=0 surrogate replacement changed");
 require(GameLogicText::ToUtf8Buffer(u"",0,bytes.data(),0)==0,"Empty conversion changed");
 std::cout<<"GameLogic native UTF-16/Unicode compare, exact padded copy and bounded UTF-8 conversion PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
