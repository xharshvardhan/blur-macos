// [Blur2PC] Actual NetPlay socket API over native BSD UDP, including twenty independent peers.
#include <Core/Core.h>
#include <netinet/in.h>
#include <network/UnreliableSocket.h>
#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#include <fcntl.h>
static void require(bool v,const char* text){if(!v)throw std::runtime_error(text);}
int main(){alarm(15);try{
 const int server=::socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);require(server>=0,"reference BSD socket creation failed");
 sockaddr_in target{};target.sin_len=sizeof(target);target.sin_family=AF_INET;target.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 require(::bind(server,reinterpret_cast<sockaddr*>(&target),sizeof(target))==0,"reference BSD bind failed");
 socklen_t length=sizeof(target);require(getsockname(server,reinterpret_cast<sockaddr*>(&target),&length)==0,"reference local address failed");
 require(fcntl(server,F_SETFL,O_NONBLOCK)==0,"reference nonblocking mode failed");
 std::array<NetPlay::C_UnreliableSocket,20> peers;
 for(unsigned n=0;n<peers.size();++n){auto& peer=peers[n];require(peer.Open()&&peer.Bind(0)&&peer.EnableNonBlockingMode(true),"actual native UDP peer setup failed");
  unsigned count=99;unsigned char empty[4]{};sockaddr_in from{};require(peer.RecvFrom(sizeof(empty),empty,count,from)&&count==0,"actual nonblocking empty receive contract changed");
  unsigned char payload[]{0x42,0x4c,0x52,static_cast<unsigned char>(n)};require(peer.SendTo(payload,sizeof(payload),target),"actual native UDP send failed");}
 unsigned mask=0;const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
 while(mask!=((1u<<peers.size())-1)){
  unsigned char data[32]{};sockaddr_in from{};socklen_t size=sizeof(from);const auto count=recvfrom(server,data,sizeof(data),0,reinterpret_cast<sockaddr*>(&from),&size);
  if(count<0){require(errno==EAGAIN||errno==EWOULDBLOCK,"reference UDP receive failed");require(std::chrono::steady_clock::now()<deadline,"native twenty-peer UDP receive timed out");std::this_thread::yield();continue;}
  require(count==4&&data[0]==0x42&&data[3]<peers.size(),"native UDP payload changed");mask|=1u<<data[3];
  require(sendto(server,data,count,0,reinterpret_cast<sockaddr*>(&from),size)==count,"reference UDP echo failed");
 }
 for(unsigned n=0;n<peers.size();++n){unsigned char data[32]{};unsigned count=0;sockaddr_in from{};
  do{require(peers[n].RecvFrom(sizeof(data),data,count,from),"native UDP response failed");if(!count)std::this_thread::yield();}while(!count&&std::chrono::steady_clock::now()<deadline);
  require(count==4&&data[3]==n&&from.sin_port==target.sin_port,"native UDP response or sender address changed");
  require(peers[n].EnableBroadcastMode(true)&&peers[n].EnableBroadcastMode(false),"native broadcast socket option failed");
 }
 require(!peers[0].Open(true)&&errno==EPROTONOSUPPORT,"unsupported Xbox VDP falsely succeeded");
 ::close(server);alarm(0);std::cout<<"Actual NetPlay BSD UDP: twenty independent peers, bidirectional payload/sender addresses, nonblocking receive, broadcast options and VDP rejection PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
