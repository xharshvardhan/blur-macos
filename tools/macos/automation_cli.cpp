// [Blur2PC] Drive only existing Automation v2 handlers over localhost, without OS input/focus.
#include "automation_client.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace BlurNativeAutomation;
namespace {
unsigned number(const char* value,unsigned maximum){std::size_t end=0;const auto n=std::stoul(value,&end);if(!*value||value[0]=='-'||end!=std::string(value).size()||n>maximum)throw std::invalid_argument("Invalid numeric option");return unsigned(n);}
void output(const std::string& kind,const std::vector<std::string>& fields){std::cout<<"{\"kind\":"<<JsonString(kind)<<",\"fields\":[";for(std::size_t i=0;i<fields.size();++i){if(i)std::cout<<',';std::cout<<JsonString(fields[i]);}std::cout<<"]}\n"<<std::flush;}
}
int main(int argc,char** argv){try{
 unsigned timeout=5000,port=6317;int at=1;
 while(at<argc&&std::string(argv[at]).rfind("--",0)==0){const std::string key=argv[at++];if(key=="--help"){
  std::cout<<"Native original Automation v2 client (localhost only)\n"
   "Usage: blur_mac_automation [--timeout-ms 1..60000] [--port 1..65535] COMMAND [ARGS]\n"
   "Commands: ping | version | build | screen | subscreen | objects | status\n"
   "          lua CODE | debug NAME VALUE | watch MILLISECONDS\n"
   "lua reports dispatch acknowledgement; it does not return a Lua expression value.\n"
   "screen queries the original next-screen handler; watch emits real game notifications.\n"
   "Native keyboard helpers are separate game hooks, not an invented network command.\n";return 0;}
  if(at>=argc)throw std::invalid_argument("Missing option value");if(key=="--timeout-ms")timeout=number(argv[at++],60000);else if(key=="--port")port=number(argv[at++],65535);else throw std::invalid_argument("Unknown option "+key);
 }
 if(at>=argc)throw std::invalid_argument("Command required; use --help");const std::string operation=argv[at++];std::vector<std::string> args;while(at<argc)args.emplace_back(argv[at++]);
 const std::string command=operation=="ping"?"Ping":operation=="version"?"AutomationVersion":operation=="build"?"BuildVersion":operation=="screen"?"CurrentUIScreen":operation=="subscreen"?"CurrentUISubscreen":operation=="objects"?"LevelObjectInfo":"";
 if((!command.empty()||operation=="status")&&!args.empty())throw std::invalid_argument("Query does not take arguments");
 if(command.empty()&&operation!="status"&&operation!="lua"&&operation!="debug"&&operation!="watch")throw std::invalid_argument("Unknown command "+operation);
 if((operation=="lua"&&args.size()!=1)||(operation=="debug"&&args.size()!=2)||(operation=="watch"&&args.size()!=1))throw std::invalid_argument("Wrong argument count");
 unsigned watch=0;if(operation=="watch")watch=number(args[0].c_str(),3600000);
 Client client(timeout,std::uint16_t(port));
 if(!command.empty())output(command,client.Command(command));
 else if(operation=="status"){for(const auto& query:{"BuildVersion","CurrentUIScreen","CurrentUISubscreen"})output(query,client.Command(query));}
 else if(operation=="lua")output("ExecuteLuaCommand-dispatch",client.Command("ExecuteLuaCommand",args));
 else if(operation=="debug")output("DebugValue",client.DebugValue(args[0],args[1]));
 const auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(watch);
 do{const auto now=std::chrono::steady_clock::now();const unsigned remaining=now>=end?0:unsigned(std::chrono::duration_cast<std::chrono::milliseconds>(end-now).count());for(const auto& event:client.Events(remaining)){output(event.kind,event.fields);if(event.kind=="Assert"||event.kind=="STOP")return 2;}}while(std::chrono::steady_clock::now()<end);
 return 0;
}catch(const std::exception& error){std::cerr<<"Automation client: "<<error.what()<<'\n';return 1;}}
