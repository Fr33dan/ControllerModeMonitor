#pragma once
#include <string>

class TVController {
public:
	virtual void SetInput(int HDMINumber) = 0;
	virtual std::wstring GetName() = 0;
	virtual std::wstring Serialize() = 0;
	virtual bool Equals(TVController*) = 0;
	virtual int HDMICount() = 0;
};