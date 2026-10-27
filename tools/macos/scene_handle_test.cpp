// [Blur2PC][macOS] The production handle with destroyed/reused polymorphic storage.
#include <core/Core.h>
#include <cstdint>
#include <memory>
#include <new>
#include <iostream>
#include <stdexcept>
#include <cassert>
#include <scenegraph/Handle.h>
struct Object {
    virtual ~Object()=default;
    U32 m_Salt=0;
    SceneGraph::C_HandleLifetimeOwner m_HandleLifetime;
};
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
    SceneGraph::C_Handle<Object> empty;require(!empty.Object(),"Default handle is valid");
    auto* storage=::operator new(sizeof(Object));
    auto* first=new(storage) Object;
    SceneGraph::C_Handle<Object> stale(first);const auto constStale=stale;
    auto copy=stale;require(stale.Object()==first&&constStale.Object()==first&&copy.Object()==first,"Live/copy/const handles failed");
    Object copied(*first);SceneGraph::C_Handle<Object> copiedHandle(&copied);
    Object assigned;SceneGraph::C_Handle<Object> assignedHandle(&assigned);assigned=*first;
    require(assignedHandle.Object()==&assigned,"Assignment changed the live destination lifetime");
    ++first->m_Salt;require(!stale.Object(),"Explicit salt invalidation failed");
    first->~Object();
    // The pool's pointer-sized free-list link occupies the former virtual table.
    *static_cast<void**>(storage)=reinterpret_cast<void*>(std::uintptr_t(0x40800000));
    require(!stale.Object()&&!constStale.Object()&&!copy.Object(),"A surviving object copy retained the destroyed source lifetime");
    require(copiedHandle.Object()==&copied&&assignedHandle.Object()==&assigned,"Destroying source invalidated copied objects");
    auto* replacement=new(storage) Object;
    SceneGraph::C_Handle<Object> current(replacement);
    require(!stale.Object()&&current.Object()==replacement,"Same-address salt0 reuse revived stale handle");
    replacement->~Object();::operator delete(storage);
    require(!stale.Object()&&!constStale.Object()&&!copy.Object()&&!current.Object(),"Handle accessed released pool memory");
    std::cout<<"Native scene handle live/salt/destruction/overlaid-vptr/same-address reuse/released-storage PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
