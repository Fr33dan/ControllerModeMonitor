#pragma once
#include "TV.h"

class RokuTVController : public TVController {
public:
	RokuTVController(std::string);
	static std::list<TVController*> SearchDevices();
	void SetInput(int HDMINumber);
private:
	std::string ipAddress;
};