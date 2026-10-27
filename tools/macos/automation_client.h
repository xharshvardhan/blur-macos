// [Blur2PC] Native tool client for the existing Artery Automation v2 service.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace BlurNativeAutomation {
struct Event {std::string kind;std::vector<std::string> fields;};
class Client {
public:
 explicit Client(unsigned timeoutMilliseconds=5000,std::uint16_t port=6317);
 ~Client();Client(const Client&)=delete;Client& operator=(const Client&)=delete;
 std::vector<std::string> Command(const std::string& command,const std::vector<std::string>& arguments={});
 std::vector<std::string> DebugValue(const std::string& name,const std::string& value);
 std::vector<Event> Events(unsigned milliseconds);
private:
 struct State;std::unique_ptr<State> state;
 std::vector<std::string> Request(const std::string&,const std::string&,const std::vector<std::string>&);
 void Close()noexcept;
};
std::string JsonString(const std::string& value);
}
