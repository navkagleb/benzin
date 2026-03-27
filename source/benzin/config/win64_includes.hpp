#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#undef min
#undef max

#include <comdef.h>
#include <synchapi.h>

#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
