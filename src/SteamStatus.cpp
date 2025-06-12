#include "SteamStatus.h"

#include <shellapi.h>

#include "../resource.h"

VOID SteamSendCommandString(UINT);

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
VOID SteamActivateBigPicture() {
	SteamSendCommandString(IDS_CMD_BIG_PICTURE_ACTIVATE);
}
VOID SteamExitBigPicture() {
	SteamSendCommandString(IDS_CMD_BIG_PICTURE_DEACTIVATE);
}

VOID SteamSendCommandString(UINT stringID) {
	WCHAR commandText[MAX_LOADSTRING];
	LoadString(hInst, stringID, commandText, MAX_LOADSTRING);
	ShellExecute(0, L"open", commandText, nullptr, nullptr, SW_SHOWNORMAL);

}