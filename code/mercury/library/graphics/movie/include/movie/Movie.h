#ifndef MOVIE_MOVIE_H
#define MOVIE_MOVIE_H

#ifdef _MSC_VER
#pragma once
#endif

#if defined(PLATFORM_MACOS) // [Blur2PC] Source-native decoder; no Windows/Bink SDK dependency.
#include "Movie/Mac/MacMovie.h"
#elif defined(PLATFORM_PS3)
#include "Movie/PS3/PS3Movie.h"
#elif defined(PLATFORM_PC)
#include "Movie/W32/W32Movie.h"
#else
#pragma message("Unsupported platform define when compiling Movie.h")
#endif

namespace Movie
{

//! C_Movie

#if defined(PLATFORM_MACOS) // [Blur2PC] Keep the original public renderer facade.
class C_Movie : public Display::I_Renderer, public C_MacMovie
#elif defined(PLATFORM_PS3)
class C_Movie : public Display::I_Renderer, public C_PS3Movie
#elif defined(PLATFORM_PC)
class C_Movie : public Display::I_Renderer, public C_W32Movie
#else
class C_Movie : public Display::I_Renderer
#endif
{
public:

	C_Movie();
	~C_Movie();

	bool OnCreate();
	bool OnCreateVolatile();
	void OnDestroyVolatile();
	void OnDestroy();

	bool Open(const Util::C_String& fileName, const C_MovieParams& params);
	void Close();
#if defined(PLATFORM_MACOS) // [Blur2PC] Registered idle renderers have no movie resources to restore.
private:
	bool m_PlaybackRequested = false;
#endif
};

}; // namespace Movie

#endif // MOVIE_MOVIE_H
