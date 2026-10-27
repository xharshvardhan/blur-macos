// [Blur2PC][macOS] Link every original core object and exercise engine math.
#include <core/Core.h>
#include <core/Maths.h>
#include <core/containers.h>
#include <btl/containers/map.h>
#include <core/maths/hvec/New/IncludePhysics.h>
#include <core/maths/hvec/New/IncludePhysics.inl>
#include <cmath>
#include <algorithm>
#include <btl/allocators/stackallocator.h>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
static void near(float actual, float expected, const char* message) {
    require(std::isfinite(actual) && std::abs(actual - expected) < 0.0001f, message);
}
struct FloatStream {
    std::vector<float> values;
    size_t position = 0;
    FloatStream& operator>>(float& out) { out = values.at(position++); return *this; }
};
struct NonDefaultKey {
    static inline int live = 0;
    int value;
    NonDefaultKey() = delete;
    explicit NonDefaultKey(int v) : value(v) { ++live; }
    NonDefaultKey(const NonDefaultKey& other) : value(other.value) { ++live; }
    ~NonDefaultKey() { --live; }
    friend bool operator<(const NonDefaultKey& a, const NonDefaultKey& b) { return a.value < b.value; }
    friend bool operator==(const NonDefaultKey& a, const NonDefaultKey& b) { return a.value == b.value; }
};
int main() {
    using namespace Maths;
    try {
        static_assert(alignof(Ut::AlignedObject<16>) == 16);
        static_assert(alignof(Ut::AlignedObject<128>) == 128);
        fenv_t saved;
        require(fegetenv(&saved) == 0, "cannot save FP environment");
        fesetround(FE_DOWNWARD);
        feraiseexcept(FE_INEXACT);
        FP_Init(false, false);
        require(fegetround() == FE_TONEAREST && !fetestexcept(FE_ALL_EXCEPT), "FP initialization failed");
        fenv_t state;
        fegetenv(&state);
        require(!(state.__fpcr & NativeMac::ExceptionTraps) && !(state.__fpcr & __fpcr_flush_to_zero), "FP mask/denormal setup failed");
        FP_Init(true, true);
        fegetenv(&state);
        require(state.__fpcr & __fpcr_flush_to_zero, "FP denormal flush not enabled");
        FP_MaskExceptions();
        fesetenv(&saved);

        const HVec3 a(1.f, 2.f, 3.f), b(4.f, 5.f, 6.f);
        const HVec3 sum(a + b);
        near(sum.X(), 5, "vector addition x"); near(sum.Z(), 9, "vector addition z");
        near(static_cast<float>(HF32(a | b)), 32, "vector dot product");
        const HVec3 cross(a ^ b);
        near(cross.X(), -3, "cross x"); near(cross.Y(), 6, "cross y"); near(cross.Z(), -3, "cross z");
        near(Wrap(-0.25f, 1.f), 0.75f, "scalar select negative wrap");
        near(Wrap(1.25f, 1.f), 0.25f, "scalar select positive wrap");
        require(__fsel(std::numeric_limits<float>::quiet_NaN(), 1, 2) == 2, "scalar select NaN semantics");

        const Mat44 matrix = MakeMat44(2,0,0,0, 0,3,0,0, 0,0,4,0, 0,0,0,1);
        near(GetF32(Determinant(matrix)), 24, "original Mat44 determinant object");
        const Mat44 inverse = Inverse(matrix);
        near(GetF32(Element<E_RowX,E_ColX>(inverse)), 0.5f, "original Mat44 inverse object");
        near(GetF32(Element<E_RowY,E_ColY>(inverse)), 1.f/3.f, "original Mat44 inverse y");

        // These constructors used to truncate malloc addresses to signed 32-bit.
        C_BigMatrix<float> large(3, 5);
        require(reinterpret_cast<uintptr_t>(large[0]) % 128 == 0, "big matrix alignment");
        large[2][4] = 19;
        near(large[2][4], 19, "big matrix address/write failed");
        std::vector<char> storage(C_BigMatrix<float>::Sizeof(3, 5) + 1);
        C_BigMatrix<float> external(3, 5, storage.data() + 1, static_cast<S32>(storage.size() - 1));
        require(reinterpret_cast<uintptr_t>(external[0]) % 128 == 0, "external matrix alignment");
        external[1][2] = 7;
        near(external[1][2], 7, "external matrix write failed");
        C_BigMatrix<float> initialized;
        initialized.Initialise(3, 5);
        initialized[0][0] = 3;
        near(initialized[0][0], 3, "initialized matrix address failed");

        // Instantiate previously ill-formed stream paths and check every lane.
        FloatStream eight{{1,2,3,4,5,6,7,8}};
        Vec8F32 v8;
        Deserialise(eight, &v8);
        StorageVec8F32 s8;
        Store(v8, &s8);
        near(s8.m_Data.v0, 1, "Vec8 first lane"); near(s8.m_Data.v7, 8, "Vec8 last lane");
        FloatStream twelve{{1,2,3,4,5,6,7,8,9,10,11,12}};
        Vec12F32 v12;
        Deserialise(twelve, &v12);
        StorageVec12F32 s12;
        Store(v12, &s12);
        near(s12.m_Data.v0, 1, "Vec12 first lane"); near(s12.m_Data.v11, 12, "Vec12 last lane");
        Vec16F32 original16;
        original16.m_V0 = original16.m_V1 = original16.m_V2 = Const<Vec4F32>::Zero();
        original16.m_V3 = MakeVec4F32(13, 14, 15, 16);
        Vec16F32 copied16(original16), assigned16;
        assigned16 = copied16;
        MathsStorage::Float4 lastFour;
        Store(assigned16.m_V3, &lastFour);
        near(lastFour.x, 13, "Vec16 copy first trailing lane");
        near(lastFour.w, 16, "Vec16 copy last trailing lane");
        FloatStream rigid{{0,0,0,1, 3,5,7}};
        DualQuaternion transform;
        Deserialise(rigid, &transform);
        const Vector3 translation = GetTranslation(transform);
        near(GetF32(X(translation)), 3, "dual quaternion output x");
        near(GetF32(Z(translation)), 7, "dual quaternion output z");
        Ut::UtMap<U32, U64> nativeMap;
        for (U32 i = 0; i < 128; ++i) {
            const U32 key = (i * 37) % 128;
            nativeMap[key] = 0x100000000ULL + key;
        }
        for (U32 key = 0; key < 128; ++key) {
            auto found = nativeMap.Find(key);
            require(found != nativeMap.End() && found->GetKey() == key &&
                    found->GetValue() == 0x100000000ULL + key, "native dependent-type map lookup failed");
        }
        for (U32 key = 0; key < 128; key += 2) nativeMap.Erase(nativeMap.Find(key));
        require(nativeMap.Size() == 64, "native map erase count changed");
        U32 expectedKey = 1;
        for (auto it = nativeMap.Begin(); it != nativeMap.End(); ++it, expectedKey += 2)
            require(it->GetKey() == expectedKey, "native map erase broke traversal order");
        nativeMap.Clear();
        require(nativeMap.Empty(), "native map clear failed");
        Btl::Map<U32, U64> btlMap;
        for (U32 key = 0; key < 64; ++key) btlMap[key] = key + 0x100000000ULL;
        for (U32 key = 0; key < 64; ++key)
            require(btlMap.Find(key)->GetValue() == key + 0x100000000ULL, "Btl map lookup changed");
        for (U32 key = 0; key < 64; key += 2) btlMap.Erase(btlMap.Find(key));
        require(btlMap.Size() == 32, "Btl map erase count changed");
        btlMap.Clear(); require(btlMap.Empty(), "Btl map clear failed");
        {
            Ut::UtSet<NonDefaultKey> payloads;
            require(NonDefaultKey::live == 0, "sentinel constructed a data payload");
            for (int i = 0; i < 64; ++i) payloads.Insert(NonDefaultKey(i));
            require(NonDefaultKey::live == 64, "tree payload construction changed");
            payloads.Clear();
            require(NonDefaultKey::live == 0, "tree payload destruction leaked");
        }
        require(NonDefaultKey::live == 0, "sentinel destroyed an inactive payload");
        Ut::Vector<int> checked;for(int i=511;i>=0;--i)checked.PushBack((i*97)%512);
        std::sort(checked.Begin(),checked.End());
        for(int i=0;i<512;++i)require(checked[i]==i,"Original checked iterator lost an element during libc++ sort");
        auto checkedIterator=checked.Begin();checkedIterator+=27;checkedIterator-=12;require(*checkedIterator==15&&checkedIterator[9]==24&&*(3+checkedIterator)==18,"Original checked iterator arithmetic changed element positions");
        Btl::StackAllocatorSimple<> arena;require(!arena.Allocate<U32>()&&arena.BytesUsed()==0,"Empty original arena did not request growth");arena.Initialise(31);require(arena.Allocate<U8>(1),"Native arena first allocation failed");auto* aligned=arena.Allocate<U32>(3,16);require(aligned&&!(reinterpret_cast<INTPTR>(aligned)&15)&&arena.BytesUsed()==28,"Native original arena alignment changed");require(!arena.Allocate<U64>()&&arena.BytesUsed()==28,"Exhausted native arena advanced beyond its storage");arena.Reset();require(arena.BytesUsed()==0,"Original arena reset changed");
        require(CoreDebug::CheckMemoryIntegrity(), "Darwin heap consistency check failed");
        (void)CoreDebug::IsDebuggerAttached();
        std::cout << "Whole engine core linked; scalar vectors, matrices, streams and FP environment passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
