#include <StdAfx.h>
#include <Network\UnreliableSocket.h>

#if defined(PLATFORM_MACOS)
// [Blur2PC] The original socket API backed by real nonblocking BSD UDP.
namespace NetPlay {
C_UnreliableSocket::C_UnreliableSocket():m_Socket(-1),m_BroadcastMode(false){}
C_UnreliableSocket::~C_UnreliableSocket(){CloseSocket();}
bool C_UnreliableSocket::Open(const bool voiceAndData) {
 CloseSocket();
 if(voiceAndData){errno=EPROTONOSUPPORT;return false;} // Xbox VDP is not a UDP voice codec.
 m_Socket=::socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);return m_Socket>=0;
}
bool C_UnreliableSocket::Bind(const U16 portNumber) {
 if(m_Socket<0)return false;
 sockaddr_in address{};address.sin_len=sizeof(address);address.sin_family=AF_INET;
 address.sin_addr.s_addr=INADDR_ANY;address.sin_port=htons(portNumber);
 return ::bind(m_Socket,reinterpret_cast<const sockaddr*>(&address),sizeof(address))==0;
}
bool C_UnreliableSocket::EnableNonBlockingMode(const bool enabled) {
 if(m_Socket<0)return false;
 const int flags=fcntl(m_Socket,F_GETFL,0);if(flags<0)return false;
 return fcntl(m_Socket,F_SETFL,enabled?flags|O_NONBLOCK:flags&~O_NONBLOCK)==0;
}
bool C_UnreliableSocket::EnableBroadcastMode(const bool enabled) {
 if(m_Socket<0)return false;const int value=enabled?1:0;
 if(setsockopt(m_Socket,SOL_SOCKET,SO_BROADCAST,&value,sizeof(value))!=0)return false;
 m_BroadcastMode=enabled;return true;
}
bool C_UnreliableSocket::RecvFrom(const U32 capacity,void* buffer,U32& received,sockaddr_in& address) {
 received=0;address={};if(m_Socket<0 || !buffer || !capacity)return false;
 iovec span{buffer,capacity};sockaddr_in sender{};msghdr message{};
 message.msg_name=&sender;message.msg_namelen=sizeof(sender);message.msg_iov=&span;message.msg_iovlen=1;
 ssize_t result;do{result=recvmsg(m_Socket,&message,0);}while(result<0 && errno==EINTR);
 if(result<0)return errno==EAGAIN || errno==EWOULDBLOCK;
 if(message.msg_flags&MSG_TRUNC){errno=EMSGSIZE;return false;}
 received=static_cast<U32>(result);address=sender;return true;
}
bool C_UnreliableSocket::SendTo(void* buffer,const U32 size,const sockaddr_in& remote) {
 if(m_Socket<0 || !buffer || !size)return false;
 sockaddr_in address=remote;address.sin_len=sizeof(address);
 ssize_t result;do{result=sendto(m_Socket,buffer,size,0,reinterpret_cast<const sockaddr*>(&address),sizeof(address));}while(result<0 && errno==EINTR);
 return result>=0 && static_cast<U32>(result)==size;
}
bool C_UnreliableSocket::SendBroadcast(void* buffer,const U32 size,const U32 port) {
 if(!m_BroadcastMode || port>65535)return false;
 sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=INADDR_BROADCAST;address.sin_port=htons(static_cast<U16>(port));
 return SendTo(buffer,size,address);
}
void C_UnreliableSocket::CloseSocket(){if(m_Socket>=0){::close(m_Socket);m_Socket=-1;}m_BroadcastMode=false;}
}
#else
extern in_addr sIn;

const char* GetLastErrorStr()
{
#define WSATOSTR(x) case WSA##x: return "WSA"#x;
	const int err = WSAGetLastError();
	switch(err)
	{
		WSATOSTR(EWOULDBLOCK)
			WSATOSTR(EINPROGRESS)
			WSATOSTR(EALREADY)
			WSATOSTR(ENOTSOCK)
			WSATOSTR(EDESTADDRREQ)
			WSATOSTR(EMSGSIZE)
			WSATOSTR(EPROTOTYPE)
			WSATOSTR(ENOPROTOOPT)
			WSATOSTR(EPROTONOSUPPORT)
			WSATOSTR(ESOCKTNOSUPPORT)
			WSATOSTR(EOPNOTSUPP)
			WSATOSTR(EPFNOSUPPORT)
			WSATOSTR(EAFNOSUPPORT)
			WSATOSTR(EADDRINUSE)
			WSATOSTR(EADDRNOTAVAIL)
			WSATOSTR(ENETDOWN)
			WSATOSTR(ENETUNREACH)
			WSATOSTR(ENETRESET)
			WSATOSTR(ECONNABORTED)
			WSATOSTR(ECONNRESET)
			WSATOSTR(ENOBUFS)
			WSATOSTR(EISCONN)
			WSATOSTR(ENOTCONN)
			WSATOSTR(ESHUTDOWN)
			WSATOSTR(ETOOMANYREFS)
			WSATOSTR(ETIMEDOUT)
			WSATOSTR(ECONNREFUSED)
			WSATOSTR(ELOOP)
			WSATOSTR(ENAMETOOLONG)
			WSATOSTR(EHOSTDOWN)
			WSATOSTR(EHOSTUNREACH)
			WSATOSTR(ENOTEMPTY)
			WSATOSTR(EPROCLIM)
			WSATOSTR(EUSERS)
			WSATOSTR(EDQUOT)
			WSATOSTR(ESTALE)
			WSATOSTR(EREMOTE)
			WSATOSTR(EDISCON)
			WSATOSTR(SYSNOTREADY)
			WSATOSTR(VERNOTSUPPORTED)
			WSATOSTR(NOTINITIALISED)
	default:
		return "WSA ERROR UNHANDELED";
	}
}

namespace NetPlay
{
	// constructor
	C_UnreliableSocket::C_UnreliableSocket() : m_Socket(INVALID_SOCKET), m_BroadcastMode(false)
	{
	}
	
	// destructor
	C_UnreliableSocket::~C_UnreliableSocket() 
	{
		CloseSocket();
	}


	// try and open a new socket. UDP or VDP on xenon..
	bool C_UnreliableSocket::Open(const bool voiceAndData)
	{
		CloseSocket();
#if defined (LIVE_ON_WINDOWS)
		m_Socket = XSocketCreate(AF_INET, SOCK_DGRAM, voiceAndData ? IPPROTO_VDP : IPPROTO_UDP);
		if (INVALID_SOCKET == m_Socket)
		{
			LOGWARN(g_Log, "Unable to create unreliable socket.");
		}
#endif
		return m_Socket != INVALID_SOCKET;
	}
  

	// try and bind the opened socket to a specific port
	bool C_UnreliableSocket::Bind(const U16 portNumber)
	{
#if defined (LIVE_ON_WINDOWS)
		if (INVALID_SOCKET != m_Socket)
		{
			SOCKADDR_IN sI;
			ZeroMemory(&sI, sizeof(sI));
			sI.sin_family		= AF_INET;
			sI.sin_addr.s_addr	= INADDR_ANY;
			sI.sin_port			= XSocketHTONS(portNumber);

			S32 res = XSocketBind(m_Socket, (const sockaddr*)(&sI), sizeof(SOCKADDR_IN));
			return res != SOCKET_ERROR;
		}
#endif
		return false;
	}

	// change the socket into sync or asynchronous send / receive
	bool C_UnreliableSocket::EnableNonBlockingMode(const bool enableNonBlocking)
	{
#if defined (LIVE_ON_WINDOWS)
		if (INVALID_SOCKET != m_Socket)
		{
			DWORD nonBlock = enableNonBlocking ? 1 : 0;
			const S32 res = XSocketIOCTLSocket(m_Socket, FIONBIO, & nonBlock);
			return res != SOCKET_ERROR;
		}
#endif
		return false;
	}


	// change the socket into or out of broadcast mode..
	bool C_UnreliableSocket::EnableBroadcastMode(const bool enableBroadcast)
	{
#if defined (LIVE_ON_WINDOWS)
		if (INVALID_SOCKET != m_Socket)
		{
			S32 res = XSocketSetSockOpt(m_Socket, SOL_SOCKET, SO_BROADCAST, (const char*)&enableBroadcast, sizeof(enableBroadcast));
			if (res != SOCKET_ERROR)
			{
				m_BroadcastMode = enableBroadcast;
				return true;
			}
		}
#endif
		return false;
	}


	// attempt to read data from the socket..
	bool C_UnreliableSocket::RecvFrom(const U32 maxBytesToReceive, void* pOutBuffer, U32& outBytesReceived, sockaddr_in& outAddress)
	{
#if defined (LIVE_ON_WINDOWS)
		if (INVALID_SOCKET != m_Socket)
		{
			if (maxBytesToReceive)
			{
				if (pOutBuffer)
				{
					S32 size = sizeof(SOCKADDR_IN);
					SOCKADDR_IN sender;	
					memset(&outAddress, 0, sizeof(outAddress));
					S32 bytesReceived = XSocketRecvFrom(m_Socket, (char*)(pOutBuffer), maxBytesToReceive, 0, (sockaddr*)(&sender), &size);
					
					if (SOCKET_ERROR != bytesReceived)
					{
						outBytesReceived		=	bytesReceived;
						outAddress				=	sender;
						return true;
					}
				}
			}
		}

		// [OR - 17 February 2006] - OK, no data in.
		// if socket was blocking, it would block, waiting for data
		const int errCode = WSAGetLastError();
		if(errCode == WSAEWOULDBLOCK)
		{
			outBytesReceived = 0;
			return true;
		}

		LOGWARN( g_Log, "Socket error : (%d) \"%s\".\n", WSAGetLastError(), GetLastErrorStr());
#endif
		return false;
	}

	// attempt to send data to a remote client.
	// assumes all of remoteAddress is correctly configured.. address and port
	bool C_UnreliableSocket::SendTo(void* pSendBuffer, const U32 numBytesToSend, const sockaddr_in& remoteAddress)
	{
#if defined (LIVE_ON_WINDOWS)
		if (INVALID_SOCKET != m_Socket && numBytesToSend && pSendBuffer)
		{

			sockaddr_in sa = {0};
			sa.sin_addr   = remoteAddress.sin_addr;
			sa.sin_family = remoteAddress.sin_family;
			sa.sin_port   = remoteAddress.sin_port;

			const S32 bytesSent = XSocketSendTo(m_Socket, (const char*)(pSendBuffer), numBytesToSend, 0, (const sockaddr*)(&sa), sizeof(sa));
		
			if (SOCKET_ERROR == bytesSent)
			{
				/*
				char buff[32];
				sprintf_s(buff, sizeof(buff), "%u.%u.%u.%u:%u", 
					remoteAddress.m_Address.sin_addr.S_un.S_un_b.s_b1, 
					remoteAddress.m_Address.sin_addr.S_un.S_un_b.s_b2,
					remoteAddress.m_Address.sin_addr.S_un.S_un_b.s_b3,
					remoteAddress.m_Address.sin_addr.S_un.S_un_b.s_b4,
					ntohs(remoteAddress.m_Address.sin_port));

				LOGERROR( g_Log, DBSTR "SendTo[%s] Socket error : (%d) \"%s\".\n", buff, WSAGetLastError(), GetLastErrorStr());
				/**/
				return false;
			}
			else
			{
				return (bytesSent == numBytesToSend);
			}
		
		}
#endif
		return false;
	}


	// broadcast data to all addresses on specified port..
	bool C_UnreliableSocket::SendBroadcast(void* pSendBuffer, const U32 numBytesToSend, const U32 portNumber)
	{
#if defined (LIVE_ON_WINDOWS)
		if (INVALID_SOCKET != m_Socket && numBytesToSend && pSendBuffer)
		{
			if (m_BroadcastMode)
			{
				SOCKADDR_IN sI;
				ZeroMemory(&sI, sizeof(sI));
				sI.sin_family		= AF_INET;
				sI.sin_addr.s_addr	= INADDR_BROADCAST;
				sI.sin_port			= XSocketHTONS(portNumber);

				const S32 bytesSent = XSocketSendTo(m_Socket, (const char*)(pSendBuffer), numBytesToSend, 0, (const sockaddr*)(&sI), sizeof(SOCKADDR_IN));
				if (SOCKET_ERROR != bytesSent)
					return bytesSent == numBytesToSend; 
			}
			else
			{
				LOGWARN( g_Log,  "Attempting to broadcast data on a non broadcast enabled socket..\n");
			}
		}
#endif
		return false;
	}


	// private function to close the socket if open
	void C_UnreliableSocket::CloseSocket(void)
	{
#if defined (LIVE_ON_WINDOWS)
		if (INVALID_SOCKET != m_Socket)
		{
			XSocketClose(m_Socket);
			m_Socket = INVALID_SOCKET;
		}
#endif
	}
}
#endif // [Blur2PC] Native BSD/legacy Windows platform selection.
