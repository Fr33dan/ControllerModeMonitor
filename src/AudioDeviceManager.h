#pragma once

#include "framework.h"
#include "PolicyConfig.h"

class AudioDeviceManager {
public:
	VOID SetDefault(UINT);
	std::wstring GetDefaultName();
	std::wstring GetDefaultID();
	std::wstring GetName(UINT);
	std::wstring GetID(UINT);
	BOOL IsDefault(UINT);
	UINT DefaultIndex();
	UINT DeviceCount();
	VOID Refresh();
	AudioDeviceManager();
	~AudioDeviceManager();
private:
	std::wstring GetStringProp(UINT, PROPERTYKEY);
	IMMDeviceEnumerator* pEnumerator = NULL;
	IMMDeviceCollection* audioDevices = NULL;
	UINT deviceCount = 0;
	UINT defaultIndex = 0;
};