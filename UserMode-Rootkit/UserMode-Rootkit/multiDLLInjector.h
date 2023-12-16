#include <Windows.h>
#include <iostream>
#include <vector>
#include <TlHelp32.h>
#include <unordered_map>

using namespace std;

int getPIDbyProcName(const string& procName) {
    int pid = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);
    if (Process32FirstW(hSnap, &pe32) != FALSE) {
        while (pid == 0 && Process32NextW(hSnap, &pe32) != FALSE) {
            wstring wideProcName(procName.begin(), procName.end());
            if (wcscmp(pe32.szExeFile, wideProcName.c_str()) == 0) {
                pid = pe32.th32ProcessID;
            }
        }
    }
    CloseHandle(hSnap);
    return pid;
}

bool injectDLL(string dllPath, int pid) {
	char* dllPathChar = new char[dllPath.length() + 1];
	strcpy_s(dllPathChar, dllPath.length() + 1, dllPath.c_str());
	dllPathChar[dllPath.length()] = '\0';
	HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	if (hProc == NULL) {
		cout << "OpenProcess failed" << endl;
		return false;
	}
	LPVOID LoadLibAddr = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
	if (LoadLibAddr == NULL) {
		cout << "GetProcAddress failed" << endl;
		return false;
	}
	LPVOID dereercomp = VirtualAllocEx(hProc, NULL, strlen(dllPathChar), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (dereercomp == NULL) {
		cout << "VirtualAllocEx failed" << endl;
		return false;
	}
	if (WriteProcessMemory(hProc, dereercomp, dllPathChar, strlen(dllPathChar), NULL) == 0) {
		cout << "WriteProcessMemory failed" << endl;
		return false;
	}
	HANDLE hThread = CreateRemoteThread(hProc, NULL, NULL, (LPTHREAD_START_ROUTINE)LoadLibAddr, dereercomp, NULL, NULL);
	if (hThread == NULL) {
		cout << "CreateRemoteThread failed" << endl;
		return false;
	}
	CloseHandle(hProc);
	CloseHandle(hThread);
	
	return true;

}

int injectDlls(unordered_map <string, vector<string>> injectionMap) {
	for (auto& injectRow : injectionMap) {
		for (auto& vectorDLL : injectRow.second) {
           injectDLL(vectorDLL, getPIDbyProcName(injectRow.first));
        }
		cout << endl;
	}
	return 0;
}