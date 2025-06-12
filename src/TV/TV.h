#pragma once

#include "../framework.h"

class TV {
public:
	virtual void SetInput(int HDMINumber) = 0;
	virtual std::wstring GetName() = 0;
	virtual std::string Serialize() = 0;
	virtual bool Equals(std::shared_ptr<TV>) = 0;
	virtual int HDMICount() = 0;
};