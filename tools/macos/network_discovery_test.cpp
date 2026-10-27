// [Blur2PC] Actual original LAN advertisement/discovery responder over loopback, without hosted services.
#include <StdAfx.h>
#include <Lan/NetLanDiscovery.h>
#include <Misc/NetVersion.h>
#include <bdNet/bdNet.h>
#include <bdCore/bdSocket/bdSocket.h>
#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <unistd.h>
static void require(bool v,const char* text){if(!v)throw std::runtime_error(text);}
int main(){alarm(20);try{
 bdCore::init();
 {
  bdNetStartParams parameters;parameters.m_onlineGame=false;parameters.m_localAddresses.pushBack(bdInetAddr::Loopback());
  bdNetImpl* net=bdNet::getInstance();require(net->start(parameters)&&net->getStatus()==bdNetImpl::BD_NET_DONE,"actual original offline router failed startup");
  auto common=net->getLocalCommonAddr();require(!common.isNull(),"actual offline router omitted its common address");
  std::array<U8,BD_COMMON_ADDR_SERIALIZED_SIZE> serialized{};common->serialize(serialized.data());
  RacerNet::C_LanDiscoveryManager host;require(host.Initialise(RacerNet::AMAX_LAN_ID),"actual LAN responder failed to bind original PC discovery port");
  RacerNet::C_LanDiscoveryManager collision;require(!collision.Initialise(RacerNet::AMAX_LAN_ID),"negative original bind failure was reported as a successful LAN service");
  SysProfile::BaseProfile profile;profile.m_Uid=0x1020304050607080ull;const char16_t name[]=u"Native LAN \u03bb";std::memcpy(profile.m_Name,name,sizeof(name));
  NetPlay::SessionSecurity security;RacerNet::NetRaceSettingsQos settings;settings.m_EventID=0x12345678;settings.m_PackVersion=0x1122;
  require(host.Advertise(profile,security,serialized.data(),8,7,0x1122334455667788ull,settings),"actual LAN advertisement failed");
  std::array<bdSocket,6> peers;const bdAddr destination(bdInetAddr::Loopback(),RacerNet::AMAX_LAN_DISCOVERY_PORT);
  std::array<NetPlay::Nonce,6> nonces{};
  for(unsigned n=0;n<peers.size();++n){require(peers[n].create(false,false)&&peers[n].bind(0)==BD_NET_SUCCESS,"actual LAN query peer failed to bind");
   RacerNet::LanDiscoveryRequest request{};request.m_TitleId=htonl(RacerNet::AMAX_LAN_ID);request.m_PacketType=RacerNet::eLanDiscoveryPacketType_Request;
#ifdef CFG_GOLD
   request.m_PacketVersion=htonl(RacerNet::NET_PACKET_VERSION);
#else
   request.m_PacketVersion=htonl(eNetBuildVersion_Master);
#endif
   request.m_NumPlayersToJoin=1;request.m_Nonce.Create();nonces[n]=request.m_Nonce;
   require(peers[n].sendTo(destination,&request,sizeof(request))==sizeof(request),"actual LAN query send failed");}
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
  for(unsigned n=0;n<peers.size();++n){RacerNet::LanDiscoveryReply reply{};bdAddr from;int received;
   do{host.Poll();received=peers[n].receiveFrom(from,&reply,sizeof(reply));if(received==BD_NET_WOULD_BLOCK)std::this_thread::yield();}while(received==BD_NET_WOULD_BLOCK&&std::chrono::steady_clock::now()<deadline);
   require(received==160&&reply.m_Nonce==nonces[n]&&reply.m_PacketType==RacerNet::eLanDiscoveryPacketType_Reply,"actual LAN responder lost a query beyond three peers or changed PC wire bytes");
   require(reply.m_MaxSlots==8&&reply.m_SlotsTaken==1&&reply.m_HostProfile.m_Uid==profile.m_Uid&&reply.m_RaceQos.m_EventID==settings.m_EventID,"actual LAN reply changed host/slot/race metadata");
   require(std::memcmp(reply.m_HostCommonAddress,serialized.data(),serialized.size())==0,"actual LAN reply changed offline router address");}
  require(host.Advertise(profile,security,serialized.data(),RacerNet::NET_MAX_VEHICLES,RacerNet::NET_MAX_VEHICLES-1,0x1122334455667788ull,settings),"actual race-capacity advertisement failed");
  RacerNet::LanDiscoveryRequest capacityQuery{};capacityQuery.m_TitleId=htonl(RacerNet::AMAX_LAN_ID);capacityQuery.m_PacketType=RacerNet::eLanDiscoveryPacketType_Request;
  capacityQuery.m_NumPlayersToJoin=RacerNet::NET_MAX_VEHICLES-1;capacityQuery.m_Nonce.Create();
  require(peers[0].sendTo(destination,&capacityQuery,sizeof(capacityQuery))==sizeof(capacityQuery),"actual capacity-boundary query failed");
  RacerNet::LanDiscoveryReply capacityReply{};bdAddr capacityFrom;int capacityBytes;
  const auto capacityDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
  do{host.Poll();capacityBytes=peers[0].receiveFrom(capacityFrom,&capacityReply,sizeof(capacityReply));if(capacityBytes==BD_NET_WOULD_BLOCK)std::this_thread::yield();}while(capacityBytes==BD_NET_WOULD_BLOCK&&std::chrono::steady_clock::now()<capacityDeadline);
  require(capacityBytes==160&&capacityReply.m_MaxSlots==20&&capacityReply.m_SlotsTaken==1,"actual responder changed original twenty-player race capacity");
  capacityQuery.m_NumPlayersToJoin=RacerNet::NET_MAX_VEHICLES;
  require(peers[0].sendTo(destination,&capacityQuery,sizeof(capacityQuery))==sizeof(capacityQuery),"actual over-capacity query failed");
  const auto rejectionDeadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(30);
  do{host.Poll();require(peers[0].receiveFrom(capacityFrom,&capacityReply,sizeof(capacityReply))==BD_NET_WOULD_BLOCK,"actual responder advertised unavailable player slots");std::this_thread::yield();}while(std::chrono::steady_clock::now()<rejectionDeadline);
  host.UnAdvertise();host.Shutdown();for(auto& peer:peers)peer.close();common=BD_NULL;net->stop();
 }
 bdCore::quit();alarm(0);std::cout<<"Actual original offline bdNet router and LAN responder: six distinct query peers, original160-byte metadata, twenty-player race capacity/over-capacity rejection and real bind-conflict rejection PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
