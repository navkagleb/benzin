#pragma once

#define NOMINMAX
#include <Windows.h>

#undef min
#undef max

#include <comdef.h>
#include <synchapi.h>

#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

#define WINDOWS_MESSAGE_HANDLER_FUNC_NAME WindowsMessageHandler
#define WindowsMessageHandlerDeclaration() LRESULT WINDOWS_MESSAGE_HANDLER_FUNC_NAME(HWND, UINT, WPARAM, LPARAM)
