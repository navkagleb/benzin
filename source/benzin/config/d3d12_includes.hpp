#pragma once

#include <d3d12.h>
#pragma comment(lib, "d3d12.lib")

#include <dxgi1_6.h>
#include <dxgidebug.h>
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

#include <DirectXCollision.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>

// DirectX Agile SDK
// Ref: https://devblogs.microsoft.com/directx/gettingstarted-dx12agility/
#if defined(BENZIN_PROJECT)
extern "C"
{
    __declspec(dllexport) extern const UINT D3D12SDKVersion = BENZIN_AGILE_SDK_VERSION;
    __declspec(dllexport) extern const char* D3D12SDKPath = BENZIN_AGILE_SDK_PATH;
}
#endif

// DXC
#include <d3d12shader.h>
#include <dxcapi.h>
#include <dxcerrors.h>
#pragma comment(lib, "dxcompiler.lib")
