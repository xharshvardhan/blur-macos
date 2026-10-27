// [Blur2PC][macOS] Actual independent Artery services and original LAN discovery
// managers with per-socket native settings. This is not four joined game players.
#include "../../code/amax/library/Network/stdafx.h"
#include "../../code/mercury/library/common/system/source/artery/ArteryImpl.h"
#include <Lan/NetLanDiscovery.h>
#include <Misc/NetVersion.h>
#include <bdNet/bdNet.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <thread>
#include <unistd.h>

static void require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
static void setting(const char* name, const char* value) { require(value ? !setenv(name,value,1) : !unsetenv(name), "Cannot isolate instance configuration"); }
static unsigned freePort(int type)
{
    static std::set<unsigned> selected;
    for (;;) {
        const int fd=socket(AF_INET,type,0);require(fd>=0,"Cannot allocate a real fixture endpoint");
        sockaddr_in address{};address.sin_len=sizeof(address);address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        const bool bound=bind(fd,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0;socklen_t size=sizeof(address);
        const bool queried=bound&&getsockname(fd,reinterpret_cast<sockaddr*>(&address),&size)==0;close(fd);
        require(queried,"Cannot select an available fixture endpoint");const unsigned port=ntohs(address.sin_port);
        if(selected.insert(port).second)return port;
    }
}
struct State { std::atomic<bool> release{false};std::atomic<unsigned> clients{0},servers{0}; };
static void echo(ArteryConnection connection,void* context)
{
    auto& state=*static_cast<State*>(context);++state.servers;
    while(connection.IsConnected()) { ArteryMessage message;if(connection.Receive(message,50)&&message)connection.Send(message); }
}
static void client(ArteryConnection connection,void* context)
{
    auto& state=*static_cast<State*>(context);++state.clients;
    while(connection.IsConnected()&&!state.release)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    connection.Close();
}
template<class Predicate>static void await(Predicate predicate)
{
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(!predicate()){require(std::chrono::steady_clock::now()<deadline,"Actual instance service timed out");std::this_thread::sleep_for(std::chrono::milliseconds(1));}
}
struct Service { ArteryService actual;~Service(){actual.Destroy();} };
static void artery()
{
    setting("BLUR_MAC_ARTERY_TCP_PORT",nullptr);setting("BLUR_MAC_ARTERY_UDP_PORT",nullptr);
    for(const char* name:{"BLUR_MAC_ARTERY_TCP_PORT","BLUR_MAC_ARTERY_UDP_PORT"}) {
        for(const char* invalid:{"","0","-1","+42"," 42","42 ","42x","65536","999999999999999999999999"}) {
            setting(name,invalid);Service rejected;require(!rejected.actual.Initialise(true,"invalid native endpoint",8),"Invalid Artery endpoint silently fell back");
        }setting(name,nullptr);
    }
    State state;std::array<Service,4> services;std::array<unsigned,4> tcp{},udp{};
    for(unsigned i=0;i<services.size();++i) {
        tcp[i]=freePort(SOCK_STREAM);udp[i]=freePort(SOCK_DGRAM);
        setting("BLUR_MAC_ARTERY_TCP_PORT",std::to_string(tcp[i]).c_str());setting("BLUR_MAC_ARTERY_UDP_PORT",std::to_string(udp[i]).c_str());
        require(services[i].actual.Initialise(true,"native instance",16),"Actual separately configured Artery service failed");
        require(services[i].actual.Register("instance-config",echo,&state),"Actual Artery service registration failed");
    }
    // Reusing the final settings must fail and roll back without disturbing its owner.
    Service collision;require(!collision.actual.Initialise(true,"collision",8),"Occupied configured Artery endpoint reported success");
    std::array<ArteryConnection,4> connections;
    for(unsigned i=0;i<connections.size();++i)require(services[0].actual.Connect(ArterySockAddr(INADDR_LOOPBACK,tcp[i]),"instance-config",client,&state,connections[i]),"Actual per-instance Artery connection failed");
    await([&]{return state.clients==4&&state.servers==4;});
    for(unsigned i=0;i<connections.size();++i) {
        const unsigned char bytes[]={static_cast<unsigned char>(i),0x12,0x34,0x56};ArteryMessage request(bytes,sizeof(bytes)),reply;
        require(connections[i].Send(request),"Actual instance send failed");await([&]{return connections[i].TryReceive(reply);});
        require(reply.DataLength()==sizeof(bytes)&&!std::memcmp(reply.Data(),bytes,sizeof(bytes)),"Native instance endpoint changed original framed bytes");
    }
    state.release=true;for(auto& connection:connections)connection.Close();
    for(auto& service:services)service.actual.Destroy();
    setting("BLUR_MAC_ARTERY_TCP_PORT",nullptr);setting("BLUR_MAC_ARTERY_UDP_PORT",nullptr);
}
static void discovery()
{
    setting("BLUR_MAC_LAN_DISCOVERY_BIND_PORT",nullptr);setting("BLUR_MAC_LAN_DISCOVERY_TARGET_PORT",nullptr);setting("BLUR_MAC_LAN_DISCOVERY_TARGET_IP",nullptr);
    for(const char* name:{"BLUR_MAC_LAN_DISCOVERY_BIND_PORT","BLUR_MAC_LAN_DISCOVERY_TARGET_PORT"}) {
        for(const char* invalid:{"","-1","+42"," 42","42 ","42x","65536","999999999999999999999999"}) {
            setting(name,invalid);RacerNet::C_LanDiscoveryManager rejected;require(!rejected.Initialise(RacerNet::AMAX_LAN_ID),"Invalid discovery port silently fell back");
        }setting(name,nullptr);
    }
    setting("BLUR_MAC_LAN_DISCOVERY_TARGET_PORT","0");{RacerNet::C_LanDiscoveryManager rejected;require(!rejected.Initialise(RacerNet::AMAX_LAN_ID),"Zero discovery destination port accepted");}setting("BLUR_MAC_LAN_DISCOVERY_TARGET_PORT",nullptr);
    for(const char* invalid:{"","localhost","::1","127.0.0.999","127.0.0.1x","0.0.0.0"}) {
        setting("BLUR_MAC_LAN_DISCOVERY_TARGET_IP",invalid);RacerNet::C_LanDiscoveryManager rejected;require(!rejected.Initialise(RacerNet::AMAX_LAN_ID),"Invalid discovery target silently fell back");
    }setting("BLUR_MAC_LAN_DISCOVERY_TARGET_IP",nullptr);
    bdCore::init();struct CoreCleanup {~CoreCleanup(){bdNet::getInstance()->stop();bdCore::quit();}} cleanup;
    bdNetStartParams parameters;parameters.m_onlineGame=false;parameters.m_localAddresses.pushBack(bdInetAddr::Loopback());
    auto* net=bdNet::getInstance();require(net->start(parameters)&&net->getStatus()==bdNetImpl::BD_NET_DONE,"Original offline router failed startup");
    auto common=net->getLocalCommonAddr();require(!common.isNull(),"Original router omitted actual common address");
    std::array<U8,BD_COMMON_ADDR_SERIALIZED_SIZE> serialized{};common->serialize(serialized.data());
    RacerNet::C_LanDiscoveryManager host;require(host.Initialise(RacerNet::AMAX_LAN_ID),"Original default host discovery50001 unavailable");
    {RacerNet::C_LanDiscoveryManager collision;require(!collision.Initialise(RacerNet::AMAX_LAN_ID),"Default discovery bind collision reported success");}
    SysProfile::BaseProfile profile;profile.m_Uid=0x1234567812345678ull;NetPlay::SessionSecurity security;RacerNet::NetRaceSettingsQos qos;
    require(host.Advertise(profile,security,serialized.data(),20,19,common->getHash(),qos),"Original fixture advertisement failed");
    setting("BLUR_MAC_LAN_DISCOVERY_BIND_PORT","0");setting("BLUR_MAC_LAN_DISCOVERY_TARGET_IP","127.0.0.1");setting("BLUR_MAC_LAN_DISCOVERY_TARGET_PORT","50001");
    std::array<RacerNet::C_LanDiscoveryManager,3> peers;
    for(auto& peer:peers)require(peer.Initialise(RacerNet::AMAX_LAN_ID),"Ephemeral discovery client bind failed");
    // Settings are frozen on successful Initialise, not sampled again while enumerating.
    setting("BLUR_MAC_LAN_DISCOVERY_TARGET_IP","not-an-address");
    for(auto& peer:peers)require(peer.Enumerate(1),"Configured client failed original enumeration request");
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(4);
    while(peers[0].IsEnumerating()||peers[1].IsEnumerating()||peers[2].IsEnumerating()) {
        require(std::chrono::steady_clock::now()<deadline,"Original discovery enumeration timer did not finish");
        host.Poll();for(auto& peer:peers)peer.Poll();std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    for(auto& peer:peers) {
        RacerNet::LanDiscoveryReply reply{};require(peer.NumEnumerated()==1&&peer.GetEnumeratedSesionData(0,reply),"Actual discovery client lost its own nonce reply");
        require(reply.m_HostProfile.m_Uid==profile.m_Uid&&reply.m_MaxSlots==20&&reply.m_SlotsTaken==1&&
                !std::memcmp(reply.m_HostCommonAddress,serialized.data(),serialized.size())&&
                !std::memcmp(&reply.m_SessionSecurity,&security,sizeof(security)),"Configured discovery altered original wire/security/address metadata");
    }
    setting("BLUR_MAC_LAN_DISCOVERY_BIND_PORT",nullptr);setting("BLUR_MAC_LAN_DISCOVERY_TARGET_PORT",nullptr);setting("BLUR_MAC_LAN_DISCOVERY_TARGET_IP",nullptr);
}
int main(){alarm(30);try{artery();discovery();alarm(0);std::cout<<"Native instance settings: strict rejection, four actual Artery services/framing, default port collisions, original host plus three ephemeral nonce clients PASS (not joined game players)\n";return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
