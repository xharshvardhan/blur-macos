// [Blur2PC][macOS] Real transient trail blocks, original super-spawn/update/retirement and owned emitter salts.
#include "../../code/mercury/library/graphics/dynamic/stdafx.h"
#include <dynamic/Renderer.h>
#include <dynamic/particle/Manager.h>
#include <dynamic/particle/Generic/SpriteRotating.h>
#include <dynamic/particle/Generic/SpriteRotatingSuper.h>
#include <dynamic/particle/SuperSpawn.h>
#include <dynamic/particle/SuperEffect.h>
#include <display/buffer/VertexDeclarations.h>
#include <system/scheduler/jobqueue.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace DynamicEffect;
using namespace DynamicEffect::Particle;
namespace {
void Require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
RenderSettings Settings(){
    RenderSettings s;s.vertexShader=E_VertexShader_UV;s.pixelShader=E_PixelShader_Default;s.blendMode=E_BlendMode_Alpha;s.primitiveType=Display::E_PrimitiveType_TriangleList;s.pVertexDeclaration=nullptr;
    s.uvScale=MathsStorage::Float4(1,1,0,0);s.vertices=0;s.vertexSize=0;s.lodFade=MathsStorage::Float2(0,0);s.cullDistance=100;s.soft=false;s.additive=false;s.blendModeTransparent=true;
    auto& v=s.vertexBuildSettings;v.eVertexBuild=E_VertexBuild_SpriteRotating;v.eParticleShape=E_ParticleShape_SpriteRotating;v.eParticleSort=E_ParticleSort_PerBlock;v.eLimitSize=E_ParticleLimitSize_NoLimit;v.limitSize=MathsStorage::Float4A16(0,0,0,0);v.pivot=MathsStorage::Float4A16(0,0,0,0);v.eAtlasType=E_AtlasType_Random;v.fAtlasAnimPeriod=0;v.iAtlasSize=1;v.iAtlasOffset=0;v.iElementSize=sizeof(C_Particle3D_R1D);v.iParticleLibraryIndex=-1;v.pUserShape3DVariants=nullptr;
    v.sizeTable.vSize=MathsStorage::Float4A16(0,0,0,0);v.colourTable.vSize=MathsStorage::Float4A16(0,0,0,0);return s;
}

void ActualTrails(unsigned cycle){
    C_Renderer renderer(nullptr);C_ParticleManager manager(&renderer);
    auto* pool=manager.GetParticleTrailBlockManager();
    Require(pool->GetElementSize()==sizeof(C_Particle_E),"Transient trail pool does not preserve the complete native pointer/salt stride");
    Require(sizeof(C_Particle_E)==16&&offsetof(C_Particle_E,iSalt)==sizeof(void*),"Unexpected native trail metadata layout");
    manager.ReserveSuperEmitters(16);auto* emitters=manager.GetSuperEffectEmitterList();
    SpriteRotating child(&renderer,&manager,2000+cycle);SpriteRotatingSuper effect(&renderer,&manager,3000+cycle);
    const auto render=Settings();UpdateSettings update;PhysicsSettings physics;physics.vAcceleration=MathsStorage::Float4A16(0,0,0,0);RenderStates states{};
    child.Create(update,states,render,physics);effect.Create(update,states,render,physics);
    EmitterDefinition definition{};definition.pEffect=&child;definition.eSpace=E_EmitterSpace_World;definition.eOrientation=E_EmitterOrientation_World;definition.fLife=2;definition.fDuration=10;definition.fExpiryTime=10;
    effect.AddChild(E_EffectChild_Trail,definition,MathsStorage::Float4A16(0,0,0,0));effect.AddChild(E_EffectChild_Trail,definition,MathsStorage::Float4A16(0,0,0,0));
    definition.pEffect=&effect;C_Emitter parent(&manager,nullptr,0);parent.SetDefinition(&definition);
    pool->Update();effect.m_SpawnSettings.pBlockManager->Update();
    S32 activeCount=0;I_ParticleSuperEffect* active[2]{};EmitterUpdateConstants constants{};constants.ppActivedSuperEffects=active;constants.pActivedSuperEffectsCount=&activeCount;constants.time=.1f;
    TrailPtrs trails;trails.Resize(2);U32 count=0,total=9;
    auto* block=BeginCreateSuperNew(count,total,&parent,&effect.m_SpawnSettings,&effect.m_SuperEffects,trails,&constants);
    Require(block&&count==9&&activeCount==1&&active[0]==&effect,"Original super-effect block acquisition did not publish actual ownership");
    for(S32 t=0;t<2;++t){Require(trails[t]&&trails[t]->m_Element.ElementSize()==sizeof(C_Particle_E),"Actual trail block reserved the old PC32 stride");std::memset(trails[t]->m_Element.PushBack(trails[t]->m_Element.Capacity()),0xcd,trails[t]->m_Element.Capacity()*sizeof(C_Particle_E));trails[t]->m_Element.Clear();}
    C_Emitter* saved[2][8]{};
    for(S32 i=0;i<9;++i){
        auto* particle=i==8?NewParticles(block,trails,1,&effect.m_SuperEffects):NewParticle(block,trails,&effect.m_SuperEffects);std::memset(particle,i+1,block->m_Element.ElementSize());
        CreateTrails(block,trails,MathsStorage::Float4A16(float(i),2,3,0),MathsStorage::Float4A16(0,0,0,0),nullptr);
        for(S32 t=0;t<2;++t){auto* ref=reinterpret_cast<C_Particle_E*>(&trails[t]->m_Element[i]);
            if(i<8){Require(ref->pEmitter&&ref->iSalt==ref->pEmitter->GetSalt(),"Original creation corrupted an adjacent live pointer/salt record");saved[t][i]=ref->pEmitter;Require(ref->pEmitter->GetPositionPrevious().x==float(i),"Original trail creation did not publish the actual particle position");}
            else Require(!ref->pEmitter&&ref->iSalt==0,"Exhausted raw trail slot retained poisoned reference metadata");
        }
    }
    Require(emitters->m_ActiveElements==16,"Owned child emitter count disagrees with actual trail creation");
    for(S32 i=0;i<8;++i){UpdateTrails(&effect.m_SuperEffects,i,trails,Maths::HVec3(float(i+10),4,5),Maths::C_Quaternion(0,0,0,1),Maths::HVec3(0,0,0),nullptr,0x40+i);
        for(S32 t=0;t<2;++t){auto* ref=reinterpret_cast<C_Particle_E*>(&trails[t]->m_Element[i]);Require(ref->pEmitter==saved[t][i]&&ref->iSalt==ref->pEmitter->GetSalt()&&ref->pEmitter->GetVisibilityFlags()==U32(0x40+i),"Original update lost a live emitter identity/salt/visibility");}}
    // Retire one live emitter; the original stale-salt check must clear its borrowed trail reference.
    auto* stale=reinterpret_cast<C_Particle_E*>(&trails[0]->m_Element[1]);auto* recycled=stale->pEmitter;const U32 salt=stale->iSalt;emitters->Free(recycled);
    Require(recycled->GetSalt()==salt+1,"Actual emitter retirement failed to advance its generation");
    UpdateTrails(&effect.m_SuperEffects,1,trails,Maths::HVec3(0,0,0),Maths::C_Quaternion(0,0,0,1),Maths::HVec3(0,0,0),nullptr,0);
    Require(!stale->pEmitter,"Original stale-generation reference survived retirement");
    // Remove an interior parent and verify the complete last trail record follows its moved parent.
    FreeParticle(8,block,trails,emitters);
    auto last=*reinterpret_cast<C_Particle_E*>(&trails[1]->m_Element[7]);FreeParticle(2,block,trails,emitters);emitters->Update();
    Require(block->m_Element.Size()==7&&trails[0]->m_Element.Size()==7&&trails[1]->m_Element.Size()==7,"Original paired parent/trail retirement lost matching counts");
    auto moved=*reinterpret_cast<C_Particle_E*>(&trails[1]->m_Element[2]);Require(moved.pEmitter==last.pEmitter&&moved.iSalt==last.iSalt,"Interior retirement failed to copy complete native metadata");
    NewParticle(block,trails,&effect.m_SuperEffects);
    for(S32 t=0;t<2;++t){auto* ref=reinterpret_cast<C_Particle_E*>(&trails[t]->m_Element[7]);Require(ref->pEmitter&&ref->iSalt==ref->pEmitter->GetSalt(),"Original emitter pool could not reuse retired trail ownership");}
    parent.SetBlock(nullptr);effect.Reset();emitters->Update();
    Require(effect.GetNumBlocks()==0&&pool->GetAllocatedBlocks()==0&&effect.m_SpawnSettings.pBlockManager->GetAllocatedBlocks()==0&&emitters->m_ActiveElements==0,"Original effect reset retained parent/trail blocks or active child emitters");
}
}
int main(){try{
    Display::C_Device device;Display::C_DeviceParams params;params.m_Width=16;params.m_Height=8;Require(device.Create(params)&&device.CreateVolatile(params),"Actual trail fixture display lease failed");Display::CreateVertexDeclarations();
    Scheduler::Init();Scheduler::SetGroupPoolSize(32);Scheduler::Start();
    ActualTrails(0);ActualTrails(1);Scheduler::Shutdown();
    Require(device.GetDisplay2().WaitIdle(),"Actual trail fixture GPU lease could not retire");device.DestroyVolatile();device.Destroy();
    std::cout<<"Actual native trail stride, adjacent poisoned records, original super-spawn/update, exhaustion, salt retirement, interior erase, reuse and complete effect cleanup PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
