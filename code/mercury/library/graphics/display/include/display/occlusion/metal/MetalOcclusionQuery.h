// [Blur2PC][macOS] Original Display query contract over native GPU sample counts.
#pragma once
#include <display/occlusion/OcclusionQueryTypes.h>
#include <display2/context/MetalOcclusion.h>
namespace Display {namespace Metal {
class C_OcclusionQuery {
public:
    bool Create();
    bool IsCreated()const{return m_Query.IsCreated();}
    void Destroy(){m_Query.Destroy();}
    void Begin(){m_Query.Begin();}
    void End(){m_Query.End();}
    E_OcclusionQueryResult Visible()const;
    E_OcclusionQueryResult Visible(U32& count)const;
    E_OcclusionQueryResult Visible(F32& fraction)const;
private:
    Display2::C_OcclusionQuery m_Query;
};
}
using C_OcclusionQuery=Metal::C_OcclusionQuery;
}
