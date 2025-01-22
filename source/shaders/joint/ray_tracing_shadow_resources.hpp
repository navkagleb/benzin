#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    struct RayTracing_ShadowConsts
    {
        uint IsBlueNoiseUsed : 1;
        uint IsNoiseAnimated : 1;
    };

    struct RayTracing_ShadowPayload
    {
        float DistanceToOccluder;
    };

    enum class Rc_RayTracing_Shadow : uint // TODO: Ugly name
    {
        WorldNormal,
        Depth,
        BlueNoise,

        OutNoisyPenumbra,
    };

}

#if !defined(__cplusplus) && !defined(BenzinRenderPassConstsType0)
    #define BenzinRenderPassConstsType0 joint::RayTracing_ShadowConsts
#endif
