#include "joint/environment_resources.hpp"
#include "unified_root_parameters.hlsli"

#include "common.hlsli"
#include "space_convertions.hlsli"

float3 GetCubeUv(float2 faceUv, uint32_t faceIndex)
{
    const float2 expandedFaceUv = UvToNdc(faceUv);

    switch (faceIndex)
    {
        case 0: return float3(1.0, expandedFaceUv.y, -expandedFaceUv.x);
        case 1: return float3(-1.0, expandedFaceUv.y, expandedFaceUv.x);
        case 2: return float3(expandedFaceUv.x, 1.0, -expandedFaceUv.y);
        case 3: return float3(expandedFaceUv.x, -1.0, expandedFaceUv.y);
        case 4: return float3(expandedFaceUv.x, expandedFaceUv.y, 1.0);
        case 5: return float3(-expandedFaceUv.x, expandedFaceUv.y, -1.0);
    }

    return g_NaN;
}

float2 ConvertUnitCartesianToSpherical(float3 cartesian)
{
    const float phi = atan2(cartesian.z, cartesian.x); // Azimuthal angle
    const float theta = acos(cartesian.y); // Polar angle

    return float2(phi, theta);
}

float2 ConvertSphericalToUv(float phi, float theta)
{
    const float u = 0.5f - phi / g_TwoPi;
    const float v = theta / g_Pi;

    return float2(u, v);
}

BenzinDeclareRootResource(Texture2D<float4>, g_EquirectangularTexture, joint::EquirectangularToCubeResources::EquirectangularTexture);
BenzinDeclareRootResource(RWTexture2DArray<float4>, g_OutCubeMap, joint::EquirectangularToCubeResources::OutCubeMap);

[numthreads(8, 8, 1)]
void CsMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    float width;
    float height;
    float depth;
    g_OutCubeMap.GetDimensions(width, height, depth);

    const float2 faceUv = (dispatchThreadId.xy + 0.5) / float2(width, height);
    const float3 cubeUV = GetCubeUv(faceUv, dispatchThreadId.z);

    const float3 direction = normalize(cubeUV);
    const float2 spherical = ConvertUnitCartesianToSpherical(direction);
    const float2 equirectangularUv = ConvertSphericalToUv(spherical.x, spherical.y);

    g_OutCubeMap[dispatchThreadId] = g_EquirectangularTexture.SampleLevel(g_LinearClampSampler, equirectangularUv, 0.0);
}