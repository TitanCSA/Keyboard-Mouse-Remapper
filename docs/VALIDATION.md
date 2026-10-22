# Validation and practical limits

Version **1.0.0** was built with MSVC x64 Release, warnings as errors, static CRT,
ASLR, DEP and Control Flow Guard. Eight CTest suites and the offline native
interface test passed. [Machine-readable results](validation.json).

| Area | Coverage |
| --- | --- |
| Language and visual documents | All named/raw scan controls, script compilation, parameters, fractional scaling, cross-family chords, visual round-trip and instruction-budget faults |
| Input engine | Mapping collisions, physical destination preservation, key repeats, release on Stop, cross-family output, raw mouse deltas, wheel impulses, absolute coordinates and multiple keyboards |
| Stress | 20,000 passthrough and 100,000 transformed packets; 20,000 fractional-scaling frames; 100,000 concurrent UI notifications |
| Actual worker, isolated transport | Automatic observation, script/parameter changes, Stop, revocation latch, explicit resume and transport lifetime |
| Interface | Native controls, library popup, script parameters, two workspaces, IN/OUT tabs and repeated DPI/resize resource checks |
| Driver selection | Correct edition URL/filename and rejection of missing/unrelated setup |

The synthetic active-engine test averaged about **2 microseconds per packet**
on this build machine with a memory output sink. It does not include driver I/O,
Windows delivery, USB, scheduling, rendering or hardware latency. It is not a
zero-latency claim.

## Real Windows consumer test

The exact release application was also run in an existing interactive session
of the dedicated **InterceptorWin11** Hyper-V guest, with the Microsoft-signed
Keyboard/Mouse driver and an existing laboratory VHF source. The source submitted
HID keyboard/mouse reports; a separate Raw Input window in the test mode observed
what Windows received after interception.

All seven checks passed:

1. Physical A reached Windows as B; the original A was suppressed.
2. Physical B remained pressed when the remapped A was released.
3. Releasing the last contributor released the mapped B.
4. Mouse Y inversion reached Raw Input with the expected signed delta.
5. A mouse side button reached Windows as Space.
6. A timed key macro released its output without further physical input.
7. Stop restored the original physical keyboard input.

The test mode requires explicit VM opt-in and the existing fixture. It is not
part of ordinary startup and does not install a fixture on users' PCs. Offline
tests never captured on the build computer. The driver and SDK are unchanged.

These results cover controlled virtual devices and one Windows 11 guest. They
do not certify every physical keyboard, mouse, connection, application or OS
version. Unsupported vendor controls follow the driver's compatibility limits.
OUT means successful driver acceptance, not proof that every application read an
event. Observation starts a capture and therefore consumes the driver's shared
two-hour per-boot allowance.
