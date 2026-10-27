// [Blur2PC] Use original transport/framing/serializers; no replacement socket protocol.
#include <core/Core.h>
#include <system/artery/Artery.h>
#include "automation_client.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>
namespace BlurNativeAutomation {
namespace {std::atomic<bool> owner{false};constexpr std::size_t maxText=1024*1024;}
struct Client::State {
 ArteryConnection connection;std::mutex mutex,requestMutex;std::condition_variable changed;
 unsigned timeout;ArteryU32 nextId=1,pendingId=0;bool hello=false,failed=false,started=false,ownsService=false,callbackScheduled=false,callbackDone=false;
 std::atomic<bool> stopping{false};std::string error;std::optional<std::vector<std::string>> response;std::deque<Event> events;
 explicit State(unsigned timeout):timeout(timeout){}
 void Failure(const std::string& text){std::lock_guard<std::mutex> guard(mutex);if(!failed)error=text;failed=true;changed.notify_all();}
 void Message(const ArteryMessage& message){
  ArteryMessageReader reader(message);const char* tag=nullptr;
  if(!reader.Read(tag))throw std::runtime_error("Malformed Automation message tag");
  std::lock_guard<std::mutex> guard(mutex);
  if(std::string(tag)=="Automation says Hello")hello=true;
  else if(std::string(tag)=="Automation"){
   ArteryU32 id=0,count=0;if(!reader.Read(id)||!reader.Read(count)||count>1024)throw std::runtime_error("Malformed Automation reply header");
   if(!pendingId||id!=pendingId||response)throw std::runtime_error("Automation reply has unexpected/duplicate command ID");
   std::vector<std::string> lines;std::size_t total=0;
   for(ArteryU32 i=0;i<count;++i){const char* text=nullptr;if(!reader.Read(text))throw std::runtime_error("Truncated Automation reply");lines.emplace_back(text);total+=lines.back().size();if(total>4*maxText)throw std::runtime_error("Automation reply exceeds tool limit");}
   response=std::move(lines);
  }else{
   Event event;event.kind=tag;
   if(event.kind=="Notify"||event.kind=="Problem"||event.kind=="STOP"){
    const char* text=nullptr;if(!reader.Read(text))throw std::runtime_error("Malformed Automation event");event.fields.emplace_back(text);
   }else if(event.kind=="Assert"){
    const char *condition=nullptr,*file=nullptr,*function=nullptr,*text=nullptr;ArteryI32 line=0;
    if(!reader.Read(condition)||!reader.Read(file)||!reader.Read(line)||!reader.Read(function)||!reader.Read(text))throw std::runtime_error("Malformed Automation assert event");
    event.fields={condition,file,std::to_string(line),function,text};
   }else throw std::runtime_error("Unexpected Automation service message: "+event.kind);
   if(event.kind=="Assert"){failed=true;error="Game assert: "+event.fields[0]+" at "+event.fields[1]+":"+event.fields[2]+" ("+event.fields[3]+") "+event.fields[4];}
   if(event.kind=="STOP"){failed=true;error="Game stopped automation: "+event.fields[0];}
   if(events.size()>=4096)throw std::runtime_error("Automation event queue exceeded tool limit");events.push_back(std::move(event));
  }
  ArteryU8 extra;if(reader.Read(extra))throw std::runtime_error("Trailing bytes in Automation message");changed.notify_all();
 }
 static void Connected(ArteryConnection connection,void* context){
  State& self=*static_cast<State*>(context);
  struct Completion {State& self;~Completion(){std::lock_guard<std::mutex> lock(self.mutex);self.callbackDone=true;self.changed.notify_all();}} completion{self};
  try{
   if(!connection.IsConnected())throw std::runtime_error("Cannot connect to localhost Automation service");
   while(!self.stopping&&connection.IsConnected()){
    ArteryMessage message;if(connection.Receive(message,100)&&message)self.Message(message);
   }
   if(!self.stopping)self.Failure("Automation service disconnected");
  }catch(const std::exception& error){self.Failure(error.what());connection.Close();}
 }
};
Client::Client(unsigned timeout,std::uint16_t port):state(new State(timeout)){
 if(!timeout||timeout>60000||!port)throw std::invalid_argument("Timeout must be1..60000ms and port1..65535");
 if(owner.exchange(true))throw std::logic_error("Only one Automation client may own Artery in this process");state->ownsService=true;
 try{
  if(!ArteryStartup(false,"Blur native automation client"))throw std::runtime_error("Cannot start original Artery client transport");state->started=true;
  state->callbackScheduled=true;
  if(!ArteryConnect(state->connection,ArterySockAddr(INADDR_LOOPBACK,port),"Automation",State::Connected,state.get())){state->callbackScheduled=false;throw std::runtime_error("Cannot allocate original Artery connection");}
  {std::unique_lock<std::mutex> lock(state->mutex);if(!state->changed.wait_for(lock,std::chrono::milliseconds(timeout),[&]{return state->hello||state->failed;}))throw std::runtime_error("Timed out waiting for original Automation greeting");if(state->failed)throw std::runtime_error(state->error);}
  const auto version=Command("AutomationVersion");if(version.size()!=1||version[0]!="2")throw std::runtime_error("Game does not provide Automation protocol version2");
 }catch(...){Close();throw;}
}
Client::~Client(){Close();}
void Client::Close()noexcept{
 if(!state)return;state->stopping=true;
 if(state->started){
  state->connection.Close();
  // [Blur2PC] Artery is reference-counted: another in-process service owner can
  // survive ArteryCleanup. Retain the callback context until this peer returns.
  if(state->callbackScheduled){std::unique_lock<std::mutex> lock(state->mutex);state->changed.wait(lock,[&]{return state->callbackDone;});}
  ArteryCleanup();state->started=false;
 }
 if(state->ownsService){state->ownsService=false;owner=false;}
}
std::vector<std::string> Client::Request(const std::string& envelope,const std::string& command,const std::vector<std::string>& arguments){
 std::lock_guard<std::mutex> request(state->requestMutex);ArteryMessageBuilder builder;ArteryU32 id;
 {std::lock_guard<std::mutex> lock(state->mutex);if(state->failed)throw std::runtime_error(state->error);if(!state->nextId)throw std::overflow_error("Automation command IDs exhausted");id=state->nextId++;state->pendingId=id;state->response.reset();}
 if(!builder.Write(envelope.c_str())||!builder.Write(id))throw std::runtime_error("Cannot serialize original Automation envelope");
 if(envelope=="Automation"&&!builder.Write(command.c_str()))throw std::runtime_error("Cannot serialize Automation command");
 for(const auto& argument:arguments){if(argument.size()>maxText||argument.find('\0')!=std::string::npos||!builder.Write(argument.c_str()))throw std::invalid_argument("Invalid/oversized Automation string argument");}
 ArteryMessage message;if(!builder.Build(message)||!state->connection.Send(message))throw std::runtime_error("Original Automation send failed");
 std::unique_lock<std::mutex> lock(state->mutex);
 if(!state->changed.wait_for(lock,std::chrono::milliseconds(state->timeout),[&]{return bool(state->response)||state->failed;})){state->failed=true;state->error="Timed out waiting for Automation command reply (game may be loading or not updating)";throw std::runtime_error(state->error);}
 if(state->failed)throw std::runtime_error(state->error);
 auto result=std::move(*state->response);state->response.reset();state->pendingId=0;
 if(result.size()==1&&result[0]=="-UNKNOWN-")throw std::runtime_error("Game has no handler for "+command);return result;
}
std::vector<std::string> Client::Command(const std::string& command,const std::vector<std::string>& arguments){
 const bool query=command=="AutomationVersion"||command=="BuildVersion"||command=="Ping"||command=="CurrentUIScreen"||command=="CurrentUISubscreen"||command=="LevelObjectInfo";
 if((query&&!arguments.empty())||(!query&&(command!="ExecuteLuaCommand"||arguments.size()!=1)))throw std::invalid_argument("Unsupported command/argument shape; use existing read-only queries or ExecuteLuaCommand");
 // [Blur2PC] The original UI C_UiLuaCommandMessage owns128 bytes including NUL.
 // Reject oversized commands before dispatch; a transport ACK is not Lua execution.
 if(command=="ExecuteLuaCommand" && arguments[0].size()>=128)
     throw std::invalid_argument("Lua command exceeds original UI message capacity (127 bytes)");
 return Request("Automation",command,arguments);
}
std::vector<std::string> Client::DebugValue(const std::string& name,const std::string& value){
 if(name.empty())throw std::invalid_argument("Debug variable name is empty");auto result=Request("DebugValue","DebugValue",{name,value});
 if(result.size()!=1||result[0]!="0")throw std::runtime_error("Game did not find/set debug variable "+name);return result;
}
std::vector<Event> Client::Events(unsigned milliseconds){
 std::unique_lock<std::mutex> lock(state->mutex);if(state->events.empty()&&milliseconds)state->changed.wait_for(lock,std::chrono::milliseconds(milliseconds),[&]{return !state->events.empty()||state->failed;});
 if(state->failed&&state->events.empty())throw std::runtime_error(state->error);std::vector<Event> result;while(!state->events.empty()){result.push_back(std::move(state->events.front()));state->events.pop_front();}return result;
}
std::string JsonString(const std::string& value){
 const char hex[]="0123456789abcdef";std::string out="\"";
 for(unsigned char c:value){if(c=='"'||c=='\\'){out+='\\';out+=char(c);}else if(c<32){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=char(c);}return out+'"';
}
}
