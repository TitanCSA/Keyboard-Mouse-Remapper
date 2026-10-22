#include "Service.h"
#include "InputEngine.h"
#include "Bridge.h"
#include "SnapshotNotification.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <system_error>
namespace {
Remapper::SnapshotNotification notification;
std::int64_t Now(){return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
std::string Explain(const std::exception& e){
 std::string message=e.what();const auto* sys=dynamic_cast<const std::system_error*>(&e);
 if(!sys)return message;const auto code=sys->code().value();message+=" [Windows "+std::to_string(code)+"]";
 if(code==ERROR_FILE_NOT_FOUND||code==ERROR_SERVICE_DOES_NOT_EXIST)message+=". Use Driver setup to install the Keyboard & Mouse edition, then restart if requested.";
 if(code==ERROR_ACCESS_DENIED)message+=". Administrator rights are required.";
 if(code==IC_ERROR_TRIAL_EXPIRED)message+=". The two-hour per-boot driver demo has expired. Restart Windows.";
 if(code==ERROR_OPERATION_ABORTED)message+=". Capture was revoked. Passthrough is restored; release the emergency chord and explicitly Apply again.";
 if(code==ERROR_BUSY||code==ERROR_SHARING_VIOLATION)message+=". Close other keyboard/mouse capture applications.";
 return message;
}
class Timer final {
 HANDLE timer_=nullptr,wake_=nullptr;PTP_WAIT wait_=nullptr;bool armed_=false;
 static void CALLBACK Fired(PTP_CALLBACK_INSTANCE,void* context,PTP_WAIT,TP_WAIT_RESULT){SetEvent(static_cast<Timer*>(context)->wake_);}
public:
 explicit Timer(HANDLE wake):wake_(wake){
  timer_=CreateWaitableTimerExW(nullptr,nullptr,0x2,TIMER_ALL_ACCESS);
  if(!timer_)timer_=CreateWaitableTimerW(nullptr,FALSE,nullptr);
  if(!timer_)throw std::system_error(static_cast<int>(GetLastError()),std::system_category(),"Create macro timer");
  wait_=CreateThreadpoolWait(Fired,this,nullptr);
  if(!wait_){const auto error=GetLastError();CloseHandle(timer_);throw std::system_error(static_cast<int>(error),std::system_category(),"Create macro wait");}
 }
 void Arm(int ms){
  if(armed_){SetThreadpoolWait(wait_,nullptr,nullptr);CancelWaitableTimer(timer_);WaitForThreadpoolWaitCallbacks(wait_,TRUE);armed_=false;}
  if(ms<=0)return;LARGE_INTEGER due{};due.QuadPart=-static_cast<LONGLONG>(ms)*10000;
  if(!SetWaitableTimer(timer_,&due,0,nullptr,nullptr,FALSE))throw std::system_error(static_cast<int>(GetLastError()),std::system_category(),"Arm macro timer");
  SetThreadpoolWait(wait_,timer_,nullptr);armed_=true;
 }
 ~Timer(){if(wait_){SetThreadpoolWait(wait_,nullptr,nullptr);WaitForThreadpoolWaitCallbacks(wait_,TRUE);CloseThreadpoolWait(wait_);}if(timer_)CloseHandle(timer_);}
};
}
extern "C" void __cdecl GR_SetSnapshotWindow(HWND window,UINT message){notification.Attach(window,message);}
extern "C" BOOL __cdecl GR_AcknowledgeSnapshot(HWND window,ULONG_PTR generation){return notification.Acknowledge(window,generation)?TRUE:FALSE;}
namespace gpr {
struct RemapperService::Impl {
 struct Command {std::shared_ptr<const ScriptProgram> Program;ParameterValues Values{};std::uint64_t Generation=0;bool ParametersOnly=false;};
 Factory factory;std::mutex lifecycle,stateGate,commandGate,cacheGate;std::condition_variable applied;
 Command command;std::uint64_t appliedGeneration=0;std::thread worker;HANDLE wake=nullptr;
 std::atomic<bool> stopping{false},working{false},latched{false};
 std::unique_ptr<InputEngine> engine;ServiceSnapshot cached;std::string failure;double processing=0;
 explicit Impl(Factory f):factory(std::move(f)){wake=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!wake)throw std::system_error(static_cast<int>(GetLastError()),std::system_category(),"Create worker event");}
 ~Impl(){Shutdown();CloseHandle(wake);}
 void Shutdown(){
  std::lock_guard<std::mutex> lock(lifecycle);stopping=true;SetEvent(wake);if(worker.joinable())worker.join();working=false;
 }
 void Start(bool explicitApply){
  std::lock_guard<std::mutex> lock(lifecycle);
  if(working)return;if(latched&&!explicitApply)return;
  if(worker.joinable())worker.join();latched=false;stopping=false;
  {std::lock_guard<std::mutex> state(stateGate);failure.clear();}
  working=true;try{worker=std::thread([this]{Run();});}catch(...){working=false;throw;}
 }
 void Run()noexcept{
  std::unique_ptr<InputTransport> transport;
  try {
   transport=factory?factory():CreateDriverTransport();if(!transport)throw std::runtime_error("Missing input transport.");
   const auto devices=transport->Open();
   {std::lock_guard<std::mutex> lock(stateGate);engine=std::make_unique<InputEngine>([&](const InputPacket& p){transport->Send(p);});engine->SetDevices(devices);}
   notification.Changed();
   Timer timer(wake);std::vector<InputPacket> packets;packets.reserve(512);std::int64_t deadline=0;
   while(!stopping){
    Command next;{std::lock_guard<std::mutex> lock(commandGate);next=command;}
    if(next.Generation!=appliedGeneration){
     {std::lock_guard<std::mutex> lock(stateGate);
      if(next.ParametersOnly)engine->SetParameters(next.Values,Now());else engine->SetProgram(next.Program,next.Values,Now());
      appliedGeneration=next.Generation;deadline=engine->NextWakeMilliseconds()?Now()+engine->NextWakeMilliseconds()*1000ll:0;}
     applied.notify_all();notification.Changed();
    }
    if(stopping)break;
    const auto now=Now();
    if(deadline&&now>=deadline){
     {std::lock_guard<std::mutex> lock(stateGate);engine->Tick(now);deadline=engine->NextWakeMilliseconds()?Now()+engine->NextWakeMilliseconds()*1000ll:0;}
     notification.Changed();
    }
    int waitMs=0;if(deadline)waitMs=static_cast<int>(std::max<std::int64_t>(1,(deadline-Now()+999)/1000));
    timer.Arm(waitMs);transport->Wait(INFINITE,wake);if(stopping)break;
    transport->Read(packets);
    if(!packets.empty()){
     const auto started=Now();
     {std::lock_guard<std::mutex> lock(stateGate);
      for(const auto& p:packets)engine->Process(p,Now());
      deadline=engine->NextWakeMilliseconds()?Now()+engine->NextWakeMilliseconds()*1000ll:0;
      const auto us=static_cast<double>(Now()-started)/static_cast<double>(packets.size());processing=processing?processing*.95+us*.05:us;}
     notification.Changed();
    }
   }
   {std::lock_guard<std::mutex> lock(stateGate);if(engine&&engine->Active())engine->SetProgram(nullptr,{},Now());}
  }catch(const std::exception& error){
   std::lock_guard<std::mutex> lock(stateGate);failure=Explain(error);latched=true;
  }catch(...){std::lock_guard<std::mutex> lock(stateGate);failure="Unexpected capture worker failure. Passthrough has been restored.";latched=true;}
  // Close the context before publishing that passthrough is restored.
  transport.reset();working=false;applied.notify_all();notification.Changed();
 }
 std::uint64_t Submit(std::shared_ptr<const ScriptProgram> program,const ParameterValues& values,bool parameters){
  if(program)program->Interface().Validate(values);
  std::lock_guard<std::mutex> lock(commandGate);command.Program=std::move(program);command.Values=values;command.ParametersOnly=parameters;++command.Generation;SetEvent(wake);return command.Generation;
 }
 void WaitApplied(std::uint64_t generation){
  std::unique_lock<std::mutex> lock(stateGate);
  if(!applied.wait_for(lock,std::chrono::seconds(5),[&]{return appliedGeneration>=generation||!working;}))throw std::runtime_error("The capture worker did not apply the configuration within five seconds.");
  if(!failure.empty())throw std::runtime_error(failure);
 }
};
RemapperService::RemapperService(Factory factory):impl_(std::make_unique<Impl>(std::move(factory))){}
RemapperService::~RemapperService()=default;
void RemapperService::Inject(std::shared_ptr<const ScriptProgram> p){if(!p)throw std::invalid_argument("A compiled script is required.");const auto values=p->Interface().Defaults();Inject(std::move(p),values);}
void RemapperService::Inject(std::shared_ptr<const ScriptProgram> p,const ParameterValues& values){if(!p)throw std::invalid_argument("A compiled script is required.");const auto gen=impl_->Submit(std::move(p),values,false);impl_->Start(true);impl_->WaitApplied(gen);}
bool RemapperService::TryUpdate(std::shared_ptr<const ScriptProgram> p){if(!p)throw std::invalid_argument("A compiled script is required.");const auto values=p->Interface().Defaults();return TryUpdate(std::move(p),values);}
bool RemapperService::TryUpdate(std::shared_ptr<const ScriptProgram> p,const ParameterValues& values){if(!IsRunning())return false;Inject(std::move(p),values);return true;}
bool RemapperService::SetParameters(const std::shared_ptr<const ScriptProgram>& p,const ParameterValues& values){
 if(!p||!IsRunning())return false;
 {std::lock_guard<std::mutex> lock(impl_->commandGate);if(impl_->command.Program!=p)return false;}
 impl_->Submit(p,values,true);return true;
}
void RemapperService::StartMonitoring(){impl_->Start(false);}
void RemapperService::StopMonitoring(){impl_->Shutdown();}
void RemapperService::Stop(){if(!impl_->working){impl_->Submit(nullptr,{},false);return;}const auto gen=impl_->Submit(nullptr,{},false);impl_->WaitApplied(gen);}
bool RemapperService::IsRunning(){return GetSnapshot().Running;}
ServiceSnapshot RemapperService::GetSnapshot(){
 std::unique_lock<std::mutex> lock(impl_->stateGate,std::try_to_lock);
 if(!lock){std::lock_guard<std::mutex> cache(impl_->cacheGate);return impl_->cached;}
 auto snapshot=impl_->engine?impl_->engine->Snapshot():ServiceSnapshot{};
 snapshot.Monitoring=impl_->working&&!impl_->latched;snapshot.Running=snapshot.Monitoring&&snapshot.Running;
 snapshot.ProcessingMicroseconds=impl_->processing;
 if(!snapshot.Monitoring){snapshot.MonitorError=impl_->failure;snapshot.MonitorStatus=impl_->latched?"Capture suspended - apply explicitly to resume":"Connecting to the Keyboard & Mouse driver";snapshot.MonitorDropped=0;}
 {std::lock_guard<std::mutex> cache(impl_->cacheGate);impl_->cached=snapshot;}return snapshot;
}
}
