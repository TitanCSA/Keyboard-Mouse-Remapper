#include "Test.h"
#include "../Capture/InputEngine.h"
#include <mutex>
using namespace gpr;
struct State {
 std::mutex Gate;std::vector<InputPacket> Pending,Sent;
 HANDLE Ready=CreateEventW(nullptr,FALSE,FALSE,nullptr);unsigned Opens=0,Closes=0;bool Fail=false;
 ~State(){CloseHandle(Ready);}
 void Push(InputPacket packet){std::lock_guard<std::mutex> lock(Gate);Pending.push_back(std::move(packet));SetEvent(Ready);}
};
class Fake final:public InputTransport {
 std::shared_ptr<State> state_;
public:
 explicit Fake(std::shared_ptr<State> state):state_(std::move(state)){}
 ~Fake(){std::lock_guard<std::mutex> lock(state_->Gate);++state_->Closes;}
 std::vector<InputDevice> Open()override{{std::lock_guard<std::mutex> lock(state_->Gate);++state_->Opens;}return Devices();}
 std::vector<InputDevice> Devices()override{return {{1,1,"Keyboard","fixture:k"},{2,2,"Mouse","fixture:m"}};}
 void Wait(DWORD timeout,HANDLE wake)override{HANDLE events[]={wake,state_->Ready};WaitForMultipleObjects(2,events,FALSE,timeout);}
 void Read(std::vector<InputPacket>& out)override{std::lock_guard<std::mutex> lock(state_->Gate);out.swap(state_->Pending);state_->Pending.clear();}
 void Send(const InputPacket& p)override{std::lock_guard<std::mutex> lock(state_->Gate);if(state_->Fail)throw std::system_error(ERROR_OPERATION_ABORTED,std::system_category(),"Fixture revocation");state_->Sent.push_back(p);}
};
int main(){try{
 const auto state=std::make_shared<State>();RemapperService service([state]{return std::make_unique<Fake>(state);});
 service.StartMonitoring();Until([&]{return service.GetSnapshot().Devices.size()==2;});
 Check(service.GetSnapshot().Monitoring&&!service.GetSnapshot().Running,"Automatic observation starts in passthrough");
 Interceptor::Keyboard_Input key{};key.device=1;key.stroke.make_code=30;state->Push(key);
 Until([&]{return service.GetSnapshot().Forwarded==1;});
 auto program=ScriptCompiler::Compile("ui {toggle enabled(\"Enabled\",true);} main {if(enabled)remap(KEY_A,KEY_B);}");
 service.Inject(program);Check(service.IsRunning(),"Applying starts the script on the existing worker");
 auto values=program->Interface().Defaults();values[0]=0;Check(service.SetParameters(program,values),"Live parameter update accepted");
 Until([&]{const auto s=service.GetSnapshot();return !s.Devices.empty()&&s.Devices[0].OutputValues[30]==100;});
 service.Stop();Check(!service.IsRunning()&&service.GetSnapshot().Monitoring,"Stop removes the script while preserving observation");
 key.stroke.flags=Interceptor::KeyUp;state->Push(key);Until([&]{return service.GetSnapshot().Received>=2;});
 service.Inject(ScriptCompiler::Compile("main {remap(KEY_A,KEY_B);}"));
 {std::lock_guard<std::mutex> lock(state->Gate);state->Fail=true;}key.stroke.flags=0;state->Push(key);
 Until([&]{return !service.GetSnapshot().MonitorError.empty();});
 Check(!service.GetSnapshot().Monitoring,"Revocation ends capture");
 unsigned opens=0;{std::lock_guard<std::mutex> lock(state->Gate);opens=state->Opens;}
 service.StartMonitoring();std::this_thread::sleep_for(std::chrono::milliseconds(20));
 {std::lock_guard<std::mutex> lock(state->Gate);Check(state->Opens==opens,"Emergency revocation must not auto-recapture");state->Fail=false;}
 service.Inject(ScriptCompiler::Compile("main {}"));Check(service.IsRunning(),"Explicit apply resumes after revocation");
 service.StopMonitoring();Check(!service.GetSnapshot().Monitoring,"Worker shutdown completes before destruction");
 {std::lock_guard<std::mutex> lock(state->Gate);Check(state->Opens==state->Closes,"Every transport is closed");}
 std::cout<<checks<<" actual-worker lifecycle checks passed without driver access\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
