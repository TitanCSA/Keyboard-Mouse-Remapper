#pragma once
#include <Windows.h>
extern "C" void __cdecl GR_SetSnapshotWindow(HWND window,UINT message);
extern "C" BOOL __cdecl GR_AcknowledgeSnapshot(HWND window,ULONG_PTR generation);
