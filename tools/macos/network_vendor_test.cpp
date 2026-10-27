// [Blur2PC] Real original Demonware UDP, wire serialization, crypto and Darwin platform runtime checks.
#include <bdCore/bdCore.h>
#include <bdLobby/bdTitleUtilities/bdOnlineUserInfo.h>
#include <bdLobby/bdContentStreaming/bdUploadInterceptor.h>
#include <bdLobby/bdContentStreaming/bdDownloadInterceptor.h>
#include <typeinfo>
#include <bdCore/bdSocket/bdSocket.h>
#include <bdCore/bdCrypto/bdCypher3Des.h>
#include <bdCore/bdCrypto/bdECCKey.h>
#include <bdPlatform/bdPlatformThread/bdPlatformMutex.h>
#include <bdPlatform/bdPlatformThread/bdPlatformSemaphore.h>
#include <bdPlatform/bdPlatformThread/bdPlatformSignal.h>
#include <bdPlatform/bdPlatformThread/bdNativeCountState.h>
#include <bdPlatform/bdPlatformTiming/bdPlatformTiming.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static bdAddr localAddress(const bdSocket& socket){
 sockaddr_in address{};socklen_t length=sizeof(address);
 require(getsockname(socket.getHandle(),reinterpret_cast<sockaddr*>(&address),&length)==0,"getsockname failed");
 return bdAddr(bdInetAddr::Loopback(),ntohs(address.sin_port));
}
static void awaitWaiter(bdNativeCountState* state){
 const auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(2);
 for(;;){pthread_mutex_lock(&state->mutex);const bool waiting=state->waiters!=0;pthread_mutex_unlock(&state->mutex);
  if(waiting)return;require(std::chrono::steady_clock::now()<limit,"native waiter did not block");std::this_thread::yield();}
}
int main(){alarm(20);try{
 bdCore::init();
 {
  // [Blur2PC] Actual key-function units must emit RTTI for legacy callbacks:
  // the game UBSan link previously failed on all three missing definitions.
  bdOnlineUserInfo onlineInfo;
  bdTaskResult* taskResult=&onlineInfo;
  require(typeid(*taskResult)==typeid(bdOnlineUserInfo)&&dynamic_cast<bdOnlineUserInfo*>(taskResult)==&onlineInfo,
          "original task result RTTI/key-function source missing");
  require(typeid(bdUploadInterceptor)!=typeid(bdDownloadInterceptor),
          "original upload/download interceptor RTTI source missing");
  auto mutex=bdPlatformMutex::createMutex();require(mutex!=nullptr,"native mutex creation failed");
  unsigned protectedCount=0;std::array<std::thread,4> workers;
  for(auto& worker:workers)worker=std::thread([&]{for(unsigned n=0;n<4000;++n){bdPlatformMutex::lock(mutex);++protectedCount;bdPlatformMutex::unlock(mutex);}});
  for(auto& worker:workers)worker.join();require(protectedCount==16000,"native mutex lost exclusion");bdPlatformMutex::destroy(mutex);
  auto semaphore=bdPlatformSemaphore::createSemaphore(1,2);require(semaphore!=nullptr,"native semaphore creation failed");
  require(bdPlatformSemaphore::wait(semaphore),"initial semaphore count lost");
  auto wait=std::async(std::launch::async,[&]{return bdPlatformSemaphore::wait(semaphore);});awaitWaiter(semaphore);
  bdPlatformSemaphore::release(semaphore);require(wait.get(),"native semaphore release did not wake waiting thread");
  auto canceled=std::async(std::launch::async,[&]{return bdPlatformSemaphore::wait(semaphore);});awaitWaiter(semaphore);
  bdPlatformSemaphore::destroy(semaphore);require(!canceled.get()&&semaphore==nullptr,"destroy did not cancel semaphore wait");
  auto event=bdPlatformSignal::create();require(event!=nullptr,"native event creation failed");
  bdPlatformSignal::signal(event);bdPlatformSignal::signal(event);require(bdPlatformSignal::wait(event),"signal-before-wait lost event");
  auto eventWait=std::async(std::launch::async,[&]{return bdPlatformSignal::wait(event);});awaitWaiter(event);
  bdPlatformSignal::signal(event);require(eventWait.get(),"auto-reset event did not wake");bdPlatformSignal::destroy(event);
  const auto before=bdPlatformTiming::getHiResTimeStamp();bdPlatformTiming::sleep(12);const auto after=bdPlatformTiming::getHiResTimeStamp();
  require(after>=before&&bdPlatformTiming::getElapsedTime(before,after)>=0.010f,"native monotonic timer units changed");
  bdSocket host;require(host.create(false,false)&&host.bind(0)==BD_NET_SUCCESS,"original UDP host creation/bind failed");
  const auto destination=localAddress(host);std::array<bdSocket,6> peers;
  for(unsigned n=0;n<peers.size();++n){require(peers[n].create(false,false)&&peers[n].bind(0)==BD_NET_SUCCESS,"original UDP peer bind failed");
   const std::array<unsigned char,4> payload{0x42,0x4c,0x52,static_cast<unsigned char>(n)};
   require(peers[n].sendTo(destination,payload.data(),payload.size())==payload.size(),"original UDP peer send failed");}
  unsigned mask=0;const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
  while(mask!=((1u<<peers.size())-1)){
   std::array<unsigned char,16> data{};bdAddr from;const int count=host.receiveFrom(from,data.data(),data.size());
   if(count==BD_NET_WOULD_BLOCK){require(std::chrono::steady_clock::now()<deadline,"original UDP receive timed out");std::this_thread::yield();continue;}
   require(count==4&&data[0]==0x42&&data[3]<peers.size(),"original UDP payload altered");mask|=1u<<data[3];
   require(host.sendTo(from,data.data(),count)==count,"original UDP response failed");
  }
  for(auto& peer:peers){std::array<unsigned char,16> data{};bdAddr from;int count;
   do{count=peer.receiveFrom(from,data.data(),data.size());if(count==BD_NET_WOULD_BLOCK)std::this_thread::yield();}while(count==BD_NET_WOULD_BLOCK&&std::chrono::steady_clock::now()<deadline);
   require(count==4,"original UDP peer response missing");require(peer.close(),"original peer socket did not close");}
  require(host.close(),"original host socket did not close");
  const bdAddr golden(bdInetAddr("127.0.0.1"),0x1234);std::array<unsigned char,6> serialized{};bdUInt end=0;
  require(golden.serialize(serialized.data(),serialized.size(),0,end)&&end==6,"original address wire size changed");
  require(serialized==std::array<unsigned char,6>{127,0,0,1,0x34,0x12},"original PC little-endian address wire bytes changed");
  bdAddr decoded;require(decoded.deserialize(serialized.data(),serialized.size(),0,end)&&decoded==golden,"original address roundtrip changed");
  const std::array<unsigned char,24> key{0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,0x01,0x45,0x67,0x89,0xab,0xcd,0xef,0x01,0x23};
  const std::array<unsigned char,8> iv{0x12,0x34,0x56,0x78,0x90,0xab,0xcd,0xef},plain{0,0x11,0x22,0x33,0x44,0x55,0x66,0x77};
  std::array<unsigned char,8> cipher{},clear{};bdCypher3Des crypt;
  require(crypt.init(key.data(),key.size())&&crypt.encrypt(iv.data(),plain.data(),cipher.data(),plain.size()),"original crypto/native LibTomCrypt failed");
  // Authored independent OpenSSL3 EVP cross-check with identical key/IV/plaintext.
  require(cipher==std::array<unsigned char,8>{0x39,0xff,0x86,0x20,0xe5,0x87,0x4b,0xd0},"native cipher changed legacy wire bytes");
  require(crypt.decrypt(iv.data(),cipher.data(),clear.data(),clear.size())&&clear==plain,"original crypto decrypt failed");
  bdECCKey first,second;require(first.init()&&second.init(),"native original ECC initialization failed");
  std::array<unsigned char,BD_ECC_EXPORTED_KEY_SIZE> firstPublic{},secondPublic{};bdUInt firstLength=firstPublic.size(),secondLength=secondPublic.size();
  require(first.exportKey(firstPublic.data(),firstLength)&&second.exportKey(secondPublic.data(),secondLength),"native original ECC public export failed");
  std::array<unsigned char,BD_ECC_SHARED_SECRET_SIZE> firstSecret{},secondSecret{};
  require(first.generateSharedSecret(secondPublic.data(),secondLength,firstSecret.data(),firstSecret.size())&&second.generateSharedSecret(firstPublic.data(),firstLength,secondSecret.data(),secondSecret.size())&&firstSecret==secondSecret,"native original ECC peer agreement failed");
 }
 bdCore::quit();alarm(0);std::cout<<"Original Demonware native runtime: mutex/semaphore/event/timer, six UDP peers, PC address wire bytes, independent crypto vector, ECC agreement and original callback RTTI PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
