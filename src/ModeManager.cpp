#include "ModeManager.h"

#include <time.h>

#include "../resource.h"
#include "ControllerMonitor.h"
#include "SteamStatus.h"

//
//   FUNCTION: Init()
//
//   PURPOSE: Initialize the mode manager. 
//
//   COMMENTS:
//
//        Initialize the WMI connection used to monitor devices and audio device manager.
//
HRESULT ModeManager::Init(VOID(*writeSettingsCallback)()) {
    HRESULT hRes;
    hRes = InitializeWMI();
    if (FAILED(hRes))
    {
        return hRes;
    }
    hRes = audioDeviceManager.Init();
    if (FAILED(hRes))
    {
        return hRes;
    }
    audioDeviceManager.Refresh();
    this->WriteSettingsCallback = writeSettingsCallback;
    return hRes;
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
//        a controller was detected, controller mode is deactivated. Settings may be
//        saved if controller mode is activated to store previous default audio device.
//
VOID ModeManager::UpdateStatus() {
    audioDeviceManager.Refresh();
    BOOL isConnected = IsDeviceConnected();

    if (!isConnected) {
        time_t currentTime = clock();
        isConnected = (currentTime - timeLastSeen) < 3000;
    }
    else {
        timeLastSeen = clock();
    }

    if (controllerModeActive && !isConnected) {
        // Only exit controller mode if the running game has been closed.
        if (!SteamIsGameRunning()) {
            // Deactivate contoller mode.
            controllerModeActive = false;

            if (savedAudioDefaultDevice != -1) {
                audioDeviceManager.SetDefault(savedAudioDefaultDevice);
                savedAudioDefaultDevice = -1;
                this->WriteSettingsCallback();
            }
            SteamExitBigPicture();
        }
    }
    else if (!controllerModeActive && isConnected) {
        // Activate Controller Mode.
        controllerModeActive = true;

        if (currentTV != nullptr) {
            currentTV->SetInput(currentHDMI);
        }

        if (controllerModeAudioDevice != -1) {
            savedAudioDefaultDevice = audioDeviceManager.DefaultIndex();
            audioDeviceManager.SetDefault(controllerModeAudioDevice);
            this->WriteSettingsCallback();
        }
        SteamActivateBigPicture();
    }
}

//
//   FUNCTION: RestoreAudioDevice()
//
//   PURPOSE: Restore the default audio device after premature exit.
// 
//   COMMENTS:
//        The configuration will be saved to clear the saved default
//        audio device.
//
VOID ModeManager::RestoreAudioDevice() {
    if (!controllerModeActive && savedAudioDefaultDevice != -1) {
        audioDeviceManager.SetDefault(savedAudioDefaultDevice);
        savedAudioDefaultDevice = -1;
        this->WriteSettingsCallback();
    }
}
