#pragma once
#include "TV.h"
#include "pugixml.hpp"

class RokuTVController : public TVController {
public:
	RokuTVController(std::string);
	static std::list<TVController*> SearchDevices();
	void SetInput(int HDMINumber);
	std::wstring GetName();
	std::wstring Serialize();
	bool Equals(TVController*);
private:
	std::string ipAddress;
	pugi::xml_document* status;
	void SendCommand(std::string);
	void UpdateStatus();
};