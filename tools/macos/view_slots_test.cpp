// [Blur2PC][macOS] Actual SceneGraph/typed-renderer lazy-slot ownership and failure boundaries.
#include "../../code/amax/game/GameLogic/stdafx.h"
#define SCENEGRAPH_ALLOW_INCLUDE
#include <scenegraph/SceneGraphImpl.h>
#include <scenegraph/View.h>
#include <scenegraph/RendererInterface.h>
#include <scenegraph/RendererView.h>
#include <dynamic/Renderer.h>
#include <framework/layer/LayerManager.h>
#include <framework/layer/Layer.h>
#include <fx/Fx.h>
#include <system/scheduler/jobqueue.h>
#include <iostream>
#include <stdexcept>
#include <string>
namespace {
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Counts {unsigned creates=0,destroys=0,deletes=0,liveGroups=0,reserves=0,throwCreate=0,throwReserve=0;};
Counts* active=nullptr;
class Child final:public SceneGraph::I_RendererView {
    Scheduler::GroupHandle group=Scheduler::JQ_GROUP_NONE;
public:
    Child(SceneGraph::I_Renderer* renderer,const SceneGraph::C_View* parent):I_RendererView(renderer,parent){}
    ~Child(){Require(group==Scheduler::JQ_GROUP_NONE,"Child deleted before acquired group retirement");++active->deletes;}
    void Create()override{
        group=Scheduler::CreateGroup();++active->liveGroups;
        if(++active->creates==active->throwCreate)throw std::runtime_error("Authored renderer-view Create failure");
    }
    void Destroy()override{
        Require(group!=Scheduler::JQ_GROUP_NONE,"Child Destroy called twice");
        Scheduler::DestroyGroup(group);group=Scheduler::JQ_GROUP_NONE;--active->liveGroups;++active->destroys;
    }
};
class Renderer final:public SceneGraph::T_Renderer<Child> {
public:
    explicit Renderer(SceneGraph::C_SceneGraph& graph):T_Renderer(&graph){}
    void NativeReserveViewSlot(const SceneGraph::C_View* parent)override{
        if(++active->reserves==active->throwReserve)throw std::runtime_error("Authored renderer-view slot reservation failure");
        T_Renderer::NativeReserveViewSlot(parent);
    }
    unsigned Slots()const{return m_Views.Size();}
    const Child* At(unsigned index)const{return m_Views[index];}
};
class DynamicRenderer final:public DynamicEffect::C_Renderer {
public:
    explicit DynamicRenderer(SceneGraph::C_SceneGraph& graph):C_Renderer(&graph){}
    unsigned Slots()const{return m_Views.Size();}
    const DynamicEffect::C_View* At(unsigned index)const{return m_Views[index];}
};
void LazyAndPermutation(){
    Counts counts;active=&counts;SceneGraph::C_SceneGraph graph;Renderer renderer(graph);
    auto* a=graph.AddView();auto* b=graph.AddView();auto* c=graph.AddView();auto* d=graph.AddView();
    Require(renderer.Slots()==4&&!counts.creates,"Scene registration did not reserve lazy slots without materialization");
    c->Initialise();b->Initialise();
    Require(renderer.At(2)->GetView()==c&&renderer.At(1)->GetView()==b&&!renderer.At(0)&&!renderer.At(3),"Out-of-order initialization appended instead of materializing stable slots");
    graph.RemoveView(a);Require(d->GetIndex()==0&&renderer.Slots()==3&&!renderer.At(0),"Cancelling lazy first view did not move the lazy final slot");
    d->Initialise();graph.RemoveView(b);
    Require(c->GetIndex()==1&&renderer.Slots()==2&&renderer.At(1)->GetView()==c&&renderer.At(0)->GetView()==d,"Scene and renderer nonordered permutations diverged");
    graph.RemoveView(c);graph.RemoveView(d);
    Require(!renderer.Slots()&&counts.creates==3&&counts.destroys==3&&counts.deletes==3&&!counts.liveGroups,"Mixed lazy/materialized views did not retire exactly once");
    auto* disabled=graph.AddView();disabled->DisableRenderNode(renderer.GetNodeFlag());disabled->Initialise();
    Require(renderer.Slots()==1&&!renderer.At(0)&&counts.creates==3,"Disabled view unexpectedly materialized a renderer child");
    graph.RemoveView(disabled);Require(!renderer.Slots(),"Disabled reserved slot was not retired");
}
void TransactionAndRetry(){
    Counts counts;active=&counts;SceneGraph::C_SceneGraph graph;Renderer first(graph),second(graph);
    counts.throwReserve=2;bool failed=false;
    try{graph.AddView();}catch(const std::runtime_error& error){failed=std::string(error.what())=="Authored renderer-view slot reservation failure";}
    Require(failed&&!first.Slots()&&!second.Slots()&&!counts.creates,"Failed registration leaked a scene/renderer slot or created a child");
    counts.throwReserve=0;auto* view=graph.AddView();Require(view->GetIndex()==0,"Failed registration published a scene view");
    counts.throwCreate=2;failed=false;
    try{view->Initialise();}catch(const std::runtime_error& error){failed=std::string(error.what())=="Authored renderer-view Create failure";}
    Require(failed&&first.Slots()==1&&second.Slots()==1&&!first.At(0)&&!second.At(0)&&counts.creates==2&&counts.destroys==2&&counts.deletes==2&&!counts.liveGroups,"Throwing materialization did not roll back failed and previously completed child owners");
    counts.throwCreate=0;view->Initialise();view->Initialise();
    Require(first.At(0)->GetView()==view&&second.At(0)->GetView()==view&&counts.creates==4,"Failed view could not retry once, or successful initialization repeated");
    graph.RemoveView(view);Require(!first.Slots()&&!second.Slots()&&counts.destroys==4&&counts.deletes==4&&!counts.liveGroups,"Retried materialization did not retire once");
}
void RealDynamicViews(){
    SceneGraph::C_SceneGraph graph;DynamicRenderer renderer(graph);
    auto* main=graph.AddView();auto* cube=graph.AddView();auto* lightTrails=graph.AddView();
    cube->Initialise();main->Initialise();
    Require(renderer.Slots()==3&&renderer.At(0)->GetView()==main&&renderer.At(1)->GetView()==cube&&!renderer.At(2),"Actual Dynamic view table did not retain the lazy LightTrails slot");
    graph.RemoveView(main);Require(lightTrails->GetIndex()==0&&renderer.Slots()==2&&!renderer.At(0),"Actual Dynamic renderer diverged during early Main-view retirement");
    lightTrails->Initialise();graph.RemoveView(cube);graph.RemoveView(lightTrails);Require(!renderer.Slots(),"Actual Dynamic views/groups did not retire");
    DynamicEffect::C_View unused;unused.Destroy();unused.Destroy();
    DynamicEffect::C_View owned;owned.Create();owned.Destroy();owned.Destroy();
}
struct LayerCounts{unsigned renders=0,destroys=0;};
class ViewLayer final:public Framework::C_Layer {
    SceneGraph::C_View* view;LayerCounts& counts;
public:
    ViewLayer(SceneGraph::C_View* v,LayerCounts& c):C_Layer("Actual scene-view materialization failure"),view(v),counts(c){}
    ~ViewLayer()override{++counts.destroys;}
    HRESULT ProcessMessage(Framework::E_LayerMessage message)override{
        if(message==Framework::E_LayerMessage_Create)view->Initialise();
        if(message==Framework::E_LayerMessage_RenderOnce)++counts.renders;
        return S_OK;
    }
};
void MainFailureHandoff(){
    Counts counts;active=&counts;counts.throwCreate=1;SceneGraph::C_SceneGraph graph;Renderer renderer(graph);auto* view=graph.AddView();
    LayerCounts layers;Framework::C_LayerManager manager;auto* layer=new ViewLayer(view,layers);manager.InsertLayer(layer);
    bool failed=false;try{manager.Update();}catch(const std::runtime_error& error){failed=std::string(error.what()).find("Authored renderer-view Create failure")!=std::string::npos;}
    manager.SendMessageToLayers(Framework::E_LayerMessage_RenderOnce);
    Require(failed&&!layers.renders&&layer->GetLayerState()==Framework::E_LayerState_Constructed&&!renderer.At(0)&&!counts.liveGroups,"Actual layer callback lost materialization failure or promoted its failed view to Ready/Running");
    manager.NativeQuiesceCreation();manager.NativeDestroyLayerNow(layer);manager.ShutDown();
    graph.RemoveView(view);Require(layers.destroys==1&&!renderer.Slots()&&counts.destroys==1&&counts.deletes==1,"Failed actual layer/view cancellation did not retire once");
}
}
int main(){try{
    Scheduler::Init();Scheduler::SetGroupPoolSize(128);Scheduler::Start();
    Display::C_Device device;Display::C_DeviceParams parameters;parameters.m_Width=32;parameters.m_Height=24;
    Require(device.Create(parameters)&&device.CreateVolatile(parameters),"Actual scene-view fixture Display initialization failed");
    FX::Initialise();
    std::cerr<<"view-slots: lazy/out-of-order/permutation\n";LazyAndPermutation();
    std::cerr<<"view-slots: reservation/materialization rollback/retry\n";TransactionAndRetry();
    std::cerr<<"view-slots: actual Dynamic view groups\n";RealDynamicViews();
    std::cerr<<"view-slots: actual layer first-error handoff\n";MainFailureHandoff();
    FX::Shutdown();Require(device.GetDisplay2().WaitIdle(),"Actual scene-view GPU quiescence failed");device.DestroyVolatile();device.Destroy();Scheduler::Shutdown();
    std::cout<<"Actual SceneGraph/T_Renderer registration, lazy cancellation, out-of-order materialization, disabled slots, matching nonordered removal, acquired-group rollback/retry, real Dynamic views and main first-error/no-Ready/true process exit PASS (not gameplay)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
