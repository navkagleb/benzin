#pragma once

// NOTE: include this file after "unified_root_parameters.hlsli" include file !!!

bool IsInFrustum(float3 worldCenter, float worldRadius)
{
    for (uint i = 0; i < (uint)joint::FrustumPlane::Count; ++i)
    {
        const float4 frustumPlane = GetCameraConsts().WorldFrustumPlanes[i];
        const float distance = dot(frustumPlane.xyz, worldCenter) + frustumPlane.w;

        if (distance > worldRadius)
        {
            return false;
        }
    }

    return true;
}

bool IsInFrustum(float3 center, float radius, float4x4 localToWorld)
{
    const float localToWorldScaleX = length(localToWorld[0].xyz);
    const float localToWorldScaleY = length(localToWorld[1].xyz);
    const float localToWorldScaleZ = length(localToWorld[2].xyz);
    const float localToWorldScale = max(max(localToWorldScaleX, localToWorldScaleY), localToWorldScaleZ);

    const float3 worldCenter = mul(float4(center, 1.0), localToWorld).xyz;
    const float worldRadius = radius * localToWorldScale;

    return IsInFrustum(worldCenter, worldRadius);
}
