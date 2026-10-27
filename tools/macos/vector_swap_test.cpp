// [Blur2PC][macOS] Native base-container ownership swaps must preserve reused derived tail padding.
#include <core/Core.h>
#include <core/Maths.h>
#include <core/containers.h>
#include <btl/containers/vector.h>
#include <btl/containers/radix.h>
#include <btl/allocators/pool.h>
#include <dynamic/Atomic.h>
#include <dynamic/particle/SpawnSettings.h>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
void Require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
template<class Base>struct Derived:Base{U32 first=0x5a9341bd,second=0x72863ac9;void Check()const{Require(first==0x5a9341bd&&second==0x72863ac9,"A base vector operation overwrote derived state in reused tail padding");}};
struct Owner {
    static int live,copies;static bool failDefault;int* value;
    Owner():value(nullptr){if(failDefault)throw std::runtime_error("Authored owner default-constructor failure");value=new int(0);++live;}
    Owner(const Owner& o):value(new int(*o.value)){++live;++copies;}
    Owner& operator=(const Owner& o){*value=*o.value;return *this;}
    ~Owner(){delete value;--live;}
};int Owner::live=0;int Owner::copies=0;bool Owner::failDefault=false;
void Dynamic(){
    using Base=Btl::Vector<Owner>;Derived<Base> a;Base b;a.Resize(3);b.Resize(1);for(int i=0;i<3;++i)*a[i].value=10+i;*b[0].value=99;
    auto* left=a[0].value;auto* right=b[0].value;const int copies=Owner::copies;Btl::Swap(static_cast<Base&>(a),b);a.Check();Require(a.Size()==1&&b.Size()==3&&a[0].value==right&&b[0].value==left&&Owner::copies==copies,"Dynamic ownership swap copied, lost or duplicated an allocated owner");
    a.SetCapacity(200);a.Check();Require(a.Size()==1&&*a[0].value==99&&a.Capacity()>=200,"Derived dynamic SetCapacity lost data/size");a.TrimCapacity();a.Check();Require(a.Capacity()==a.Size()&&*a[0].value==99,"Derived dynamic TrimCapacity lost data");a.Reserve(300);a.Check();a.Clear();a.Resize(2);*a[0].value=77;Btl::Swap(static_cast<Base&>(a),static_cast<Base&>(a));a.Check();Require(a.Size()==2&&*a[0].value==77,"Dynamic self-swap changed its owner");Base empty;Btl::Swap(static_cast<Base&>(a),empty);a.Check();Require(a.Empty()&&empty.Size()==2,"Dynamic empty/live swap changed size");
}
void Inline(){
    using Static=Btl::Vector<Owner,Btl::VectorParameters_Static<8>>;Derived<Static> a;Static b;Owner first,second,third;*first.value=11;*second.value=22;*third.value=33;a.PushBack(first);a.PushBack(second);a.PushBack(third);b.PushBack(third);Btl::Swap(static_cast<Static&>(a),b);a.Check();Require(a.Size()==1&&b.Size()==3&&*a[0].value==33&&*b[0].value==11&&*b[2].value==33,"Unequal-size static owner swap lost active values");Btl::Swap(static_cast<Static&>(a),b);a.Check();Require(a.Size()==3&&b.Size()==1&&*a[1].value==22,"Static owner reverse swap failed");b.Clear();Btl::Swap(static_cast<Static&>(a),b);a.Check();Require(a.Empty()&&b.Size()==3,"Static empty/live swap failed");Btl::Swap(b,b);Require(b.Size()==3,"Static self-swap changed active size");
    // Intentionally initialize only active scalar slots, matching original CrowdDelimiter usage.
    using Scalars=Btl::Vector<U64,Btl::VectorParameters_Static<16>>;Derived<Scalars> x;Scalars y;x.PushBack(U64(9));x.PushBack(U64(7));y.PushBack(U64(4));Btl::Swap(static_cast<Scalars&>(x),y);x.Check();Require(x.Size()==1&&x[0]==4&&y.Size()==2&&y[1]==7,"Static scalar swap read or published inactive slots");
    using Array=Btl::Vector<Owner,Btl::VectorParameters_Array<3>>;Derived<Array> p;Array q;for(int i=0;i<3;++i){*p[i].value=i+1;*q[i].value=i+10;}Btl::Swap(static_cast<Array&>(p),q);p.Check();Require(*p[2].value==12&&*q[2].value==3,"Fixed inline-array swap lost live elements");
}
void Fixed(){using Base=Btl::Vector<Owner,Btl::VectorParameters_FixedSize<>>;Derived<Base> a;Base b;a.InitialiseSize(3);b.InitialiseSize(1);*a[0].value=15;*b[0].value=31;auto* old=a[0].value;const int copies=Owner::copies;Btl::Swap(static_cast<Base&>(a),b);a.Check();Require(a.Size()==1&&b.Size()==3&&*a[0].value==31&&b[0].value==old&&copies==Owner::copies,"Fixed-size heap storage swap changed ownership");Base empty;Btl::Swap(static_cast<Base&>(a),empty);a.Check();Require(a.Empty()&&empty.Size()==1,"Fixed-size empty/live swap failed");}
void StaticFailure(){
    using Base=Btl::Vector<Owner,Btl::VectorParameters_Static<8>>;Derived<Base> a;Base b;Owner value;*value.value=19;a.PushBack(value);a.PushBack(value);a.PushBack(value);b.PushBack(value);
    const int live=Owner::live;bool original=false;Owner::failDefault=true;
    try{Btl::Swap(static_cast<Base&>(a),b);}catch(const std::runtime_error& e){original=std::string(e.what())=="Authored owner default-constructor failure";}
    Owner::failDefault=false;a.Check();Require(original&&Owner::live==live,"Static swap default-constructor failure destroyed a still-counted owner or lost the original error");Require(a.Size()<=8&&b.Size()<=8,"Failed static swap published an invalid active count");
    Btl::Swap(static_cast<Base&>(a),b);a.Check();Require(a.Size()==1&&b.Size()==3,"Static swap could not retry after a real value-construction failure");
}
void ParticleRadix(){
    using Record=DynamicEffect::Particle::SpawningEmitter;using Base=Btl::Vector<Record>;using Queue=DynamicEffect::AtomicVector<Record>;
    Queue queue;queue.Resize(128);std::cout<<"Actual particle vector sizeof(base)="<<sizeof(Base)<<", atomic-count offset="<<(reinterpret_cast<const char*>(queue.SizePtr())-reinterpret_cast<const char*>(&queue))<<'\n';
    Btl::RadixSorter<Base> sorter;
    for(int round=0;round<3;++round)for(int count:{0,1,2,3,31,32,33,127,128,129,159,160,161,511}){
        queue.Clear();const int bound=queue.Capacity();
        for(int i=0;i<count;++i){const S32 slot=MT::AtomicIncrement(queue.SizePtr());if(slot<bound){
            // Opaque pointer identities are never dereferenced: keys deliberately collide in low32 bits.
            queue[slot].pEmitter=reinterpret_cast<DynamicEffect::Particle::C_Emitter*>(std::uintptr_t(0x100000010ull+U64(i)*16));
            queue[slot].pEffect=reinterpret_cast<DynamicEffect::Particle::I_ParticleEffectBase*>(std::uintptr_t(0x44ull+(1+U64((i*17)%19))*0x100000000ull));
        }}
        queue.Update();std::vector<Record> expected;for(int i=0;i<queue.Size();++i)expected.push_back(queue[i]);std::stable_sort(expected.begin(),expected.end(),[](const auto& l,const auto& r){return INTPTR(l.pEffect)<INTPTR(r.pEffect);});
        auto* atomicCount=queue.SizePtr();const int capacity=queue.Capacity();if(queue.Size()<capacity)queue.Base::Resize(queue.Size());sorter.Sort(queue);queue.Base::Resize(capacity);queue.UpdateElementPtr();
        Require(queue.SizePtr()==atomicCount&&queue.Size()==S32(expected.size()),"Original particle radix base swap overwrote its derived atomic count/owner");
        for(int i=0;i<queue.Size();++i)Require(queue[i].pEmitter==expected[i].pEmitter&&queue[i].pEffect==expected[i].pEffect,"Original pointer-width particle radix lost, duplicated or regrouped an emitter/effect record");
    }
}
}
int main(){try{Require(Owner::live==0,"Owner fixture started with retained state");for(int cycle=0;cycle<3;++cycle){Dynamic();Inline();Fixed();StaticFailure();Require(Owner::live==0,"Vector ownership swap/growth/trim/retry leaked a live value");}ParticleRadix();std::cout<<"Native original Btl vector dynamic/static/array/fixed owner swaps, derived-tail canaries, growth/trim/retry/self/empty and constructor-failure lifetimes, exact AtomicVector particle radix record preservation with high32-key collisions PASS\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
