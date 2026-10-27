// [Blur2PC][macOS] Preserve the PC blocking query and saturated U32 count contract.
#include "StdAfx.h"
#include <display/occlusion/metal/MetalOcclusionQuery.h>
#include <display/device/Device.h>
#include <limits>
namespace Display {namespace Metal {
bool C_OcclusionQuery::Create(){return m_Query.Create(Display::Device()->GetDisplay2().GetContext());}
E_OcclusionQueryResult C_OcclusionQuery::Visible(U32& count)const{
    count=0;if(!m_Query.IsCreated())return E_OcclusionQueryResult_NotVisible;
    U64 samples=0;if(!m_Query.GetResult(samples))return E_OcclusionQueryResult_Failed;
    count=samples>std::numeric_limits<U32>::max()?std::numeric_limits<U32>::max():static_cast<U32>(samples);
    return samples?E_OcclusionQueryResult_Visible:E_OcclusionQueryResult_NotVisible;
}
E_OcclusionQueryResult C_OcclusionQuery::Visible()const{U32 count=0;return Visible(count);}
E_OcclusionQueryResult C_OcclusionQuery::Visible(F32& fraction)const{
    U32 count=0;const auto result=Visible(count);fraction=result==E_OcclusionQueryResult_Visible?1.f:0.f;return result;
}
}}
