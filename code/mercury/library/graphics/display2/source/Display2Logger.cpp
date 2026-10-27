#if !defined(__APPLE__) // [Blur2PC][macOS] Logger header includes its complete native dependencies.
#include "StdAfx.h"
#endif
#include "Display2/Display2Logger.h"

namespace Display2
{

Debug::Logger& g_Log = Debug::GetLogger("mercury.display2");

}; // namespace Display2
