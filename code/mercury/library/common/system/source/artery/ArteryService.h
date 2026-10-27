#ifndef ARTERYSERVICE_H
#define ARTERYSERVICE_H
#if defined(PLATFORM_MACOS) // [Blur2PC]
#include <atomic>
#endif

#ifdef _MSC_VER
#pragma once
#endif

class ArteryService
{
public:
	ArteryService();
	~ArteryService();

public:
	bool Initialise(bool isServer, const char* gameName, int maxPeers);
	void Destroy();

public:
	bool Register(const char* id, ArteryCallback callback, void* context);

public:
	bool Connect(const ArterySockAddr& target, const char* id, ArteryCallback callback, void* context, ArteryConnection& con);

private:
	enum TaskType
	{
		kTtAddPeer,
		kTtRemovePeer,
		kTtShutdown,
	};

private:
	static const int kMaxServices	= 16;

private:
	struct ServiceInfo
	{
		char			id[kArteryMaxServiceIdLength];
		ArteryCallback	callback;
		void*			context;
	};

private:
	void Main();

private:
	static void ThreadProc(void* context);

private:
	void InitialiseBroadcastReply(const char* gameName);

private:
	ServiceInfo* FindService(const char* id);
	bool SetupPeerCallback(ArteryPeer* peer);

private:
	void WakeUp();

private:
	int						_references;

	int						_maxPeers;

#if defined(PLATFORM_PC)
	HANDLE					_wakeEvent;
#elif defined(PLATFORM_PS3) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS) /* [Blur2PC] BSD event transport. */
	ArterySocket			_wakeIn;
	ArterySocket			_wakeOut;
#else
#error platform not supported
#endif

	ArteryThread			_thread;

	ArterySocket			_listen;
	ArterySocket			_broadcast;

	ArteryLock				_serviceLock;
	ServiceInfo				_services[kMaxServices];

	ArteryLock				_peerLock;
	ArteryPeer*				_peers;

	ArteryBroadcastReply	_broadcastReply;

#if defined(PLATFORM_MACOS) // [Blur2PC] Worker/application publication.
    std::atomic<bool> _shutdown;
#else
	bool					_shutdown;
#endif
	bool					_isServer;

friend class ArteryPeer;
};

#endif // ARTERYSERVICE_H
