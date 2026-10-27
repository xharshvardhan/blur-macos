// [Blur2PC][macOS] The actual particle manager, emitter update and SpriteRotating spawn on native worker slot 7.
#include "../../code/mercury/library/graphics/dynamic/stdafx.h"
#include <dynamic/Renderer.h>
#include <dynamic/particle/Manager.h>
#include <dynamic/particle/Emitter.h>
#include <dynamic/particle/Generic/SpriteRotating.h>
#include <dynamic/particle/SpawnSettings.h>
#include <display/buffer/VertexDeclarations.h>
#include <system/scheduler/jobqueue.h>
#include <atomic>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace DynamicEffect;
using namespace DynamicEffect::Particle;
namespace {
void Require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
template<class F>void Reject(F fn,const char* text){bool bad=false;try{fn();}catch(const std::exception&){bad=true;}Require(bad,text);}
void WorkerProof(Scheduler::Task* task){*static_cast<std::atomic<int>*>(task->Output)=Scheduler::GetWorkerID();}
void Await(Scheduler::GroupHandle group){Scheduler::Flush(group,false,5000);Require(Scheduler::Poll(group),"Actual highest-slot particle task did not finish");}
struct SchedulerScope{SchedulerScope(){Scheduler::Init();Scheduler::SetGroupPoolSize(32);Scheduler::jqEnableWorkers(Scheduler::JQ_CORE_7);Scheduler::Start();}~SchedulerScope(){Scheduler::Shutdown();}};
RenderSettings Settings(){
    RenderSettings s;s.vertexShader=E_VertexShader_UV;s.pixelShader=E_PixelShader_Default;s.blendMode=E_BlendMode_Alpha;s.primitiveType=Display::E_PrimitiveType_TriangleList;s.pVertexDeclaration=nullptr;
    s.uvScale=MathsStorage::Float4(1,1,0,0);s.vertices=0;s.vertexSize=0;s.lodFade=MathsStorage::Float2(0,0);s.cullDistance=100;s.soft=false;s.additive=false;s.blendModeTransparent=true;
    auto& v=s.vertexBuildSettings;v.eVertexBuild=E_VertexBuild_SpriteRotating;v.eParticleShape=E_ParticleShape_SpriteRotating;v.eParticleSort=E_ParticleSort_PerBlock;v.eLimitSize=E_ParticleLimitSize_NoLimit;v.limitSize=MathsStorage::Float4A16(0,0,0,0);v.pivot=MathsStorage::Float4A16(0,0,0,0);v.eAtlasType=E_AtlasType_Random;v.fAtlasAnimPeriod=0;v.iAtlasSize=1;v.iAtlasOffset=0;v.iElementSize=sizeof(C_Particle3D_R1D);v.iParticleLibraryIndex=-1;v.pUserShape3DVariants=nullptr;
    v.sizeTable.vSize=MathsStorage::Float4A16(0,0,0,0);v.colourTable.vSize=MathsStorage::Float4A16(0,0,0,0);return s;
}
void ActualParticles(unsigned cycle,const Scheduler::PolicyHandle policy){
    C_Renderer renderer(nullptr);C_ParticleManager manager(&renderer);
    const auto& owned=manager.GetEmitterUpdateConstants();
    for(S32 i=0;i<E_RandomThreadCount;++i){Require(owned.pRandom[i]==&manager.GetRandom(i),"Particle manager failed to publish an actual owned worker generator");for(S32 j=0;j<i;++j)Require(owned.pRandom[i]!=owned.pRandom[j],"Distinct native worker slots alias the same mutable generator");manager.GetRandom(i).Seed(100+cycle*10+i);}
    Reject([&]{manager.GetRandom(-1);},"Negative particle owner index was accepted");Reject([&]{manager.GetRandom(E_RandomThreadCount);},"Out-of-domain particle owner index was accepted");
    EmitterUpdateConstants constants{};for(S32 i=0;i<E_RandomThreadCount;++i)constants.pRandom[i]=owned.pRandom[i];
    Reject([&]{constants.GetWorkerRandom(-1);},"Negative particle task worker was accepted");Reject([&]{constants.GetWorkerRandom(E_RandomThreadCount);},"Out-of-domain particle task worker was accepted");auto* high=constants.pRandom[7];constants.pRandom[7]=nullptr;Reject([&]{constants.GetWorkerRandom(7);},"Missing highest-slot random owner silently fell back");constants.pRandom[7]=high;
    SpriteRotating effect(&renderer,&manager,1000+cycle);const auto render=Settings();UpdateSettings update;PhysicsSettings physics;physics.vAcceleration=MathsStorage::Float4A16(0,0,0,0);RenderStates states{};effect.Create(update,states,render,physics);
    EmitterDefinition definition{};definition.pEffect=&effect;definition.eSpawn=E_EmitterSpawn_Burst;definition.eSpace=E_EmitterSpace_World;definition.eOrientation=E_EmitterOrientation_World;definition.eDistribution=E_EmitterDistribution_Box;definition.eColourMode=E_EmitterColourMode_SingleColour;definition.eUV=E_EmitterUV_DefaultUV;
    definition.fRate=4;definition.fDuration=10;definition.fLife=2;definition.fSize=1;definition.fForce=1;definition.fExpiryTime=10;definition.cullDistance=100;definition.vPosition=MathsStorage::Float4A16(0,0,0,0);definition.vSize=MathsStorage::Float4A16(2,4,6,0);definition.vColour1=MathsStorage::Float4A16(1,1,1,1);definition.vColour2=definition.vColour1;
    C_Emitter emitter(&manager,nullptr,0);emitter.SetDefinition(&definition);emitter.SetPosition(Maths::HVec3(0,0,0));emitter.SetPositionPrevious(Maths::HVec3(0,0,0));
    SpawningEmitter spawning[4]{};S32 spawningCount=0,customCount=0,activeCount=0,activeSuperCount=0;I_ParticleEffect* active[4]{};
    constants.time=.1f;constants.timeStep=.1f;constants.timeStepRcp=10;constants.intensity=1;constants.pSpawningEmitters=spawning;constants.pSpawningEmittersCount=&spawningCount;constants.spawningEmittersCapacity=4;constants.pSpawningCustomEmittersCount=&customCount;constants.ppActivedEffects=active;constants.pActivedEffectsCount=&activeCount;constants.pActivedSuperEffectsCount=&activeSuperCount;
    C_Emitter* pointer=&emitter;EmitterUpdateData emitterData{&pointer,1,&constants};auto group=Scheduler::CreateGroup(true);
    static_assert(sizeof(EmitterUpdateData)<=sizeof(Scheduler::Task::ParamData)&&sizeof(EffectSpawnData)<=sizeof(Scheduler::Task::ParamData),"Actual native task payload storage cannot hold particle pointer structs");
    const int initialIndex=manager.GetRandom(7).mti;
    Scheduler::AddTask(ParticleEmitterUpdateModule,nullptr,nullptr,group,policy,&emitterData,sizeof(emitterData));Await(group);
    Require(spawningCount==1&&spawning[0].pEmitter==&emitter&&spawning[0].pEffect==&effect&&emitter.GetSpawnAmount()==4,"Actual emitter task lost pointer-width payload or did not publish authored emission");
    manager.GetParticleBlockManager(sizeof(C_Particle3D_R1D))->Update();EffectSpawnData spawn{&effect.m_SpawnSettings,spawning,1};
    Scheduler::AddTask(*effect.GetSpawnModule(),&constants,nullptr,group,policy,&spawn,sizeof(spawn));Await(group);
    auto* block=emitter.GetBlock();Require(block&&block->m_Element.Size()==4&&activeCount==1&&active[0]==&effect,"Actual SpriteRotating task failed to allocate/fill its original particle block");
    Require(manager.GetRandom(7).mti!=initialIndex,"Actual highest-slot spawn failed to consume its own random generator");
    for(S32 i=0;i<7;++i)Require(manager.GetRandom(i).mti==RAND_N,"Highest-slot particle task consumed another worker's generator");
    for(S32 i=0;i<4;++i){const auto* p=reinterpret_cast<const C_Particle3D_R1D*>(&block->m_Element[i]);Require(std::isfinite(p->vPosition.x)&&std::isfinite(p->vPosition.y)&&std::isfinite(p->vPosition.z)&&std::abs(p->vPosition.x)<=1&&std::abs(p->vPosition.y)<=2&&std::abs(p->vPosition.z)<=3,"Real box-distributed SpriteRotating particle positions are outside the authored volume");Require(std::isfinite(p->vForceSizeLife.w)&&p->vForceSizeLife.w>0,"Real particle life initialization failed");}
    emitter.SetBlock(nullptr);effect.Reset();Require(effect.GetNumBlocks()==0&&manager.GetParticleBlockManager(sizeof(C_Particle3D_R1D))->GetAllocatedBlocks()==0,"Actual particle block owner did not retire after task completion");Scheduler::DestroyGroup(group);
}
}
int main(){try{
    static const Scheduler::Policy highest[8]={0,0,0,0,0,0,0,1};const auto policy=Scheduler::CreatePolicy(highest);const Scheduler::jqModule proof(WorkerProof,"Particle highest-slot proof");
    Display::C_Device device;Display::C_DeviceParams params;params.m_Width=16;params.m_Height=8;Require(device.Create(params)&&device.CreateVolatile(params),"Actual particle fixture display lease failed");Display::CreateVertexDeclarations();
    for(unsigned cycle=0;cycle<2;++cycle){SchedulerScope scheduler;Require(Scheduler::GetNumWorkers()==1,"Particle fixture did not preserve the actual sparse highest-worker mask");auto group=Scheduler::CreateGroup(true);std::atomic<int> id{-1};Scheduler::AddTask(proof,nullptr,&id,group,policy);Await(group);Require(id==7,"Native queue did not execute actual logical worker7");Scheduler::DestroyGroup(group);ActualParticles(cycle,policy);}
    Require(device.GetDisplay2().WaitIdle(),"Actual particle fixture GPU lease could not retire");device.DestroyVolatile();device.Destroy();
    std::cout<<"Actual native worker7, owned independent random slots0..7, original emitter-update and SpriteRotating box spawn, pointer-width task payloads, particle block retirement and scheduler restart PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
