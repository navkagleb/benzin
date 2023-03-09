#pragma once

#include <d3d12.h>
#include <third_party/directx/d3dx12.h>
#pragma comment(lib, "d3d12.lib")

#if defined(BENZIN_DEBUG)
	#include <dxgidebug.h>
	#pragma comment(lib, "dxguid.lib")
#endif

#include <dxgi1_6.h>
#pragma comment(lib, "dxgi.lib")

#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

#include <third_party/directx/DDSTextureLoader12.h>

#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXCollision.h>

#include <dxcapi.h>
#pragma comment(lib, "dxcompiler.lib")

#if defined(BENZIN_PROJECT)
extern "C"
{
	__declspec(dllexport) extern const UINT D3D12SDKVersion = 608;
	__declspec(dllexport) extern const char* D3D12SDKPath = "./";
}
#endif
