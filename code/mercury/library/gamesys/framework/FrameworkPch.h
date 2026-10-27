#ifndef GAMESYS_FRAMEWORKPCH_H
#define GAMESYS_FRAMEWORKPCH_H

#ifdef _MSC_VER
#pragma once
#endif

// [Blur2PC][macOS] Establish the native platform before selecting OS headers.
#include <Core/Core.h>

#if defined(PLATFORM_PS3)

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <sys/timer.h>
#include <sysutil/sysutil_sysparam.h>

#elif !defined(PLATFORM_MACOS) // [Blur2PC] Darwin uses the standard headers below.

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0501
#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#endif

#if !defined(PLATFORM_MACOS) // [Blur2PC] Native non-rendering framework code has no Direct3D dependency.
#include <Display/Core/Includes.h>
#endif

#include <Core/Core.h>
#include <Core/Maths.h>
#include <Core/Utility/Utility.h>
#include <Core/Containers.h>
#if defined(PLATFORM_MACOS) // [Blur2PC] Explicit native framework thread and timer dependencies.
#include <Core/Threads.h>
#include <System/Time/Time.h>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#endif

#include <System/DebugLog.h>
#include <System/Debug/Debug.h>
#include <System/Serialise.h>

#if !defined(PLATFORM_MACOS) // [Blur2PC] Rendering callers must include their display dependencies explicitly.
#include <Display.h>
#endif

#include "Framework/Framework.h"

#endif // GAMESYS_FRAMEWORKPCH_H
