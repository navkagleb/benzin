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

#include <DirectXMesh.h>
#pragma comment(lib, "DirectXMesh.lib")

#include <third_party/directx/D3D12MeshShaders/MeshletGenerator/D3D12MeshletGenerator.h>

#include <packages/dxc/inc/dxcapi.h>
#include <packages/dxc/inc/d3d12shader.h>
#pragma comment(lib, "dxcompiler.lib")

#if defined(BENZIN_PROJECT)
extern "C"
{
    __declspec(dllexport) extern const UINT D3D12SDKVersion = 610;
    __declspec(dllexport) extern const char* D3D12SDKPath = "./";
}
#endif
