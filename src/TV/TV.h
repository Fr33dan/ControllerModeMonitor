#pragma once

#include "../framework.h"

class TVController {
public:
	virtual void SetInput(int HDMINumber) = 0;
	virtual std::wstring GetName() = 0;
	virtual std::string Serialize() = 0;
	virtual bool Equals(TVController*) = 0;
	virtual int HDMICount() = 0;
};