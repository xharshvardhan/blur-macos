#include "stdafx.h"
#include <Core/Core.h>
#include "../Misc/NetVersion.h"
#include "../Misc/NetDebug.h"
#include "../Misc/NetDefines.h"
#include <Network/Network.h>
#include "NetLanDiscovery.h"
#if defined(PLATFORM_MACOS)
#include <arpa/inet.h>
#include <charconv>
#include <cstdlib>
#include <cstring>
namespace {
// [Blur2PC] Strict native instance configuration; no wire fields or identities change.
bool NativeDiscoveryPort(const char* setting, U16 fallback, bool allowZero, U16& port)
{
    port = fallback;
    const char* value = std::getenv(setting);
    if (!value) return true;
    const char* end = value + std::strlen(value);
    unsigned parsed = 0;
    const auto result = std::from_chars(value, end, parsed);
    if (result.ec != std::errc() || result.ptr != end || parsed > 65535 || (!allowZero && parsed == 0))
    {
        LOGERROR(RacerNet::g_Log, "Invalid %s: expected a decimal port in %s", setting, allowZero ? "0..65535" : "1..65535");
        return false;
    }
    port = static_cast<U16>(parsed);
    return true;
}
}
#endif

#if defined ( RACER_NET_OPTIMIZE_OFF )
	#pragma optimize( "", off )
#endif

#ifndef CFG_GOLD
	DEBUG_ENUM(eNetBuildVersion)
	DE_OPTION(eNetBuildVersion_Master,			"Master")
	DE_OPTION(eNetBuildVersion_BizarreQA,		"BizarreQA")
	DE_OPTION(eNetBuildVersion_BizarreMpBalancer,"BizarreMpBalancer")
	DE_OPTION(eNetBuildVersion_ActivisionQA,	"ActivisionQA")
	DE_OPTION(eNetBuildVersion_2010Testing,		"2010MP")
	DE_OPTION(eNetBuildVersion_BCMpSession,		"BCMpSession")
	DE_OPTION(eNetBuildVersion_GamesComSysLink,	"GamesComSysLink")
	DE_OPTION(eNetBuildVersion_Misc1,			"Misc1")
	DE_OPTION(eNetBuildVersion_Misc2,			"Misc2")
	END_DEBUG_ENUM
#endif

namespace RacerNet
{

#ifndef CFG_GOLD
	Debug::Enum<eNetBuildVersion> NET_PACKET_VERSION("Network.BuildVersionToJoin", eNetBuildVersion_Master, Debug::Persist());
#endif

	C_LanDiscoveryManager::C_LanDiscoveryManager() :	m_Initialised(false), m_Advertising(false), m_Enumerating(false), m_TitleId(0), 
														m_AdvertisedNumSlotsOpen(0), m_AdvertisedMaxSlots(0), m_AdvertisedConsoleId(0),
														m_pEnumeratedSessions(NULL)
	{
		for(U32 i=0; i<BD_COMMON_ADDR_SERIALIZED_SIZE; i++) 
			m_AdvertisedCommonAddress[i] = 0;
	}

	C_LanDiscoveryManager::~C_LanDiscoveryManager()
	{
		Shutdown();
	}

	bool C_LanDiscoveryManager::Initialise(const U32 titleId)
	{
		if (m_Initialised)
			return true;

#if defined(PLATFORM_MACOS)
        U16 nativeBindPort, nativeDestinationPort;
        if (!NativeDiscoveryPort("BLUR_MAC_LAN_DISCOVERY_BIND_PORT", AMAX_LAN_DISCOVERY_PORT, true, nativeBindPort) ||
            !NativeDiscoveryPort("BLUR_MAC_LAN_DISCOVERY_TARGET_PORT", AMAX_LAN_DISCOVERY_PORT, false, nativeDestinationPort))
            return false;
        const char* nativeDestinationIp = std::getenv("BLUR_MAC_LAN_DISCOVERY_TARGET_IP");
        if (!nativeDestinationIp) nativeDestinationIp = "255.255.255.255";
        in_addr parsedAddress{};
        if (inet_pton(AF_INET, nativeDestinationIp, &parsedAddress) != 1 || parsedAddress.s_addr == INADDR_ANY)
        {
            LOGERROR(g_Log, "Invalid BLUR_MAC_LAN_DISCOVERY_TARGET_IP: expected a nonzero numeric IPv4 address");
            return false;
        }
        const bdAddr nativeDestination(bdInetAddr(nativeDestinationIp), nativeDestinationPort);
#endif

		// create a broadcast socket.
		if (m_Socket.create(false, true))
		{
#if defined(PLATFORM_MACOS)
            const bdAddr bindAddr(bdInetAddr::Any(), nativeBindPort); // [Blur2PC] Zero requests a real ephemeral client source.
#else
			const bdAddr bindAddr(bdInetAddr::Any(), AMAX_LAN_DISCOVERY_PORT);
#endif
			if (m_Socket.bind(bindAddr) == BD_NET_SUCCESS) // [Blur2PC] Negative socket status is a real bind failure.
			{
#if defined(PLATFORM_MACOS)
                m_NativeDiscoveryDestination = nativeDestination; // [Blur2PC] Freeze settings for this socket lifetime.
#endif
				m_TitleId = htonl(titleId);
				m_Initialised = true;
			}
			else
			{
				LOGWARN(g_Log, "C_LanDiscoveryManager::Initialise() - unable to bind lan discovery socket.");
				Shutdown();
			}
		}

		return m_Initialised;
	}

	void C_LanDiscoveryManager::Shutdown()
	{
		if (m_pEnumeratedSessions)
			delete m_pEnumeratedSessions;
		m_pEnumeratedSessions = NULL;

		m_Initialised = false;
		m_Enumerating = false;
		m_Advertising = false;
		m_TitleId = 0;
		m_EnumerateTimer.Stop();
		if (m_Socket.getHandle() != BD_INVALID_SOCKET_HANDLE)
			m_Socket.close();
	}

	bool C_LanDiscoveryManager::Advertise(const SysProfile::BaseProfile& host, const NetPlay::SessionSecurity& security, const U8* pCommonAddress, const U32 maxSlots, const U32 numSlotsOpen, const U64 consoleId, const NetRaceSettingsQos& raceQos)
	{
		if (!m_Initialised || NULL == pCommonAddress)
			return false;

		m_AdvertisedNumSlotsOpen = numSlotsOpen;
		m_AdvertisedMaxSlots = maxSlots;
		m_AdvertisedConsoleId = consoleId;
		m_AdvertisedHost = host;
		m_AdvertisedSecurity = security;
		m_RaceSettingsQos = raceQos;
		m_Advertising = true;

		for(U32 i=0; i<BD_COMMON_ADDR_SERIALIZED_SIZE; i++) 
			m_AdvertisedCommonAddress[i] = pCommonAddress[i];

		return true;
	}

	void C_LanDiscoveryManager::UnAdvertise(void)
	{
		if (!m_Initialised)
			return;

		m_Advertising = false;
		m_AdvertisedNumSlotsOpen = 0;
		m_AdvertisedMaxSlots = 0;
		m_AdvertisedConsoleId = 0;

		for(U32 i=0; i<BD_COMMON_ADDR_SERIALIZED_SIZE; i++) 
			m_AdvertisedCommonAddress[i] = 0;
	}

	bool C_LanDiscoveryManager::Enumerate( const U32 numToJoin )
	{
		if (!m_Initialised || 0 == numToJoin)
			return false;

		m_Enumerating = false;
		if (m_pEnumeratedSessions)
			m_pEnumeratedSessions->Clear();
		else
		{
			m_pEnumeratedSessions = new LanDiscoveryReplies;
			if (NULL == m_pEnumeratedSessions)
				return false;
		}

		// generate a nonce..
		m_EnumerateNonce.Create();

		// send request broadcast.
		LanDiscoveryRequest req{}; // [Blur2PC] Initialize the transmitted PC object representation.
		req.m_PacketVersion		= htonl(NET_PACKET_VERSION);
		req.m_TitleId			= m_TitleId;
		req.m_PacketType		= eLanDiscoveryPacketType_Request;	
		req.m_Nonce				= m_EnumerateNonce;
		req.m_NumPlayersToJoin	= numToJoin;

		// broadcast this packet..
#if defined(PLATFORM_MACOS)
        const bdAddr& bcAddr = m_NativeDiscoveryDestination; // [Blur2PC] Same original request/reply, independent source/destination ports.
#else
		const bdAddr bcAddr(bdInetAddr::Broadcast(), AMAX_LAN_DISCOVERY_PORT);
#endif
		const S32 sendResult = m_Socket.sendTo(bcAddr , &req, sizeof(req));
#if defined(PLATFORM_MACOS)
        if (sendResult != sizeof(req))
        {
            LOGERROR(g_Log, "C_LanDiscoveryManager::Enumerate() - unable to send the native discovery request");
            return false; // [Blur2PC] A real transport failure must not report an active search.
        }
#endif

		m_Enumerating = true;
		m_EnumerateTimer.Start();

		return true;
	}

	bool C_LanDiscoveryManager::IsEnumerating(void)
	{
		if (!m_Initialised)
			return false;
		return m_Enumerating;
	}

	U32 C_LanDiscoveryManager::NumEnumerated(void) const
	{
		if (!m_Initialised || m_Enumerating || NULL == m_pEnumeratedSessions)
			return 0;
		return static_cast <U32> (m_pEnumeratedSessions->Size());
	}

	bool C_LanDiscoveryManager::GetEnumeratedSesionData(const U32 sessionIndex, LanDiscoveryReply& sessionData)
	{
		if (!m_Initialised || m_Enumerating || NULL == m_pEnumeratedSessions)
			return false; // no sesions to return at this time.

		if (sessionIndex >= (U32)m_pEnumeratedSessions->Size())
		{
			LOGERROR(NetPlay::g_Log, "C_LanDiscoveryManager::GetEnumeratedSesionData() - trying to access enumerated lan session that doesn't exist!");
			return false;
		}

		sessionData = (*m_pEnumeratedSessions)[sessionIndex];
		return true;
	}

	void C_LanDiscoveryManager::DestroyEnumerated(void)
	{
		if (!m_Initialised)
			return;
		m_Enumerating = false;
		m_EnumerateTimer.Stop();
		if (m_pEnumeratedSessions)
			delete m_pEnumeratedSessions;
		m_pEnumeratedSessions = NULL;
	}

	void C_LanDiscoveryManager::Poll(void)
	{
		if (!m_Initialised)
			return;

		if (m_Socket.getHandle() == BD_INVALID_SOCKET_HANDLE)
			return;

		bdAddr from;
		alignas(LanDiscoveryReply) bdUByte8 data[BD_MAX_DATAGRAM_SIZE]; // [Blur2PC] Aligned storage for the original wire structures.
		S32 received =  -1;
		do
		{
			received = m_Socket.receiveFrom(from, data, BD_MAX_DATAGRAM_SIZE);
			const S32 minPackstSize = sizeof(LanDiscoveryHeader); 
			if (received > minPackstSize)
			{
				LanDiscoveryHeader* pHdr = reinterpret_cast<LanDiscoveryHeader*> (data);
			
				// m_TitleId if is already stored in network byte order.	
				if (pHdr->m_TitleId == m_TitleId) 
				{
					// overwrite to put the header variables back into the correct format for this platform
					// game logic will need to reference the packet version before attemptting to join.
					pHdr->m_PacketVersion = ntohl(pHdr->m_PacketVersion);
					pHdr->m_TitleId = ntohl(pHdr->m_TitleId);

						// if we get here then this packet is for our game and platform.
					if (pHdr->m_PacketType == eLanDiscoveryPacketType_Request) 
					{
						if (m_Advertising)
						{
							if (received == sizeof(LanDiscoveryRequest))
							{
								const LanDiscoveryRequest* pReq = reinterpret_cast<LanDiscoveryRequest*> (data);
								if ( m_AdvertisedNumSlotsOpen >= pReq->m_NumPlayersToJoin ) // TODO_MP_SYSLINK
								{
									LanDiscoveryReply reply{}; // [Blur2PC] Initialize all transmitted fields before copying host data.
									reply.m_PacketVersion	= htonl(NET_PACKET_VERSION);
									reply.m_TitleId			= m_TitleId;
									reply.m_PacketType		= eLanDiscoveryPacketType_Reply;	
									reply.m_Nonce			= pReq->m_Nonce;

									reply.m_ConsoleId		= m_AdvertisedConsoleId;
									reply.m_HostProfile		= m_AdvertisedHost;
									reply.m_SessionSecurity	= m_AdvertisedSecurity;
									reply.m_RaceQos			= m_RaceSettingsQos;
									reply.m_MaxSlots		= m_AdvertisedMaxSlots;
									reply.m_SlotsTaken		= m_AdvertisedMaxSlots - m_AdvertisedNumSlotsOpen;

									for(U32 i=0; i<BD_COMMON_ADDR_SERIALIZED_SIZE; i++) 
										reply.m_HostCommonAddress[i] = m_AdvertisedCommonAddress[i];

									// send packet back..
									const S32 sendResult = m_Socket.sendTo(from , &reply, sizeof(reply));
								}
							}
						}
					}
					else if (pHdr->m_PacketType == eLanDiscoveryPacketType_Reply)
					{
						if (m_Enumerating)
						{
							if (received == sizeof(LanDiscoveryReply))
							{
								const LanDiscoveryReply* pReply = reinterpret_cast<LanDiscoveryReply*> (data);
								if (pReply->m_Nonce == m_EnumerateNonce)
								{
									// woot a reply to our original enumeration request.
									if (m_pEnumeratedSessions && m_pEnumeratedSessions->Size() != m_pEnumeratedSessions->Capacity())
										m_pEnumeratedSessions->PushBack(*pReply);
								}
							}
						}
					}
				}
			}
		}while (received > 0);

		// stop enumerating after a set time period.
		if (m_Enumerating)
		{
			if (m_EnumerateTimer.ElapsedTimeInSeconds() >= 2.0f)
			{
				m_EnumerateTimer.Stop();
				m_Enumerating = false;
			}
		}
	}

}

#if defined ( RACER_NET_OPTIMIZE_OFF )
	#pragma optimize( "", on )
#endif
