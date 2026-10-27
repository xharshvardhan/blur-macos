#ifndef STDAFX_H
#define STDAFX_H

#ifdef _MSC_VER
#pragma once
#endif

#if defined(PLATFORM_MACOS) // [Blur2PC] Native headers establish types before the original module dependencies.
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <ctime>
#include <cwchar>
#include <chrono>
#include <strings.h>
#elif defined(PLATFORM_PC)

#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <windows.h>
#include <time.h>

#elif defined(PLATFORM_PS3)

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/timer.h>
#include <sysutil/sysutil_sysparam.h>
#include <string.h>
#include <wchar.h>
#include <cell/pad.h>

#endif

#include <Display/Core/Includes.h>

#include <Core/Core.h>
#include <Core/Maths.h>
#include <Core/Utility/Utility.h>
#include <Core/Containers.h>
#include <Core/Threads.h> // [Blur2PC] Original job and single-player atomics.
#include <Core/Threads/Atomic.h>
#include <AmaxText.h> // [Blur2PC] Keep PC and native text in UTF-16.
#if defined(PLATFORM_MACOS)
#include <core/utility/macos/Utf16.h>
#endif
// [Blur2PC] umbrellas the game code assumes are visible (the engine UI PCH includes them too)
#include <btl/Containers/containers.h>
#include <btl/utility/string.h>
#include <misc_shared/Messaging/GameMessaging.h>

#include <System/DebugLog.h>
#include <System/Debug/Debug.h>
#include <System/Serialise.h>

#include <Display.h>

#endif // STDAFX_H
