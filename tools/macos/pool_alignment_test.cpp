// [Blur2PC][macOS] Real Btl pools must align both allocated values and free links.
#include <core/Core.h>
#include <core/containers.h>
#include <core/threads.h>
#include <btl/allocators/pool.h>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>
static void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
static int constructed=0,destroyed=0;
struct Record12 {U32 value,guard,tail;Record12():value(0),guard(0x12345678),tail(0x87654321){++constructed;}~Record12(){++destroyed;}};
static_assert(sizeof(Record12)==12 && alignof(Record12)==4,"authored value/wire layout changed");
struct alignas(16) SimdRecord {U32 data[4];};
struct alignas(128) CacheRecord {U32 data[32];};
struct Incomplete;
static Btl::DynamicPool<Incomplete> incompletePool;
struct Incomplete {U32 data[3];};
// A byte allocator promises only byte alignment. Pools must preserve the raw
// allocation owner while aligning the actual block within this storage.
struct SkewAllocator {
    static inline int allocated=0,freed=0;
    static U8* Allocate(int bytes){auto* raw=static_cast<U8*>(std::malloc(bytes+1));if(raw)++allocated;return raw?raw+1:nullptr;}
    static void Free(U8* pointer,int){if(pointer){++freed;std::free(pointer-1);}}
};
template<class T> static void aligned(T* value){require(reinterpret_cast<uintptr_t>(value)%alignof(T)==0,"allocated value alignment changed");require(reinterpret_cast<uintptr_t>(value)%alignof(void*)==0,"slot is not aligned for its intrusive free link");}
template<class T> static void staticPool(){
    Btl::StaticPool<T,9> pool;std::vector<T*> values;
    require(pool.Remaining()==9&&pool.Allocated()==0,"static pool capacity changed");
    for(int i=0;i<9;++i){auto* value=pool.Allocate();aligned(value);values.push_back(value);}
    require(pool.Empty()&&pool.Allocated()==9,"static pool accounting changed");
    auto expected=values;std::sort(expected.begin(),expected.end(),std::less<T*>());
    require(std::adjacent_find(expected.begin(),expected.end())==expected.end(),"static slots overlap");
    for(auto* value:values)pool.Free(value);values.clear();
    for(int i=0;i<9;++i){auto* value=pool.Allocate();aligned(value);values.push_back(value);}
    auto actual=values;std::sort(actual.begin(),actual.end(),std::less<T*>());require(actual==expected,"static slot reuse lost storage");
    for(auto* value:values)pool.Free(value);
    require(pool.Allocated()==0&&pool.Remaining()==9,"static free list not restored");
}
template<class T> static void dynamicPool(){
    using Pool=Btl::DynamicPool<T,SkewAllocator,Btl::SingleThreaded>;
    const auto owners=SkewAllocator::allocated,retired=SkewAllocator::freed;
    {
        Pool pool;const int count=Btl::PoolTypes<T>::N+3;std::vector<T*> values;
        for(int i=0;i<count;++i){auto* value=pool.Allocate();aligned(value);values.push_back(value);}
        require(pool.Allocated()==count&&pool.Remaining()==2*Btl::PoolTypes<T>::N-count,"dynamic slot-stride accounting changed");
        require(pool.SizeInBytes()==2*Btl::PoolTypes<T>::AllocationBytes,"dynamic allocation bytes lost alignment padding");
        auto sorted=values;std::sort(sorted.begin(),sorted.end(),std::less<T*>());require(std::adjacent_find(sorted.begin(),sorted.end())==sorted.end(),"dynamic slots overlap");
        for(auto* value:values)pool.Free(value);values.clear();pool.Prefetch();
        auto* value=pool.AllocatePrefetch();aligned(value);pool.Free(value);
        auto* raw=pool.AllocateNoConstructor();aligned(raw);new(raw) T;pool.Free(raw);
        require(pool.Allocated()==0&&pool.Remaining()==2*Btl::PoolTypes<T>::N,"dynamic free list not restored");
    }
    require(SkewAllocator::allocated-owners==2&&SkewAllocator::freed-retired==2,"aligned blocks lost original byte-allocation owner");
}
int main(){try{
    staticPool<Record12>();dynamicPool<Record12>();require(constructed==destroyed,"pool construction/destruction pairing changed");
    staticPool<U32>();dynamicPool<U32>();staticPool<SimdRecord>();dynamicPool<SimdRecord>();staticPool<CacheRecord>();dynamicPool<CacheRecord>();
    auto* incomplete=incompletePool.Allocate();aligned(incomplete);incompletePool.Free(incomplete);
    std::cout<<"Native Btl static/dynamic pools:12-byte values, small values,16/128-byte alignment, growth/reuse/lifetime PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
