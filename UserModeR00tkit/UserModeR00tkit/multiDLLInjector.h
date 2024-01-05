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

	// Found only DLL name (without path) using last path part then of last  \\ 
	char* dllName = strrchr(dllPathChar, '\\');
	// Check if the DLL is already injected
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
	MODULEENTRY32W me32;
	me32.dwSize = sizeof(MODULEENTRY32W);
	if (Module32FirstW(hSnap, &me32) != FALSE) {
		while (Module32NextW(hSnap, &me32) != FALSE) {
			if (wcscmp(me32.szModule, (wstring(dllPathChar, dllPathChar + strlen(dllPathChar))).c_str()) == 0) {
				OutputDebugStringW(L"Already injected");
				return false;
			}
		}
	}

	HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	if (hProc == NULL) {
		OutputDebugStringW(L"OpenProcess failed");
		return false;
	}
	LPVOID LoadLibAddr = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
	if (LoadLibAddr == NULL) {
		OutputDebugStringW(L"GetProcAddress failed");
		return false;
	}
	LPVOID dereercomp = VirtualAllocEx(hProc, NULL, strlen(dllPathChar), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (dereercomp == NULL) {
		OutputDebugStringW(L"VirtualAllocEx failed");
		return false;
	}
	if (WriteProcessMemory(hProc, dereercomp, dllPathChar, strlen(dllPathChar), NULL) == 0) {
		OutputDebugStringW(L"WriteProcessMemory failed");
		return false;
	}
	HANDLE hThread = CreateRemoteThread(hProc, NULL, NULL, (LPTHREAD_START_ROUTINE)LoadLibAddr, dereercomp, NULL, NULL);
	if (hThread == NULL) {
		OutputDebugStringW(L"CreateRemoteThread failed");
		return false;
	}
	CloseHandle(hProc);
	CloseHandle(hThread);
	// DLL Name
	OutputDebugStringA(dllName);
	// pid
	OutputDebugStringW(to_wstring(pid).c_str());
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