// [Blur2PC] Actual native global-new domain across late cxa finalization.
// No framework telemetry, game asset, GPU, audio or unowned worker is required.
#include "../../code/mercury/library/common/system/SystemPch.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <pthread.h>
#include <stdexcept>

struct LateDomainOwner {
    void* earlyMalloc=nullptr;
    U32* retained=nullptr;
    pthread_t worker{};
    pthread_mutex_t gateMutex=PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t gateCondition=PTHREAD_COND_INITIALIZER;
    bool release=false,started=false,workerOkay=false;
    LateDomainOwner():earlyMalloc(::operator new(64)){}
    ~LateDomainOwner();
};
// This verifier is registered before the original priority102 allocator's
// destructor would have been registered, and therefore executes after it.
static LateDomainOwner lateOwner __attribute__((init_priority(101)));
#define ENABLE_SMALL_BLOCK_ALLOCATOR
#include <system/memory/AllocatorFunctions.inl>

static void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
static U32 liveBlocks(){
    std::unique_ptr<Memory::BlockAllocator::Stats> stats(g_SmallBlockAllocator.GetStats());
    return stats->m_nAllocs;
}
static void* LateWorker(void* context){
    auto& owner=*static_cast<LateDomainOwner*>(context);
    pthread_mutex_lock(&owner.gateMutex);
    while(!owner.release)pthread_cond_wait(&owner.gateCondition,&owner.gateMutex);
    pthread_mutex_unlock(&owner.gateMutex);
    bool okay=true;
    try{
        for(U32 round=0;round<4096;++round){
            const U32 count=16+(round%15)*16;
            auto* small=new U8[count];
            okay=okay&&g_SmallBlockAllocator.Initialized()&&g_SmallBlockAllocator.MemoryOwnedByAllocator(small);
            small[0]=0x37;small[count-1]=0x73;
            delete[] small;
            auto* large=new U8[1024];
            okay=okay&&!Memory::BizSmallBlockOwns(large)&&malloc_size(large)>0;
            delete[] large;
        }
    }catch(...){okay=false;}
    delete[] owner.retained;owner.retained=nullptr;
    owner.workerOkay=okay;return nullptr;
}
LateDomainOwner::~LateDomainOwner(){
    // The old declaration fails deterministically and cleanly before any
    // worker is released or dead ownership metadata is inspected.
    if(!g_SmallBlockAllocator.Initialized()){
        std::fputs("Late global-new domain was destroyed before its client; negative control exit73\n",stderr);
        std::_Exit(73);
    }
    if(!started){std::fputs("Late allocator worker was not started\n",stderr);std::_Exit(74);}
    const U32 before=liveBlocks();
    pthread_mutex_lock(&gateMutex);release=true;pthread_cond_signal(&gateCondition);pthread_mutex_unlock(&gateMutex);
    if(pthread_join(worker,nullptr)!=0||!workerOkay||!before||liveBlocks()+1!=before){
        std::fputs("Late allocator new/delete or retained payload retirement failed\n",stderr);std::_Exit(75);
    }
    if(!malloc_size(earlyMalloc)||Memory::BizSmallBlockOwns(earlyMalloc)){
        std::fputs("Pre-bootstrap malloc domain was misclassified during late exit\n",stderr);std::_Exit(76);
    }
    ::operator delete(earlyMalloc);earlyMalloc=nullptr;
    pthread_cond_destroy(&gateCondition);pthread_mutex_destroy(&gateMutex);
    std::puts("Late cxa worker actual global new/delete, retained payload and genuine early malloc retirement PASS");
}
int main(){
    try{
        require(g_SmallBlockAllocator.Initialized(),"Native global allocator is not initialized");
        require(malloc_size(lateOwner.earlyMalloc)>0&&!Memory::BizSmallBlockOwns(lateOwner.earlyMalloc),"Before-bootstrap global new did not retain malloc routing");
        const U32 baseline=liveBlocks();
        // These are real payload generations, not allocator resets. The domain
        // surviving does not allow client payloads to accumulate between owners.
        for(U32 generation=0;generation<8;++generation){
            std::array<U8*,16> payload{};
            for(U32 i=0;i<payload.size();++i)payload[i]=new U8[(i+1)*16];
            for(auto* pointer:payload)delete[] pointer;
            require(liveBlocks()==baseline,"Payload generation leaked a small-block allocation");
        }
        lateOwner.retained=new U32[16];
        require(g_SmallBlockAllocator.MemoryOwnedByAllocator(lateOwner.retained),"Late payload is not from the actual native allocator");
        require(pthread_create(&lateOwner.worker,nullptr,LateWorker,&lateOwner)==0,"Cannot start owned late allocator worker");
        lateOwner.started=true;
        std::puts("Actual global-new payload generations balanced; late finalization verifier must still exit0");
        return 0;
    }catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}
}
