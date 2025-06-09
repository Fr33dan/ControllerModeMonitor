#pragma once

#include <set>

#include "framework.h"

BOOL InitializeWMI();
BOOL IsDeviceConnected();
VOID CloseMonitor();
std::set<std::wstring> GetDeviceList();

extern std::set<std::wstring> monitoredDeviceList;