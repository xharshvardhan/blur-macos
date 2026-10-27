#ifndef __FX_MANAGER_H__
#define __FX_MANAGER_H__

#include <Btl\Containers\Vector.h>

namespace Sys
{
	class I_FileSystem;
}

namespace FX
{

class ReloadableEffect;
class Effect;
class I_Effect; // [Blur2PC] The public preload signature must be independently includable.

/// keeps track of all of the effects that have been loaded, can reload effects
class Manager
{
	friend class Effect;
class I_Effect; // [Blur2PC] The public preload signature must be independently includable.
public:
	/// constructor
	Manager();

	/// destructor
	~Manager();

	void SetFileSystem(Sys::I_FileSystem* pFs);

	Sys::I_FileSystem* GetFileSystem();

	/// reload any updated effects
	void ReloadEffects(bool bForce);

	/// current reload count. anytime a shader gets reloaded this number is incremented
	U32 GetReloadCount() { return m_iReloadCount; }

	/// preload a list of effects
	void LoadEffects(const Btl::Vector<const char *> &list, Btl::Vector<FX::I_Effect*> &pointers);

	/// load an effect
	Effect *LoadEffect(const char *pszFilename);

	/// release an effect
	void ReleaseEffect(Effect *);
#if defined(PLATFORM_MACOS)
	// [Blur2PC] Release only manager-owned preload pins; report surviving clients.
	// Call after material/UI clients retire, while the display device is alive.
	bool NativeShutdownCache();
#endif

	/// show all conflicting bindings for the given parameter
	void DumpConflictingBindings( Debug::Logger &log, const char *pszName, int nConflicts);

	/// returns the root node of the reloadable effect linked list
	ReloadableEffect *GetRootEffect();

	/// sets pre-loaded flag
	void SetPreLoaded(bool preloaded) { m_bPreLoaded = preloaded; }

private:
	/// interal function to load an effect from disk
	ReloadableEffect *LoadEffectInternal(const char *pszFilename);

	/// find the given effect in the list of loaded effects
	ReloadableEffect *GetEffect(const char *pszFilename);

	Sys::I_FileSystem* m_pFileSystem;

	ReloadableEffect *m_pRoot;
	ReloadableEffect *m_pReloadCheck;

	int m_iReloadCount;
	bool m_bPreLoaded;
};

extern Manager g_Manager;


}

#endif
