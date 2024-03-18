#pragma once

#include <d3d12.h>
#pragma comment(lib, "d3d12.lib")

#if defined(BENZIN_DEBUG_BUILD)
  #include <dxgidebug.h>
  #pragma comment(lib, "dxguid.lib")
#endif

#include <dxgi1_6.h>
#pragma comment(lib, "dxgi.lib")

#include <third_party/directx/DDSTextureLoader12.h>

#include <DirectXCollision.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>

// DirectX Agile SDK
#if defined(BENZIN_PROJECT)
extern "C"
{
    __declspec(dllexport) extern const UINT D3D12SDKVersion = BENZIN_AGILE_SDK_VERSION;
    __declspec(dllexport) extern const char* D3D12SDKPath = BENZIN_AGILE_SDK_PATH;
}
#endif

// DXC
#include <packages/Microsoft.Direct3D.DXC.1.7.2308.12/build/native/include/d3d12shader.h>
#include <packages/Microsoft.Direct3D.DXC.1.7.2308.12/build/native/include/dxcapi.h>
#include <packages/Microsoft.Direct3D.DXC.1.7.2308.12/build/native/include/dxcerrors.h>

#pragma comment(lib, "dxcompiler.lib")
