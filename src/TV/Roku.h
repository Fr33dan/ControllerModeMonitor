#pragma once

#include "pugixml.hpp"

#include "../framework.h"
#include "TV.h"

class RokuTV : public TV {
public:
	RokuTV(std::string);
	static std::vector<TV*> SearchDevices();
	void SetInput(int HDMINumber);
	std::wstring GetName();
	std::string Serialize();
	static bool Validate(std::string&);
	bool Equals(TV*);
	int HDMICount();
private:
	std::string ipAddress;
	pugi::xml_document* deviceInfo;
	pugi::xml_document* mediaPlayer;
	void SendCommand(std::string);
	void UpdateDocument(std::string, pugi::xml_document*);
	BOOL IsAtHDMI();
};