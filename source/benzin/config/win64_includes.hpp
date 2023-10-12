#pragma once

#define NOMINMAX
#include <Windows.h>

#undef min
#undef max

#include <comdef.h>
#include <synchapi.h>

#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
