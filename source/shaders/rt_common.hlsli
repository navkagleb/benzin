#pragma once

#include "common.hlsli"

struct Ray
{
    float3 Origin;
    float3 Direction;
};

Ray GetObjectRay()
{
    Ray objectRay;
    objectRay.Origin = ObjectRayOrigin();
    objectRay.Direction = ObjectRayDirection();

    return objectRay;
}
    
Ray TransformRay(Ray ray, float4x4 transformMatrix)
{
    Ray transformedRay;
    transformedRay.Origin = mul(float4(ray.Origin, 1.0f), transformMatrix).xyz;
    transformedRay.Direction = normalize(mul(ray.Direction, (float3x3)transformMatrix)); // Drop the transition

    return transformedRay;
};
    
float3 GetRayHitPosition(Ray ray, float t)
{
    return ray.Origin + t * ray.Direction;
}

float2 GetRayUv()
{
    return (DispatchRaysIndex().xy + 0.5f) / DispatchRaysDimensions().xy;
}

float2 GetRayScreenPosition()
{
    const float2 texelCenter = DispatchRaysIndex().xy + 0.5f;

    float2 screenPosition = texelCenter / DispatchRaysDimensions().xy * 2.0f - 1.0f;
    screenPosition.y = -screenPosition.y; // Invert Y for DirectX-style coordinates

    return screenPosition;
}

bool IsRayCulled(Ray ray, float3 hitSurfaceNormal)
{
    // Test if a hit is culled based on specified RayFlags

    const float rayDirectionNormalDot = dot(ray.Direction, hitSurfaceNormal);

    const bool isCulled =
        ((RayFlags() & RAY_FLAG_CULL_BACK_FACING_TRIANGLES) && rayDirectionNormalDot > 0.0f) ||
        ((RayFlags() & RAY_FLAG_CULL_FRONT_FACING_TRIANGLES) && rayDirectionNormalDot < 0.0f);

    return isCulled;
}

bool IsValidRayHit(Ray ray, float thit, float3 hitSurfaceNormal)
{
    return IsInRange(thit, RayTMin(), RayTCurrent()) && !IsRayCulled(ray, hitSurfaceNormal);
}
