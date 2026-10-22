#include "InputEngine.h"
#include <algorithm>
#include <cstring>
#include <climits>
namespace gpr {
namespace {
unsigned Key(const Interceptor::Keyboard_Input& k){
 if(k.stroke.make_code>255 || (k.stroke.flags&6)==6)return 768;
 return k.stroke.make_code+((k.stroke.flags&Interceptor::KeyE1)?512u:(k.stroke.flags&Interceptor::KeyE0)?256u:0u);
}
std::uint64_t Identity(unsigned family,unsigned id){return (static_cast<std::uint64_t>(family)<<32)|id;}
std::uint64_t Timestamp(){LARGE_INTEGER t{};QueryPerformanceCounter(&t);return static_cast<std::uint64_t>(t.QuadPart);}
bool Modifier(int key){return key==29||key==42||key==54||key==56||key==0x11d||key==0x138||key==0x15b||key==0x15c;}
template<class T> void Raw(std::vector<std::uint8_t>& bytes,const T& stroke){bytes.resize(sizeof(stroke));std::memcpy(bytes.data(),&stroke,sizeof(stroke));}
}
InputEngine::InputEngine(std::function<void(const InputPacket&)> sink):sink_(std::move(sink)){if(!sink_)throw std::invalid_argument("An accepted-output sink is required.");}
InputEngine::DeviceState& InputEngine::Device(unsigned id,unsigned family){
 const auto key=Identity(family,id);auto found=devices_.find(key);
 if(found==devices_.end()){
  if(devices_.size()>=IC_MAX_DEVICES)throw std::runtime_error("The driver device bound was exceeded.");
  DeviceState d;d.Info={id,family,(family==1?"Keyboard ":"Mouse ")+std::to_string(id),"session:"+std::to_string(family)+":"+std::to_string(id)};
  d.View.Key=id;d.View.Family=family;d.View.Name=d.Info.Name;d.View.Token=d.Info.Token;d.View.Telemetry=true;
  d.View.Diagnostic="IN: original captured stroke. OUT: successfully accepted driver send; not an application-consumption measurement.";
  found=devices_.emplace(key,std::move(d)).first;
 }
 return found->second;
}
void InputEngine::SetDevices(const std::vector<InputDevice>& devices){
 for(const auto& d:devices){auto& state=Device(d.Id,d.Family);state.Info=d;state.View.Name=d.Name;state.View.Token=d.Token;}
}
unsigned InputEngine::Destination(unsigned family,bool absolute)const{
 unsigned exact=UINT_MAX,fallback=UINT_MAX;
 for(const auto& pair:devices_){const auto& d=pair.second;if(d.Info.Family!=family)continue;
  fallback=std::min(fallback,d.Info.Id);if(family==1||d.Relative!=absolute)exact=std::min(exact,d.Info.Id);
 }
 return exact!=UINT_MAX?exact:fallback!=UINT_MAX?fallback:0;
}
void InputEngine::ZeroRelative(){for(int i=MouseX;i<=MouseHWheel;++i)physical_[i]=0;}
void InputEngine::Aggregate(){
 // Physical buttons are maintained incrementally, preserving independent
 // contributors while avoiding a full keyboard scan on every mouse report.
}
void InputEngine::Send(InputPacket packet){
 sink_(packet);++Forwarded;++outputSequence_;
 std::visit([&](const auto& value){
  using T=std::decay_t<decltype(value)>;constexpr unsigned family=std::is_same_v<T,Interceptor::Keyboard_Input>?1u:2u;
  auto& d=Device(value.device,family);auto& view=d.View;
  view.OutputValid=true;view.OutputSequence=outputSequence_;view.OutputTimestamp=Timestamp();Raw(view.OutputRaw,value.stroke);
  if constexpr(family==1){const auto key=Key(value);if(key<768){const int pressed=(value.stroke.flags&Interceptor::KeyUp)?0:100;view.OutputValues[key]=pressed;view.OutputAvailable[key]=view.Available[key]=true;delivered_[key]=pressed;if(pressed)route_[key]=value.device;}}
  else{
   for(int i=0;i<5;++i){const auto down=static_cast<unsigned short>(1u<<(2*i));if(value.stroke.button_flags&(down|(down<<1))){const int pressed=(value.stroke.button_flags&down)?100:0;const int key=MouseLeft+i;view.OutputValues[key]=delivered_[key]=pressed;view.OutputAvailable[key]=view.Available[key]=true;if(pressed)route_[key]=value.device;}}
   const bool absolute=(value.stroke.flags&Interceptor::MouseAbsolute)!=0;const int x=absolute?AbsoluteX:MouseX,y=absolute?AbsoluteY:MouseY;
   view.OutputValues[x]=value.stroke.x;view.OutputValues[y]=value.stroke.y;view.OutputAvailable[x]=view.OutputAvailable[y]=view.Available[x]=view.Available[y]=true;
   if(absolute){delivered_[x]=value.stroke.x;delivered_[y]=value.stroke.y;}
   for(const int slot:{MouseWheel,MouseHWheel}){const auto bit=slot==MouseWheel?Interceptor::Wheel:Interceptor::HorizontalWheel;view.OutputValues[slot]=(value.stroke.button_flags&bit)?static_cast<short>(value.stroke.button_data):0;if(value.stroke.button_flags&bit)view.OutputAvailable[slot]=view.Available[slot]=true;}
  }
 },packet);
}
void InputEngine::Reconcile(const std::array<int,ControlCount>& desired,const InputPacket* seed){
 const auto* keySeed=seed?std::get_if<Interceptor::Keyboard_Input>(seed):nullptr;
 const auto* mouseSeed=seed?std::get_if<Interceptor::Mouse_Input>(seed):nullptr;
 for(int i=0;i<768;++i){
  const int target=desired[i]?100:0;if(target==delivered_[i])continue;
  const unsigned id=!target&&route_[i]?route_[i]:keySeed?keySeed->device:Destination(1);
  if(!id)continue;
  Interceptor::Keyboard_Input out{};out.device=id;if(keySeed){out.stroke.unit_id=keySeed->stroke.unit_id;out.stroke.extra_information=keySeed->stroke.extra_information;}
  out.stroke.make_code=static_cast<unsigned short>(i%256);out.stroke.flags=static_cast<unsigned short>((i>=512?Interceptor::KeyE1:i>=256?Interceptor::KeyE0:0)|(target?0:Interceptor::KeyUp));
  Send(out);
 }
 for(int i=0;i<5;++i){
  const int slot=MouseLeft+i,target=desired[slot]?100:0;if(target==delivered_[slot])continue;
  const unsigned id=!target&&route_[slot]?route_[slot]:mouseSeed?mouseSeed->device:Destination(2);
  if(!id)continue;
  Interceptor::Mouse_Input out{};out.device=id;if(mouseSeed){out.stroke.unit_id=mouseSeed->stroke.unit_id;out.stroke.extra_information=mouseSeed->stroke.extra_information;}
  out.stroke.button_flags=static_cast<unsigned short>(1u<<(2*i+(target?0:1)));Send(out);
 }
}
void InputEngine::Evaluate(std::int64_t now,const InputPacket* seed){
 ControlFrame frame;frame.Physical=physical_;frame.Values=physical_;frame.DeviceKey=1;frame.Family=0;
 const bool keyboard=Destination(1)!=0;bool relative=false,absolute=false;
 for(const auto& pair:devices_)if(pair.second.Info.Family==2){relative|=pair.second.Relative;absolute|=!pair.second.Relative;}
 for(int i=0;i<768;++i)frame.Available[i]=keyboard;
 for(int i=MouseLeft;i<MouseX;++i)frame.Available[i]=Destination(2)!=0;
 for(int i=MouseX;i<=MouseHWheel;++i)frame.Available[i]=relative;
 frame.Available[AbsoluteX]=frame.Available[AbsoluteY]=absolute;
 if(script_){
  try{
   script_->Execute(frame,now,static_cast<int>(std::clamp<std::int64_t>((now-lastTime_)/1000,0,INT_MAX)));
   nextWake_=script_->NextWakeMilliseconds();
  }catch(const std::exception& e){Error=e.what();++Faults;script_.reset();program_.reset();frame.Values=physical_;nextWake_=0;}
 }
 lastTime_=now;
 if(frame.Values!=physical_)++Changed;
 Reconcile(frame.Values,seed);
 // Relative movement is an impulse, not a held gamepad axis. It is sent only
 // by Process or by an explicit macro deadline, then cleared before execution.
 physical_=frame.Values;
}
void InputEngine::SetProgram(std::shared_ptr<const ScriptProgram> program,const ParameterValues& values,std::int64_t now){
 ZeroRelative();Aggregate();
 std::unique_ptr<ScriptInstance> next;
 if(program){program->Interface().Validate(values);next=program->CreateInstance();next->SetParameters(values);}
 program_=std::move(program);script_=std::move(next);Error.clear();nextWake_=0;lastTime_=now;
 auto physical=physical_;Evaluate(now);physical_=physical;
 for(auto& pair:devices_)for(auto& targets:pair.second.Repeats)targets.clear();
}
void InputEngine::SetParameters(const ParameterValues& values,std::int64_t now){
 if(!script_||!program_)return;program_->Interface().Validate(values);script_->SetParameters(values);Tick(now);
}
void InputEngine::Process(const InputPacket& packet,std::int64_t now){
 ++Received;ZeroRelative();bool repeat=false;unsigned slot=768;DeviceState* current=nullptr;
 std::visit([&](const auto& value){
  using T=std::decay_t<decltype(value)>;constexpr unsigned family=std::is_same_v<T,Interceptor::Keyboard_Input>?1u:2u;
  auto& d=Device(value.device,family);current=&d;auto& view=d.View;
  view.PhysicalValid=true;view.InputSequence=value.sequence;view.InputTimestamp=value.timestamp;Raw(view.InputRaw,value.stroke);
  if constexpr(family==1){slot=Key(value);if(slot<768){const bool down=(value.stroke.flags&Interceptor::KeyUp)==0;repeat=down&&d.Held[slot];if(d.Held[slot]!=down){if(down)++contributors_[slot];else --contributors_[slot];}d.Held[slot]=down;physical_[slot]=contributors_[slot]?100:0;view.Values[slot]=down?100:0;view.InputAvailable[slot]=view.Available[slot]=true;}}
  else{
   d.Relative=(value.stroke.flags&Interceptor::MouseAbsolute)==0;
   for(int i=0;i<5;++i){const auto bit=static_cast<unsigned short>(1u<<(2*i));if(value.stroke.button_flags&(bit|(bit<<1))){const bool down=(value.stroke.button_flags&bit)!=0;const int control=MouseLeft+i;if(d.Buttons[i]!=down){if(down)++contributors_[control];else --contributors_[control];}d.Buttons[i]=down;physical_[control]=contributors_[control]?100:0;view.Values[control]=down?100:0;view.InputAvailable[control]=view.Available[control]=true;}}
   const int x=d.Relative?MouseX:AbsoluteX,y=d.Relative?MouseY:AbsoluteY;physical_[x]=view.Values[x]=value.stroke.x;physical_[y]=view.Values[y]=value.stroke.y;
   view.InputAvailable[x]=view.InputAvailable[y]=view.Available[x]=view.Available[y]=true;
   for(const int index:{MouseWheel,MouseHWheel}){const auto bit=index==MouseWheel?Interceptor::Wheel:Interceptor::HorizontalWheel;view.Values[index]=(value.stroke.button_flags&bit)?static_cast<short>(value.stroke.button_data):0;physical_[index]=view.Values[index];if(value.stroke.button_flags&bit)view.InputAvailable[index]=view.Available[index]=true;}
  }
 },packet);Aggregate();
 if(!script_){Send(packet);return;}
 const auto before=delivered_;const auto original=physical_;
 // Evaluate reuses the single combined script instance across both families.
 Evaluate(now,&packet);const auto desired=physical_;physical_=original;
 Motion(desired,&packet);
 if(slot<768){
  auto& targets=current->Repeats[slot];
  if(!repeat&&current->Held[slot]){targets.clear();for(int i=0;i<768;++i)if(delivered_[i]&&!before[i]&&!Modifier(i))targets.push_back(i);if(targets.empty()&&desired[slot])targets.push_back(static_cast<int>(slot));}
  if(repeat)for(const int key:targets)if(desired[key]&&route_[key]){
   Interceptor::Keyboard_Input out{};out.device=route_[key];out.stroke.make_code=static_cast<unsigned short>(key%256);out.stroke.flags=static_cast<unsigned short>(key>=512?Interceptor::KeyE1:key>=256?Interceptor::KeyE0:0);Send(out);
  }
  if(!current->Held[slot])targets.clear();
  return;
 }
 if(std::holds_alternative<Interceptor::Keyboard_Input>(packet))Send(packet);
}
void InputEngine::Motion(const std::array<int,ControlCount>& desired,const InputPacket* seed){
 const auto* mouse=seed?std::get_if<Interceptor::Mouse_Input>(seed):nullptr;
 if(mouse){
  auto out=*mouse;out.stroke.button_flags&=static_cast<unsigned short>(~0x3ffu);
  const bool abs=(out.stroke.flags&Interceptor::MouseAbsolute)!=0;
  out.stroke.x=desired[abs?AbsoluteX:MouseX];out.stroke.y=desired[abs?AbsoluteY:MouseY];
  const int wheel=(mouse->stroke.button_flags&Interceptor::HorizontalWheel)?MouseHWheel:MouseWheel;
  if(out.stroke.button_flags&(Interceptor::Wheel|Interceptor::HorizontalWheel)){
   const auto value=std::clamp(desired[wheel],static_cast<int>(SHRT_MIN),static_cast<int>(SHRT_MAX));
   out.stroke.button_data=static_cast<unsigned short>(static_cast<short>(value));
   if(!value)out.stroke.button_flags&=static_cast<unsigned short>(~(Interceptor::Wheel|Interceptor::HorizontalWheel));
  }
  if(out.stroke.x||out.stroke.y||out.stroke.flags||out.stroke.button_flags||out.stroke.raw_buttons)Send(out);
 }else{
  const auto id=Destination(2);
  if(id&&Device(id,2).Relative&&(desired[MouseX]||desired[MouseY])){
   Interceptor::Mouse_Input out{};out.device=id;out.stroke.x=desired[MouseX];out.stroke.y=desired[MouseY];Send(out);
  }
  const auto absolute=Destination(2,true);
  if(absolute&&!Device(absolute,2).Relative&&(desired[AbsoluteX]!=delivered_[AbsoluteX]||desired[AbsoluteY]!=delivered_[AbsoluteY])){
   Interceptor::Mouse_Input out{};out.device=absolute;out.stroke.flags=Interceptor::MouseAbsolute;out.stroke.x=desired[AbsoluteX];out.stroke.y=desired[AbsoluteY];Send(out);
  }
  for(const int slot:{MouseWheel,MouseHWheel})if(id&&desired[slot]){
   Interceptor::Mouse_Input out{};out.device=id;out.stroke.button_flags=slot==MouseWheel?Interceptor::Wheel:Interceptor::HorizontalWheel;
   out.stroke.button_data=static_cast<unsigned short>(static_cast<short>(std::clamp(desired[slot],static_cast<int>(SHRT_MIN),static_cast<int>(SHRT_MAX))));Send(out);
  }
 }
}
void InputEngine::Tick(std::int64_t now){
 if(!script_)return;ZeroRelative();Aggregate();const auto physical=physical_;const auto before=Forwarded;
 Evaluate(now);const auto desired=physical_;physical_=physical;
 Motion(desired);
 TimerOutputs+=Forwarded-before;
}
ServiceSnapshot InputEngine::Snapshot()const{
 ServiceSnapshot result;result.Running=Active();result.Monitoring=true;result.Status=Active()?"Script active":"Passthrough observation";result.MonitorStatus="Live capture: original IN / driver-accepted OUT";result.Error=Error;
 result.Received=Received;result.Forwarded=Forwarded;result.Changed=Changed;result.TimerOutputs=TimerOutputs;result.Faults=Faults;result.MonitorFrames=Received;
 LARGE_INTEGER f{};QueryPerformanceFrequency(&f);result.MonitorTimestampFrequency=static_cast<std::uint64_t>(f.QuadPart);
 for(const auto& pair:devices_)result.Devices.push_back(pair.second.View);
 std::sort(result.Devices.begin(),result.Devices.end(),[](const auto& a,const auto& b){return a.Family!=b.Family?a.Family<b.Family:a.Key<b.Key;});
 return result;
}
}
