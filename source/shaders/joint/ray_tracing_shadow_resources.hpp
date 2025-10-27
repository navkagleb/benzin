#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    struct RayTracing_ShadowConsts
    {
        uint m_IsShadowsEnabled : 1;
        uint m_IsBlueNoiseUsed : 1;
        uint m_IsNoiseAnimated : 1;
    };

    struct RayTracing_ShadowPayload
    {
        float m_DistanceToOccluder;
    };

    enum class RayTracing_ShadowResources : uint
    {
        WorldNormal,
        Depth,
        BlueNoise,
        NoisyPenumbra,
    };

}

#if !defined(__cplusplus) && !defined(BenzinRenderPassConstsType0)
    #define BenzinRenderPassConstsType0 joint::RayTracing_ShadowConsts
#endif
