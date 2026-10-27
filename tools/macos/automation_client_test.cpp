// [Blur2PC] Exercise the actual original Automation.cpp server and Artery client.
// No substitute game/UI handlers are registered; absent UI commands must fail.
#include <core/Core.h>
#include <core/containers.h>
#include <system/automation/Automation.h>
#include <system/debug/variables.h>
#include "automation_client.h"
#include <atomic>
#include <chrono>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <unistd.h>
namespace {
Debug::Int fixtureValue("MAC034.FixtureInt",7);
void require(bool value,const char* error){if(!value)throw std::runtime_error(error);}
}
int main(){alarm(20);try{
 require(ArteryStartupEx(true,"Original Automation fixture",8),"Automation fixture needs free localhost6317; it never stops another game");
 struct Cleanup{bool active=true;~Cleanup(){if(active)ArteryCleanup();}}cleanup;
 Automation::InitialiseNetworkConnection();
 std::atomic<bool> finished{false},notify{false};std::exception_ptr failure;
 std::thread client([&]{try{
  BlurNativeAutomation::Client connection(2500);
  require(connection.Command("Ping")==std::vector<std::string>{"Ping"},"Original Ping reply mismatch");
  require(connection.Command("AutomationVersion")==std::vector<std::string>{"2"},"Original version mismatch");
  require(!connection.Command("BuildVersion").empty(),"Original build query returned no lines");
  require(connection.DebugValue("MAC034.FixtureInt","37")==std::vector<std::string>{"0"},"Original DebugValue did not acknowledge");
  bool missingVariable=false;try{connection.DebugValue("MAC034.DoesNotExist","1");}catch(const std::runtime_error&){missingVariable=true;}
  require(missingVariable,"Missing debug variable reported success");
  bool missingUI=false;try{connection.Command("CurrentUIScreen");}catch(const std::runtime_error&){missingUI=true;}
  require(missingUI,"Absent actual UI callback reported a fabricated screen");
  bool oversized=false;try{connection.Command("ExecuteLuaCommand",{std::string(128,'x')});}catch(const std::invalid_argument&){oversized=true;}
  require(oversized,"Oversized UI command was dispatched into the fixed original message");
  bool boundaryReachedServer=false;try{connection.Command("ExecuteLuaCommand",{std::string(127,'x')});}catch(const std::runtime_error&){boundaryReachedServer=true;}
  require(boundaryReachedServer,"127-byte command did not reach the original server/absent UI callback");
  bool disallowed=false;try{connection.Command("StartEvent");}catch(const std::invalid_argument&){disallowed=true;}
  require(disallowed,"Client exposed gameplay setup/bypass command");
  notify=true;const auto events=connection.Events(2500);
  require(events.size()==1&&events[0].kind=="Notify"&&events[0].fields==std::vector<std::string>{"MAC034 actual server event"},"Original Notify event lost/altered");
  require(BlurNativeAutomation::JsonString("quote\"\\\nλ")=="\"quote\\\"\\\\\\u000aλ\"","JSON event escaping changed bytes");
 }catch(...){failure=std::current_exception();}finished=true;});
 try{
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
  while(!finished){Automation::NetworkUpdate();if(notify.exchange(false))Automation::Notify("MAC034 actual server event");require(std::chrono::steady_clock::now()<deadline,"Actual Automation fixture timed out");std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 }catch(...){client.join();throw;}
 client.join();if(failure)std::rethrow_exception(failure);require(int(fixtureValue)==37,"Actual server debug variable did not change");
 ArteryCleanup();cleanup.active=false;
 std::cout<<"Actual original Automation v2 greeting/version/IDs/Ping/DebugValue/Notify/client cleanup and absent-UI failures PASS\n"<<std::flush;return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
