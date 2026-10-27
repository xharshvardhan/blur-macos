// [Blur2PC][macOS] Included inside namespace Scheduler by the original queue.
// Full-width pointer plus change counter; never pack an arm64 pointer into U32.
template<class T> struct alignas(16) jqAtomicQueuePtr {
    union {
        struct { T* volatile Ptr; volatile U32 Count; U32 padding; };
        volatile unsigned __int128 Value;
    };
    jqAtomicQueuePtr() : Value(0) {}
    jqAtomicQueuePtr(T* pointer, U32 count) : Value(0) { Ptr = pointer; Count = count; }
    jqAtomicQueuePtr(const jqAtomicQueuePtr& other) : Value(other.Value) {}
    jqAtomicQueuePtr& operator=(const jqAtomicQueuePtr& other) { Value = other.Value; return *this; }
};
static_assert(__atomic_always_lock_free(16, nullptr), "native queue requires arm64 128-bit atomics");
template<class T> inline jqAtomicQueuePtr<T> jqPtrLoad(const volatile jqAtomicQueuePtr<T>* pointer) {
    jqAtomicQueuePtr<T> value;
    value.Value = __atomic_load_n(&pointer->Value, __ATOMIC_SEQ_CST);
    return value;
}
template<class T> inline bool jqPtrCompareAndSwap(volatile jqAtomicQueuePtr<T>* pointer,
        const jqAtomicQueuePtr<T>& next, const jqAtomicQueuePtr<T>& compare) {
    auto expected = compare.Value;
    return __atomic_compare_exchange_n(&pointer->Value, &expected, next.Value,
                                       false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}
template<class T> inline bool jqPtrEqual(const jqAtomicQueuePtr<T>& a, const jqAtomicQueuePtr<T>& b) {
    return a.Value == b.Value;
}
