// [Blur2PC][macOS] Native operations on the engine's existing integer storage.
#pragma once
#include <core/Core.h>
#include <type_traits>

namespace MT {
inline void MemoryFence() { __atomic_thread_fence(__ATOMIC_SEQ_CST); }

// The PC interlocked wrappers return the value BEFORE modification, including
// increment/decrement. Callers such as reference counts rely on that contract.
template<class T> inline T AtomicGet(const volatile T* value) {
    static_assert(std::is_integral<T>::value && (sizeof(T) == 4 || sizeof(T) == 8));
    return __atomic_load_n(value, __ATOMIC_SEQ_CST);
}
template<class T> inline void AtomicSet(volatile T* value, T next) {
    static_assert(std::is_integral<T>::value && (sizeof(T) == 4 || sizeof(T) == 8));
    __atomic_store_n(value, next, __ATOMIC_SEQ_CST);
}
inline void AtomicSet(volatile U32* p, U32 v) { AtomicSet<U32>(p, v); }
inline void AtomicSet(volatile S32* p, S32 v) { AtomicSet<S32>(p, v); }
inline void AtomicSet(volatile U64* p, U64 v) { AtomicSet<U64>(p, v); }
inline void AtomicSet(volatile S64* p, S64 v) { AtomicSet<S64>(p, v); }
template<class T> inline bool AtomicCompareAndSwap(volatile T* value, T next, T compare) {
    return __atomic_compare_exchange_n(value, &compare, next, false,
                                       __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}
inline bool AtomicCompareAndSwap(volatile U32* p, U32 v, U32 c) {
    return AtomicCompareAndSwap<U32>(p, v, c);
}
inline bool AtomicCompareAndSwap(volatile U64* p, U64 v, U64 c) {
    return AtomicCompareAndSwap<U64>(p, v, c);
}
inline U32 AtomicAdd(volatile U32* p, U32 v) { return __atomic_fetch_add(p, v, __ATOMIC_SEQ_CST); }
inline U64 AtomicAdd(volatile U64* p, U64 v) { return __atomic_fetch_add(p, v, __ATOMIC_SEQ_CST); }
// [Blur2PC] Darwin uintptr_t is unsigned long, distinct from legacy U64.
inline INTPTR AtomicAdd(volatile INTPTR* p, INTPTR v){return __atomic_fetch_add(p,v,__ATOMIC_SEQ_CST);}
inline int AtomicIncrement(volatile int* p) { return __atomic_fetch_add(p, 1, __ATOMIC_SEQ_CST); }
inline int AtomicDecrement(volatile int* p) { return __atomic_fetch_sub(p, 1, __ATOMIC_SEQ_CST); }
inline S64 AtomicIncrement(volatile S64* p) { return __atomic_fetch_add(p, 1, __ATOMIC_SEQ_CST); }
inline S64 AtomicDecrement(volatile S64* p) { return __atomic_fetch_sub(p, 1, __ATOMIC_SEQ_CST); }
inline U32 AtomicAnd(volatile U32* p, U32 v) { return __atomic_fetch_and(p, v, __ATOMIC_SEQ_CST); }
inline U64 AtomicAnd(volatile U64* p, U64 v) { return __atomic_fetch_and(p, v, __ATOMIC_SEQ_CST); }
inline U32 AtomicOr(volatile U32* p, U32 v) { return __atomic_fetch_or(p, v, __ATOMIC_SEQ_CST); }
inline U64 AtomicOr(volatile U64* p, U64 v) { return __atomic_fetch_or(p, v, __ATOMIC_SEQ_CST); }
}
