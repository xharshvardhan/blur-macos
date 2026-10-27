// [Blur2PC][macOS] Darwin arm64 floating-point state, using SDK fenv definitions.
#pragma once
#include <fenv.h>
#include <stdexcept>

namespace Maths { namespace NativeMac {
#if !defined(__arm64__)
#error "Native floating-point control currently supports Apple Silicon only"
#endif
constexpr unsigned long long ExceptionTraps = __fpcr_trap_invalid |
    __fpcr_trap_divbyzero | __fpcr_trap_overflow | __fpcr_trap_underflow |
    __fpcr_trap_inexact | __fpcr_trap_denormal;
inline void SetExceptions(bool unmask, bool clear) {
    fenv_t state;
    if (fegetenv(&state)) throw std::runtime_error("fegetenv failed");
    state.__fpcr &= ~ExceptionTraps;
    if (unmask) state.__fpcr |= __fpcr_trap_invalid | __fpcr_trap_divbyzero;
    if (clear) state.__fpsr &= ~static_cast<unsigned long long>(FE_ALL_EXCEPT);
    if (fesetenv(&state)) throw std::runtime_error("fesetenv failed");
}
inline void Init(bool flush) {
    fenv_t state;
    if (fegetenv(&state)) throw std::runtime_error("fegetenv failed");
    state.__fpcr &= ~(ExceptionTraps | FE_TOWARDZERO | __fpcr_flush_to_zero);
    if (flush) state.__fpcr |= __fpcr_flush_to_zero;
    state.__fpsr &= ~static_cast<unsigned long long>(FE_ALL_EXCEPT);
    if (fesetenv(&state)) throw std::runtime_error("fesetenv failed");
    // arm64 float arithmetic already has 24 significand bits; there is no x87
    // extended-precision control. No approximation of that obsolete mode.
}
}}
