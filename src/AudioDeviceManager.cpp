#include "AudioDeviceManager.h"

#include "propvarutil.h"

#pragma comment(lib, "Propsys.lib")

#define MAX_STR_LENGTH 50

HRESULT RegisterDevice(LPCWSTR, ERole);

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);

AudioDeviceManager::AudioDeviceManager() {
	HRESULT hr = S_OK;
	hr = CoCreateInstance(
		CLSID_MMDeviceEnumerator, NULL,
		CLSCTX_ALL, IID_IMMDeviceEnumerator,
		(void**)&pEnumerator);
}

AudioDeviceManager::~AudioDeviceManager() {
	if (this->pEnumerator != NULL) {
		this->pEnumerator->Release();
	}
	if (this->audioDevices != NULL) {
		this->audioDevices->Release();
	}
}

std::wstring AudioDeviceManager::GetDefaultID() {
	return this->GetID(this->defaultIndex);
}

std::wstring AudioDeviceManager::GetDefaultName() {
	return this->GetName(this->defaultIndex);
}

std::wstring AudioDeviceManager::GetID(UINT index) {
	return this->GetStringProp(index, PKEY_Device_InstanceId);
}

std::wstring AudioDeviceManager::GetName(UINT index) {
	return this->GetStringProp(index, PKEY_Device_FriendlyName);
}

BOOL AudioDeviceManager::IsDefault(UINT index) {
	return index == this->defaultIndex;
}

VOID AudioDeviceManager::SetDefault(UINT index) {
	IMMDevice* audioEndpoint;
	LPWSTR deviceID = NULL;
	this->audioDevices->Item(index, &audioEndpoint);
	audioEndpoint->GetId(&deviceID);
	RegisterDevice(deviceID, eMultimedia);

	CoTaskMemFree(deviceID);
}

UINT AudioDeviceManager::DeviceCount() {
	return this->deviceCount;
}

UINT AudioDeviceManager::DefaultIndex() {
	return this->defaultIndex;
}

std::wstring AudioDeviceManager::GetStringProp(UINT index, PROPERTYKEY key) {
	WCHAR propertyStrBuffer[MAX_STR_LENGTH];
	IMMDevice* audioEndpoint;
	IPropertyStore* devProperties;
	PROPVARIANT nameProp;

	PropVariantInit(&nameProp);
	this->audioDevices->Item(index, &audioEndpoint);
	audioEndpoint->OpenPropertyStore(STGM_READ, &devProperties);
	devProperties->GetValue(key, &nameProp);
	std::wstring devName;
	if (nameProp.vt != VT_EMPTY) {
		PropVariantToString(nameProp, propertyStrBuffer, MAX_STR_LENGTH);
		devName = std::wstring(propertyStrBuffer, wcslen(propertyStrBuffer));
	}
	else {
		devName = L"";
	}
	devProperties->Release();
	audioEndpoint->Release();
	return devName;
}

VOID AudioDeviceManager::Refresh() {
	LPWSTR defaultID = NULL;
	LPWSTR deviceID = NULL;
	HRESULT hr = S_OK;
	IMMDevice* audioEndpoint;

	pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &audioEndpoint);

	audioEndpoint->GetId(&defaultID);
	audioEndpoint->Release();

	if (this->audioDevices != NULL) {
		this->audioDevices->Release();
	}
	pEnumerator->EnumAudioEndpoints(eRender, eMultimedia, &this->audioDevices);

	this->audioDevices->GetCount(&this->deviceCount);

	for (int i = 0; i < this->deviceCount;i++) {
		
		this->audioDevices->Item(i, &audioEndpoint);
		audioEndpoint->GetId(&deviceID);

		if (!wcscmp(defaultID, deviceID)) {
			this->defaultIndex = i;
		}

		CoTaskMemFree(deviceID);
		audioEndpoint->Release();
	}
	CoTaskMemFree(defaultID);

}

// See: http://social.microsoft.com/Forums/en/Offtopic/thread/9ebd7ad6-a460-4a28-9de9-2af63fd4a13e
HRESULT RegisterDevice(LPCWSTR devID, ERole role)
{
	IPolicyConfig* pPolicyConfig = nullptr;

	HRESULT hr = CoCreateInstance(__uuidof(CPolicyConfigClient), NULL, CLSCTX_ALL, __uuidof(IPolicyConfig), (LPVOID*)&pPolicyConfig);
	if (pPolicyConfig == nullptr) {
		hr = CoCreateInstance(__uuidof(CPolicyConfigClient), NULL, CLSCTX_ALL, __uuidof(IPolicyConfig10), (LPVOID*)&pPolicyConfig);
	}
	if (pPolicyConfig == nullptr) {
		hr = CoCreateInstance(__uuidof(CPolicyConfigClient), NULL, CLSCTX_ALL, __uuidof(IPolicyConfig7), (LPVOID*)&pPolicyConfig);
	}
	if (pPolicyConfig == nullptr) {
		hr = CoCreateInstance(__uuidof(CPolicyConfigClient), NULL, CLSCTX_ALL, __uuidof(IPolicyConfigVista), (LPVOID*)&pPolicyConfig);
	}
	if (pPolicyConfig == nullptr) {
		hr = CoCreateInstance(__uuidof(CPolicyConfigClient), NULL, CLSCTX_ALL, __uuidof(IPolicyConfig10_1), (LPVOID*)&pPolicyConfig);
	}

	if (pPolicyConfig != NULL) {
		hr = pPolicyConfig->SetDefaultEndpoint(devID, role);
		pPolicyConfig->Release();
	}
	return hr;
}