#pragma once
#ifndef IC_SDK_INTERNAL
#define IC_SDK_INTERNAL
#endif
#include "Interceptor.h"
#include "Service.h"
#include <unordered_map>
#include <variant>
namespace gpr {
using InputPacket=std::variant<Interceptor::Keyboard_Input,Interceptor::Mouse_Input>;
struct InputDevice {std::uint32_t Id=0,Family=0;std::string Name,Token;};
class InputTransport {
public:
 virtual ~InputTransport()=default;
 virtual std::vector<InputDevice> Open()=0;
 virtual std::vector<InputDevice> Devices()=0;
 virtual void Wait(DWORD timeout,HANDLE wake)=0;
 virtual void Read(std::vector<InputPacket>& packets)=0;
 virtual void Send(const InputPacket& packet)=0;
};
std::unique_ptr<InputTransport> CreateDriverTransport();
// Single-owner state machine. The sink returns only after driver acceptance.
class InputEngine {
 struct DeviceState {
  InputDevice Info;ServiceDevice View;
  std::array<bool,768> Held{};
  std::array<std::vector<int>,768> Repeats;
  std::array<bool,5> Buttons{};
  bool Relative=true;
 };
 std::unordered_map<std::uint64_t,DeviceState> devices_;
 std::array<int,ControlCount> physical_{},delivered_{};
 std::array<unsigned short,ControlCount> contributors_{};
 std::array<std::uint32_t,ControlCount> route_{};
 std::shared_ptr<const ScriptProgram> program_;
 std::unique_ptr<ScriptInstance> script_;
 std::function<void(const InputPacket&)> sink_;
 std::int64_t lastTime_=0;std::uint64_t outputSequence_=0;
 int nextWake_=0;
 DeviceState& Device(unsigned id,unsigned family);
 unsigned Destination(unsigned family,bool absolute=false)const;
 void Aggregate();
 void Evaluate(std::int64_t now,const InputPacket* seed=nullptr);
 void Motion(const std::array<int,ControlCount>& desired,const InputPacket* seed=nullptr);
 void Reconcile(const std::array<int,ControlCount>& desired,const InputPacket* seed=nullptr);
 void Send(InputPacket packet);
 void ZeroRelative();
public:
 explicit InputEngine(std::function<void(const InputPacket&)> sink);
 void SetDevices(const std::vector<InputDevice>& devices);
 void SetProgram(std::shared_ptr<const ScriptProgram> program,const ParameterValues& values,std::int64_t now);
 void SetParameters(const ParameterValues& values,std::int64_t now);
 void Process(const InputPacket& packet,std::int64_t now);
 void Tick(std::int64_t now);
 int NextWakeMilliseconds()const noexcept{return nextWake_;}
 bool Active()const noexcept{return script_!=nullptr;}
 ServiceSnapshot Snapshot()const;
 std::uint64_t Received=0,Forwarded=0,Changed=0,TimerOutputs=0,Faults=0;
 std::string Error;
};
}
