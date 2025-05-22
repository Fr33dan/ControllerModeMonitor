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
#include <set>

#define MAX_LOADSTRING 100
#define TRUE_DISCONNECT_COUNT 5


UINT const WMAPP_NOTIFYCALLBACK = WM_APP + 1;
UINT const MU_DEVICE_ID = 0xC000;
UINT const IDT_UPDATETIMER = 1;
static const GUID TRAY_GUID = { 0xf6860a80, 0x58c5, 0x46eb, {0xb3, 0x6d, 0x49, 0xe9, 0x15, 0x37, 0x8b, 0x58} };

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
WCHAR szController[MAX_LOADSTRING];            // the main window class name
BOOL controllerModeActive;
UINT disconnectCount;

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE);
BOOL                InitInstance(HINSTANCE);
VOID                InitNotifyTray(HWND);
VOID                InitConfig();
VOID                WriteConfig();
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
VOID                AddNewDevice(HWND);
VOID                ShowContextMenu(HWND, POINT);
VOID                UpdateStatus();


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    controllerModeActive = false;
    disconnectCount = TRUE_DISCONNECT_COUNT;

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_CONTROLLERMODEMONITOR, szWindowClass, MAX_LOADSTRING);
    LoadStringW(hInstance, IDS_CONTROLLER, szController, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance))
    {
        return FALSE;
    }

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

    HWND hWnd = CreateWindowW(szWindowClass, szController, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, HWND_MESSAGE, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }
    SetTimer(hWnd, IDT_UPDATETIMER, 10, nullptr);
    InitNotifyTray(hWnd);
    InitConfig();

    int hRes;
    hRes = InitializeWMI();

    if (FAILED(hRes))
    {
        exit(-1);
    }

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

VOID InitConfig() {
    WCHAR rawPath[MAX_LOADSTRING];
    WCHAR path[MAX_LOADSTRING];
    LoadString(hInst, IDS_CONFIG_PATH, rawPath, MAX_LOADSTRING);
    ExpandEnvironmentStrings(rawPath, path, MAX_LOADSTRING);

    if (!CreateDirectory(path, NULL) &&
        ERROR_ALREADY_EXISTS != GetLastError()) {
        exit(-1);
    }

    LoadString(hInst, IDS_CONFIG_LIST_LOCATION, rawPath, MAX_LOADSTRING);
    ExpandEnvironmentStrings(rawPath, path, MAX_LOADSTRING);

    INT configAttr = GetFileAttributes(path);

    if (configAttr != INVALID_FILE_ATTRIBUTES) {
        std::wifstream file(path);
        std::wstring deviceName;
        while (std::getline(file, deviceName)) {
            monitoredDeviceList.insert(deviceName);
        }
    }
}

VOID WriteConfig() {
    WCHAR rawPath[MAX_LOADSTRING];
    WCHAR path[MAX_LOADSTRING];
    LoadString(hInst, IDS_CONFIG_LIST_LOCATION, rawPath, MAX_LOADSTRING);
    ExpandEnvironmentStrings(rawPath, path, MAX_LOADSTRING);

    std::wofstream file(path);
    for (std::wstring deviceName : monitoredDeviceList) {
        file << deviceName << std::endl;
    }
    file.close();
}

VOID AddNewDevice(HWND hWnd) {
    std::set<std::wstring> setContainingDevice, setWithoutDevice;
    LPCWSTR secondPrompt;
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
        WriteConfig();
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
        WriteConfig();
    }
}

VOID UpdateStatus() {
    BOOL isConnected = IsDeviceConnected();
    WCHAR commandText[MAX_LOADSTRING] = L"";

    if (!isConnected) {
        // Require multiple failed detections for a true disconnect
        // to avoid triggering on momentary loss.
        isConnected = ++disconnectCount < TRUE_DISCONNECT_COUNT;
    }
    else {
        disconnectCount = 0;
    }

    
    if (controllerModeActive && !isConnected) {
        controllerModeActive = false;
        LoadString(hInst, IDS_CMD_BIG_PICTURE_DEACTIVATE, commandText, MAX_LOADSTRING);
    }
    else if (!controllerModeActive && isConnected) {
        controllerModeActive = true;
        LoadString(hInst, IDS_CMD_BIG_PICTURE_ACTIVATE, commandText, MAX_LOADSTRING);

    }

    if (commandText[0] != 0) {
        ShellExecute(0, L"open", commandText, nullptr, nullptr, SW_SHOWNORMAL);
    }
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
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_ADDDEVICE:
                AddNewDevice(hWnd);
                break;
            case IDM_EXIT:
                CloseMonitor();
                KillTimer(hWnd, IDT_UPDATETIMER);
                DestroyWindow(hWnd);
                break;
            default:
                UINT deviceNumber = wmId & ~MU_DEVICE_ID;
                RemoveDevice(hWnd, deviceNumber);
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
            case NIN_SELECT:
                // for NOTIFYICON_VERSION_4 clients, NIN_SELECT is prerable to listening to mouse clicks and key presses
                // directly.
                break;

            case NIN_BALLOONTIMEOUT:
                break;

            case NIN_BALLOONUSERCLICK:
                break;

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
                deviceItem.fState = MFS_DEFAULT;
                deviceItem.wID = MU_DEVICE_ID | i;
                deviceItem.dwTypeData = const_cast<LPWSTR>(device.c_str());
                InsertMenuItem(hDeviceMenu, newItemPos, true, &deviceItem);
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
