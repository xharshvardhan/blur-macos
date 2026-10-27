#include "stdafx.h"

#include <ui/UiInterface.h>
#include "ui/Graphics/movie.h"
#include "ui/Game/UIScreenControl.h"
#include "ui/Game/Gadgets/ButtonController.h"
#include "ui/Game/Gadgets/ButtonMessages.h"
#include <system\misc\StringUtil.h>
#if defined(PLATFORM_MACOS) // [Blur2PC] Failed playback owns and retires its staged decoder/GPU resources.
#include <memory>
#include <exception>
#include <stdexcept>
#endif

#ifdef PLATFORM_PS3
#include <cell/cell_fs.h>
#include <sysutil/sysutil_bgmplayback.h>
#endif

#ifdef CFG_ALLOW_OPTIMIZE_OFF_RENDER_MOVIE
#pragma optimize( "", off ) // CFG_ALLOW_OPTIMIZE_OFF_RENDER_MOVIE
#endif

Debug::Bool g_debugMovieEnableSound("Graphics.Render.Movie.EnableSound", true);

#ifdef PLATFORM_PS3
extern void C_GameFileSystem_UpdateSystemMessages();
#endif

namespace UI
{

char *gHddDirectorySearchPath = NULL;

Movie gMovie[MAX_MOVIE_SLOTS];

Movie::Movie() :
#if defined(PLATFORM_MACOS) // [Blur2PC] Local/partial owners must not depend on static zero-fill.
        m_Active(false),m_bMemoryResident(false),m_bUseSound(true),m_Index(-1),
        m_pMovie(NULL),m_bStillRenderTheWorld(false),m_bIsBackgroundMovie(false),
        m_bNeedToRestoreBackgroundMusic(false),m_Volume(32768)
#else
		m_Active(false),
		m_bUseSound(true),
		m_bIsBackgroundMovie(false),
		m_bNeedToRestoreBackgroundMusic(false),
		m_pMovie(NULL)
#endif
{
}

Movie::~Movie()
{
	assert(!m_pMovie && "The movie has not been destroyed");
}

void Movie::Create
(
	const S32	index, 
	const char*	filename, 
	const bool	in_memory, 
	const U32	io_memory_size, 
	const bool	bStillRenderTheWorld, 
	const bool	bBackgroundMovie, 
	const S32	volume
)
{
	m_Index = index;
	m_Volume = volume;

	RESOURCE_MEMORY_ID("Movie");

	if(m_pMovie)
	{
		LOGWARN(g_Log, "Trying to create a movie %s in slot %i where movie already exists\n", filename, index);
		return;
	}

#if defined(PLATFORM_MACOS)
    // [Blur2PC] Publish playback only after a real open, decoded first frame and
    // output texture succeed. The existing PC failed-open completion is retained.
    m_Active = false;
    std::unique_ptr<::Movie::C_Movie> nativeOwner;
    try {
        nativeOwner.reset(new ::Movie::C_Movie());
        m_pMovie = nativeOwner.get();
#else
	m_pMovie = new ::Movie::C_Movie();
#endif

	m_MovieParams.Reset(); // Make sure movie params are reset to defaults

	m_MovieParams.m_MemoryFlags = ::Movie::E_MovieMemoryFlags_System; // Always allocate out of system memory

	if(io_memory_size)
	{
		m_MovieParams.m_CreateFlags |= ::Movie::E_MovieCreateFlags_SetBufferIOSize;
		m_MovieParams.m_BinkIOSize = io_memory_size;
	}
	else
	if(in_memory)
	{
		m_MovieParams.m_CreateFlags = ::Movie::E_MovieCreateFlags_LoadIntoMemory;
	}

	// PR - #23103 - different versions of the movie for 50 and 60Hz
	S32 sBufferSize = 512;

#if defined(PLATFORM_MACOS)
    std::unique_ptr<char[]> nativeFileName(new char[sBufferSize]);
    char* pFileName = nativeFileName.get();
#else
	char* pFileName = new char[ sBufferSize ];
#endif
	assert( pFileName );

	sprintf_s( pFileName, sBufferSize, "%s", filename );

	// rename for 50Hz versions?
	if ( Display::Device()->GetRefreshRate() == Display::E_RefreshRate_50HZ )
		UiInterface()->RenameMovieFor50Hz( filename, pFileName, sBufferSize );
	else
	if ( Display::C_Device::GetCpuSpeed() == Display::E_CpuSpeed_Low )
		UiInterface()->RenameMovieForSmallNTSC( filename, pFileName, sBufferSize );

#ifdef PLATFORM_PS3
	char hddFileName[256];
	if ( gHddDirectorySearchPath )
	{
		strcpy( hddFileName, gHddDirectorySearchPath );
		strcat( hddFileName, "/image/" );
		strcat( hddFileName, pFileName );
		char searchStr[] = "/USRDIR/";
		U32 lower = (U32)((char *)strstr(hddFileName, searchStr) - (char *)hddFileName) + strlen(searchStr);
//		printf("lower %d\n",lower);
		for (unsigned int w = 0 ; w < strlen(hddFileName) ; w++)
		{
			if (hddFileName[w] == '\\') hddFileName[w] = '/';
			if (w > lower) hddFileName[w] = tolower( hddFileName[w] );
		}
		
//		printf("%s\n",hddFileName);
		bool succeeded = false;
		do 
		{
			int fd = 0;
			int ret = cellFsOpen( hddFileName, CELL_FS_O_RDONLY, &fd, NULL, 0 );
			if (ret == CELL_FS_SUCCEEDED)
			{
				cellFsClose( fd );
				if(!m_pMovie->Open(hddFileName, m_MovieParams))
				{
					LOGWARN(g_Log, "Movie player failed to open %s\n", hddFileName);
					sys_timer_usleep( 30000 );

				}
				else
				{
					succeeded = true;
				}
			}
			else
			{
				Sys::C_FilePath discName = Sys::C_FileSystem::Instance().GetWorkingDirectory() + "\\" + pFileName;
				if(!m_pMovie->Open(discName.Chars(), m_MovieParams))
				{
					LOGWARN(g_Log, "Movie player failed to open %s\n", discName.Chars());
					sys_timer_usleep( 30000 );
				}
				else
				{
					succeeded = true;
				}
				
			}

			C_GameFileSystem_UpdateSystemMessages();

			if ( Display::Device()->HaveThreadOwnership() )
			{
				bool inScene = Display::Device()->InScene();
				if (inScene) Display::Device()->EndScene();
				{
					Display::Device()->BeginScene();
					UiInterface()->PollPlatformCallbacks();
					sys_timer_usleep( 10000 );
					Display::Device()->EndScene();
					Display::Device()->Present();
				}		

				if (inScene) Display::Device()->BeginScene();
			}

#if 0
			cellGcmResetFlipStatus();
			cellGcmSetFlip(0);
			cellGcmFlush();
			cellGcmSetWaitFlip();
#endif
		//	
		
		} while (!succeeded);
	}
	else
	{
		Sys::C_FilePath discName = Sys::GetDefaultFileSystem()->GetSystemFilePath( pFileName );
		if(!m_pMovie->Open(discName.Chars(), m_MovieParams))
		{
			LOGWARN(g_Log, "Movie player failed to open %s\n", discName.Chars());
		}
	}
#elif defined (PLATFORM_PC) || defined(PLATFORM_MACOS) // [Blur2PC] Native uses the original PC engine-file open route.

	m_bIsBackgroundMovie = bBackgroundMovie;

	if(!m_pMovie->Open(filename, m_MovieParams))
	{
		LOGWARN(g_Log, "Movie player failed to open %s\n", filename);
#if defined(PLATFORM_MACOS)
        m_pMovie = NULL;
        _UI_MESSAGE1(E_UiMsg_MovieFinished, m_Index);
        return; // Staged player destroys any partial decoder/texture acquisition.
#endif
	}

#else


	Sys::C_FilePath discName = Sys::C_FileSystem::Instance().GetWorkingDirectory() + "\\" + pFileName;
	if(!m_pMovie->Open(discName.Chars(), m_MovieParams))
	{
		LOGWARN(g_Log, "Movie player failed to open %s\n", pFileName);
	}
	

#endif

#if !defined(PLATFORM_MACOS)
	SAFE_DELETE_PTR( pFileName );
#endif

#ifndef PLATFORM_PS3
	Util::C_String myString;
	myString.Printf("MovieTarget%02d",index);
#endif // PLATFORM_PS3

	m_pMovie->SetLooping(true);

	m_Active = true;

	m_bStillRenderTheWorld = bStillRenderTheWorld;
	m_bIsBackgroundMovie = bBackgroundMovie;

	if (!m_bIsBackgroundMovie)
	{
#ifdef PLATFORM_PS3
		cellSysutilDisableBgmPlayback();
#endif
		m_bNeedToRestoreBackgroundMusic = true;
	}

	// Set volume.
	m_pMovie->SetVolume( m_Volume );
#if defined(PLATFORM_MACOS)
        nativeOwner.release();
    } catch(const std::exception& error) {
        m_pMovie = NULL;m_Active = false;m_bNeedToRestoreBackgroundMusic = false;
        LOGWARN(g_Log,"Movie player failed to initialize %s: %s",filename,error.what());
        _UI_MESSAGE1(E_UiMsg_MovieFinished,m_Index);
    } catch(...) {
        m_pMovie = NULL;m_Active = false;m_bNeedToRestoreBackgroundMusic = false;
        LOGWARN(g_Log,"Movie player failed to initialize %s",filename);
        _UI_MESSAGE1(E_UiMsg_MovieFinished,m_Index);
    }
#endif
}

void Movie::Destroy()
{
	RESOURCE_MEMORY_ID("Movie");

	if(m_pMovie)
	{
		m_pMovie->Close();
		delete m_pMovie;
		m_pMovie = NULL;
	}

	m_Active = false;

	if (m_bNeedToRestoreBackgroundMusic)
	{
#ifdef PLATFORM_PS3
		cellSysutilEnableBgmPlayback();
#endif
		m_bNeedToRestoreBackgroundMusic = false;
	}
 }

void Movie::SetTrackIDsToPlay(const U32 track0, const U32 track1, const U32 track2, const U32 track3, const U32 track4, const U32 track5)
{
	m_MovieParams.m_TrackIDsToPlay[0] = track0;
	m_MovieParams.m_TrackIDsToPlay[1] = track1;
	m_MovieParams.m_TrackIDsToPlay[2] = track2;
	m_MovieParams.m_TrackIDsToPlay[3] = track3;
	m_MovieParams.m_TrackIDsToPlay[4] = track4;
	m_MovieParams.m_TrackIDsToPlay[5] = track5;
}

void Movie::Pause(const bool pause)
{
#if defined(PLATFORM_MACOS) // [Blur2PC] A failed or destroyed player cannot be made active by a later Lua control.
    if(!m_pMovie || !m_pMovie->IsCreated()){m_Active=false;return;}
    m_pMovie->SetPaused(pause); // The native elapsed-media clock must exclude a UI pause.
#endif
	m_Active = !pause;
}

void Movie::Looping(const bool loop)
{
	if(m_pMovie)
	{
		m_pMovie->SetLooping(loop);
	}
}

bool Movie::Update()
{
	if (!m_Active)
		return true;

	if(!m_pMovie || !m_pMovie->IsCreated())
	{
		// [Blur2PC] no Bink on PC: a movie that failed to open counts as finished, otherwise
		// screens that wait for E_UiMsg_MovieFinished (SpMovieScreen, attract) never advance.
		_UI_MESSAGE1( E_UiMsg_MovieFinished, m_Index );
		m_Active = false;
		return false;
	}

	#ifdef PLATFORM_PC
	if(!Display::Device()->GetValid())
		return true;
	#endif

#if defined(PLATFORM_MACOS)
    // [Blur2PC] Keep the acquired device owner balanced if native scene/render operations throw.
    struct NativeMovieOwnership {
        Display::C_Device* device;
        NativeMovieOwnership():device(Display::Device()) {
            if(!device->AcquireThreadOwnership())throw std::runtime_error("Movie could not acquire native device ownership");
        }
        ~NativeMovieOwnership(){device->ReleaseThreadOwnership();}
    } nativeOwnership;
#else
	Display::Device()->AcquireThreadOwnership();
#endif

	bool wasInScene = Display::Device()->InScene();

	if(!wasInScene)
	{
		if(!Display::Device()->BeginScene())
		{
			LOGERROR(g_Log, "Failed to begin scene when rendering the movie\n");
#if !defined(PLATFORM_MACOS)
			Display::Device()->ReleaseThreadOwnership();
#endif
			return false;
		}
	}

	m_pMovie->Render();

	if(!wasInScene)
	{
		Display::Device()->EndScene();
	}

#if defined(PLATFORM_MACOS)
    // [Blur2PC] Decode/upload failure finishes once even for looping playback;
    // the real last frame may remain available until the screen destroys it.
    if(!m_pMovie->IsCreated()) {
        _UI_MESSAGE1(E_UiMsg_MovieFinished,m_Index);m_Active=false;return false;
    }
#else
	Display::Device()->ReleaseThreadOwnership();
#endif

	// [LloydW] If looping the video let bink seek back to beginning rather than forcing a rewind.
	// Also it seems the the movie finished message has to be sent at the end of the video even if its looping.
	if(m_pMovie->HasReachedEnd())
	{
		LOGDEBUG( g_Log,  "***** Sending message");

		_UI_MESSAGE1( E_UiMsg_MovieFinished, m_Index );

		if ( !m_pMovie->GetLooping() )
			m_Active = false;
	}

	return true;
}

void Movie::DebugRender(F32& x, F32& y, const F32 s)
{
	#if !defined(CFG_GOLD) && defined(PLATFORM_PS3)
	if(m_pMovie)
		m_pMovie->DebugRender(x, y, s);
	#endif
}

UITextureHandle Movie::GetTexture( ) const
{
#if defined(PLATFORM_MACOS)
    // [Blur2PC] Retain a genuine last decoded frame until screen teardown, but
    // never expose an unopened player's empty texture wrapper as a GPU resource.
    const auto* texture=m_pMovie?m_pMovie->GetTexture():NULL;
    return texture&&texture->IsCreated()?texture:NULL;
#elif defined( _GENESIS_ )
	return m_pMovie ? m_pMovie->GetTexture() : NULL;
#else
	return m_pMovie ? m_pMovie->GetTexture() : NULL;
#endif // _GENESIS_
}

}; // namespace UI
