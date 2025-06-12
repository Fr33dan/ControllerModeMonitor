#include "ControllerMonitor.h"

#include <comdef.h>
#include <Wbemidl.h>
#include <cassert>

#pragma comment(lib, "wbemuuid.lib")

IWbemLocator* pLoc = NULL;
IWbemServices* pSvc = NULL;

std::set<std::wstring> monitoredDeviceList;

HRESULT InitializeWMI()
{
    HRESULT hres;

    // Step 1: --------------------------------------------------
    // Initialize COM. ------------------------------------------

    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres))
    {
        // No need to close, nothing was opened.
        return 1;                  // Program has failed.
    }

    // Step 2: --------------------------------------------------
    // Set general COM security levels --------------------------

    hres = CoInitializeSecurity(
        NULL,
        -1,                          // COM authentication
        NULL,                        // Authentication services
        NULL,                        // Reserved
        RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication 
        RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation  
        NULL,                        // Authentication info
        EOAC_NONE,                   // Additional capabilities 
        NULL                         // Reserved
    );


    if (FAILED(hres))
    {
        CloseMonitor();
        return hres;                    // Program has failed.
    }

    // Step 3: ---------------------------------------------------
    // Obtain the initial locator to WMI -------------------------



    hres = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)&pLoc);

    if (FAILED(hres))
    {
        CloseMonitor();
        return hres;                 // Program has failed.
    }

    // Step 4: -----------------------------------------------------
    // Connect to WMI through the IWbemLocator::ConnectServer method

    // Connect to the root\cimv2 namespace with
    // the current user and obtain pointer pSvc
    // to make IWbemServices calls.
    hres = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"), // Object path of WMI namespace
        NULL,                    // User name. NULL = current user
        NULL,                    // User password. NULL = current
        0,                       // Locale. NULL indicates current
        NULL,                    // Security flags.
        0,                       // Authority (for example, Kerberos)
        0,                       // Context object 
        &pSvc                    // pointer to IWbemServices proxy
    );

    if (FAILED(hres))
    {
        CloseMonitor();
        return hres;                // Program has failed.
    }

    // Step 5: --------------------------------------------------
    // Set security levels on the proxy -------------------------

    hres = CoSetProxyBlanket(
        pSvc,                        // Indicates the proxy to set
        RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
        RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
        NULL,                        // Server principal name 
        RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx 
        RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
        NULL,                        // client identity
        EOAC_NONE                    // proxy capabilities 
    );

    if (FAILED(hres))
    {
        CloseMonitor();
        return hres;               // Program has failed.
    }
    return hres;
}

BOOL IsDeviceConnected(){
    HRESULT hres;
    // Step 6: --------------------------------------------------
    // Use the IWbemServices pointer to make requests of WMI ----

    // For example, get the name of the operating system
    IEnumWbemClassObject* pEnumerator = NULL;

    if (monitoredDeviceList.size() == 0) {
        return false;
    }

    std::wstring query(L"SELECT * FROM Win32_PnPEntity WHERE PNPClass = 'HIDClass' and (");
    for (std::wstring devName: monitoredDeviceList) {
        query.append(L"Name = '");
        query.append(devName);
        query.append(L"' or");
    }
    // Remove last ' or' and end and close parenthesis
    query.resize(query.length() - 3);
    query.append(L")");
    BSTR queryStr = SysAllocStringLen(query.data(), (UINT)query.size());
    hres = pSvc->ExecQuery(
        bstr_t("WQL"),
        queryStr,
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator);
    SysFreeString(queryStr);

    if (FAILED(hres))
    {
        return false;               // Program has failed.
    }

    // Step 7: -------------------------------------------------
    // Get the data from the query in step 6 -------------------

    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;

    BOOL deviceFound = false;
    while (pEnumerator)
    {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1,
            &pclsObj, &uReturn);

        if (0 == uReturn)
        {
            break;
        }

        deviceFound = true;

        pclsObj->Release();
    }

    pEnumerator->Release();

    return deviceFound;
}

std::set<std::wstring> GetDeviceList() {
    std::set<std::wstring> returnVal;
    HRESULT hres;
    // Step 6: --------------------------------------------------
    // Use the IWbemServices pointer to make requests of WMI ----

    // For example, get the name of the operating system
    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM Win32_PnPEntity WHERE PNPClass = 'HIDClass'"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator);

    if (FAILED(hres))
    {
        return returnVal;
    }

    // Step 7: -------------------------------------------------
    // Get the data from the query in step 6 -------------------

    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;

    BOOL deviceFound = false;
    while (pEnumerator)
    {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1,
            &pclsObj, &uReturn);

        if (0 == uReturn)
        {
            break;
        }

        deviceFound = true;

        VARIANT nameProp;

        VariantInit(&nameProp);
        // Get the value of the Name property
        hr = pclsObj->Get(L"Name", 0, &nameProp, 0, 0);
        std::wstring devName(nameProp.bstrVal, SysStringLen(nameProp.bstrVal));
        returnVal.insert(devName);
        VariantClear(&nameProp);

        pclsObj->Release();
    }

    pEnumerator->Release();

    return returnVal;
}

VOID CloseMonitor() {
    if (pSvc != NULL) pSvc->Release();
    if (pLoc != NULL) pLoc->Release();
    pLoc = NULL;
    pSvc = NULL;
    CoUninitialize();
}