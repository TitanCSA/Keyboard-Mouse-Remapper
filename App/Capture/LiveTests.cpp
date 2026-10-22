#include "Service.h"
#include "../ThirdParty/json.hpp"
#include <Windows.h>
#include <array>
#include <fstream>
#include <filesystem>
#include <stdexcept>
namespace gpr {
namespace {
struct Probe {
 std::array<unsigned,ControlCount> Down{},Up{};unsigned MouseMoves=0;bool MouseYInverted=false;std::string Error;
 static LRESULT CALLBACK Window(HWND w,UINT m,WPARAM a,LPARAM b){
  auto* p=reinterpret_cast<Probe*>(GetWindowLongPtrW(w,GWLP_USERDATA));
  if(m==WM_NCCREATE){p=static_cast<Probe*>(reinterpret_cast<CREATESTRUCTW*>(b)->lpCreateParams);SetWindowLongPtrW(w,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(p));}
  if(m==WM_INPUT&&p){
   RAWINPUT input{};UINT bytes=sizeof(input);
   if(GetRawInputData(reinterpret_cast<HRAWINPUT>(b),RID_INPUT,&input,&bytes,sizeof(RAWINPUTHEADER))!=static_cast<UINT>(-1)){
    if(input.header.dwType==RIM_TYPEKEYBOARD){
     const auto& k=input.data.keyboard;const unsigned slot=k.MakeCode+((k.Flags&RI_KEY_E1)?512u:(k.Flags&RI_KEY_E0)?256u:0u);
     if(slot<768){if(k.Flags&RI_KEY_BREAK)++p->Up[slot];else ++p->Down[slot];}
    }else if(input.header.dwType==RIM_TYPEMOUSE){++p->MouseMoves;if(input.data.mouse.lLastX==4&&input.data.mouse.lLastY==-7)p->MouseYInverted=true;}
   }
  }
  return DefWindowProcW(w,m,a,b);
 }
};
void Pump(unsigned ms){
 const auto end=GetTickCount64()+ms;
 do{MSG m{};while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}MsgWaitForMultipleObjectsEx(0,nullptr,5,QS_ALLINPUT,MWMO_INPUTAVAILABLE);}while(GetTickCount64()<end);
}
struct Fixture {
 HANDLE File=INVALID_HANDLE_VALUE;
 Fixture(){File=CreateFileW(L"\\\\.\\FILTERHidTest",GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);if(File==INVALID_HANDLE_VALUE)throw std::runtime_error("The dedicated VHF keyboard/mouse fixture is absent.");}
 void Report(const std::array<unsigned char,16>& data,unsigned length){
  struct Packet{unsigned long Version,Length;unsigned char Data[16];} p{1,length,{}};
  std::copy(data.begin(),data.end(),p.Data);DWORD bytes=0;
  if(!DeviceIoControl(File,static_cast<DWORD>(CTL_CODE(0x8338,0x800,METHOD_BUFFERED,FILE_READ_DATA|FILE_WRITE_DATA)),&p,sizeof(p),nullptr,0,&bytes,nullptr))throw std::runtime_error("VHF report submission failed: "+std::to_string(GetLastError()));
 }
 void Keys(unsigned a=0,unsigned b=0){std::array<unsigned char,16> r{};r[0]=1;r[3]=static_cast<unsigned char>(a);r[4]=static_cast<unsigned char>(b);Report(r,9);}
 void Mouse(unsigned buttons=0,short x=0,short y=0){std::array<unsigned char,16> r{};r[0]=2;r[1]=static_cast<unsigned char>(buttons);r[2]=static_cast<unsigned char>(x);r[3]=static_cast<unsigned char>(static_cast<unsigned short>(x)>>8);r[4]=static_cast<unsigned char>(y);r[5]=static_cast<unsigned char>(static_cast<unsigned short>(y)>>8);Report(r,8);}
 ~Fixture(){if(File!=INVALID_HANDLE_VALUE){try{Keys();Mouse();}catch(...){}CloseHandle(File);}}
};
}
int RunLiveTest(const std::wstring& reportPath,int seconds){
 nlohmann::json report={{"Passed",false},{"Simulated",false},{"HostCapture",false},{"Consumer","Windows Raw Input"}};
 HWND window=nullptr;
 try{
  wchar_t opt[32]{},model[256]{};DWORD bytes=sizeof(model);
  if(!GetEnvironmentVariableW(L"KMR_LIVE_TEST_VM",opt,32)||std::wstring(opt)!=L"1"||
   RegGetValueW(HKEY_LOCAL_MACHINE,L"HARDWARE\\DESCRIPTION\\System\\BIOS",L"SystemProductName",RRF_RT_REG_SZ,nullptr,model,&bytes)!=ERROR_SUCCESS||std::wstring(model)!=L"Virtual Machine")
   throw std::runtime_error("Live tests require the dedicated VM and KMR_LIVE_TEST_VM=1.");
  if(seconds<2||seconds>120)throw std::invalid_argument("Live-test duration must be 2..120 seconds.");
  Probe probe;WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"KmrConsumerProbe";cls.lpfnWndProc=Probe::Window;
  if(!RegisterClassW(&cls)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)throw std::runtime_error("Probe window registration failed.");
  window=CreateWindowW(cls.lpszClassName,L"Keyboard Mouse Remapper VM validation",0,0,0,0,0,HWND_MESSAGE,nullptr,cls.hInstance,&probe);
  if(!window)throw std::runtime_error("Probe window creation failed.");
  RAWINPUTDEVICE raw[]={{1,6,RIDEV_INPUTSINK,window},{1,2,RIDEV_INPUTSINK,window}};
  if(!RegisterRawInputDevices(raw,2,sizeof(raw[0])))throw std::runtime_error("Raw Input registration failed.");
  Fixture fixture;fixture.Keys();fixture.Mouse();Pump(150);
  RemapperService service;
  service.Inject(ScriptCompiler::Compile("combo pulse {set_val(KEY_C,100);wait(30);set_val(KEY_C,0);} main {remap(KEY_A,KEY_B);remap(MOUSE_X1,KEY_SPACE);invert(MOUSE_Y);if(event_press(KEY_F6))combo_run(pulse);}"));
  Pump(100);
  fixture.Keys(4);Pump(80);
  const bool mapped=probe.Down[48]>0&&probe.Down[30]==0;
  const auto beforeRelease=probe.Up[48];
  fixture.Keys(4,5);Pump(50);fixture.Keys(5);Pump(50);
  const bool preserved=probe.Up[48]==beforeRelease;
  fixture.Keys();Pump(50);const bool released=probe.Up[48]>beforeRelease;
  fixture.Mouse(0,4,7);Pump(80);
  fixture.Mouse(8);Pump(60);fixture.Mouse();Pump(60);
  const bool cross=probe.Down[57]>0&&probe.Up[57]>0;
  fixture.Keys(0x3f);Pump(120);
  const bool timed=probe.Down[46]>0&&probe.Up[46]>0;
  fixture.Keys();Pump(50);
  service.Stop();Pump(30);
  const auto aBefore=probe.Down[30];fixture.Keys(4);Pump(70);fixture.Keys();Pump(70);
  const bool passthrough=probe.Down[30]>aBefore;
  const auto state=service.GetSnapshot();service.StopMonitoring();
  report["KeyRemapReachedWindows"]=mapped;report["PhysicalTargetPreserved"]=preserved;
  report["MappedKeyReleased"]=released;report["MouseYInversionReachedWindows"]=probe.MouseYInverted;
  report["MouseToKeyboardReachedWindows"]=cross;report["TimedMacroWithoutPhysicalInput"]=timed;
  report["StopRestoredPhysicalInput"]=passthrough;report["Received"]=state.Received;report["Forwarded"]=state.Forwarded;
  report["Changed"]=state.Changed;report["DeviceCount"]=state.Devices.size();report["Error"]=state.Error;report["MonitorError"]=state.MonitorError;
  report["ProcessingMicroseconds"]=state.ProcessingMicroseconds;report["MouseEvents"]=probe.MouseMoves;
  report["Passed"]=mapped&&preserved&&released&&probe.MouseYInverted&&cross&&timed&&passthrough&&state.Error.empty()&&state.MonitorError.empty();
 }catch(const std::exception& e){report["Error"]=e.what();}
 if(window)DestroyWindow(window);
 const std::filesystem::path path(reportPath);if(path.has_parent_path())std::filesystem::create_directories(path.parent_path());
 std::ofstream file(path,std::ios::binary);file<<report.dump(2)<<"\n";return report["Passed"].get<bool>()?0:1;
}
}
