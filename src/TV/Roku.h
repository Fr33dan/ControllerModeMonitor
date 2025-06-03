#pragma once
#include <vector>
#include "TV.h"
#include "pugixml.hpp"

class RokuTVController : public TVController {
public:
	RokuTVController(std::string);
	static std::vector<TVController*> SearchDevices();
	void SetInput(int HDMINumber);
	std::wstring GetName();
	std::wstring Serialize();
	bool Equals(TVController*);
	int HDMICount();
private:
	std::string ipAddress;
	pugi::xml_document* deviceInfo;
	pugi::xml_document* mediaPlayer;
	void SendCommand(std::string);
	void UpdateDocument(std::string, pugi::xml_document*);
	BOOL IsAtHDMI();
};