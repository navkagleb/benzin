// Ref: https://www.shadertoy.com/view/3sfBWs - Blue Noise RT Shadows TAA: Dir
// Ref: https://momentsingraphics.de/BlueNoise.html - Free blue noise textures
// Ref: https://blog.demofox.org/2020/05/16/using-blue-noise-for-raytraced-soft-shadows/ - Using Blue Noise For Raytraced Soft Shadows
// Ref: https://blog.demofox.org/2017/10/31/animating-noise-for-integration-over-time/ - Animating Noise For Integration Over Time
// Ref: https://blog.demofox.org/2017/11/03/animating-noise-for-integration-over-time-2-uniform-over-time/ - Animating Noise For Integration Over Time 2: Uniform Over Time

#include "joint/ray_tracing_shadow_resources.hpp"
#include "unified_root_parameters.hlsli"

#include "common.hlsli"
#include "sigma_denoiser/sigma_public.hlsli"
#include "space_convertions.hlsli"

BenzinDeclareRootResource(Texture2D<float4>, g_WorldNormal, joint::RayTracing_ShadowResources::WorldNormal);
BenzinDeclareRootResource(Texture2D<float>, g_Depth, joint::RayTracing_ShadowResources::Depth);
BenzinDeclareRootResource(Texture2D<float2>, g_BlueNoise, joint::RayTracing_ShadowResources::BlueNoise);
BenzinDeclareRootResource(RWTexture2D<float>, g_NoisyPenumbra, joint::RayTracing_ShadowResources::NoisyPenumbra);

float3 OffsetRayPosition(float3 position, float3 normal)
{
    // Normal points outward for rays exiting the surface, else is flipped

    const float rayOrigin = 1.0 / 32.0;
    const float floatScale = 1.0 / 65536.0;
    const float intScale = 256.0;

    const int3 intOffset = intScale * normal;

    const float3 intPosition = float3(
        asfloat(asint(position.x) + ((position.x < 0.0) ? -intOffset.x : intOffset.x)),
        asfloat(asint(position.y) + ((position.y < 0.0) ? -intOffset.y : intOffset.y)),
        asfloat(asint(position.z) + ((position.z < 0.0) ? -intOffset.z : intOffset.z)));

    return float3(
        abs(position.x) < rayOrigin ? position.x + floatScale * normal.x : intPosition.x,
        abs(position.y) < rayOrigin ? position.y + floatScale * normal.y : intPosition.y,
        abs(position.z) < rayOrigin ? position.z + floatScale * normal.z : intPosition.z);
}

float2 Hash23(float3 p3)
{
    // White noise
    // Ref: https://www.shadertoy.com/view/4djSRW

    p3 = frac(p3 * float3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);

    return frac((p3.xx + p3.yz) * p3.zy);
}

float2 GetWhiteNoise()
{
    const uint frameIndex = g_PassConsts.m_IsNoiseAnimated * g_FrameConsts.CpuFrameIndex;
    return Hash23(float3(DispatchRaysIndex().xy, frameIndex));
}

float2 GetBlueNoise()
{
    if (!g_PassConsts.m_IsBlueNoiseUsed)
        return GetWhiteNoise();

    float width;
    float height;
    g_BlueNoise.GetDimensions(width, height);

    const float2 uv = DispatchRaysIndex().xy / width;
    float2 blueNoise = g_BlueNoise.SampleLevel(g_PointWrapSampler, uv, 0.0).rg;

    if (g_PassConsts.m_IsNoiseAnimated)
    {
        const float goldenRatioConjugate = 0.61803398875; // frac(GoldenRatio)
        const float maxFrameCount = 4;

        const uint frameIndex = g_FrameConsts.CpuFrameIndex % maxFrameCount;
        blueNoise = frac(blueNoise + goldenRatioConjugate * frameIndex);
    }

    return blueNoise;
}

float3 CreateRandomUnitRay(float2 random)
{
    // Converts spherical coordinates to direction (ray)

    const float phi = random.x * g_Pi * 2.0;

    const float cosTheta = sqrt(random.y);
    const float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    float3 ray;
    ray.x = sinTheta * cos(phi);
    ray.y = sinTheta * sin(phi);
    ray.z = cosTheta;

    return ray;
}

void BuildOrthonormalBasis(float3 normal, out float3 outTangent, out float3 outBitangent)
{
    const float3 upDir = abs(normal.y) < 0.9999 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);

    outTangent = normalize(cross(upDir, normal));
    outBitangent = cross(normal, outTangent);
}

float3 CalcShadowRayDirection(float3 toLightDirection, float tanLightAngularRadius)
{
    float2 blueNoise = GetBlueNoise();
    blueNoise = CreateRandomUnitRay(blueNoise).xy;
    blueNoise *= tanLightAngularRadius;

    float3 toLightTangent;
    float3 toLightBitangent;
    BuildOrthonormalBasis(toLightDirection, toLightTangent, toLightBitangent);

    float3 rayDirection = toLightDirection;
    rayDirection += toLightTangent * blueNoise.x;
    rayDirection += toLightBitangent * blueNoise.y;
    rayDirection = normalize(rayDirection);

    return rayDirection;
}

float TraceShadowRay(float depth)
{
    const uint2 pixelPosition = DispatchRaysIndex().xy;
    const float3 worldNormal = g_WorldNormal[pixelPosition].xyz;

    const float2 pixelUv = (pixelPosition + 0.5) / DispatchRaysDimensions().xy;
    const float3 worldPosition = ReconstructWorldPosition(pixelUv, depth, GetCameraConsts().ClipToView, GetCameraConsts().ViewToWorld);

    const float3 toLightDirection = g_SunLightConsts.WorldPosition;
    const float distanceToLight = sigma::g_Fp16Max;
    const float tanLightAngularRadius = g_SunLightConsts.WorldRadius;

    RayDesc rayDesc;
    rayDesc.Origin = OffsetRayPosition(worldPosition, worldNormal);
    rayDesc.Direction = CalcShadowRayDirection(toLightDirection, tanLightAngularRadius);
    rayDesc.TMin = 0.01;
    rayDesc.TMax = distanceToLight;

    // Ref: https://github.com/microsoft/DirectX-Specs/blob/master/d3d/Raytracing.md#ray-flags
    uint rayFlags = RAY_FLAG_NONE;
    rayFlags |= RAY_FLAG_FORCE_OPAQUE; // Skip any hit shaders
    rayFlags |= RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES;

    joint::RayTracing_ShadowPayload payload;
    payload.m_DistanceToOccluder = 0.0;

    const uint g_InstanceMask = ~0;
    const uint g_HitGroupIndex = 0;
    const uint g_HitGroupStride = 1;
    const uint g_MissShaderIndex = 0;
    TraceRay(
        g_SceneTlas,
        rayFlags,
        g_InstanceMask,
        g_HitGroupIndex,
        g_HitGroupStride,
        g_MissShaderIndex,
        rayDesc,
        payload);

    const float penumbra = sigma::PackPenumbra(payload.m_DistanceToOccluder, tanLightAngularRadius);
    return penumbra;
}

[shader("raygeneration")]
void RayGeneration()
{
    const uint2 pixelPosition = DispatchRaysIndex().xy;

    const float depth = g_Depth[pixelPosition];
    const float isNeeded = g_PassConsts.m_IsShadowsEnabled && depth != 0.0;
    const float penumbra = isNeeded ? TraceShadowRay(depth) : sigma::g_Fp16Max;

    g_NoisyPenumbra[pixelPosition] = penumbra;
}

[shader("closesthit")]
void ClosestHit(inout joint::RayTracing_ShadowPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    payload.m_DistanceToOccluder = RayTCurrent();
}

[shader("miss")]
void Miss(inout joint::RayTracing_ShadowPayload payload)
{
    payload.m_DistanceToOccluder = sigma::g_Fp16Max;
}
