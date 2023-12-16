#include <iostream>
#include <Windows.h>
#include <vector>
#include <unordered_map>
#include "multiDLLInjector.h"

using namespace std;

int main(){
    unordered_map<string, vector<string>> injectionMap;
    injectionMap["taskmgr.exe"] = { "C:\\Users\\Public\\polla.dll","C:\\Users\\Public\\pollamen.dll"};
    injectionMap["explorer.exe"] = { "C:\\Users\\Public\\polla.dll" };
    injectionMap["regedit.exe"] = { "C:\\Users\\Public\\polla.dll" };
    injectionMap["notepad.exe"] = { "C:\\Users\\Public\\polla.dll" };

    injectDlls(injectionMap);
}


