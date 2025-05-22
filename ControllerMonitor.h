#pragma once

#include "framework.h"
#include <set>
#include <string>

BOOL InitializeWMI();
BOOL IsDeviceConnected();
VOID CloseMonitor();
std::set<std::wstring> GetDeviceList();

extern std::set<std::wstring> monitoredDeviceList;