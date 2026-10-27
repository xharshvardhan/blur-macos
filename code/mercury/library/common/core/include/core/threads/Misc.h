#ifndef __CORE_THREAD_MISC_H__
#define __CORE_THREAD_MISC_H__

#if defined(PLATFORM_MACOS)
// [Blur2PC][macOS] Native IDs are 64-bit; yielding must actually yield.
#include <pthread.h>
#include <sched.h>
#include <system_error>
#endif

#if PLATFORM_PC
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#if PLATFORM_PS3
#include <sys/ppu_thread.h>
#endif
#if PLATFORM_SPU
#include <cell/spurs/common.h>
#endif

namespace MT
{

int GetProcessID();
typedef U64 ThreadId;

/// Returns the ID of the currently active thread.
inline ThreadId GetThreadId()
{
#if defined(PLATFORM_MACOS)
    uint64_t identifier = 0;
    const int error = pthread_threadid_np(nullptr, &identifier);
    if (error) throw std::system_error(error, std::generic_category(), "pthread_threadid_np");
    return static_cast<ThreadId>(identifier);
#endif
#if PLATFORM_PC
	return (ThreadId)GetCurrentThreadId();
#endif
#if PLATFORM_PS3
	ThreadId id;
	sys_ppu_thread_get_id(&id);
	return id;
#endif
#if PLATFORM_SPU
	return (ThreadId)cellSpursGetCurrentSpuId()|0x80000000;
#endif
}

/// Causes the current thread to yield to the next scheduled thread. If no other thread is scheduled, it does nothing.
inline void YieldThread()
{
#if defined(PLATFORM_MACOS)
    sched_yield(); // [Blur2PC] No empty platform fallback.
#endif
#if PLATFORM_PC
//	SwitchToThread();
#endif
#if PLATFORM_PS3
	sys_ppu_thread_yield();
#endif
}

} // namespace MT

#endif

