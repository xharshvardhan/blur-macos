#ifndef __FX_RELOADABLEEFFECT_H__
#define __FX_RELOADABLEEFFECT_H__

namespace FX
{

/// effect with some extra parameters to make it reloadable using the effect manager
class ReloadableEffect : public Effect
{
public:
#if defined(PLATFORM_MACOS)
	// [Blur2PC] Runtime ownership only, never part of the PC FXB format.
	ReloadableEffect() : m_pNext(NULL), m_pPrev(NULL), m_iRefCount(0), m_NativePreloadOwner(false) {}
#endif
	ReloadableEffect *m_pNext;
	ReloadableEffect *m_pPrev;
	Util::C_String	m_sFilename;
friend class Manager;
protected:
	int	m_iRefCount;
#if defined(PLATFORM_MACOS)
	bool m_NativePreloadOwner; // [Blur2PC] The new-preload ref belongs to Manager, not an external client.
#endif
	Time::C_Time	m_iTime;
};

}

#endif
