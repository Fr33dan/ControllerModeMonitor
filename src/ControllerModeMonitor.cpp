// ControllerModeMonitor.cpp : Defines the entry point for the application.
//
#include "ControllerModeMonitor.h"

#include <atomic>
#include <filesystem>
#include <set>
#include <setupapi.h>
#include <shellapi.h>
#include <thread>

#include "ControllerMonitor.h"
#include "TV/TV.h"
#include "TV/Roku.h"
#include "AudioDeviceManager.h"

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define MAX_LOADSTRING 100
#define MAX_IP_LENGTH 15
#define TRUE_DISCONNECT_COUNT 5
#define CUSTOM_COMMAND_MASK 0xF000
#define CC_DEVICE_ID 0xC000
#define CC_TV_ID 0x4000
#define CC_AUDIO_ID 0x2000
#define CC_INDIVIDUAL_COMMAND_ID 0x8000
#define CC_CUSTOM_START (CC_INDIVIDUAL_COMMAND_ID & 0x0800)
#define CC_DEVICE_NOT_FOUND (CC_CUSTOM_START + 1)
#define CC_CONFIG_NOT_FOUND (CC_CUSTOM_START + 2)
#define CC_AUDIO_DEVICE_NOT_FOUND (CC_CUSTOM_START + 3)
#define CC_AUDIO_DEFAULT_NOT_RESTORED (CC_CUSTOM_START + 4)

UINT const WMAPP_NOTIFYCALLBACK = WM_APP + 1;

UINT const IDT_UPDATETIMER = 1;
static const GUID TRAY_GUID = { 0xf6860a80, 0x58c5, 0x46eb, {0xb3, 0x6d, 0x49, 0xe9, 0x15, 0x37, 0x8b, 0x58} };

// Global Variables:
HINSTANCE hInst;                                // current instance
HWND hWnd;                                      // Main (hidden) window instance
HWND hWndTVDialog;                              // Window to manually set TV.
HICON hIcon;
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
WCHAR szController[MAX_LOADSTRING];             // the main window class name
WCHAR szConfigLocation[MAX_LOADSTRING];         // the main window class name
BOOL controllerModeActive;                      // If controller mode is currently activated
clock_t timeLastSeen;                           // Clock time the controller was last seen
std::thread* tvSearchThread;                    // Thread to run the TV search on
std::atomic<bool> tvSearchRunning(false);       // Atomic boolean to monitor if search is running
std::vector<TV*> tvList;              // List of TVs found on most recent search.
TV* currentTV;                // Current TV to attempt to change input on
                                                // when controller mode is activated.
int currentHDMI;                                // HDMI port to set the TV to when controller
                                                // mode is activated.
AudioDeviceManager* audioDeviceManager;         // Audio device control object
int saveAudioDefaultDevice = -1;                // Audio device to restore when controller mode is exited.
int controllerModeAudioDevice = -1;             // Audio device to activate when controller mode is activated.


// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE);
BOOL                InitInstance(HINSTANCE);
VOID                InitNotifyTray();
VOID                WriteXmlSettings();
VOID                ReadXmlSettings();
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    ManualAdd(HWND, UINT, WPARAM, LPARAM);
VOID                AddNewDevice();
VOID                ShowContextMenu(POINT);
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
    LoadString(hInstance, IDC_CONTROLLERMODEMONITOR, szWindowClass, MAX_LOADSTRING);
    LoadString(hInstance, IDS_CONTROLLER, szController, MAX_LOADSTRING);
    LoadString(hInstance, IDS_CONFIG_PATH, rawPath, MAX_LOADSTRING);
    hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CONTROLLERMODEMONITOR));
    ExpandEnvironmentStrings(rawPath, szConfigLocation, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance))
    {
        return FALSE;
    }

    int hRes;
    hRes = InitializeWMI();

    if (FAILED(hRes))
    {
        exit(-1);
    }

    audioDeviceManager = new AudioDeviceManager();
    audioDeviceManager->Refresh();

    ReadXmlSettings();
    TriggerTVSearch();
    SetTimer(hWnd, IDT_UPDATETIMER, 100, nullptr);

    

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
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
    wcex.hIcon          = hIcon;
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_CONTROLLERMODEMONITOR);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = wcex.hIcon;//LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates UI elements.
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create a hidden window handle and notification tray entry.
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
    InitNotifyTray();
    
    return TRUE;
}

//
//   FUNCTION: InitNotifyTray(HWND)
//
//   PURPOSE: Creates notification tray entry.
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
VOID InitNotifyTray() {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hWnd;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_GUID | NIF_MESSAGE;
    nid.uVersion = NOTIFYICON_VERSION_4;
    nid.uCallbackMessage = WMAPP_NOTIFYCALLBACK;

    nid.guidItem = TRAY_GUID;

    // This text will be shown as the icon's tooltip.

    LoadString(hInst, IDS_CONTROLLER, nid.szTip, ARRAYSIZE(nid.szTip));

    // Load the icon for high DPI.
    nid.hIcon = hIcon;

    // Show the notification.
    Shell_NotifyIcon(NIM_ADD, &nid) ? S_OK : E_FAIL;
    Shell_NotifyIcon(NIM_SETVERSION, &nid) ? S_OK : E_FAIL;
}

//
//   FUNCTION: ReadXmlSettings()
//
//   PURPOSE: Load the xml configuration file containing TV and controllers.
//
//   COMMENTS:
//
//        In this function, we load the configuration file from the local
//        app data folder and populates monitored device list, TV, and 
//        HDMI port information.
//
VOID ReadXmlSettings() {
    std::filesystem::path configRoot(szConfigLocation);
    std::filesystem::path xmlFile("Configuration.xml");
    std::filesystem::path full_path = configRoot / xmlFile;

    pugi::xml_document configDocument;

    pugi::xml_parse_result status = configDocument.load_file(full_path.c_str());
    if (!status) {
        SendMessage(hWnd, WM_COMMAND, CC_CONFIG_NOT_FOUND, 0);
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
            currentTV = new RokuTV(initInfo);
        }
        tvList.push_back(currentTV);
        currentHDMI = std::stoi(tvNode.child("HDMIPort").child_value());
    }

    pugi::xml_node audioNode = configDocument.child("AudioInfo");
    if (audioNode) {
        pugi::xml_node audioDeviceNode = audioNode.child("ControllerModeAudioDevice");
        std::wstring controllerModeDeviceName = pugi::as_wide(audioDeviceNode.child_value());

        pugi::xml_node audioStoredDefaultDeviceNode = audioNode.child("ControllerModeSavedDefaultAudioDevice");
        std::wstring storedDefaultDeviceName = pugi::as_wide(audioStoredDefaultDeviceNode.child_value());

        for (int j = 0; j < audioDeviceManager->DeviceCount(); j++) {
            std::wstring foundDeviceName = audioDeviceManager->GetName(j);
            if (foundDeviceName == controllerModeDeviceName) {
                controllerModeAudioDevice = j;
            }

            if (foundDeviceName == storedDefaultDeviceName) {
                saveAudioDefaultDevice = j;
            }
        }
        if (controllerModeAudioDevice == -1) {
            SendMessage(hWnd, WM_COMMAND, CC_AUDIO_DEVICE_NOT_FOUND, 0);
        }
        if (saveAudioDefaultDevice != -1) {
            SendMessage(hWnd, WM_COMMAND, CC_AUDIO_DEFAULT_NOT_RESTORED, 0);
        }
    }
}

//
//   FUNCTION: WriteXmlSettings()
//
//   PURPOSE: Save the xml configuration file containing TV and controllers.
//
//   COMMENTS:
//
//        In this function, we save the configuration file to the local
//        app data folder including the monitored device list, TV, and HDMI 
//        port information.
//
VOID WriteXmlSettings() {
    std::filesystem::path configRoot(szConfigLocation);
    std::filesystem::path xmlFile("Configuration.xml");
    std::filesystem::path full_path = configRoot / xmlFile;

    pugi::xml_document configDocument;

    pugi::xml_node devicesNode = configDocument.append_child("MonitoredControllers");
    for (std::wstring deviceName : monitoredDeviceList) {
        pugi::xml_node controllerNode = devicesNode.append_child("DeviceName");
        controllerNode.append_child(pugi::node_pcdata).set_value(pugi::as_utf8(deviceName));
    }

    if (currentTV != nullptr) {
        pugi::xml_node tvNode = configDocument.append_child("TVInfo");

        std::string deviceType;

        if (dynamic_cast<RokuTV*>(currentTV)) {
            deviceType = "Roku";
        }
        tvNode.append_child("TVType").append_child(pugi::node_pcdata).set_value(deviceType);
        tvNode.append_child("TVInitializeInfo").append_child(pugi::node_pcdata).set_value(currentTV->Serialize());
        tvNode.append_child("HDMIPort").append_child(pugi::node_pcdata).set_value(std::to_string(currentHDMI));
    }

    if (controllerModeAudioDevice != -1) {
        pugi::xml_node audioNode = configDocument.append_child("AudioInfo");
        std::wstring audioDeviceName = audioDeviceManager->GetName(controllerModeAudioDevice);

        audioNode.append_child("ControllerModeAudioDevice").append_child(pugi::node_pcdata).set_value(pugi::as_utf8(audioDeviceName));

        if (saveAudioDefaultDevice != -1) {
            audioDeviceName = audioDeviceManager->GetName(controllerModeAudioDevice);
            audioNode.append_child("ControllerModeSavedDefaultAudioDevice").append_child(pugi::node_pcdata).set_value(pugi::as_utf8(audioDeviceName));
        }
    }

    configDocument.save_file(full_path.c_str());
}

BOOL IsStartupRegistered() {
    HKEY hKey;
    LSTATUS status = RegCreateKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ, NULL, &hKey, NULL);

    if (ERROR_SUCCESS == status) {
        status = RegQueryValueEx(hKey, L"ControllerModeMonitor", 0, NULL, NULL, NULL);
        RegCloseKey(hKey);
    }
    return (status == ERROR_SUCCESS);
}

BOOL RegisterAsStartup() {
    HKEY hKey;
    WCHAR exePath[MAX_LOADSTRING];

    // This is probably an unhinged way to
    // build this quoted string but I wanted to know if it would work
    // and it does so ¯\_(ツ)_/¯
    exePath[0] = L'\"';
    GetModuleFileName(NULL, (exePath + 1), MAX_LOADSTRING - 2);
    WCHAR quotedPath[MAX_LOADSTRING];
    int strLength = wcslen(exePath);
    exePath[strLength++] = L'\"';
    exePath[strLength++] = 0;

    LSTATUS status = RegCreateKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);

    if (ERROR_SUCCESS == status) {
        status = RegSetValueEx(hKey, L"ControllerModeMonitor",0 , REG_SZ, (LPBYTE)exePath, strLength * sizeof(WCHAR));
        RegCloseKey(hKey);
    }
    return (status == ERROR_SUCCESS);
}

BOOL UnregisterAsStartup() {
    HKEY hKey;

    LSTATUS status = RegCreateKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);

    if (ERROR_SUCCESS == status) {
        status = RegDeleteKeyValue(hKey, NULL, L"ControllerModeMonitor");
        RegCloseKey(hKey);
    }
    return (status == ERROR_SUCCESS);
}

//
//   FUNCTION: AddNewDevice()
//
//   PURPOSE: Add a new controller device through a series of prompts.
//
//   COMMENTS:
//
//        In this function, we check the list of attached HID devices,
//        ask the user to change the state of the controller (off to on
//        or vice versa), check the list again to deduce the controller
//        desired and confirm that choice with the user. If add is 
//        confirmed the list (and all other configurations) is saved.
//
VOID AddNewDevice() {
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

//
//   FUNCTION: RemoveDevice(UINT)
//
//   PURPOSE: Confirm then remove a controller from the device list.
//
//   COMMENTS:
//
//        In this function, use a prompt to confirm the device indicated
//        by the device index is the one the user intended to remove and
//        if so remove that device from the monitored list and save the
//        list (and all other configurations).
//
VOID RemoveDevice(UINT deviceIndex) {
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

//
//   FUNCTION: UpdateStatus()
//
//   PURPOSE: Check the state of controller to activate/deactivate controller mode.
//
//   COMMENTS:
//
//        In this function, check if any of the monitored devices are connected. If
//        a device is found and controller mode is not activated, it is activated.
//        If controller mode is active and it has been more than 3 seconds since
//        a controller was detected, controller mode is deactivated.
//
VOID UpdateStatus() {
    audioDeviceManager->Refresh();
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
        // Deactivate contoller mode.
        controllerModeActive = false;
        LoadString(hInst, IDS_CMD_BIG_PICTURE_DEACTIVATE, commandText, MAX_LOADSTRING);

        if (saveAudioDefaultDevice != -1) {
            audioDeviceManager->SetDefault(saveAudioDefaultDevice);
            saveAudioDefaultDevice = -1;
            WriteXmlSettings();
        }
    }
    else if (!controllerModeActive && isConnected) {
        // Activate Controller Mode.
        controllerModeActive = true;

        if (currentTV != nullptr) {
            currentTV->SetInput(currentHDMI);
        }

        if (controllerModeAudioDevice != -1) {
            saveAudioDefaultDevice = audioDeviceManager->DefaultIndex();
            audioDeviceManager->SetDefault(controllerModeAudioDevice);
            WriteXmlSettings();
        }

        LoadString(hInst, IDS_CMD_BIG_PICTURE_ACTIVATE, commandText, MAX_LOADSTRING);
    }

    if (commandText[0] != 0) {
        ShellExecute(0, L"open", commandText, nullptr, nullptr, SW_SHOWNORMAL);
    }
}

//
//   FUNCTION: TriggerTVSearch()
//
//   PURPOSE: Start a thread to search for TVs available on the network.
//
//   COMMENTS:
//
//        In this function, if there is not already a thread running then
//        a new thread is created to discover TV devices on the network.
//
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

//
//   FUNCTION: SearchTVs()
//
//   PURPOSE: Run the thread to discover TVs on the network.
//
//   COMMENTS:
//
//        In this function, we send a broadcast signal and monitor
//        collect devices that respond and use that to populate the 
//        list of devices shown in the notification tray icon's context
//        menu. If there is a TV instance created either loaded from
//        the configuration file or selected from a previous search
//        and that TV is not found in the search a message to generate
//        a warning is sent to the main message loop.
//
VOID SearchTVs() {
    std::vector<TV*> tvsFound = RokuTV::SearchDevices();

    for (auto&& child : tvList) {
        if (child != currentTV) {
            delete child;
        }
    }
    tvList = tvsFound;
    if (currentTV != nullptr) {
        bool matchFound = false;
        for (TV* tv : tvList) {
            matchFound |= tv->Equals(currentTV);
        }
        if (!matchFound) {
            SendMessage(hWnd, WM_COMMAND, CC_DEVICE_NOT_FOUND, 0);
            tvList.push_back(currentTV);
        }
    }
    tvSearchRunning = false;
}

//
//   FUNCTION: SetTV(UINT)
//
//   PURPOSE: Change the TV to set when controller mode is activated.
//
//   COMMENTS:
//
//        In this function, we set the instance of the TV controller 
//        to use to turn on and set the input for when controller mode
//        is activated. The selected TV (and all other configurations)
//        is saved.
//
VOID SetTV(UINT index) {
    currentTV = tvList[index];
    WriteXmlSettings();
}

VOID ShowManualTVWindow() {

    if (hWndTVDialog == NULL) {
        hWndTVDialog = CreateDialog(hInst, MAKEINTRESOURCE(IDD_MANUAL_ADD), hWnd, ManualAdd);
        SendMessage(hWndTVDialog, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    }

    if (!IsWindowVisible(hWndTVDialog)) {
        ShowWindow(hWndTVDialog, SW_SHOW);
    }
}

VOID SetAudioDevice(int index) {
    controllerModeAudioDevice = index;
    WriteXmlSettings();
}

//
//   FUNCTION: DisplayBalloonMessage(UINT)
//
//   PURPOSE: Display a balloon message from the notification tray icon.
//
//   COMMENTS:
//
//        In this function, we load the string from a given string resource
//        id and then display that to the user in the form of a notification
//        "balloon" (display as standard notification in Win11).
//
VOID DisplayBalloonMessage(UINT stringID) {
    NOTIFYICONDATA updateData;
    //memset(&updateData, 0, sizeof(NOTIFYICONDATA));
    //ZeroMemory(&updateData, sizeof(updateData));
    updateData.cbSize = sizeof(updateData);
    updateData.hWnd = hWnd;
    updateData.uFlags = NIF_INFO | NIF_GUID;
    updateData.guidItem = TRAY_GUID;
    updateData.uTimeout = 5000;
    LoadString(hInst, stringID, updateData.szInfo, sizeof(updateData.szInfo) / sizeof(WCHAR));
    LoadString(hInst, IDS_BALLOON_TITLE, updateData.szInfoTitle, sizeof(updateData.szInfoTitle) / sizeof(WCHAR));

    //GetLastError();
    int res = Shell_NotifyIcon(NIM_MODIFY, &updateData);
    OutputDebugString(L"Notification Sent.\r\n");
}


//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main (hidden) window.
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
            switch (wmId & CUSTOM_COMMAND_MASK) {
            case CC_DEVICE_ID: {
                UINT deviceNumber = wmId & ~CC_DEVICE_ID;
                RemoveDevice(deviceNumber);
                break;
            }
            case CC_TV_ID: {
                UINT tvNumber = (wmId & ~CC_TV_ID) >> 8;
                currentHDMI = wmId & ~CC_TV_ID & 0xFF;
                SetTV(tvNumber);
                break;
            }
            case CC_AUDIO_ID: {
                int deviceNumber = wmId & ~CC_AUDIO_ID;
                SetAudioDevice(deviceNumber);
                break;
            }
            default:
                switch (wmId)
                {
                case CC_DEVICE_NOT_FOUND:
                    DisplayBalloonMessage(IDS_ERR_TV_NOT_FOUND);
                    break;
                case CC_CONFIG_NOT_FOUND:
                    DisplayBalloonMessage(IDS_WELCOME);
                    break;
                case CC_AUDIO_DEVICE_NOT_FOUND:
                    DisplayBalloonMessage(IDS_AUDIO_DEVICE_NOT_FOUND);
                    break;
                case CC_AUDIO_DEFAULT_NOT_RESTORED:
                    DisplayBalloonMessage(IDS_RESTORE_AUDIO_DEVICE);
                    break;
                case IDM_AUDIO_CLEAR:
                    SetAudioDevice(-1);
                    break;
                case IDM_ABOUT:
                    DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                    break;
                case IDM_ADDDEVICE:
                    AddNewDevice();
                    break;
                case IDM_TV_SEARCH:
                    TriggerTVSearch();
                    break;
                case ID_TV_ADDMANUALLY:
                    ShowManualTVWindow();
                    break;
                case ID_TV_CLEAR:
                    currentTV = NULL;
                    WriteXmlSettings();
                    break;
                case ID_FILE_RUNATLOGIN:
                    if (IsStartupRegistered()) {
                        UnregisterAsStartup();
                    }
                    else {
                        RegisterAsStartup();
                    }
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
                if (!controllerModeActive && saveAudioDefaultDevice != -1) {
                    audioDeviceManager->SetDefault(saveAudioDefaultDevice);
                    saveAudioDefaultDevice = -1;
                    WriteXmlSettings();
                }
                break;

            case NIN_SELECT:
            case WM_CONTEXTMENU:
            {
                POINT const pt = { LOWORD(wParam), HIWORD(wParam) };
                ShowContextMenu(pt);
            }
            break;
        }
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

//
//   FUNCTION: ShowContextMenu(UINT)
//
//   PURPOSE: Display the notification tray icon context menu.
//
//   COMMENTS:
//
//        In this function, we load the the context menu from 
//        the embedded resources, populate the monitored devices
//        and found TVs before displaying to user.
//
void ShowContextMenu(POINT pt)
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
                deviceItem.wID = CC_DEVICE_ID | i;
                deviceItem.dwTypeData = const_cast<LPWSTR>(device.c_str());
                InsertMenuItem(hDeviceMenu, newItemPos, true, &deviceItem);
                i++;
            }


            HMENU hTVMenu = GetSubMenu(hMainMenu, 1);
            i = 0;
            for (TV* tv : tvList) {
                int newItemPos = GetMenuItemCount(hTVMenu);
                std::wstring deviceName = tv->GetName();
                MENUITEMINFOW deviceItem = {};
                bool controllerMatch = currentTV != nullptr && currentTV->Equals(tv);
                deviceItem.cbSize = sizeof(deviceItem);
                deviceItem.fMask = MIIM_STRING | MIIM_ID | MIIM_STATE | MIIM_SUBMENU;
                deviceItem.fState = controllerMatch ? MFS_CHECKED : MFS_UNCHECKED;
                deviceItem.wID = CC_TV_ID | (i << 8) | currentHDMI;
                deviceItem.dwTypeData = const_cast<LPWSTR>(deviceName.c_str());
                
                

                HMENU hHdmiMenu = CreateMenu();

                for (int j = 0; j < tv->HDMICount(); j++) {
                    MENUITEMINFOW hdmiItem = {};
                    
                    hdmiItem.cbSize = sizeof(deviceItem);
                    hdmiItem.fMask = MIIM_STRING | MIIM_ID | MIIM_STATE;
                    hdmiItem.fState = controllerMatch && currentHDMI == j ? MFS_CHECKED : MFS_UNCHECKED;
                    hdmiItem.wID = CC_TV_ID | (i << 8) | j;
                    std::wstring portName = (L"HDMI" + std::to_wstring(j + 1));
                    hdmiItem.dwTypeData = const_cast<LPWSTR>(portName.c_str());

                    InsertMenuItem(hHdmiMenu, newItemPos, true, &hdmiItem);
                }
                deviceItem.hSubMenu = hHdmiMenu;
              
                InsertMenuItem(hTVMenu, newItemPos, true, &deviceItem);

                i++;
            }

            HMENU hAudioMenu = GetSubMenu(hMainMenu, 2);
            for (UINT i = 0; i < audioDeviceManager->DeviceCount(); i++) {
                int newItemPos = GetMenuItemCount(hAudioMenu);
                std::wstring endpointName = audioDeviceManager->GetName(i);

                if (audioDeviceManager->IsDefault(i)) {
                    endpointName += L" (default)";
                }

                MENUITEMINFOW audioEndpointItem = {};
                //bool controllerMatch = currentController != nullptr && currentController->Equals(tv);
                audioEndpointItem.cbSize = sizeof(MENUITEMINFOW);
                audioEndpointItem.fMask = MIIM_STRING | MIIM_ID | MIIM_STATE;
                audioEndpointItem.fState = (i == controllerModeAudioDevice ? MFS_CHECKED : MFS_UNCHECKED) \
                                                | (controllerModeActive ? MFS_DISABLED : MFS_ENABLED) ;
                audioEndpointItem.wID = CC_AUDIO_ID | i;
                audioEndpointItem.dwTypeData = const_cast<LPWSTR>(endpointName.c_str());

                InsertMenuItem(hAudioMenu, newItemPos, true, &audioEndpointItem);
            }

            if (IsStartupRegistered())
            {
                WCHAR menuText[MAX_LOADSTRING];
                GetMenuString(hMainMenu, ID_FILE_RUNATLOGIN, menuText, MAX_LOADSTRING, MF_BYCOMMAND);
                ModifyMenu(hMainMenu, ID_FILE_RUNATLOGIN, MF_BYCOMMAND | MF_CHECKED, ID_FILE_RUNATLOGIN, menuText);
            }

            TrackPopupMenuEx(hMainMenu, uFlags, pt.x, pt.y, hWnd, NULL);
        }
        DestroyMenu(hMenu);
    }
}


//
//   FUNCTION: ShowContextMenu(UINT)
//
//   PURPOSE: Message handler for about box.
//
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

//
//   FUNCTION: ShowContextMenu(UINT)
//
//   PURPOSE: Message handler for manual add box.
//
INT_PTR CALLBACK ManualAdd(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG: {
        WCHAR newItem[] = L"Roku";
        HWND hComboBox = GetDlgItem(hDlg, IDC_TV_COMBO);

        SendMessage(hComboBox, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)newItem);
        SendMessage(hComboBox, CB_SETCURSEL, (WPARAM)0, (LPARAM)0);
        SendMessage(hComboBox, CB_LIMITTEXT, (WPARAM)MAX_IP_LENGTH, (LPARAM)0);
        return (INT_PTR)TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK){
            CHAR inputText[MAX_IP_LENGTH];

            GetDlgItemTextA(hDlg, IDC_IP_ADDRESS, inputText, MAX_IP_LENGTH);

            std::string newIPAddress(inputText);

            if (RokuTV::Validate(newIPAddress)) {
                currentTV = new RokuTV(newIPAddress);
                
                if (!std::any_of(tvList.begin(), tvList.end(), [](TV* t) { return t->Equals(currentTV); })) {
                    tvList.push_back(currentTV);
                }
                WriteXmlSettings();
            }
            else {
                DisplayBalloonMessage(IDS_ERR_INVALID_INPUT);
            }
            OutputDebugStringA(inputText);
        }
        else if (LOWORD(wParam) != IDCANCEL) {
            break;
        }

        HWND hComboBox = GetDlgItem(hDlg, IDC_IP_ADDRESS);
        SendMessage(hComboBox, CB_RESETCONTENT, (WPARAM)0, (LPARAM)0);
        EndDialog(hDlg, LOWORD(wParam));
        return (INT_PTR)TRUE;
    }
    return (INT_PTR)FALSE;
}
