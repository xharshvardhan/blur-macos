// [Blur2PC][macOS] Real GPU sample counts, retained until command completion.
#pragma once
#include <core/core.h>
#include <memory>
namespace Display2 {
class C_Context;
class C_OcclusionQuery {
public:
    C_OcclusionQuery()=default;
    ~C_OcclusionQuery(){Destroy();}
    C_OcclusionQuery(const C_OcclusionQuery&)=delete;
    C_OcclusionQuery& operator=(const C_OcclusionQuery&)=delete;
    bool Create(C_Context& context);
    bool IsCreated()const;
    void Destroy();
    void Begin();
    void End();
    bool GetResult(U64& samples)const;
private:
    friend class C_Context;
    struct State;
    std::shared_ptr<State> m_State;
};
}
