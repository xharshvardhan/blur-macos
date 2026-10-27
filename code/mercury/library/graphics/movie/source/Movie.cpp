#include "stdafx.h"
#include "Movie/Movie.h"
#include <System/Memory.h>

#ifdef CFG_ALLOW_OPTIMIZE_OFF_RENDER_MOVIE
#pragma optimize( "", off ) // CFG_ALLOW_OPTIMIZE_OFF_RENDER_MOVIE
#endif

namespace Movie
{

C_Movie::C_Movie() : I_Renderer("Movie")
{
}

C_Movie::~C_Movie()
{
	DestroyRenderer();
}

bool C_Movie::OnCreate()
{
	//LOGINFO(g_Log, "C_Movie::OnCreate(%s)\n", m_FileName.Chars());

	RESOURCE_MEMORY_ID("Movie");

	MEMORY_TRACKER_PUSH("mercury.movie");

	/*if(!CreateBink(m_Params))
	{
		MEMORY_TRACKER_POP("mercury.movie");
		return false;
	}*/

	MEMORY_TRACKER_POP("mercury.movie");

	return true;
}

bool C_Movie::OnCreateVolatile()
{
#if defined(PLATFORM_MACOS) // [Blur2PC] Boot-layer movie renderers are registered before their filenames are assigned.
	if(!m_PlaybackRequested)
		return true;
#endif
	//LOGINFO(g_Log, "C_Movie::OnCreateVolatile(%s)\n", m_FileName.Chars());

	RESOURCE_MEMORY_ID("Movie");

	MEMORY_TRACKER_PUSH("mercury.movie");

	if(!CreateBink(m_Params))
	{
		MEMORY_TRACKER_POP("mercury.movie");
		return false;
	}
	
	if(!CreateTextures())
	{
		MEMORY_TRACKER_POP("mercury.movie");
		return false;
	}

	MEMORY_TRACKER_POP("mercury.movie");

	return true;
}

void C_Movie::OnDestroyVolatile()
{
	//LOGINFO(g_Log, "C_Movie::OnDestroyVolatile(%s)\n", m_FileName.Chars());

	DestroyTextures();

	DestroyBink();
}

void C_Movie::OnDestroy()
{
	//LOGINFO(g_Log, "C_Movie::OnDestroy(%s)\n", m_FileName.Chars());

	//DestroyBink();
}

bool C_Movie::Open(const Util::C_String& fileName, const C_MovieParams& params)
{
#if defined(PLATFORM_MACOS) // [Blur2PC] A reopened renderer must actually replace the decoder and its GPU/audio owners.
	Close();
#endif
	m_FileName = fileName;
	m_Params = params;
#if defined(PLATFORM_MACOS) // [Blur2PC] An explicit Open must decode real media, including failure for an empty filename.
	m_PlaybackRequested = true;
#endif

	if(!Create())
	{
#if defined(PLATFORM_MACOS) // [Blur2PC] A failed renderer request owns no playback to restore.
		m_PlaybackRequested = false;
#endif
		return false;
	}
	if(!CreateVolatile())
	{
		// [Blur2PC] no Bink on PC: drop the involatile state so device resets skip this renderer
		Destroy();
#if defined(PLATFORM_MACOS) // [Blur2PC] Failed requests must not be retried by unrelated device resets.
		m_PlaybackRequested = false;
#endif
		return false;
	}

	return true;
}

void C_Movie::Close()
{

	DestroyVolatile();
	Destroy();
#if defined(PLATFORM_MACOS) // [Blur2PC] A closed, still-registered renderer returns to its idle state.
	m_PlaybackRequested = false;
	m_FileName.Clear();
#endif
}

}; // namespace Movie
