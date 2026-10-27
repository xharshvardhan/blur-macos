// [Blur2PC][macOS] Real registered loggers for the wider engine/debug library.
// Separate from the small native runtime so inspection tools can operate before
// the engine's Debug/MemoryTracker global registration is integrated.
#include <core/Core.h>
#include <system/debug/log/DebugLog.h>
namespace Sys {
Debug::Logger& g_FileSystemGlobalsLog = Debug::GetLogger("Mercury.FileSystemGlobals");
}
Debug::Logger& g_SystemIoLog = Debug::GetLogger("mercury.system.io");
