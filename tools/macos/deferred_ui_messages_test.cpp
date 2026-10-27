// [Blur2PC][macOS] Real deferred message/store/pool owner closure, never callbacks during cancellation.
#include "../../code/amax/game/GameLogic/stdafx.h"
#include <ui/Game/Gadgets/ButtonController.h>
#include <ui/Game/Gadgets/ButtonMessaging.h>
#include <Ui/UiManager.h>
#include <Game/Game.h>
#include <system/serialise/C_FileSystem.h>
#include <filesystem>
#include <misc_shared/Messaging/Messaging.h>
#include <system/memory/MemoryTracker.h>
#include <array>
#include <atomic>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <malloc/malloc.h>
namespace UI {extern C_UiInterface* g_pManager;}
namespace {
void Require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
size_t RealHeapBytes(){malloc_statistics_t statistics{};malloc_zone_statistics(malloc_default_zone(),&statistics);return statistics.size_in_use;}
constexpr Messaging::TMsgType type=0x7ed00001;
// Original message contract is POD payload/byte-copy; no copied C++ object destructor is invented.
struct Message:Messaging::C_MessageBase {
    U8 payload[65536];
    Message():C_MessageBase(type,sizeof(Message)){std::memset(payload,0x37,sizeof(payload));}
};
// [Blur2PC] This explicit owner has no dynamic static constructor/destructor before bootstrap.
// Register its retirement from main while the actual game allocator is live.
UI::C_DeferredUiMessageQueue* retainedStatic=nullptr;
void RetireExitQueue(){delete retainedStatic;retainedStatic=nullptr;}
class Handler final:public Messaging::C_MessageHandler {
public:
    UI::C_ButtonController* controller=nullptr;unsigned calls=0;bool throws=false,reentrant=false;
    explicit Handler(UI::C_ButtonController& owner):controller(&owner){SetSubscriberSystem(&owner.GetSubscriberSystem());AddSubscriber(type);}
    bool MessageHandler(const Messaging::C_MessageBase* base)override{
        const auto* message=static_cast<const Message*>(base);Require(message->payload[0]==0x37&&message->payload[65535]==0x37,"Deferred payload was not an owned copy");++calls;
        if(reentrant){reentrant=false;Message next;controller->BroadcastMessage(&next);}
        if(throws){throws=false;throw std::runtime_error("Authored real deferred callback failure");}
        return true;
    }
};
void CopiesAndCancellation(){
    UI::C_DeferredUiMessageQueue queue;Message message;
    // Warm the real retained pool allocation before measuring only copied message buffers.
    queue.BroadcastMessage(&message);Require(queue.NativeCancelPendingMessages()==1,"Initial real store was not retired");
    auto& tracker=Memory::Tracker::Instance();const auto before=tracker.TotalMemory();const auto heapBefore=RealHeapBytes();
    for(unsigned i=0;i<4;++i)Require(queue.BroadcastMessage(&message),"Real message copy failed");
    const auto allocated=tracker.TotalMemory();const auto heapAllocated=RealHeapBytes();
    Require(queue.NativeCancelPendingMessages()==4&&!queue.NativeCancelPendingMessages(),"Cancellation did not retire exact pending stores once");
    const auto after=tracker.TotalMemory();const auto heapAfter=RealHeapBytes();
    if(tracker.Initialised()){
        Require(allocated>=before+4*sizeof(Message)&&after==before,"Real tracked payload allocations were not reclaimed by store destruction");
        std::cout<<"Real memory tracker: copied payload bytes acquired and reclaimed exactly\n";
    }else{
        Require(heapAllocated>=heapBefore+4*sizeof(Message)&&heapAllocated>=heapAfter+4*sizeof(Message),"Actual malloc zone did not acquire/reclaim copied payload bytes");
        std::cout<<"Actual malloc zone: copied payload bytes acquired and reclaimed (engine tracker inactive)\n";
    }
}
void CallbackAndReopen(){
    UI::C_ButtonController controller;Handler handler(controller);Message message;
    controller.BroadcastMessage(&message);message.payload[0]=0;message.payload[65535]=0;
    controller.ProcessDeferredMessages();Require(handler.calls==1,"Original deferred dispatch did not read copied bytes");
    Message valid;handler.reentrant=true;handler.throws=true;
    controller.BroadcastMessage(&valid);controller.BroadcastMessage(&valid);
    bool failed=false;try{controller.ProcessDeferredMessages();}catch(const std::runtime_error& error){failed=std::strstr(error.what(),"Authored real")!=nullptr;}
    Require(failed&&handler.calls==2,"Actual callback did not throw at the expected owner boundary");
    Require(controller.NativeCloseDeferredMessages()==2&&!controller.NativeCloseDeferredMessages(),"Popped throwing entry leaked or remaining two buffers were not cancelled");
    controller.BroadcastMessage(&valid);controller.ProcessDeferredMessages();Require(handler.calls==2&&!controller.NativeCloseDeferredMessages(),"Closed controller retained or delivered a late producer");
    for(unsigned cycle=0;cycle<3;++cycle){controller.NativeBeginDeferredMessages();controller.BroadcastMessage(&valid);controller.ProcessDeferredMessages();Require(handler.calls==3+cycle&&!controller.NativeCloseDeferredMessages(),"Legitimate new controller lifetime could not reopen and dispatch once");}
}
void ProducerClose(){
    UI::C_ButtonController controller;Handler handler(controller);Message message;std::atomic<unsigned> started{0},posted{0};
    std::array<std::thread,4> workers;
    for(auto& worker:workers)worker=std::thread([&]{++started;for(unsigned i=0;i<32;++i){controller.BroadcastMessage(&message);++posted;std::this_thread::yield();}});
    while(started<4||posted<4)std::this_thread::yield();
    const U32 cancelled=controller.NativeCloseDeferredMessages();
    for(auto& worker:workers)worker.join();
    Require(cancelled>0&&!controller.NativeCloseDeferredMessages()&&!handler.calls,"Close raced a real producer into a retained store or callback");
    controller.ProcessDeferredMessages();Require(!handler.calls,"Cancelled concurrent messages were dispatched");
    controller.NativeBeginDeferredMessages();controller.BroadcastMessage(&message);controller.ProcessDeferredMessages();Require(handler.calls==1,"Concurrent close prevented a later owner lifetime");controller.NativeCloseDeferredMessages();
}
void FailedUiOwner(){
    Game::C_Game game;Display::C_Device device;Display::C_DeviceParams parameters;parameters.m_Width=32;parameters.m_Height=24;
    Require(device.Create(parameters)&&device.CreateVolatile(parameters),"Real failed UI-owner Display initialization failed");
    auto& fs=Sys::C_FileSystem::Instance();const auto previous=fs.GetWorkingDirectory();
    const auto scratch=std::filesystem::absolute(std::filesystem::path(__FILE__).parent_path()/"../../_work/build/macos/deferred-ui-agent/empty-ui");std::filesystem::create_directories(scratch);
    Require(fs.SetWorkingDirectory(scratch.c_str()),"Cannot set empty UI resource directory");Sys::SetDefaultFileSystem(&fs);
    Handler handler(UI::gButtons);Message message;
    for(unsigned lifetime=0;lifetime<2;++lifetime){
        UI::gButtons.NativeBeginDeferredMessages();UI::gButtons.BroadcastMessage(&message);
        {UI::C_UiManager owner(UI::C_UiManager::E_LaunchMode_Debug);Require(!owner.NativeInitialise()&&!owner.NativeIsReady(),"Absent required UI resources reported Ready");
         UI::gButtons.BroadcastMessage(&message);UI::gButtons.ProcessDeferredMessages();Require(!handler.calls&&!UI::gButtons.NativeCloseDeferredMessages(),"Failed UI init retained/delivered pending or late messages before owner destruction");}
    }
    Sys::SetDefaultFileSystem(NULL);fs.SetWorkingDirectory(previous);
    Require(device.GetDisplay2().WaitIdle(),"Real failed UI-owner GPU quiescence failed");device.DestroyVolatile();device.Destroy();
}
void RejectedUiOwner(){
    // Real uniqueness gate before any device/font acquisition; this is not an initialized full UI.
    UI::C_UiInterface existing;UI::g_pManager=&existing;UI::gButtons.NativeBeginDeferredMessages();Handler handler(UI::gButtons);Message message;UI::gButtons.BroadcastMessage(&message);
    {UI::C_UiManager rejected(UI::C_UiManager::E_LaunchMode_Debug);Require(!rejected.NativeInitialise()&&std::strstr(rejected.NativeInitialisationError(),"Another UI manager"),"Real UI uniqueness gate did not reject another owner");}
    UI::gButtons.ProcessDeferredMessages();Require(handler.calls==1,"Rejected UI owner closed/cancelled the live owner's deferred queue");
    UI::gButtons.NativeCloseDeferredMessages();UI::g_pManager=nullptr;
}
}
int main(){try{
    CopiesAndCancellation();CallbackAndReopen();ProducerClose();RejectedUiOwner();FailedUiOwner();
    retainedStatic=new UI::C_DeferredUiMessageQueue;
    if(std::atexit(RetireExitQueue)!=0){RetireExitQueue();throw std::runtime_error("Cannot register allocator-live queue retirement");}
    Message message;Require(retainedStatic->BroadcastMessage(&message),"Exit-stage deferred fixture did not acquire its real payload");
    std::cout<<"Actual deferred byte copies/cancellation, callback-throw rollback, two buffers, late rejection/reopen, concurrent producer close and rejected UI-owner lifetime PASS; explicit allocator-live exit-stage queue destructor must still exit0 (not gameplay)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
