// header.h : include file for standard system include files,
// or project specific include files
//

#pragma once

#include "../targetver.h"
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>
// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
// Common C++ Standard libraries
#include <vector>
#include <string>
#include <memory>

// Common Defines
#define MAX_LOADSTRING 100

// Application global variables.
extern HINSTANCE hInst;                                // current instance