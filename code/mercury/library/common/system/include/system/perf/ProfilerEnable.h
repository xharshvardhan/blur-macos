#ifndef __PROFILER_ENABLE_H__
#define __PROFILER_ENABLE_H__

#if (CFG_DEBUG || CFG_RELEASE || CFG_PROFILE) && !defined(BLUR_MAC_DISABLE_PROFILER) // [Blur2PC] Explicit native bring-up build setting.
#define ENABLE_MERCURY_PROFILER
#endif

#endif
