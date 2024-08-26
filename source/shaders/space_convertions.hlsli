#pragma once

float3 ClipPositionToNdcPosition(float4 clipPosition)
{
    return clipPosition.xyz / clipPosition.w;
}

float2 NdcPositionToUv(float3 ndcPosition)
{
    float2 uv = ndcPosition.xy * 0.5 + 0.5; // [-1, 1] -> [0, 1]
    uv.y = 1.0 - uv.y; // Invert for DirectX

    return uv;
}

float2 DispatchThreadIdToUv(uint3 dispatchThreadId, float2 invDimensions)
{
    return (dispatchThreadId.xy + 0.5) * invDimensions;
}

float2 DispatchThreadIdToUv(uint2 dispatchThreadId, float2 invDimensions)
{
    return DispatchThreadIdToUv(uint3(dispatchThreadId, 0), invDimensions);
}

float2 ExpandUv(float2 uv)
{
    uv.y = 1.0 - uv.y;
    return 2.0 * uv - 1.0; // [0, 1] -> [-1, 1]
}
