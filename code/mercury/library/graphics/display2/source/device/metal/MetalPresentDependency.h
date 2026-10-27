// [Blur2PC][macOS] One bounded present dependency, without completion callbacks.
#pragma once
#import <Metal/Metal.h>
#include <memory>
#include <mutex>
#include "MetalTrace.h"
namespace Display2::Metal {
// [Blur2PC] The default-off path preserves the original wait without timestamp/status work.
inline void TraceCommandWait(id<MTLCommandBuffer> command, Trace::Phase phase, const void* resource) {
    if(!Trace::enabled.load(std::memory_order_relaxed)){[command waitUntilCompleted];return;}
    const int status=int(command.status);
    if(status==MTLCommandBufferStatusCompleted){[command waitUntilCompleted];return;}
    Trace::Span trace(phase,resource,(__bridge const void*)command,status);
    [command waitUntilCompleted];trace.Status(int(command.status));
}

// A record is shared by the presented Surface (including aliases) and the
// device's single outstanding-present slot. Its lock never acquires a device
// or surface lock. Terminal records retain only the cached result, not GPU work.
class PresentDependency {
public:
    PresentDependency(id<MTLCommandBuffer> producer,id<MTLCommandBuffer> consumer)
        : m_Producer(producer),m_Consumer(consumer) {}
    bool Wait(Trace::Phase phase,const void* resource) {
        std::lock_guard<std::mutex> guard(m_Mutex);
        if(m_Terminal)return m_Success;
        // Always retire both commands, even when the producer failed: a failed
        // predecessor does not prove that the submitted consumer has retired.
        bool ok=true;
        if(m_Producer){TraceCommandWait(m_Producer,phase,resource);ok=m_Producer.status==MTLCommandBufferStatusCompleted;}
        if(m_Consumer){TraceCommandWait(m_Consumer,phase,resource);ok=(m_Consumer.status==MTLCommandBufferStatusCompleted)&&ok;}
        m_Success=ok;m_Terminal=true;m_Producer=nil;m_Consumer=nil;
        return ok;
    }
    static bool CanOverlap(id<MTLCommandBuffer> producer,id<MTLCommandQueue> consumerQueue) {
        if(!producer)return true;
        const auto status=producer.status;
        return producer.retainedReferences&&producer.commandQueue==consumerQueue&&
            (status==MTLCommandBufferStatusCommitted||status==MTLCommandBufferStatusScheduled||status==MTLCommandBufferStatusCompleted);
    }
private:
    std::mutex m_Mutex;
    id<MTLCommandBuffer> m_Producer,m_Consumer;
    bool m_Terminal=false,m_Success=false;
};
struct PresentRetirement {
    std::shared_ptr<PresentDependency> outstanding;
    bool failed=false;
    // Caller owns the device lock. Waiting uses only the independent record
    // lock, so a Surface alias can retire the same record without lock inversion.
    bool Retire(Trace::Phase phase,const void* resource) {
        if(outstanding){if(!outstanding->Wait(phase,resource))failed=true;outstanding.reset();}
        return !failed;
    }
};
}
