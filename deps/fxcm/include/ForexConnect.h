#pragma once

// Ensure Windows basic types (SYSTEMTIME etc.) defined before SDK nested headers
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "forexconnect/ForexConnect.h"
#include "pricehistorymgr/pricehistorymgr.h"
#include "quotesmgr/quotesmgr2.h"
