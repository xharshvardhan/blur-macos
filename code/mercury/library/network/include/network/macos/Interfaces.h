// [Blur2PC] Native adapter enumeration for the original LAN transport.
#ifndef NETWORK_MACOS_INTERFACES_H
#define NETWORK_MACOS_INTERFACES_H
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <vector>
namespace NetPlay { namespace Native {
inline bool EnumerateIPv4(std::vector<in_addr>& addresses) {
 addresses.clear();
 ifaddrs* interfaces=nullptr;
 if(getifaddrs(&interfaces)!=0)return false;
 for(auto* item=interfaces;item;item=item->ifa_next) {
  if(!item->ifa_addr || item->ifa_addr->sa_family!=AF_INET || !(item->ifa_flags&IFF_UP) || (item->ifa_flags&IFF_LOOPBACK))continue;
  const auto address=reinterpret_cast<const sockaddr_in*>(item->ifa_addr)->sin_addr;
  if(address.s_addr==INADDR_ANY || address.s_addr==INADDR_BROADCAST)continue;
  bool duplicate=false;for(const auto existing:addresses)if(existing.s_addr==address.s_addr)duplicate=true;
  if(!duplicate)addresses.push_back(address);
 }
 freeifaddrs(interfaces);return true;
}
}}
#endif
