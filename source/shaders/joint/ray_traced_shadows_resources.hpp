#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    struct RayTracedShadowsConsts
    {
        float3 ToSunDirection;
        float TanSunAngularRadius;
        float3 ToSunTangent;
        float SunAngularRadiusInRadians;
        float3 ToSunBitangent;
        uint IsBlueNoiseUsed : 1;
        uint IsNoiseAnimated : 1;
    };

    struct ShadowRayPayload
    {
        float THit;
    };

    enum class Rc_RayTracedShadows : uint
    {
        WorldNormal,
        Depth,
        BlueNoise,

        OutNoisyPenumbra,
    };
    BenzinEnableUnaryPlusForEnum(Rc_RayTracedShadows);

}

#if !defined(__cplusplus)
    #define RenderPassConstantsType joint::RayTracedShadowsConsts
#endif
