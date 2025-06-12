#pragma once

#include "framework.h"

#include "AudioDeviceManager.h"
#include "TV\TV.h"

class ModeManager {
public:
	HRESULT Init(VOID(*)());						// Initialize anything needed to run status update loop.
	VOID UpdateStatus();							// Check if the controller mode status needs updating and 
													// update the status if needed.
	VOID RestoreAudioDevice();						// Restore the audio device after a failure.
	int currentHDMI = 0;                            // HDMI port to set the TV to when controller
													// mode is activated.

	AudioDeviceManager audioDeviceManager;			// Audio device control object
	clock_t timeLastSeen = -3000;					// Clock time the controller was last seen
	BOOL controllerModeActive = false;				// If controller mode is currently activated
	std::shared_ptr<TV> currentTV;		            // Current TV to attempt to change input on
													// when controller mode is activated.
	int savedAudioDefaultDevice = -1;                // Audio device to restore when controller mode is exited.
	int controllerModeAudioDevice = -1;             // Audio device to activate when controller mode is activated.
private:
	VOID(*WriteSettingsCallback)();
};