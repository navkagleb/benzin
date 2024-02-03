#pragma once

#ifdef __cplusplus

  #define uint uint32_t
  
  #define float2 DirectX::XMFLOAT2
  #define float3 DirectX::XMFLOAT3
  #define float4 DirectX::XMFLOAT4
  
  #define float4x4 DirectX::XMMATRIX

  static_assert(sizeof(float2) == sizeof(float) * 2);
  static_assert(sizeof(float3) == sizeof(float) * 3);
  static_assert(sizeof(float4) == sizeof(float) * 4);

#endif
