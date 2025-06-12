#include "ModeManager.h"

#include <time.h>
#include <shellapi.h>

#include "ControllerMonitor.h"
#include "../resource.h"

HRESULT ModeManager::Init(VOID(*writeSettingsCallback)()) {
    HRESULT hRes;
    hRes = InitializeWMI();
    if (FAILED(hRes))
    {
        return hRes;
    }
    this->WriteSettingsCallback = writeSettingsCallback;
    audioDeviceManager.Refresh();
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
VOID ModeManager::UpdateStatus() {
    audioDeviceManager.Refresh();
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
            audioDeviceManager.SetDefault(saveAudioDefaultDevice);
            saveAudioDefaultDevice = -1;
            this->WriteSettingsCallback();
        }
    }
    else if (!controllerModeActive && isConnected) {
        // Activate Controller Mode.
        controllerModeActive = true;

        if (currentTV != nullptr) {
            currentTV->SetInput(currentHDMI);
        }

        if (controllerModeAudioDevice != -1) {
            saveAudioDefaultDevice = audioDeviceManager.DefaultIndex();
            audioDeviceManager.SetDefault(controllerModeAudioDevice);
            this->WriteSettingsCallback();
        }

        LoadString(hInst, IDS_CMD_BIG_PICTURE_ACTIVATE, commandText, MAX_LOADSTRING);
    }

    if (commandText[0] != 0) {
        ShellExecute(0, L"open", commandText, nullptr, nullptr, SW_SHOWNORMAL);
    }
}

VOID ModeManager::RestoreAudioDevice() {
    if (!controllerModeActive && saveAudioDefaultDevice != -1) {
        audioDeviceManager.SetDefault(saveAudioDefaultDevice);
        saveAudioDefaultDevice = -1;
        this->WriteSettingsCallback();
    }
}
