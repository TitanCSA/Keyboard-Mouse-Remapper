#include "InputEngine.h"
#include <algorithm>
#include <stdexcept>
#include <system_error>
namespace gpr {
namespace {
std::string Utf8(const std::uint16_t* text){
 const auto source=reinterpret_cast<const wchar_t*>(text);const auto count=WideCharToMultiByte(CP_UTF8,0,source,-1,nullptr,0,nullptr,nullptr);
 if(count<=1)return {};std::string out(static_cast<std::size_t>(count),0);WideCharToMultiByte(CP_UTF8,0,source,-1,out.data(),count,nullptr,nullptr);out.pop_back();return out;
}
class DriverTransport final:public InputTransport {
 Interceptor::Context_Result capture_;
 std::vector<Interceptor::Keyboard_Input> keys_;
 std::vector<Interceptor::Mouse_Input> mice_;
 std::uint8_t ready_=0;
public:
 std::vector<InputDevice> Open()override {
  const auto caps=Interceptor::Get_Capabilities();
  if(caps.abi_version!=IC_VERSION||caps.input_mask!=IC_BOTH||caps.controller_mask)
   throw std::runtime_error("Install this application's Interceptor Keyboard & Mouse edition; another driver edition is loaded.");
  capture_=Interceptor::Create_Context();keys_.reserve(256);mice_.reserve(256);return Devices();
 }
 std::vector<InputDevice> Devices()override{
  std::vector<InputDevice> result;
  for(const auto& d:Interceptor::Get_Devices(*capture_)){
   if(d.type!=1&&d.type!=2)continue;
   const auto name=Utf8(d.name);
   result.push_back({d.id,d.type,name.empty()?(d.type==1?"Keyboard ":"Mouse ")+std::to_string(d.id):name,
    "capture:"+std::to_string(capture_.generation)+":"+std::to_string(d.type)+":"+std::to_string(d.id)});
  }return result;
 }
 void Wait(DWORD timeout,HANDLE wake)override{ready_=Interceptor::Wait_For_Input(*capture_,timeout,wake);}
 void Read(std::vector<InputPacket>& packets)override{
  packets.clear();
  if(ready_&Interceptor::Keyboard)Interceptor::Get_Keyboard_Input(*capture_,keys_,256);else keys_.clear();
  if(ready_&Interceptor::Mouse)Interceptor::Get_Mouse_Input(*capture_,mice_,256);else mice_.clear();
  for(const auto& key:keys_)packets.emplace_back(key);
  for(const auto& mouse:mice_)packets.emplace_back(mouse);
  std::stable_sort(packets.begin(),packets.end(),[](const auto& a,const auto& b){const auto sequence=[](const auto& p){return std::visit([](const auto& v){return v.sequence;},p);};return sequence(a)<sequence(b);});
 }
 void Send(const InputPacket& packet)override{
  std::visit([&](const auto& input){
   const auto sent=Interceptor::Send_Input(*capture_,input);
   if(sent.error||sent.accepted!=1)throw std::system_error(static_cast<int>(sent.error?sent.error:ERROR_WRITE_FAULT),std::system_category(),"Driver output acceptance");
  },packet);
 }
};
}
std::unique_ptr<InputTransport> CreateDriverTransport(){return std::make_unique<DriverTransport>();}
}
