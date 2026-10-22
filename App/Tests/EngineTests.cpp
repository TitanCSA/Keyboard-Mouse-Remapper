#include "Test.h"
#include "../Capture/InputEngine.h"
#include <cstring>
using namespace gpr;
using namespace Interceptor;
Keyboard_Input Key(int scan,bool down=true,unsigned device=1){Keyboard_Input k{};k.device=device;k.stroke.make_code=static_cast<unsigned short>(scan%256);k.stroke.flags=static_cast<unsigned short>((scan>=512?KeyE1:scan>=256?KeyE0:0)|(down?0:KeyUp));return k;}
int main(){try{
 std::vector<InputPacket> out;InputEngine e([&](const auto& p){out.push_back(p);});e.SetDevices({{1,1,"Keyboard","k"},{2,2,"Mouse","m"}});
 auto raw=Key(30);raw.stroke.extra_information=1234;e.Process(raw,1);Check(out.size()==1&&!std::memcmp(&std::get<Keyboard_Input>(out.back()),&raw,sizeof(raw)),"Idle passthrough must preserve the exact stroke");
 e.Process(Key(30,false),2);out.clear();e.SetProgram(ScriptCompiler::Compile("main { remap(KEY_A,KEY_B); }"),{},3);
 e.Process(Key(30),4);Check(out.size()==1&&std::get<Keyboard_Input>(out.back()).stroke.make_code==48,"A remaps to B");
 e.Process(Key(48),5);Check(out.size()==1,"Physical B remains usable without duplicate make");
 e.Process(Key(30,false),6);Check(out.size()==1,"Releasing A must not release physical B");
 e.Process(Key(48,false),7);Check(out.size()==2&&(std::get<Keyboard_Input>(out.back()).stroke.flags&KeyUp),"Last contributor releases B");
 out.clear();e.Process(Key(30),8);e.Process(Key(30),9);Check(out.size()==2&&std::get<Keyboard_Input>(out.back()).stroke.make_code==48,"Mapped keyboard typematic is preserved");
 e.SetProgram(nullptr,{},10);Check(std::get<Keyboard_Input>(out[out.size()-2]).stroke.make_code==30&&(std::get<Keyboard_Input>(out.back()).stroke.flags&KeyUp),"Stopping restores the held physical key and releases its mapped target");
 e.Process(Key(30,false),11);
 e.SetProgram(ScriptCompiler::Compile("main { remap(KEY_A,MOUSE_LEFT); }"),{},12);out.clear();e.Process(Key(30),13);
 Check(out.size()==1&&std::get<Mouse_Input>(out.back()).stroke.button_flags==LeftDown,"Keyboard-to-mouse remap uses the mouse driver endpoint");
 e.Process(Key(30,false),14);Check(std::get<Mouse_Input>(out.back()).stroke.button_flags==LeftUp,"Cross-family remap releases correctly");
 e.SetProgram(ScriptCompiler::Compile("main { sensitivity(MOUSE_X,200); invert(MOUSE_Y); }"),{},15);out.clear();
 Mouse_Input m{};m.device=2;m.stroke.x=1500;m.stroke.y=-23;m.stroke.extra_information=55;e.Process(m,16);
 Check(out.size()==1&&std::get<Mouse_Input>(out.back()).stroke.x==3000&&std::get<Mouse_Input>(out.back()).stroke.y==23,"Raw relative mouse deltas are transformed");
 Check(std::get<Mouse_Input>(out.back()).stroke.extra_information==55,"Mouse metadata is preserved");
 const auto before=out.size();e.Tick(1000);Check(out.size()==before,"Timer wake must not replay physical movement");
 e.SetProgram(ScriptCompiler::Compile("main { while(1) {} }"),{},2000);Check(!e.Active()&&e.Faults==1,"Script faults restore passthrough");
 out.clear();e.Process(raw,2001);Check(out.size()==1,"Physical input continues after a script fault");
 e.Process(Key(30,false),2002);e.SetProgram(ScriptCompiler::Compile("combo pulse {set_val(KEY_C,100);wait(10);set_val(KEY_C,0);} main {if(event_press(KEY_A))combo_run(pulse);}"),{},2003);out.clear();
 e.Process(Key(30),2004);Check(out.size()>=2,"A timed macro emits a new key");e.Tick(12004);
 Check((std::get<Keyboard_Input>(out.back()).stroke.flags&KeyUp)!=0,"Timed macro releases without new physical input");
 e.SetProgram(nullptr,{},13000);out.clear();
 for(int i=0;i<20000;++i){m.stroke.x=i%19-9;m.stroke.y=i%17-8;e.Process(m,14000+i);}
 Check(out.size()==20000,"Stress run preserves every passthrough input");
 e.SetProgram(ScriptCompiler::Compile("main {sensitivity(MOUSE_X,125);invert(MOUSE_Y);}"),{},40000);out.clear();
 const auto start=std::chrono::steady_clock::now();
 for(int i=0;i<100000;++i){m.stroke.x=i%19-9;m.stroke.y=i%17-8;e.Process(m,40001+i);}
 const auto elapsed=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
 Check(out.size()<=100000&&out.size()>90000,"Transformed stress stream remains complete except neutral movements");
 std::cout<<"Active engine average: "<<elapsed/100000<<" microseconds/packet (memory sink; not hardware latency)\n";
 e.SetProgram(nullptr,{},150000);out.clear();e.SetDevices({{3,1,"Second keyboard","k2"}});
 e.SetProgram(ScriptCompiler::Compile("main {remap(KEY_A,KEY_B);}"),{},150001);
 e.Process(Key(30,true,1),150002);e.Process(Key(30,true,3),150003);const auto held=out.size();e.Process(Key(30,false,1),150004);
 Check(out.size()==held,"One keyboard release must not cancel another keyboard's held mapping");e.Process(Key(30,false,3),150005);
 Check(out.size()==held+1&&(std::get<Keyboard_Input>(out.back()).stroke.flags&KeyUp),"Final keyboard contributor releases the output");
 e.SetProgram(nullptr,{},150006);m.stroke.x=m.stroke.y=0;m.stroke.button_flags=Wheel;m.stroke.button_data=120;e.Process(m,150007);
 m.stroke.button_flags=0;m.stroke.x=3;e.Process(m,150008);
 auto latest=e.Snapshot();const auto mouse=std::find_if(latest.Devices.begin(),latest.Devices.end(),[](const auto& d){return d.Family==2;});
 Check(mouse!=latest.Devices.end()&&mouse->Values[MouseWheel]==0&&mouse->OutputValues[MouseWheel]==0,"A wheel impulse does not remain stuck in OUT after a normal movement");
 m.stroke.flags=MouseAbsolute;m.stroke.x=60000;m.stroke.y=32000;out.clear();e.SetProgram(ScriptCompiler::Compile("main {remap(KEY_A,KEY_B);}"),{},150009);e.Process(m,150010);
 Check(out.size()==1&&std::get<Mouse_Input>(out[0]).stroke.x==60000&&std::get<Mouse_Input>(out[0]).stroke.y==32000,"Untouched absolute coordinates preserve their native range");
 const auto state=e.Snapshot();Check(state.Devices.size()==3&&state.Forwarded>=20000,"Per-device snapshots and output counts remain consistent");
 std::cout<<checks<<" transport/engine checks and 20000 packets passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
