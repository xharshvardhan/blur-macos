#include "SystemPch.h"
#include <System/Serialise.h>
#if !defined(PLATFORM_MACOS)
#include <system/debug/Debug.h>
#endif // [Blur2PC] Native diagnostics do not require debug variable registration.

namespace Sys
{

#if !defined(PLATFORM_MACOS)
Debug::Logger &g_FileSystemGlobalsLog = Debug::GetLogger("Mercury.FileSystemGlobals");
#endif // [Blur2PC] Native diagnostics are emitted to stderr.

I_FileSystem* g_pDefaultFileSystem					= NULL;

FileRwErrorCallback			g_pFileRwErrorCb			= NULL;
FileReadErrorStringCallback	g_pFileReadErrorStringCb	= NULL;

I_FileSystem* GetDefaultFileSystem()
{
	if (!g_pDefaultFileSystem)
	{
#ifdef TOOL
		g_pDefaultFileSystem = &Sys::C_FileSystem::Instance();
#else
		BIZ_PANIC_FAIL("No default filesystem set. This should be set on app startup!");
#endif
	}
	return g_pDefaultFileSystem;
}

void SetDefaultFileSystem(I_FileSystem* pFileSystem)
{
	g_pDefaultFileSystem = pFileSystem;
}

Btl::String GetFileReadErrorString()
{
	if (g_pFileReadErrorStringCb)
		return g_pFileReadErrorStringCb();

	return "READ ERROR";
}

void SetFileRwErrorCallback(FileRwErrorCallback errCb)
{
	g_pFileRwErrorCb = errCb;
}

void SetFileReadErrorStringCallback(FileReadErrorStringCallback errStrCb)
{
	g_pFileReadErrorStringCb = errStrCb;
}

void FileRwError(const char* fileName, bool writing, bool isCritical, void* pUserData)
{
	if (g_pFileRwErrorCb)
		g_pFileRwErrorCb(fileName, writing, isCritical, pUserData);
}

void FileOpenError(const char* fileName, bool writing, bool isCritical)
{
#if defined(PLATFORM_MACOS)
    // [Blur2PC] Keep open failures visible while the debug-log subscribers port.
    fprintf(stderr, "Failed to open file %s for %s%s.\n", fileName,
            writing ? "writing" : "reading", isCritical ? " (critical)" : "");
#elif !defined(TOOL)
	if (writing)
		LOGERROR(g_FileSystemGlobalsLog, "Failed to open file %s for writing.", fileName);
	else
		LOGERROR(g_FileSystemGlobalsLog, "Failed to open file %s for reading. Most likely the game is referencing an missing/obsolete/debug file.", fileName);
#endif
}

void FsLog(Debug::Logger& logger, ::DebugLevel::Level level, bool isSpam, const char* text, ...)
{
#if defined(PLATFORM_MACOS)
    // [Blur2PC] Real formatted diagnostics; no logger success stub.
    (void)logger; (void)isSpam;
    char buffer[1024];
    va_list arguments;
    va_start(arguments, text);
    vsnprintf(buffer, sizeof(buffer), text, arguments);
    va_end(arguments);
    fprintf(stderr, "FsLog %s\n", buffer);
    if (level == DebugLevel::Fatal) BIZ_ASSERT_FAIL_SPEED(buffer);
#else
	char* levelStr = "";

	{
		char buffer[ 1024 ];
		va_list arglist;
		va_start( arglist, text );
		vsprintf_s( buffer, lengthof(buffer), text, arglist );
		va_end( arglist );


		Btl::String str;
		str += "FsLog ";
#ifdef CFG_GOLD
	switch (level)
	{
	case ::DebugLevel::Fatal:
		levelStr = "Fatal: ";
		break;
	case ::DebugLevel::Error:
		levelStr = "Error: ";
		break;
	case ::DebugLevel::Warn:
		levelStr = "Warn: ";
		break;
	case ::DebugLevel::Info:
		levelStr = "Info: ";
		break;
	case ::DebugLevel::Debug:
		levelStr = "Debug: ";
		break;
	case ::DebugLevel::None:
		levelStr = "None: ";
	default:
		break;
	};
	str += levelStr;
	str += buffer;

	#if defined(PLATFORM_PC)
		str += "\n";
		OutputDebugString(str.Chars());
	#else
		printf("%s\r\n", str.Chars());
	#endif
#else
		str += buffer;
		if (level == DebugLevel::Fatal)
		{
			BIZ_ASSERT_FAIL_SPEED(str.Chars());
		}
		else
		{
			Debug::_dbgMessageProxy::LogS(logger, level, ::Debug::StaticLocationInfoClass(DEBUG_LOCATION), str.Chars());
		}
#endif
	}
#endif // [Blur2PC] native diagnostic / original logger
}

}
