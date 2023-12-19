#include <Windows.h>
#include <iostream>
#include <vector>

using namespace std;

class Serialitzator {
private:
    string fileName;

public:
    // Constructor
    Serialitzator(string fileName) : fileName(fileName) {}

    // Getters
    string getFileName() const {
        return fileName;
    }

    // Setters
    void setFileName(string fileName) {
        this->fileName = fileName;
    }

    // Métodos
    bool serializeHandle(HANDLE targetHandle) {
        HANDLE mappedFile = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(targetHandle), "ipcObject");

        if (mappedFile == NULL) {
            cout << "Error creating the mapped file";
            return false;
        }

        LPVOID mappedView = MapViewOfFile(mappedFile, FILE_MAP_WRITE, 0, 0, sizeof(targetHandle));

        if (mappedView == NULL) {
            cout << "Error creating the view";
            CloseHandle(mappedFile);
            return false;
        }

        RtlMoveMemory(mappedView, &targetHandle, sizeof(targetHandle));
        UnmapViewOfFile(mappedView);
        CloseHandle(mappedFile);

        return true;
    }

    HANDLE deserializeHandle() {
        HANDLE mappedFile = OpenFileMappingA(FILE_MAP_READ, FALSE, "ipcObject");

        if (mappedFile == NULL) {
            cout << "Error opening the Mapped File";
            return nullptr;
        }

        LPVOID mappedView = MapViewOfFile(mappedFile, FILE_MAP_READ, 0, 0, sizeof(HANDLE));

        if (mappedView == NULL) {
            cout << "Error creating the view";
            CloseHandle(mappedFile);
            return nullptr;
        }

        HANDLE targetHandle;
        RtlMoveMemory(&targetHandle, mappedView, sizeof(targetHandle));
        UnmapViewOfFile(mappedView);
        CloseHandle(mappedFile);

        return targetHandle;
    }

    int serializeVectorString(const vector<string>& strings, string fileName) {
        string serializedData;
        for (size_t i = 0; i < strings.size(); ++i) {
            serializedData += strings[i];
            if (i < strings.size() - 1) {
                serializedData += ','; 
            }
        }

        HANDLE fileMapping = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, serializedData.size(), fileName.c_str());
        LPVOID mappedView = MapViewOfFile(fileMapping, FILE_MAP_WRITE,0,0,0);
        memcpy(mappedView, serializedData.data(), serializedData.size());
        return 0;
    }



    vector<string> deserializeStringVector(string fileName) {
        HANDLE fileMapping = OpenFileMappingA(FILE_MAP_READ, FALSE, fileName.c_str());

        LPVOID mappedView = MapViewOfFile(fileMapping, FILE_MAP_READ, 0, 0, 0);

        string serializedData(static_cast<const char*>(mappedView));

        vector<string> deserializedData;
        size_t pos = 0;
        string token;
        while ((pos = serializedData.find(',')) != std::string::npos) {
            token = serializedData.substr(0, pos);
            deserializedData.push_back(token);
            serializedData.erase(0, pos + 1);
        }

        if (!serializedData.empty()) {
            deserializedData.push_back(serializedData);
        }

        return deserializedData;
    }


};
