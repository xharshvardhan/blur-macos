// [Blur2PC][macOS] Context-owned immutable states; no encoder, shader or sync policy.
#pragma once
#import <Metal/Metal.h>
#include <array>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <unordered_map>
namespace Display2::Metal {
struct DepthStencilFace {
    bool enabled=false;
    MTLCompareFunction compare=MTLCompareFunctionAlways;
    MTLStencilOperation fail=MTLStencilOperationKeep,depthFail=MTLStencilOperationKeep,pass=MTLStencilOperationKeep;
    uint32_t readMask=0xffffffffu,writeMask=0xffffffffu;
};
struct DepthStencilKey {
    bool depthAttached=false,stencilAttached=false;
    MTLCompareFunction compare=MTLCompareFunctionAlways;
    bool write=false;
    DepthStencilFace front,back;
    // Explicit values, not struct bytes: padding, enum storage and reference
    // values must never create accidental cache identity. Attachment presence
    // is retained even when the requested operations are disabled.
    std::array<uint32_t,18> Words()const noexcept {
        return {uint32_t(depthAttached),uint32_t(stencilAttached),uint32_t(compare),uint32_t(write),
            uint32_t(front.enabled),uint32_t(front.compare),uint32_t(front.fail),uint32_t(front.depthFail),uint32_t(front.pass),front.readMask,front.writeMask,
            uint32_t(back.enabled),uint32_t(back.compare),uint32_t(back.fail),uint32_t(back.depthFail),uint32_t(back.pass),back.readMask,back.writeMask};
    }
    bool operator==(const DepthStencilKey& other)const noexcept{return Words()==other.Words();}
};
struct DepthStencilHash {
    size_t operator()(const DepthStencilKey& key)const noexcept{
        size_t result=1469598103934665603ull;
        for(uint32_t word:key.Words()){result^=word;result*=1099511628211ull;}
        return result;
    }
};
class DepthStencilCache {
    using StateMap=std::unordered_map<DepthStencilKey,id<MTLDepthStencilState>,DepthStencilHash>;
    using Entry=StateMap::value_type;
public:
    explicit DepthStencilCache(id<MTLDevice> device):m_Device(device){
        const char* stats=std::getenv("BLUR_MAC_DEPTH_STATE_STATS");m_Stats=stats&&std::strcmp(stats,"1")==0;
    }
    ~DepthStencilCache(){
        if(m_Stats)std::fprintf(stderr,"blur2pc.depth_state_cache draw_requests=%llu clear_requests=%llu hits=%llu create_attempts=%llu created=%llu failures=%llu entries=%zu last_hits=%llu map_hits=%llu\n",
            m_Draw,m_Clear,m_Hits,m_Attempts,m_Created,m_Failures,m_States.size(),m_LastHits,m_MapHits);
    }
    DepthStencilCache(const DepthStencilCache&)=delete;
    DepthStencilCache& operator=(const DepthStencilCache&)=delete;
    // Small hot path: compare the complete effective key before hashing. The
    // owning map never erases entries; rehash preserves references to its nodes.
    // Always bind, including on a new encoder or after a partial clear. Returning
    // no Objective-C object also avoids a separate hot return-ownership boundary.
    void Bind(id<MTLRenderCommandEncoder> encoder,const DepthStencilKey& key,bool clear){
        if(m_Stats){if(clear)++m_Clear;else ++m_Draw;}
        if(m_Last&&key==m_Last->first){if(m_Stats){++m_Hits;++m_LastHits;}}
        else {
            auto found=m_States.find(key);
            if(found!=m_States.end()){m_Last=&*found;if(m_Stats){++m_Hits;++m_MapHits;}}
            else m_Last=&Create(key);
        }
        [encoder setDepthStencilState:m_Last->second];
    }
private:
    const Entry& Create(const DepthStencilKey& key){
        auto descriptor=[MTLDepthStencilDescriptor new];descriptor.depthCompareFunction=key.compare;descriptor.depthWriteEnabled=key.write;
        auto face=[](const DepthStencilFace& value)->MTLStencilDescriptor*{
            if(!value.enabled)return nil;
            auto result=[MTLStencilDescriptor new];result.stencilCompareFunction=value.compare;
            result.stencilFailureOperation=value.fail;result.depthFailureOperation=value.depthFail;result.depthStencilPassOperation=value.pass;
            result.readMask=value.readMask;result.writeMask=value.writeMask;return result;
        };
        descriptor.frontFaceStencil=face(key.front);descriptor.backFaceStencil=face(key.back);
        if(m_Stats)++m_Attempts;
        auto result=[m_Device newDepthStencilStateWithDescriptor:descriptor];
        if(!result){if(m_Stats)++m_Failures;throw std::runtime_error("Native depth/stencil state creation failed");}
        if(m_Stats)++m_Created;
        return *m_States.emplace(key,result).first;
    }
    id<MTLDevice> m_Device;
    StateMap m_States;
    const Entry* m_Last=nullptr; // Borrowed only from the strong, non-erasing map.
    bool m_Stats=false;
    unsigned long long m_Draw=0,m_Clear=0,m_Hits=0,m_Attempts=0,m_Created=0,m_Failures=0,m_LastHits=0,m_MapHits=0;
};
}
