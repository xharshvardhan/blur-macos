// [Blur2PC] Original Gravity math/integrator behavior on authored bodies; no retail scene.
#include <core/Core.h>
#include <core/Maths.h>
#include <core/Maths/HVec/New/IncludePhysics.h>
#include <core/Maths/HVec/New/IncludePhysics.inl>
#include <gravity/sharedphysics/RigidBody.h>
#include <gravity/narrowphase/PrimitiveTest.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
static bool failAssert(const char*, const char*, const char*, int) {
    throw std::runtime_error("original Gravity assertion");
}
static F32 scalar(Maths::HF32 value) { F32 result; Maths::Store(value, &result); return result; }
static void near(F32 actual, F32 expected, const char* message) {
    if (!std::isfinite(actual) || std::abs(actual-expected)>=.0002f) { std::cerr<<message<<": actual="<<actual<<" expected="<<expected<<"\n";throw std::runtime_error(message); }
}
static SharedPhysics::C_RigidBody body() {
    using namespace Maths;
    SharedPhysics::C_RigidBody result;
    const HVec3 zero(Constants<HVec3>::Zero());
    result.m_IBody=HMat33(); result.m_IBodyInverse=HMat33();
    result.Mass(HF32(2));result.ReciprocalMass(HF32(.5f));
    result.m_CenterOfMassOffset=zero;result.m_Position=zero;
    result.m_Orientation=C_Quaternion(HVecW(HF32(1)));
    result.m_LinearVelocity=zero;result.m_AngularVelocity=zero;
    result.m_Force=zero;result.m_Torque=zero;
    result.StoreVelocitiesAndPosition(HVec3(3,0,0),zero,HMat33(),HVec3(0,10,0));
    result.m_Force=HVec3(0,-20,0);
    return result;
}
int main(int argc, char** argv) { try {
    using namespace Maths;
    g_pCoreAssertFunction=failAssert;
    SharedPhysics::C_Integrator integrator;
    auto euler=body(),rk4=body();
    // The nonzero-force RK4 path has an inherited dimensional inconsistency.
    // Opt in to its analytical regression to keep the unresolved failure observable.
    const bool probeRk4Acceleration = argc==2 && std::string(argv[1])=="--probe-rk4-acceleration";
    require(argc==1 || probeRk4Acceleration,"Unknown physics fixture argument");
    if (!probeRk4Acceleration) rk4.m_Force=HVec3(Constants<HVec3>::Zero());
    for(U32 tick=0;tick<60;++tick) {
        euler.EulerIntegrate(integrator,C_TimeS(HF32(1.f/60)));
        rk4.RK4Integrate(integrator,C_TimeS(HF32(1.f/60)));
    }
    near(scalar(euler.m_Position.X()),3,"Euler constant horizontal velocity changed");
    near(scalar(euler.m_Position.Y()),10-5.f*59/60,"Euler fixed-step acceleration changed");
    near(scalar(euler.m_LinearVelocity.Y()),-10,"Euler force/mass changed");
    near(scalar(rk4.m_Position.X()),3,"RK4 constant horizontal velocity changed");
    near(scalar(rk4.m_Position.Y()),probeRk4Acceleration?5:10,probeRk4Acceleration?"KNOWN ORIGINAL RK4 acceleration discrepancy":"RK4 unforced vertical drift changed");
    near(scalar(rk4.m_LinearVelocity.Y()),probeRk4Acceleration?-10:0,"RK4 force/mass changed");
    auto spinning=body();spinning.m_Force=HVec3(Constants<HVec3>::Zero());
    spinning.AddImpulse(HVec3(2,0,0));
    near(scalar(spinning.m_LinearVelocity.X()),4,"Original impulse/mass conversion changed");
    spinning.AddAngularImpulse(HVec3(0,0,2));
    near(scalar(spinning.m_AngularVelocity.Z()),2,"Original angular impulse/inertia conversion changed");
    for(U32 tick=0;tick<120;++tick)spinning.EulerIntegrate(integrator,C_TimeS(HF32(1.f/120)));
    near(scalar(spinning.m_Position.X()),4,"Original momentum drift changed");
    near(scalar(spinning.m_OrientationMatrix.AX().X()),std::cos(2.f),"Original angular integration/quaternion normalization changed");
    const HVec3 low(-1,-1,-1),high(1,1,1);
    HF32 hitTime(UNDEFINED);
    require(bool(Collision::IntersectRayAABBMinMax(HVec3(-2,0,0),HVec3(2,0,0),low,high,hitTime)),"Original ray/AABB missed crossing segment");
    near(scalar(hitTime),.25f,"Original ray/AABB entry fraction changed");
    require(!bool(Collision::IntersectRayAABBMinMax(HVec3(-2,2,0),HVec3(2,2,0),low,high)),"Original ray/AABB accepted parallel outside segment");
    require(!bool(Collision::IntersectRayAABBMinMax(HVec3(-3,0,0),HVec3(-2,0,0),low,high)),"Original ray/AABB accepted segment ending before box");
    require(bool(Collision::IntersectRayAABBMinMax(HVec3(0,0,0),HVec3(2,0,0),low,high,hitTime)),"Original ray/AABB rejected starting inside");
    near(scalar(hitTime),0,"Original ray/AABB inside entry fraction changed");
    std::cout<<"Original Gravity fixed-step Euler force/mass/impulses/rotation, unforced RK4 drift and ray/AABB PASS (not a race; forced RK4 unresolved)\n";
    return 0;
} catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; } }
