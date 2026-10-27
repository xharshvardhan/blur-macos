#include "SystemPch.h"
#include "ArteryImpl.h"
#if defined(PLATFORM_MACOS) // [Blur2PC]
#include <poll.h>
#include <unistd.h>
#define socketpoll ::poll
#include <cstdio>
#include <cstring>
#include <limits>
#include <charconv>
#include <cstdlib>
namespace {
// [Blur2PC] Per-process development endpoints; absent settings retain the PC ports.
bool NativeArteryPort(const char* setting, int fallback, int& port)
{
    port = fallback;
    const char* value = std::getenv(setting);
    if (!value) return true;
    const char* end = value + std::strlen(value);
    unsigned parsed = 0;
    const auto result = std::from_chars(value, end, parsed);
    if (result.ec != std::errc() || result.ptr != end || parsed == 0 || parsed > 65535)
    {
        LOGERROR(Debug::GetLogger("system.artery"), "Invalid %s: expected a decimal port in 1..65535", setting);
        return false;
    }
    port = static_cast<int>(parsed);
    return true;
}
}
#endif

#if defined(PLATFORM_PS3)
#include <netex/libnetctl.h>
#include <sysutil/sysutil_common.h>
#include <sysutil/sysutil_sysparam.h>
#endif

#ifndef CFG_GOLD

#if !defined(PLATFORM_PC) && !defined(PLATFORM_PS3) && !defined(PLATFORM_LINUX) && !defined(PLATFORM_MACOS) /* [Blur2PC] */
#error Platform not supported - see SteveC
#endif

ArteryService::ArteryService() :
	_references(0),
#if defined(PLATFORM_PC)
	_wakeEvent(0),
#endif
	_shutdown(true),
	_isServer(false)
{
#if defined(PLATFORM_MACOS) // [Blur2PC] Original global was zero-filled; local instances also need initialization.
    _maxPeers = 0; _peers = nullptr;
    std::memset(_services, 0, sizeof(_services));
#endif
}

ArteryService::~ArteryService()
{
}

bool ArteryService::Initialise(bool isServer, const char* gameName, int maxPeers)
{
	if (_references > 0)
	{
		_references++;
		return true;
	}

#if defined(PLATFORM_MACOS) // [Blur2PC] Bound poll/thread/peer allocations and reject invalid callbacks.
    if (!gameName || maxPeers <= 0 || maxPeers > std::numeric_limits<int>::max() - 3) return false;
    int nativeListenPort, nativeBroadcastPort;
    if (!NativeArteryPort("BLUR_MAC_ARTERY_TCP_PORT", kArteryListenPort, nativeListenPort) ||
        !NativeArteryPort("BLUR_MAC_ARTERY_UDP_PORT", kArteryBroadcastPort, nativeBroadcastPort))
        return false; // [Blur2PC] Reject invalid configuration before allocating or binding anything.
#endif
	_maxPeers = maxPeers;

	_peers = new ArteryPeer[_maxPeers];

	_isServer = isServer;

	InitialiseBroadcastReply(gameName);

#if defined(PLATFORM_PC)
	WSADATA wd;

	if (WSAStartup(WINSOCK_VERSION, &wd) == 0)
#endif
	{
#if defined(PLATFORM_PC)
		if ((_wakeEvent = CreateEvent(0, FALSE, FALSE, 0)) != 0)
#elif defined(PLATFORM_PS3) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS) /* [Blur2PC] BSD event transport. */
		if (CrossConnect(_wakeIn, _wakeOut))
#endif
		{
			bool successful = false;

			if (_isServer)
			{
#if defined(PLATFORM_MACOS)
                // [Blur2PC] Keep the original service/wire protocol on separately configured native endpoints.
                if (_listen.Listen(ArterySockAddr(INADDR_ANY, nativeListenPort)))
#else
				if (_listen.Listen(ArterySockAddr(INADDR_ANY, kArteryListenPort)))
#endif
				{
#if defined(PLATFORM_MACOS)
                    if (_broadcast.Broadcast(nativeBroadcastPort))
#else
					if (_broadcast.Broadcast(kArteryBroadcastPort))
#endif
						successful = true;
				}
			}
			else
				successful = true;

			if (successful)
			{
				for(int i = 0; i < _maxPeers; i++)
				{
					if (!_peers[i].Create(this, i))
					{
						successful = false;
						break;
					}
				}
			}

			_shutdown = false;

			if (successful && _thread.Create(ThreadProc, this))
			{
				_references++;
				return true;
			}

			_shutdown = true;

			for(int i = 0; i < _maxPeers; i++)
				_peers[i].Destroy();

			_broadcast.Close();
			_listen.Close();

#if defined(PLATFORM_PC)
			CloseHandle(_wakeEvent);
			_wakeEvent = 0;
#elif defined(PLATFORM_PS3) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS) /* [Blur2PC] BSD event transport. */
			_wakeIn.Close();
			_wakeOut.Close();
#endif
		}

#if defined(PLATFORM_PC)
		WSACleanup();
#endif
	}

#if defined(PLATFORM_MACOS) // [Blur2PC] Clean up failed startup, including retry.
    delete[] _peers; _peers = nullptr; _maxPeers = 0;
#endif
	return false;
}

void ArteryService::Destroy()
{
	if ((_references > 0) && (--_references <= 0))
	{
		_shutdown = true;

		_peerLock.Wait();

		for(int i = 0; i < _maxPeers; i++)
		{
			ArteryPeer* peer = &_peers[i];

			if (peer->_isActive)
				peer->Close(peer->_salt);
		}

		_peerLock.Release();

		WakeUp();

		_thread.Close();

		for(int i = 0; i < _maxPeers; i++)
			_peers[i].Destroy();

		_broadcast.Close();
		_listen.Close();

#if defined(PLATFORM_PC)
		if (_wakeEvent != 0)
		{
			CloseHandle(_wakeEvent);
			_wakeEvent = 0;
		}

		WSACleanup();
#elif defined(PLATFORM_PS3) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS) /* [Blur2PC] BSD event transport. */
		_wakeIn.Close();
		_wakeOut.Close();
#endif
#if defined(PLATFORM_MACOS) // [Blur2PC] Original peers leaked across each startup/cleanup.
        delete[] _peers; _peers = nullptr; _maxPeers = 0;
        std::memset(_services, 0, sizeof(_services));
#endif
	}
}

bool ArteryService::Register(const char* id, ArteryCallback callback, void* context)
{
#if defined(PLATFORM_MACOS) // [Blur2PC] Keep the NUL-terminated on-wire service ID in range.
    if (!_references || !id || !callback || std::strlen(id) >= kArteryMaxServiceIdLength) return false;
#endif
	bool result = false;

	_serviceLock.Wait();

	if (!FindService(id))
	{
		for(int i = 0; i < kMaxServices; i++)
		{
			ServiceInfo* si = &_services[i];

			if (si->callback == 0)
			{
				si->callback	= callback;
				si->context		= context;

				_snprintf(si->id, kArteryMaxServiceIdLength, "%s", id);

				result = true;

				break;
			}
		}
	}

	_serviceLock.Release();

	return result;
}

bool ArteryService::Connect(const ArterySockAddr& target, const char* id, ArteryCallback callback, void* context, ArteryConnection& con)
{
	bool result = false;

#if defined(PLATFORM_MACOS) // [Blur2PC]
    if (!_references || !id || !callback || std::strlen(id) >= kArteryMaxServiceIdLength) return false;
#endif
	ArterySocket skt;

	if (skt.Connect(target))
	{
		_peerLock.Wait();

		if (!_shutdown)
		{
			for(int i = 0; i < _maxPeers; i++)
			{
				ArteryPeer* peer = &_peers[i];

				if (!peer->_isActive)
				{
					result = peer->ManageClient(skt, id, callback, context, target);

					if (result)
						con = ArteryConnection(peer);

					break;
				}
			}
		}

		_peerLock.Release();
	}

	return result;
}

void ArteryService::Main()
{

	for(;;)
	{
#if defined(PLATFORM_PC)
		HANDLE* handles = new HANDLE[3 + _maxPeers];

		int		peerCount	= 0;
		int		handleCount	= 0;

		if (_isServer)
		{
			handles[handleCount++]	= _listen.Event();
			handles[handleCount++]	= _broadcast.Event();
		}

		handles[handleCount++] = _wakeEvent;

		_peerLock.Wait();

		for(int i = 0; i < _maxPeers; i++)
		{
			ArteryPeer* peer = &_peers[i];

			if (peer->_isActive)
			{
				peerCount++;

				if (peer->_isConnected)
					handles[handleCount++] = peer->_socket.Event();
			}
		}

		_peerLock.Release();
		
		if (_shutdown && (peerCount == 0))
		{
			delete [] handles;
			break;
		}

		int result = WaitForMultipleObjects(handleCount, handles, FALSE, INFINITE);

		delete [] handles;

#elif defined(PLATFORM_PS3) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS) /* [Blur2PC] BSD event transport. */
		pollfd* fds = new pollfd[3 + _maxPeers];
	
		int peerCount	= 0;
		int fdCount		= 0;

		if (_isServer)
		{
			fds[fdCount].fd			= _listen.Handle();
			fds[fdCount].events		= POLLIN;
			fds[fdCount].revents	= 0;
			fdCount++;

			fds[fdCount].fd			= _broadcast.Handle();
			fds[fdCount].events		= POLLIN;
			fds[fdCount].revents	= 0;
			fdCount++;
		}

		fds[fdCount].fd			= _wakeOut.Handle();
		fds[fdCount].events		= POLLIN;
		fds[fdCount].revents	= 0;
		fdCount++;

		_peerLock.Wait();

		for(int i = 0; i < _maxPeers; i++)
		{
			ArteryPeer* peer = &_peers[i];

			if (peer->_isActive)
			{
				peerCount++;

				if (peer->_isConnected)
				{
					peer->_mutex.Wait();

					fds[fdCount].fd			= peer->_socket.Handle();
					fds[fdCount].events		= POLLIN;
					fds[fdCount].revents	= 0;

					if ((peer->_sendRemaining > 0) || !peer->_sendQ.IsEmpty())
						fds[fdCount].events |= POLLOUT;

					fdCount++;

					peer->_mutex.Release();
				}
			}
		}

		_peerLock.Release();
		
		if (_shutdown && (peerCount == 0))
		{
			delete [] fds;
			break;
		}

		socketpoll(fds, fdCount, -1);

		for(;;)
		{
			char buf[16];

			if (_wakeOut.Receive(buf, 16) <= 0)
				break;
		}

		delete [] fds;
#endif

		// Accept any new connections

		if (!_shutdown)
		{
			ArterySocket accept;

			if (_listen.Accept(accept))
			{
				_peerLock.Wait();

				for(int i = 0; i < _maxPeers; i++)
				{
					ArteryPeer* peer = &_peers[i];

					if (!peer->_isActive)
					{
						peer->ManageServer(accept);
						break;
					}
				}

				_peerLock.Release();
			}
		}

		// Check for broadcasts

		if (!_shutdown)
		{
			for(;;)
			{
				ArteryBroadcastMessage	msg;
				ArterySockAddr			addr;

				int length = _broadcast.ReceiveFrom(&msg, sizeof(msg), addr);

				if (length <= 0)
					break;

				if (
						(length == sizeof(msg)) &&
						(ntohl(msg.length) == sizeof(msg)) &&
						(ntohl(msg.tag) == kAtHeader) &&
						(ntohl(msg.request) == kAtSearch)
					)
				{
					_broadcast.SendTo(&_broadcastReply, sizeof(_broadcastReply), addr);
				}
			}
		}

		// Update peers

		_peerLock.Wait();

		for(int i = 0; i < _maxPeers; i++)
			_peers[i].Update();

		_peerLock.Release();
	}
}

void ArteryService::ThreadProc(void* context)
{
	MEMORY_TRACKER_PUSH("mercury.artery");

	((ArteryService*)context)->Main();

	MEMORY_TRACKER_POP("mercury.artery");
}

void ArteryService::InitialiseBroadcastReply(const char* gameName)
{
	_broadcastReply.length	= htonl(sizeof(_broadcastReply));
	_broadcastReply.tag		= htonl(kAtHeader);
	_broadcastReply.request	= htonl(kAtSearchReply);

	_snprintf(_broadcastReply.game, sizeof(_broadcastReply.game), "%s", gameName); // [Blur2PC]

#ifndef CFG_GOLD
#if defined(PLATFORM_PC)
	DWORD nameLen = sizeof(_broadcastReply.machine);

	if (GetComputerName(_broadcastReply.machine, &nameLen) == FALSE)
		_snprintf(_broadcastReply.machine, sizeof(_broadcastReply.machine), "unknown");
#elif defined(PLATFORM_PS3)
	char nickname[CELL_SYSUTIL_SYSTEMPARAM_NICKNAME_SIZE];

	*nickname = '\0';

	if ((cellSysutilGetSystemParamString(CELL_SYSUTIL_SYSTEMPARAM_ID_NICKNAME, nickname, sizeof(nickname)) < 0) || (*nickname == '\0'))
	{
		CellNetCtlInfo info;

		if (cellNetCtlGetInfo(CELL_NET_CTL_INFO_IP_ADDRESS, &info) == 0)
			_snprintf(_broadcastReply.machine, sizeof(_broadcastReply.machine), "ps3-%s", info.ip_address);
		else
			_snprintf(_broadcastReply.machine, sizeof(_broadcastReply.machine), "unknown");
	}
	else
		_snprintf(_broadcastReply.machine, sizeof(_broadcastReply.machine), "%s", nickname);
#elif defined(PLATFORM_MACOS) // [Blur2PC]
    if (gethostname(_broadcastReply.machine, sizeof(_broadcastReply.machine)) != 0)
        _snprintf(_broadcastReply.machine, sizeof(_broadcastReply.machine), "unknown");
    _broadcastReply.machine[sizeof(_broadcastReply.machine) - 1] = 0;
#else
	_snprintf(_broadcastReply.machine, sizeof(_broadcastReply.machine), "unknown");
#endif
#else
	_snprintf(_broadcastReply.machine, sizeof(_broadcastReply.machine), "unknown");
#endif

#if defined(PLATFORM_PC)
	const char* platform = "pc";
#elif defined(PLATFORM_PS3)
	const char* platform = "ps3";
#elif defined(PLATFORM_MACOS) // [Blur2PC]
    const char* platform = "macos";
#elif defined(PLATFORM_LINUX)
	const char* platform = "linux";
#else
	const char* platform = "unknown";
#endif

	_snprintf(_broadcastReply.platform, sizeof(_broadcastReply.platform), "%s", platform); // [Blur2PC]
}

ArteryService::ServiceInfo* ArteryService::FindService(const char* id)
{
	for(int i = 0; i < kMaxServices; i++)
	{
		ServiceInfo* si = &_services[i];

		if (si->callback == 0)
			continue;

		if (strcmp(si->id, id) == 0)
			return si;
	}

	return 0;
}

bool ArteryService::SetupPeerCallback(ArteryPeer* peer)
{
	bool result = false;

	_serviceLock.Wait();

	if (ServiceInfo* si = FindService(peer->_serviceId))
	{
		peer->_callback	= si->callback;
		peer->_context	= si->context;

		result = true;
	}

	_serviceLock.Release();

	return result;
}

void ArteryService::WakeUp()
{
#if defined(PLATFORM_PC)
	SetEvent(_wakeEvent);
#elif defined(PLATFORM_PS3) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS) /* [Blur2PC] BSD event transport. */
	_wakeIn.Send("x", 1);
#endif
}

#else

void ArteryService_ShutTheCompilerUp()
{
}

#endif // CFG_GOLD

