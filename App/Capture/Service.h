#pragma once
#include "../Core/Script.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>
namespace gpr {
class InputTransport;
struct ServiceDevice {
 std::uint32_t Key=0,Family=0;
 std::string Name,Token,Diagnostic;
 std::array<int,ControlCount> Values{},OutputValues{};
 std::array<bool,ControlCount> Available{},InputAvailable{},OutputAvailable{};
 bool PhysicalValid=false,OutputValid=false,Telemetry=false;
 std::uint64_t InputSequence=0,OutputSequence=0,InputTimestamp=0,OutputTimestamp=0;
 std::uint32_t InputStatus=0,OutputStatus=0;
 std::vector<std::uint8_t> InputRaw,OutputRaw;
};
struct ServiceSnapshot {
 std::string Status="Ready",Error,MonitorStatus,MonitorError;
 bool Running=false,Monitoring=false;
 std::uint64_t MonitorFrames=0,MonitorDropped=0,MonitorTimestampFrequency=0,Received=0,Forwarded=0,Changed=0,TimerOutputs=0,Faults=0;
 double ProcessingMicroseconds=0;
 std::vector<ServiceDevice> Devices;
};
class RemapperService final {
public:
 using Factory=std::function<std::unique_ptr<InputTransport>()>;
 explicit RemapperService(Factory factory={});
 ~RemapperService();
 RemapperService(const RemapperService&)=delete;RemapperService& operator=(const RemapperService&)=delete;
 void Inject(std::shared_ptr<const ScriptProgram> program);
 void Inject(std::shared_ptr<const ScriptProgram> program,const ParameterValues& values);
 bool TryUpdate(std::shared_ptr<const ScriptProgram> program);
 bool TryUpdate(std::shared_ptr<const ScriptProgram> program,const ParameterValues& values);
 bool SetParameters(const std::shared_ptr<const ScriptProgram>& program,const ParameterValues& values);
 void StartMonitoring();void StopMonitoring();void Stop();bool IsRunning();ServiceSnapshot GetSnapshot();
private:struct Impl;std::unique_ptr<Impl> impl_;
};
int RunLiveTest(const std::wstring& reportPath,int seconds);
}