#pragma once
#include "TV.h"

class RokuTVController : public TVController {
public:
	RokuTVController(std::string);
	static std::list<TVController*> SearchDevices();
	void SetInput(int HDMINumber);
	std::wstring GetName();
private:
	std::string ipAddress;
};