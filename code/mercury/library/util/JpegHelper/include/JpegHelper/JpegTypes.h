// [Blur2PC] Native libjpeg uses an int boolean; retain the bundled Windows ABI.
#pragma once
#include <core/Core.h>
namespace JpegHelper {
#if defined(PLATFORM_MACOS)
using JpegBoolean=int;
#else
using JpegBoolean=unsigned char;
#endif
}
