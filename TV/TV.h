#pragma once
#include <list>
#include <string>

class TVController {
public:
	virtual void SetInput(int HDMINumber) = 0;
	virtual std::wstring GetName() = 0;
};