#pragma once

float3 ClipToNdc(float4 clip)
{
    return clip.xyz / clip.w;
}

float2 NdcToUv(float2 ndc)
{
    float2 uv = ndc.xy * 0.5 + 0.5;
    uv.y = 1.0 - uv.y; // Invert for DirectX

    return uv;
}

float2 ClipToUv(float4 clip)
{
    return NdcToUv(ClipToNdc(clip).xy);
}

float2 UvToNdc(float2 uv)
{
    uv.y = 1.0 - uv.y; // Invert for DirectX

    return uv * 2.0 - 1.0;
}

float3 ReconstructViewPosition(float2 uv, float viewDepth, float2 uvToViewScale, float2 uvToViewBias)
{
    const float2 normalizedViewPosition = uv * uvToViewScale + uvToViewBias; // Normalized by viewDepth
    const float3 viewPosition = float3(normalizedViewPosition, 1.0) * viewDepth;

    return viewPosition;
}

float3 ReconstructViewPosition(float2 uv, float depth, float4x4 clipToView)
{
    const float4 clipPosition = float4(UvToNdc(uv), depth, 1.0);
    const float4 viewPosition = mul(clipPosition, clipToView);

    return viewPosition.xyz / viewPosition.w;
}

float3 ReconstructWorldPosition(float2 uv, float depth, float4x4 clipToView, float4x4 viewToWorld)
{
    const float3 viewPosition = ReconstructViewPosition(uv, depth, clipToView);
    const float3 worldPosition = mul(float4(viewPosition, 1.0), viewToWorld).xyz;

    return worldPosition;
}
