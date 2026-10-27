// [Blur2PC] Original actor handles and stagger scheduling on authored items/fixed ticks.
#include <core/Core.h>
#include <core/Maths.h>
#include <actor_system/HandleManager.h>
#include <actor_system/StaggerManager.h>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <unordered_set>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
static bool failAssert(const char*, const char*, const char*, int) {
    throw std::runtime_error("original actor service assertion");
}
struct Item : Util::C_HandleItem<Item> { U32 value=0; };
struct Visitor : Util::I_HandleVisitor<Item> {
    U32 count=0,sum=0,limit=~0u;
    bool VisitHandleItem(Item* item) override { ++count;sum+=item->value;return count<limit; }
};
static void handles() {
    constexpr U32 count=257;
    Util::C_HandleManager<Item> manager(count);
    std::array<Item,count> items;
    std::array<Util::C_Handle<Item>,count> oldHandles;
    for(U32 i=0;i<count;++i) {
        items[i].value=i;
        oldHandles[i]=manager.AllocateHandle(&items[i]);
        require(manager.IsValidHandle(oldHandles[i])&&manager.GetItemPtr(oldHandles[i])==&items[i],"Original handle lost full-width item address");
    }
    require(manager.Count()==count,"Original handle allocation count changed");
    Visitor all;require(manager.Visit(all)&&all.count==count&&all.sum==count*(count-1)/2,"Original handle visitor lost/duplicated items");
    Visitor stop;stop.limit=3;require(!manager.Visit(stop)&&stop.count==3,"Original handle visitor ignored early stop");
    for(U32 i=0;i<count;++i) {
        manager.FreeHandle(&items[i]);
        require(!manager.IsValidHandle(oldHandles[i])&&!manager.GetItemPtr(oldHandles[i]),"Retired original handle still resolves");
    }
    require(manager.Count()==0,"Original handle retirement retained assigned items");
    for(U32 i=0;i<count;++i) {
        auto replacement=manager.AllocateHandle(&items[i]);
        require(manager.GetItemPtr(replacement)==&items[i]&&!manager.IsValidHandle(oldHandles[i])&&U32(replacement)!=U32(oldHandles[i]),"Original handle cycle reused a stale identity");
    }
    for(auto& item:items)manager.FreeHandle(&item);
    require(!manager.IsValidHandle(Util::C_Handle<Item>()),"Original null handle resolved as live");
}
static void staggeredTicks() {
    Util::C_StaggerManager manager;
    for(U32 level=0;level<Util::E_StaggerLevel_Count;++level) {
        std::array<U32,32> groups{};
        std::unordered_set<U32> ids;
        U32 maskUnion=0;
        const U32 period=1u<<level;
        for(U32 i=0;i<period;++i) {
            groups[i]=manager.AllocGroup(level);
            require(ids.insert(groups[i]).second,"Original stagger allocation did not balance equal groups");
            const U32 mask=manager.GetGroupMask(groups[i]);
            require(!(maskUnion&mask),"Original stagger groups overlap within one level");
            maskUnion|=mask;
            double elapsed=0;
            for(U32 tick=0;tick<32;++tick)
                if(mask&(1u<<tick))elapsed+=(1.0/60)*manager.GetLevelFrameScale(level);
            require(std::abs(elapsed-32.0/60)<1e-9,"Original stagger schedule changed elapsed fixed simulation time");
        }
        require(maskUnion==~0u,"Original stagger level leaves ticks uncovered");
        for(U32 i=0;i<period;++i) {
            require(manager.GetGroupAlloc(groups[i])==1,"Original stagger occupancy changed");
            manager.FreeGroup(groups[i]);
            require(manager.GetGroupAlloc(groups[i])==0,"Original stagger group did not retire");
        }
    }
}
int main() { try {
    g_pCoreAssertFunction=failAssert;
    handles();staggeredTicks();
    std::cout<<"Original actor handle pointer/cycle/visitor and fixed-tick stagger services PASS (not a game actor scene)\n";
    return 0;
} catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; } }
