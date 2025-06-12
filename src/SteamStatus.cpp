#include "SteamStatus.h"

#include <shellapi.h>

#include "../resource.h"

VOID SteamSendCommandString(UINT);

//
//   FUNCTION: SteamIsGameRunning()
//
//   PURPOSE: Check the registry to determine if a game is currently running.
// 
BOOL SteamIsGameRunning() {
	HKEY hKey;
	LSTATUS status = RegCreateKeyEx(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ, NULL, &hKey, NULL);
	DWORD appID;
	DWORD dataSize = sizeof(appID);

	if (ERROR_SUCCESS == status) {
		status = RegQueryValueEx(hKey, L"RunningAppID", 0, NULL, (LPBYTE) &appID, &dataSize);
		RegCloseKey(hKey);
	}
	return (status == ERROR_SUCCESS && appID);
}
BOOL SteamIsBigPictureActivated();

//
//   FUNCTION: SteamActivateBigPicture()
//
//   PURPOSE: Send message to Steam to enter Big Picture mode.
// 
VOID SteamActivateBigPicture() {
	SteamSendCommandString(IDS_CMD_BIG_PICTURE_ACTIVATE);
}

//
//   FUNCTION: SteamExitBigPicture()
//
//   PURPOSE: Send message to Steam to exit Big Picture mode.
// 
VOID SteamExitBigPicture() {
	SteamSendCommandString(IDS_CMD_BIG_PICTURE_DEACTIVATE);
}

//
//   FUNCTION: SteamSendCommandString()
//
//   PURPOSE: Load a command string from the resources and run it to send to Steam.
// 
VOID SteamSendCommandString(UINT stringID) {
	WCHAR commandText[MAX_LOADSTRING];
	LoadString(hInst, stringID, commandText, MAX_LOADSTRING);
	ShellExecute(0, L"open", commandText, nullptr, nullptr, SW_SHOWNORMAL);

}