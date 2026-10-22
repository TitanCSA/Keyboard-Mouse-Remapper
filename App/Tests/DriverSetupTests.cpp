#include "Test.h"
#include "../UI/DriverSetup.h"
int main(){try{
 Check(std::wstring(gpr::ui::DriverInstallerName)==L"InterceptorSetup.exe","Only one setup filename is supported");
 Check(std::wstring(gpr::ui::DriverReleasesUrl).find(L"Interceptor-Keyboard-Mouse")!=std::wstring::npos,"Download fallback uses the correct driver edition");
 Check(!gpr::ui::IsKeyboardMouseDriverInstaller(L"missing-setup.exe"),"Missing installer is rejected");
 Check(gpr::ui::FindDriverInstaller(L"missing-directory").Path.empty(),"Missing setup does not launch an unrelated executable");
 std::cout<<checks<<" driver selection checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
