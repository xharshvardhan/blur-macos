// [Blur2PC] Original Win32 wire compatibility against the actual native network types and message code.
#include <Core/Core.h>
#include <System/DebugLog.h>
#include <Network/Network.h>
#include <Lan/NetLanDiscoveryDefines.h>
#include <Message/NetMessage.h>
#include <Misc/NetDatagramHeader.h>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
static void require(bool v,const char* text){if(!v)throw std::runtime_error(text);}
int main(){try{
 static_assert(sizeof(WCHAR)==2&&sizeof(SysProfile::BaseProfile)==56,"Original PC profile wire ABI changed");
 static_assert(sizeof(RacerNet::DatagramPacketHeader)==4&&sizeof(RacerNet::DatagramReliablePacketHeader)==10&&sizeof(RacerNet::DatagramAcknowledgementHeader)==6,"Original PC datagram wire sizes changed");
 SysProfile::BaseProfile profile;profile.m_Uid=0x1122334455667788ull;profile.m_IsGuest=true;
 const char16_t name[]=u"\u03bb\U0001f3ce";std::memcpy(profile.m_Name,name,sizeof(name));
 std::array<unsigned char,56> expected{};const unsigned char uid[]{0x88,0x77,0x66,0x55,0x44,0x33,0x22,0x11};
 std::memcpy(expected.data(),uid,sizeof(uid));expected[8]=1;expected[16]=0xbb;expected[17]=3;expected[18]=0x3c;expected[19]=0xd8;expected[20]=0xce;expected[21]=0xdf;
 require(std::memcmp(&profile,expected.data(),expected.size())==0,"Native profile reused MSVC tail padding or changed UTF-16 wire bytes");
 RacerNet::DatagramReliablePacketHeader header;header.m_EncryptedBytes=0x1234;header.m_PartyChannel=1;header.m_SenderReference=5;header.m_PacketType=0xab;
 header.m_InitialConnection=1;header.m_FirstPacketInStream=1;header.m_PacketSize=0x123;header.m_TotalMessageSize=0x4567;header.m_PacketSequence=0x89ab;
 const std::array<unsigned char,10> golden{0x34,0x12,0x17,0xab,0x8f,0x04,0x67,0x45,0xab,0x89};
 require(std::memcmp(&header,golden.data(),golden.size())==0,"Native reliable packet changed original PC bitfield bytes");
 RacerNet::C_NetMessage message;require(message.Initialise(128)&&message.CreateReliable(),"Original message allocation/create failed");
 require(message.WriteU8(0xa5)&&message.WriteU32(0xdeadbeef)&&message.WriteBuffer(&profile,sizeof(profile)),"Original message payload write failed");
 message.TerminateMessage();require(message.IsTerminated()&&message.IsReliable()&&message.DataSize()==61&&message.ReadOpen(),"Original reliable payload state/size changed");
 U8 tag=0;U32 number=0;SysProfile::BaseProfile roundtrip;
 require(message.ReadU8(tag)&&message.ReadU32(number)&&message.ReadBuffer(&roundtrip,sizeof(roundtrip)),"Original reliable profile read failed");
 require(tag==0xa5&&number==0xdeadbeef&&std::memcmp(&roundtrip,expected.data(),expected.size())==0,"Original reliable profile transport altered wire values");
 require(!message.ReadU8(tag),"Original message read beyond packet boundary");
 require(message.ReadClose()&&message.ReadOpen()&&message.ReadU8(tag)&&tag==0xa5,"Original message cannot replay payload read");
 require(RacerNet::NET_MAX_PARTY_MEMBERS==8&&RacerNet::NET_MAX_VEHICLES==20,"Original party/race player limits changed");
 std::cout<<"Actual network wire compatibility: Win32 profile56/LANreply160, UTF-16 surrogate bytes, reliable packet bitfields and real message read/write bounds PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
