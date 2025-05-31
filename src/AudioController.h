#pragma once
#include <string>

#include "PolicyConfig.h"

class AudioDeviceController {
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
	AudioDeviceController();
	~AudioDeviceController();
private:
	std::wstring GetStringProp(UINT, PROPERTYKEY);
	IMMDeviceEnumerator* pEnumerator = NULL;
	IMMDeviceCollection* audioDevices = NULL;
	UINT deviceCount = 0;
	UINT defaultIndex = 0;
};