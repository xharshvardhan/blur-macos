#ifndef FRAMEWORK_APPLICATION_TYPES_H
#define FRAMEWORK_APPLICATION_TYPES_H

#ifdef _MSC_VER
#pragma once
#endif

#if defined(PLATFORM_MACOS) // [Blur2PC] Preserve HRESULT's signed 32-bit success/failure contract without Win32.
typedef S32 HRESULT;
#define E_FAIL static_cast<HRESULT>(-2147467259)
#define S_OK static_cast<HRESULT>(0)
#define S_FALSE static_cast<HRESULT>(1)
#define FAILED(hr) (static_cast<HRESULT>(hr) < 0)
#define SUCCEEDED(hr) (static_cast<HRESULT>(hr) >= 0)
#elif defined(PLATFORM_PS3)

typedef U32				HRESULT;

#define _HRESULT_TYPEDEF_(_sc) ((HRESULT)_sc)

#define E_FAIL			_HRESULT_TYPEDEF_(0x80004005L)

#define S_OK			((HRESULT)0x00000000L)
#define S_FALSE			((HRESULT)0x00000001L)

#undef FAILED
#define FAILED(hr)		((HRESULT)(hr) < 0)

#undef SUCCEEDED
#define SUCCEEDED(hr)	((HRESULT)(hr) >= 0)

#endif // PLATFORM_PS3

#endif // FRAMEWORK_APPLICATION_TYPES_H
