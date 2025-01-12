#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    struct RayTracing_ShadowConsts
    {
        float3 ToSunDirection;
        float TanSunAngularRadius;
        float3 ToSunTangent;
        float SunAngularRadiusInRadians;
        float3 ToSunBitangent;
        uint IsBlueNoiseUsed : 1;
        uint IsNoiseAnimated : 1;
    };

    struct RayTracing_ShadowPayload
    {
        float THit;
    };

    enum class Rc_RayTracing_Shadow : uint // TODO: Ugly name
    {
        WorldNormal,
        Depth,
        BlueNoise,

        OutNoisyPenumbra,
    };
    BenzinEnableUnaryPlusForEnum(Rc_RayTracing_Shadow);

}
