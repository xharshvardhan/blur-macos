#if defined(__APPLE__) && defined(__MACH__) // [Blur2PC] Native BSD backend.
#include <core/Core.h>
#else
#include "SystemPch.h"
#endif
#include <System/Artery/BkSocket.h>
#if !defined(PLATFORM_MACOS) // [Blur2PC] No debug module dependency for socket primitives.
#include <System/Debug/Debug.h>
#endif

#include <sys/select.h>
#if defined(PLATFORM_MACOS) // [Blur2PC]
#include <poll.h>
#endif
#include <sys/time.h>

#if defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS) // [Blur2PC] Darwin BSD sockets.

#if !defined(PLATFORM_MACOS) // [Blur2PC]
#include <asm/socket.h>
#endif
#include <sys/socket.h>
#include <sys/termios.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/types.h>

#define socketclose ::close
#if defined(PLATFORM_MACOS) // [Blur2PC] Never block the service drain loops.
namespace {
bool NativePrepareSocket(int fd)
{
    const int flags = fcntl(fd, F_GETFL, 0);
    const int one = 1;
    return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0 &&
        fcntl(fd, F_SETFD, FD_CLOEXEC) == 0 &&
        setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one)) == 0;
}
}
#endif

#define SYS_NET_EISCONN		EISCONN
#define SYS_NET_EALREADY	EALREADY
#define SYS_NET_EINVAL		EINVAL
#define SYS_NET_ENOTCONN	ENOTCONN
#define SYS_NET_EWOULDBLOCK	EWOULDBLOCK

#define sys_net_errno errno

#endif

BkSocket::BkSocket() :
	_socket(-1)
{
}

BkSocket::~BkSocket()
{
	Close();
}

bool BkSocket::Connect(const BkSockAddr& sockAddr)
{
	Close();

	if ((_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		return false;

#if defined(PLATFORM_MACOS) // [Blur2PC] ArteryConnect initiates, then TestConnection finishes asynchronously.
    if (!NativePrepareSocket(_socket)) { Close(); return false; }
    const int connected = connect(_socket, sockAddr, sockAddr.Length());
    if (connected != 0 && errno != EINPROGRESS && errno != EINTR && errno != EALREADY)
#else
	if (connect(_socket, sockAddr, sockAddr.Length()) != 0)
#endif
	{
		Close();
		return false;
	}

	const int kTrue = 1;

#if defined(PLATFORM_PS3)
	if (setsockopt(_socket, SOL_SOCKET, SO_NBIO, (char*)&kTrue, sizeof(kTrue)) != 0)
	{
		Close();
		return false;
	}
#elif defined(PLATFORM_MACOS) // [Blur2PC]
	if (!NativePrepareSocket(_socket)) { Close(); return false; }
#else
	//u_long opts = 1;
	//ioctlsocket(_socket,FIONBIO,&opts);
#endif

	return true;
}

bool BkSocket::Listen(const BkSockAddr& sockAddr)
{
	Close();

	if ((_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		return false;

#if defined(PLATFORM_MACOS) // [Blur2PC] Permit clean service restart after TCP TIME_WAIT.
    const int reuse = 1;
    if (setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) != 0)
    { Close(); return false; }
#endif
	if (bind(_socket, sockAddr, sockAddr.Length()) != 0)
	{
		Close();
		return false;
	}

	if (listen(_socket, 5) != 0)
	{
		Close();
		return false;
	}

	const int kTrue = 1;

#if defined(PLATFORM_PS3)
	if (setsockopt(_socket, SOL_SOCKET, SO_NBIO, (char*)&kTrue, sizeof(kTrue)) != 0)
	{
		Close();
		return false;
	}
#elif defined(PLATFORM_MACOS) // [Blur2PC]
	if (!NativePrepareSocket(_socket)) { Close(); return false; }
#else
	//u_long opts = 1;
	//ioctlsocket(_socket,FIONBIO,&opts);
#endif

	return true;
}

bool BkSocket::Accept(BkSocket& socket)
{
	if (_socket < 0)
		return false;

	int rawSocket = accept(_socket, 0, 0);

	if (rawSocket < 0)
	{
		/*switch(sys_net_errno)
		{
			case SYS_NET_EINTR:			DebugLn("*** EINTR!");			break;
			case SYS_NET_EBADF:			DebugLn("*** EBADF!");			break;
			case SYS_NET_EWOULDBLOCK:	DebugLn("*** EWOULDBLOCK!");	break;
			case SYS_NET_EFAULT:		DebugLn("*** EFAULT!");			break;
			case SYS_NET_EINVAL:		DebugLn("*** EINVAL!");			break;
			case SYS_NET_EOPNOTSUPP:	DebugLn("*** EOPNOTSUPP!");		break;
			case SYS_NET_ECONNABORTED:	DebugLn("*** EOPNOTSUPP!");		break;
			default:					DebugLn("*** ?????!");			break;
		}*/

		return false;
	}

	if (socket.IsOpen())
		socket.Close();

	socket._socket = rawSocket;

	const int kTrue = 1;

#if defined(PLATFORM_PS3)
	if (setsockopt(socket._socket, SOL_SOCKET, SO_NBIO, (char*)&kTrue, sizeof(kTrue)) != 0)
	{
		Close();
		return false;
	}
#elif defined(PLATFORM_MACOS) // [Blur2PC]
	if (!NativePrepareSocket(socket._socket)) { socket.Close(); return false; }
#else
	//u_long opts = 1;
	//ioctlsocket(_socket,FIONBIO,&opts);
#endif

	return true;
}

bool BkSocket::Broadcast(int port)
{
	Close();

	if ((_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		return false;

	const int kTrue = 1;

	if (setsockopt(_socket, SOL_SOCKET, SO_BROADCAST, (char*)&kTrue, sizeof(kTrue)) != 0)
	{
		Close();
		return false;
	}

	if (setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&kTrue, sizeof(kTrue)) != 0)
	{
		Close();
		return false;
	}

#if defined(PLATFORM_PS3)
	if (setsockopt(_socket, SOL_SOCKET, SO_NBIO, (char*)&kTrue, sizeof(kTrue)) != 0)
	{
		Close();
		return false;
	}
#elif defined(PLATFORM_MACOS) // [Blur2PC]
	if (!NativePrepareSocket(_socket)) { Close(); return false; }
#else
	//u_long opts = 1;
	//ioctlsocket(_socket,FIONBIO,&opts);
#endif

	BkSockAddr sockAddr(INADDR_ANY, port);

	if (bind(_socket, sockAddr, sockAddr.Length()) != 0)
	{
		Close();
		return false;
	}

	return true;
}

void BkSocket::Close()
{
	if (_socket >= 0)
	{
		socketclose(_socket);
		_socket = -1;
	}
}

int BkSocket::TestConnection(BkSockAddr& sockAddr)
{
#if defined(PLATFORM_MACOS) // [Blur2PC] Query connection error and remote endpoint.
    if (_socket < 0) return -1;
    int error = 0;
    socklen_t length = sizeof(error);
    if (getsockopt(_socket, SOL_SOCKET, SO_ERROR, &error, &length) != 0) return -1;
    if (error == EINPROGRESS || error == EALREADY) return 0;
    if (error != 0) return -1;
    length = sockAddr.Length();
    if (getpeername(_socket, sockAddr, &length) == 0) return 1;
    return errno == ENOTCONN ? 0 : -1;
#else

	if (_socket < 0)
		return -1;

	if (recv(_socket, 0, 0, 0) >= 0)
		return 1;

	switch(sys_net_errno)
	{
		case SYS_NET_EWOULDBLOCK:
		return 1;
	}

	fd_set fwrite;
	fd_set fexcept;

	FD_ZERO(&fwrite);
	FD_ZERO(&fexcept);

	FD_SET(_socket, &fwrite);
	FD_SET(_socket, &fexcept);

	timeval tv;

	tv.tv_sec	= 0;
	tv.tv_usec	= 0;

	int result = socketselect(_socket + 1, 0, &fwrite, &fexcept, &tv);

	if (result == 0)
		return 0;
	else if (result < 0)
		return -1;

	if (FD_ISSET(_socket, &fexcept))
		return -1;

	if (FD_ISSET(_socket, &fwrite))
		return 1;

	return 0;
#endif
}

int BkSocket::Receive(void* data, int maxLength)
{
	if (_socket < 0)
		return -1;

	#if defined(PLATFORM_MACOS) // [Blur2PC]
    if (maxLength <= 0 || !data) return -1;
    int recvLength;
    do { recvLength = static_cast<int>(recv(_socket, data, maxLength, 0)); }
    while (recvLength < 0 && errno == EINTR);
#else
	int recvLength = recv(_socket, (char*)data, maxLength, 0);
#endif

	if (recvLength == 0)
		return -1;

	if (recvLength < 0)
	{
		switch(sys_net_errno)
		{
			case SYS_NET_EWOULDBLOCK:	return 0;
			default:					return -1;
		}
	}

	return recvLength;
}

int BkSocket::Send(const void* data, int length)
{
	if (_socket < 0)
		return -1;

	#if defined(PLATFORM_MACOS) // [Blur2PC]
    if (length <= 0 || !data) return -1;
    int sendLength;
    do { sendLength = static_cast<int>(send(_socket, data, length, 0)); }
    while (sendLength < 0 && errno == EINTR);
#else
	int sendLength = send(_socket, (const char*)data, length, 0);
#endif

	if (sendLength == 0)
		return -1;

	if (sendLength < 0)
	{
		switch(sys_net_errno)
		{
			case SYS_NET_EWOULDBLOCK:	return 0;
			default:					return -1;
		}
	}

	return sendLength;
}

int BkSocket::ReceiveFrom(void* data, int maxLength, BkSockAddr& address)
{
	if (_socket < 0)
		return -1;

	socklen_t addrLength = address.Length();
	#if defined(PLATFORM_MACOS) // [Blur2PC]
    if (maxLength <= 0 || !data) return -1;
    int recvLength;
    do { recvLength = static_cast<int>(recvfrom(_socket, data, maxLength, 0, address, &addrLength)); }
    while (recvLength < 0 && errno == EINTR);
#else
	int recvLength = recvfrom(_socket, (char*)data, maxLength, 0, address, &addrLength);
#endif

	if ((recvLength == 0) || ((int)addrLength != address.Length()))
		return -1;

	if (recvLength < 0)
	{
		switch(sys_net_errno)
		{
			case SYS_NET_EWOULDBLOCK:	return 0;
			default:					return -1;
		}
	}

	return recvLength;
}

int BkSocket::SendTo(const void* data, int length, const BkSockAddr& address)
{
	if (_socket < 0)
		return -1;

	#if defined(PLATFORM_MACOS) // [Blur2PC]
    if (length <= 0 || !data) return -1;
    int sendLength;
    do { sendLength = static_cast<int>(sendto(_socket, data, length, 0, address, address.Length())); }
    while (sendLength < 0 && errno == EINTR);
#else
	int sendLength = sendto(_socket, (const char*)data, length, 0, address, address.Length());
#endif

	if (sendLength == 0)
		return -1;

	if (sendLength < 0)
	{
		switch(sys_net_errno)
		{
			case SYS_NET_EWOULDBLOCK:	return 0;
			default:					return -1;
		}
	}

	return sendLength;
}

void BkSocket::Attach(BkSocket& socket)
{
	if (&socket != this)
	{
		Close();

		_socket			= socket._socket;
		socket._socket	= -1;
	}
}

bool CrossConnect(BkSocket& a, BkSocket& b)
{
	a.Close();
	b.Close();

	BkSocket l;

	if (!l.Listen(BkSockAddr(INADDR_LOOPBACK, 0)))
		return false;

	BkSockAddr addr;
	socklen_t addrLen = addr.Length();

	if (getsockname(l.Handle(), addr, &addrLen) != 0)
		return false;

	if (!a.Connect(addr))
		return false;

#if defined(PLATFORM_MACOS) // [Blur2PC] Complete the locally initiated wake connection before accepting.
    pollfd ready{l.Handle(), POLLIN, 0};
    int status;
    do { status = poll(&ready, 1, 1000); } while (status < 0 && errno == EINTR);
    if (status <= 0) { a.Close(); return false; }
#endif
	if (!l.Accept(b))
	{
		a.Close();
		return false;
	}

	return true;
}
