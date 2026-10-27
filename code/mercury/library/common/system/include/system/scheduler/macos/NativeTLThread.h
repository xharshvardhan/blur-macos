// [Blur2PC][macOS] Existing TL threading API over native MT worker lifetimes.
#pragma once
#include <core/Core.h>
#include <core/threads/Misc.h>

using tlThreadHandle = U64;
using tlThreadId = U64;
using tlThreadFunction = U32 (*)(void*);
#define TL_THREAD_DEFAULT_STACK_SIZE (32 * 1024)
#define TL_THREAD_FUNCTION(name) U32 name(void* ThreadParam)
#define TL_THREAD_RETURN(code) return code
#define tl_threadlocal thread_local

// Numeric values preserve the ordering and normal origin of the desktop API.
// Native MT maps priority classes to Darwin QoS, not Windows scheduler levels.
enum tlThreadPriority {
    TL_THREAD_PRIORITY_IDLE = -15,
    TL_THREAD_PRIORITY_LOWEST = -2,
    TL_THREAD_PRIORITY_BELOW_NORMAL = -1,
    TL_THREAD_PRIORITY_NORMAL = 0,
    TL_THREAD_PRIORITY_ABOVE_NORMAL = 1,
    TL_THREAD_PRIORITY_HIGHEST = 2,
    TL_THREAD_PRIORITY_TIME_CRITICAL = 15
};
enum tlThreadFlags {
    TL_THREAD_JOINABLE = 0,
    TL_THREAD_CREATE_SUSPENDED = 4,
    TL_THREAD_DEFAULT_FLAGS = TL_THREAD_JOINABLE
};
tlThreadHandle tlCreateThread(tlThreadFunction entry, INTPTR param, const char* name,
    int priority = TL_THREAD_PRIORITY_NORMAL, U32 stacksize = TL_THREAD_DEFAULT_STACK_SIZE,
    U32 flags = TL_THREAD_DEFAULT_FLAGS);
inline tlThreadHandle tlCreateThread(tlThreadFunction entry, INTPTR param, int priority,
        U32 stacksize, U32 flags, const char* name) {
    return tlCreateThread(entry, param, name, priority, stacksize, flags);
}
tlThreadHandle tlGetCurrentThread();
inline tlThreadId tlGetCurrentThreadId() { return MT::GetThreadId(); }
void tlSleep(U32 milliseconds);
[[noreturn]] void tlExitThread(U32 exitcode);
U32 tlWaitForThread(tlThreadHandle handle);
void tlSetCurrentThreadName(const char* name);
// Native counterparts for the scheduler's former ResumeThread/wait calls.
void tlResumeThread(tlThreadHandle handle);
bool tlIsThreadRunning(tlThreadHandle handle);
