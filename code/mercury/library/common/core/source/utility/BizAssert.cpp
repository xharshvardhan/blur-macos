#include "CorePch.h"
#include <Core/Core.h>
#include <Core/BizAssert.h>
#if !defined(PLATFORM_MACOS)
#include <System/Debug/log/DebugLog.h>
#endif // [Blur2PC] Native diagnostics do not depend on the unported system logger.

#ifdef PLATFORM_PC
#include <string.h>
#endif

// Default assert handler

#if !defined(PLATFORM_MACOS)
Debug::Logger &gAssertLog = Debug::GetLogger("mercury.system.assert");
#endif

bool DefaultBizAssertHandler(BizAssertKind kind, const char* expr, const char* msg, const char* file, int line)
{
#if defined(PLATFORM_MACOS)
    // [Blur2PC][macOS] Retain real diagnostic text and handler return semantics.
    fprintf(stderr, "%s(%i): %s : %s : %s\n", file, line, GetBizAssertKindText(kind), expr, msg);
#else
	LOGWARN(gAssertLog, "%s(%i): %s%s%s%s%s", file, line,
		GetBizAssertKindText(kind),
		*expr ? " : " : "",	expr,
		*msg ? " : " : "",	msg
		);

#endif
	return (kind == kBizAssert_Assert) || (kind == kBizAssert_Panic);
}

// BizAssertHandler interface

BizAssertHandler gBizAssertHandler = DefaultBizAssertHandler;

BizAssertHandler GetBizAssertHandler()
{
	return gBizAssertHandler;
}

void SetBizAssertHandler(BizAssertHandler handler)
{
	gBizAssertHandler = handler ? handler : DefaultBizAssertHandler;
}

const char* GetBizAssertKindText(BizAssertKind kind)
{
	switch(kind)
	{
		case kBizAssert_Assert:		return "assert";
		case kBizAssert_Notice:		return "notice";
		case kBizAssert_Warning:	return "warning";
		case kBizAssert_Panic:		return "panic";
	}

	return "";
}

// BizAssertReport

bool BizAssertReport(BizAssertKind kind, const char* expr, const char* file, int line, const char* msg, ...)
{
	char buf[2048];

	if (msg)
	{
		va_list va;
		va_start(va, msg);

#if defined(PLATFORM_PS3) || defined(PLATFORM_MACOS) // [Blur2PC] Native bounded formatting.
		vsnprintf(buf, sizeof(buf), msg, va);
#else
		_vsnprintf_s(buf, sizeof(buf), _TRUNCATE, msg, va);
#endif

		va_end(va);
	}
	else
		*buf = '\0';

	return gBizAssertHandler(kind, expr ? expr : "", buf, file, line);
}
