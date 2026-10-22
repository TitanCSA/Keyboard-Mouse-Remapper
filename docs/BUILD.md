# Build and package

Use Windows x64, Visual Studio 2022 or Build Tools with **Desktop development
with C++**, a Windows SDK and CMake 3.20+. The application is entirely native
C++17. No WDK, .NET targeting pack or installed driver is needed to build it.

```powershell
.\Build.ps1                         # Build and run offline tests
.\Build.ps1 -Target Build
.\Build.ps1 -Target Test
.\Build.ps1 -Render                 # Also render simulated interface screenshots
.\Build.ps1 -Target Package -DriverInstaller C:\Downloads\InterceptorSetup.exe
```

Output: `App/.build/Release/bin/KeyboardMouseRemapper.exe`. The SDK, compiler,
runtime and UI are linked into one EXE with a static C++ runtime. Windows system
libraries remain normal OS dependencies. Tests do not open the host driver.
`--demo`, `--check-script file.igc` and `--ui-self-test report.json` are offline.

Packaging requires the original, SHA-256-pinned **Interceptor Keyboard & Mouse
1.0.3.0** GUI installer with signed kernel **1.0.3.0** and SDK **1.0.0.0**. It rejects other
editions, modified setup files, CLR executables and ASan builds. The original
setup is copied unchanged. It produces a portable ZIP, standalone application,
one `InterceptorSetup.exe`, and `SHA256SUMS.txt` under `App/.build/dist/`.

`App/Tests/` covers the language, visual documents, actual worker with an isolated
transport, transformation/release rules, UI widgets, script library, script
parameters, coalesced snapshot notifications and correct driver selection.
Live validation uses a guarded mode in a dedicated Windows VM and a laboratory
VHF source. The test source is included; the fixture driver is not a product
dependency or release component. [Validation scope](VALIDATION.md).

The optional `-AddressSanitizer` configuration is separate from production
packaging and requires the Visual Studio ASan component. A passing ordinary
build does not imply an ASan result.
