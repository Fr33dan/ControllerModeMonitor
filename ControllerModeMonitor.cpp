// ControllerModeMonitor.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "ControllerMonitor.h"
#include "ControllerModeMonitor.h"
#include "shellapi.h"
#include "setupapi.h"
#include <iostream>
#include <comdef.h>
#include <Wbemidl.h>
#include <vector>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <set>
#include <thread>
#include <atomic>
#include <typeinfo>
#include "TV/TV.h"
#include "TV/Roku.h"

#define MAX_LOADSTRING 100
#define TRUE_DISCONNECT_COUNT 5
#define MU_MASK 0xC000
#define MU_DEVICE_ID 0xC000
#define MU_TV_ID 0x4000
#define MU_CUSTOM_START 0x8800
#define MU_DEVICE_NOT_FOUND MU_CUSTOM_START + 1
#define MU_CONFIG_NOT_FOUND MU_CUSTOM_START + 2

UINT const WMAPP_NOTIFYCALLBACK = WM_APP + 1;

UINT const IDT_UPDATETIMER = 1;
static const GUID TRAY_GUID = { 0xf6860a80, 0x58c5, 0x46eb, {0xb3, 0x6d, 0x49, 0xe9, 0x15, 0x37, 0x8b, 0x58} };

// Global Variables:
HINSTANCE hInst;                                // current instance
HWND hWnd;
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
WCHAR szController[MAX_LOADSTRING];            // the main window class name
WCHAR szConfigLocation[MAX_LOADSTRING];            // the main window class name
BOOL controllerModeActive;
clock_t timeLastSeen;
std::thread* tvSearchThread;
std::atomic<bool> tvSearchRunning(false);
std::vector<TVController*> tvList;
TVController* currentController;
int currentHDMI;


// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE);
BOOL                InitInstance(HINSTANCE);
VOID                InitNotifyTray(HWND);
VOID                WriteXmlSettings();
VOID                ReadXmlSettings();
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
VOID                AddNewDevice(HWND);
VOID                ShowContextMenu(HWND, POINT);
VOID                UpdateStatus();
VOID                TriggerTVSearch();
VOID                SearchTVs();


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    WCHAR rawPath[MAX_LOADSTRING];
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    controllerModeActive = false;
    timeLastSeen = -3000;

    // Initialize global strings
    LoadStringW(hInstance, IDC_CONTROLLERMODEMONITOR, szWindowClass, MAX_LOADSTRING);
    LoadStringW(hInstance, IDS_CONTROLLER, szController, MAX_LOADSTRING);
    LoadStringW(hInstance, IDS_CONFIG_PATH, rawPath, MAX_LOADSTRING);
    ExpandEnvironmentStrings(rawPath, szConfigLocation, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance))
    {
        return FALSE;
    }
    TriggerTVSearch();

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CONTROLLERMODEMONITOR));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CONTROLLERMODEMONITOR));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_CONTROLLERMODEMONITOR);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance)
{
    hInst = hInstance; // Store instance handle in our global variable

    hWnd = CreateWindowW(szWindowClass, szController, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, HWND_MESSAGE, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }
    SetTimer(hWnd, IDT_UPDATETIMER, 10, nullptr);
    InitNotifyTray(hWnd);
    ReadXmlSettings();

    int hRes;
    hRes = InitializeWMI();

    if (FAILED(hRes))
    {
        exit(-1);
    }

    //SearchRokuDevices();

    return TRUE;
}

VOID InitNotifyTray(HWND hWnd) {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hWnd;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_GUID | NIF_MESSAGE;
    nid.uVersion = NOTIFYICON_VERSION_4;
    nid.uCallbackMessage = WMAPP_NOTIFYCALLBACK;

    // Note: This is an example GUID only and should not be used.
    // Normally, you should use a GUID-generating tool to provide the value to
    // assign to guidItem.
    nid.guidItem = TRAY_GUID;

    // This text will be shown as the icon's tooltip.

    LoadString(hInst, IDS_CONTROLLER, nid.szTip, ARRAYSIZE(nid.szTip));

    // Load the icon for high DPI.
    nid.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_CONTROLLERMODEMONITOR));

    // Show the notification.
    Shell_NotifyIcon(NIM_ADD, &nid) ? S_OK : E_FAIL;
    Shell_NotifyIcon(NIM_SETVERSION, &nid) ? S_OK : E_FAIL;
}

VOID ReadXmlSettings() {
    std::filesystem::path configRoot(szConfigLocation);
    std::filesystem::path xmlFile("Configuration.xml");
    std::filesystem::path full_path = configRoot / xmlFile;

    pugi::xml_document configDocument;

    pugi::xml_parse_result status = configDocument.load_file(full_path.c_str());
    if (!status) {
        SendMessage(hWnd, WM_COMMAND, MU_CONFIG_NOT_FOUND, 0);
        return;
    }

    pugi::xml_node devicesNode = configDocument.child("MonitoredControllers");
    for (pugi::xml_node controllerNode : devicesNode.children()) {
        monitoredDeviceList.insert(pugi::as_wide(controllerNode.child_value()));
    }

    pugi::xml_node tvNode = configDocument.child("TVInfo");
    if (tvNode) {
        std::string tvType = tvNode.child("TVType").child_value();
        std::string initInfo = tvNode.child("TVInitializeInfo").child_value();

        if (tvType == "Roku") {
            currentController = new RokuTVController(initInfo);
        }
        currentHDMI = std::stoi(tvNode.child("HDMIPort").child_value());
    }
}

VOID WriteXmlSettings() {
    std::filesystem::path configRoot(szConfigLocation);
    std::filesystem::path xmlFile("Configuration.xml");
    std::filesystem::path full_path = configRoot / xmlFile;

    pugi::xml_document configDocument;

    pugi::xml_node devicesNode = configDocument.append_child("MonitoredControllers");
    for (std::wstring deviceName : monitoredDeviceList) {
        pugi::xml_node controllerNode = devicesNode.append_child("DeviceName");
        controllerNode.append_child(pugi::node_pcdata).set_value(pugi::as_utf8(deviceName));
        //controllerNode.child_value()
        //controllerNode.set_value("TEST");
        //controllerNode.set_value(pugi::as_utf8(deviceName));
    }

    if (currentController != nullptr) {
        pugi::xml_node tvNode = configDocument.append_child("TVInfo");

        std::string deviceType;

        if (dynamic_cast<RokuTVController*>(currentController)) {
            deviceType = "Roku";
        }
        tvNode.append_child("TVType").append_child(pugi::node_pcdata).set_value(deviceType);
        tvNode.append_child("TVInitializeInfo").append_child(pugi::node_pcdata).set_value(pugi::as_utf8(currentController->Serialize()));
        tvNode.append_child("HDMIPort").append_child(pugi::node_pcdata).set_value(std::to_string(currentHDMI));
    }

    configDocument.save_file(full_path.c_str());
}

VOID AddNewDevice(HWND hWnd) {
    std::set<std::wstring> setContainingDevice, setWithoutDevice;
    WCHAR promptText[MAX_LOADSTRING];
    WCHAR promptTitle[MAX_LOADSTRING];


    LoadStringW(hInst, IDS_ADD_PROMPT1, promptText, MAX_LOADSTRING);
    LoadStringW(hInst, IDS_ADD_PROMPT1_TITLE, promptTitle, MAX_LOADSTRING);
    int dRes1 = MessageBox(hWnd, promptText, promptTitle, MB_YESNOCANCEL);
    
    if (dRes1 == IDCANCEL) {
        return;
    }
    
    std::set<std::wstring> firstSet = GetDeviceList();
    if (dRes1 == IDYES) { 
        setContainingDevice = firstSet;
        LoadStringW(hInst, IDS_ADD_PROMPT2_YES, promptText, MAX_LOADSTRING);
    }
    else {
        setWithoutDevice = firstSet;
        LoadStringW(hInst, IDS_ADD_PROMPT2_NO, promptText, MAX_LOADSTRING);
    }
    
    LoadStringW(hInst, IDS_ADD_PROMPT2_TITLE, promptTitle, MAX_LOADSTRING);

    int dRes2 = MessageBox(hWnd, promptText, promptTitle, MB_OKCANCEL);
    
    if (dRes2 == IDCANCEL) {
        return;
    }
    std::set<std::wstring> secondSet = GetDeviceList();

    if (dRes1 == IDYES) {
        setWithoutDevice = secondSet;
    }
    else {
        setContainingDevice = secondSet;
    }

    std::vector<std::wstring> candidates{};

    std::set_difference(setContainingDevice.begin(), setContainingDevice.end()
                        , setWithoutDevice.begin(), setWithoutDevice.end()
                        , std::back_inserter(candidates));
    std::wstring foundDevice = *(candidates.begin());
    std::wstring message;

    LoadStringW(hInst, IDS_ADD_CONFIRMATION, promptText, MAX_LOADSTRING);
    LoadStringW(hInst, IDS_CONFIRM, promptTitle, MAX_LOADSTRING);
    message.append(promptText);
    message.append(foundDevice);
    dRes2 = MessageBox(hWnd, message.c_str(), promptTitle, MB_YESNO);

    if (dRes2 == IDYES) 
    {
        monitoredDeviceList.insert(foundDevice);
        WriteXmlSettings();
    }
}

VOID RemoveDevice(HWND hWnd, UINT deviceIndex) {
    WCHAR promptText[MAX_LOADSTRING];
    WCHAR promptTitle[MAX_LOADSTRING];
    LoadStringW(hInst, IDS_REMOVE_PROMPT, promptText, MAX_LOADSTRING);
    LoadStringW(hInst, IDS_CONFIRM, promptTitle, MAX_LOADSTRING);

    std::wstring deviceToRemove = *std::next(monitoredDeviceList.begin(), deviceIndex);

    std::wstring message;
    message.append(promptText);
    message.append(deviceToRemove);

    int dRes = MessageBox(hWnd, message.c_str(), promptTitle, MB_YESNOCANCEL);

    if (dRes == IDYES) {
        monitoredDeviceList.erase(deviceToRemove);
        WriteXmlSettings();
    }
}

VOID UpdateStatus() {
    BOOL isConnected = IsDeviceConnected();
    WCHAR commandText[MAX_LOADSTRING] = L"";

    if (!isConnected) {
        time_t currentTime = clock();
        isConnected = (currentTime - timeLastSeen) < 3000;
    }
    else {
        timeLastSeen = clock();
    }
    
    if (controllerModeActive && !isConnected) {
        controllerModeActive = false;
        LoadString(hInst, IDS_CMD_BIG_PICTURE_DEACTIVATE, commandText, MAX_LOADSTRING);
    }
    else if (!controllerModeActive && isConnected) {
        controllerModeActive = true;

        if (currentController != nullptr) {
            currentController->SetInput(currentHDMI);
        }
        LoadString(hInst, IDS_CMD_BIG_PICTURE_ACTIVATE, commandText, MAX_LOADSTRING);

    }

    if (commandText[0] != 0) {
        ShellExecute(0, L"open", commandText, nullptr, nullptr, SW_SHOWNORMAL);
    }
}

VOID TriggerTVSearch() {
    if (!tvSearchRunning) {
        tvSearchRunning = true;
        if (tvSearchThread != nullptr) {
            tvSearchThread->join();
            delete tvSearchThread;
        }
        tvSearchThread = new std::thread(SearchTVs);
    }
}

VOID SearchTVs() {
    std::vector<TVController*> tvsFound = RokuTVController::SearchDevices();

    for (auto&& child : tvList) {
        if (child != currentController) {
            delete child;
        }
    }
    tvList = tvsFound;
    if (currentController != nullptr) {
        bool matchFound = false;
        for (TVController* tv : tvList) {
            matchFound |= tv->Equals(currentController);
        }
        if (!matchFound) {
            SendMessage(hWnd, WM_COMMAND, MU_DEVICE_NOT_FOUND, 0);
        }
    }
    tvSearchRunning = false;
}

VOID SetTV(UINT index) {
    currentController = tvList[index];
    WriteXmlSettings();
}

VOID DisplayBalloonMessage(UINT stringID) {
    NOTIFYICONDATA updateData;
    //memset(&updateData, 0, sizeof(NOTIFYICONDATA));
    //ZeroMemory(&updateData, sizeof(updateData));
    updateData.cbSize = sizeof(updateData);
    updateData.hWnd = hWnd;
    updateData.uFlags = NIF_INFO | NIF_GUID;
    updateData.guidItem = TRAY_GUID;
    updateData.uTimeout = 5000;
    LoadString(hInst, stringID, updateData.szInfo, sizeof(updateData.szInfo));
    LoadString(hInst, IDS_BALLOON_TITLE, updateData.szInfoTitle, sizeof(updateData.szInfoTitle));

    //GetLastError();
    int res = Shell_NotifyIcon(NIM_MODIFY, &updateData);
    OutputDebugString(L"Notification Sent.\r\n");
}


//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            switch (wmId & MU_MASK) {
            case MU_DEVICE_ID: {
                UINT deviceNumber = wmId & ~MU_DEVICE_ID;
                RemoveDevice(hWnd, deviceNumber);
                break;
            }
            case MU_TV_ID: {
                UINT tvNumber = (wmId & ~MU_TV_ID) >> 8;
                currentHDMI = wmId & ~MU_TV_ID & 0xFF;
                SetTV(tvNumber);
                break;
            }
            default:
                switch (wmId)
                {
                case MU_DEVICE_NOT_FOUND:
                    DisplayBalloonMessage(IDS_TV_NOT_FOUND);
                    break;
                case MU_CONFIG_NOT_FOUND:
                    DisplayBalloonMessage(IDS_CONFIG_NOT_FOUND);
                    break;
                case IDM_ABOUT:
                    DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                    break;
                case IDM_ADDDEVICE:
                    AddNewDevice(hWnd);
                    break;
                case IDM_TV_SEARCH:
                    TriggerTVSearch();
                    break;
                case IDM_EXIT:
                    CloseMonitor();
                    KillTimer(hWnd, IDT_UPDATETIMER);
                    DestroyWindow(hWnd);
                    break;
                }
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_TIMER:
        UpdateStatus();
        break;
    case WMAPP_NOTIFYCALLBACK:
        switch (LOWORD(lParam))
        {
            case NIN_BALLOONTIMEOUT:
                break;

            case NIN_BALLOONUSERCLICK:
                break;

            case NIN_SELECT:
            case WM_CONTEXTMENU:
            {
                POINT const pt = { LOWORD(wParam), HIWORD(wParam) };
                ShowContextMenu(hWnd, pt);
            }
            break;
        }
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void ShowContextMenu(HWND hWnd, POINT pt)
{
    HMENU hMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDC_CONTROLLERMODEMONITOR));
    if (hMenu)
    {
        HMENU hMainMenu = GetSubMenu(hMenu, 0);
        if (hMainMenu)
        {
            // our window must be foreground before calling TrackPopupMenu or the menu will not disappear when the user clicks away
            SetForegroundWindow(hWnd);

            // respect menu drop alignment
            UINT uFlags = TPM_RIGHTBUTTON;
            if (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0)
            {
                uFlags |= TPM_RIGHTALIGN;
            }
            else
            {
                uFlags |= TPM_LEFTALIGN;
            }

            HMENU hDeviceMenu = GetSubMenu(hMainMenu, 0);

            UINT i = 0;
            for (std::wstring device : monitoredDeviceList) {
                int newItemPos = GetMenuItemCount(hDeviceMenu);
                MENUITEMINFOW deviceItem = {};
                deviceItem.cbSize = sizeof(deviceItem);
                deviceItem.fMask = MIIM_STRING | MIIM_ID;
                deviceItem.wID = MU_DEVICE_ID | i;
                deviceItem.dwTypeData = const_cast<LPWSTR>(device.c_str());
                InsertMenuItem(hDeviceMenu, newItemPos, true, &deviceItem);
                i++;
            }


            HMENU hTVMenu = GetSubMenu(hMainMenu, 1);
            i = 0;
            for (TVController* tv : tvList) {
                int newItemPos = GetMenuItemCount(hTVMenu);
                std::wstring deviceName = tv->GetName();
                MENUITEMINFOW deviceItem = {};
                bool controllerMatch = currentController != nullptr && currentController->Equals(tv);
                deviceItem.cbSize = sizeof(deviceItem);
                deviceItem.fMask = MIIM_STRING | MIIM_ID | MIIM_STATE | MIIM_SUBMENU;
                deviceItem.fState = controllerMatch ? MFS_CHECKED : MFS_UNCHECKED;
                deviceItem.wID = MU_TV_ID | (i << 8) | currentHDMI;
                deviceItem.dwTypeData = const_cast<LPWSTR>(deviceName.c_str());
                
                

                HMENU hHdmiMenu = CreateMenu();

                for (int j = 0; j < tv->HDMICount(); j++) {
                    MENUITEMINFOW hdmiItem = {};
                    
                    hdmiItem.cbSize = sizeof(deviceItem);
                    hdmiItem.fMask = MIIM_STRING | MIIM_ID | MIIM_STATE;
                    hdmiItem.fState = controllerMatch && currentHDMI == j ? MFS_CHECKED : MFS_UNCHECKED;
                    hdmiItem.wID = MU_TV_ID | (i << 8) | j;
                    std::wstring portName = (L"HDMI" + std::to_wstring(j + 1));
                    hdmiItem.dwTypeData = const_cast<LPWSTR>(portName.c_str());

                    InsertMenuItem(hHdmiMenu, newItemPos, true, &hdmiItem);
                }
                deviceItem.hSubMenu = hHdmiMenu;
              
                InsertMenuItem(hTVMenu, newItemPos, true, &deviceItem);

                i++;
            }

            TrackPopupMenuEx(hMainMenu, uFlags, pt.x, pt.y, hWnd, NULL);
        }
        DestroyMenu(hMenu);
    }
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
